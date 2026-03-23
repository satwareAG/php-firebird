/*
 * pdo_fbird — connection driver (T10, T11, T15)
 * DSN parsing, handle factory/closer, transactions, attributes
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "zend_smart_str.h"
#include "ext/pdo/php_pdo.h"
#include "ext/pdo/php_pdo_driver.h"
#include "php_pdo_fbird.h"
#include "php_pdo_fbird_int.h"
#include <ibase.h>
#include "../firebird_utils.h"
#include "../php_firebird.h"
#include "../php_fbird_includes.h"

extern const struct pdo_stmt_methods pdo_fbird_stmt_methods;

/* Named-param preprocessor (defined in pdo_fbird_stmt.c) */
extern zend_string *php_firebird_preprocess(const char *sql, size_t sql_len,
	pdo_fbird_named_param **out_params, unsigned int *out_count);

/* Forward declarations */
static void pdo_fbird_fetch_error_func(pdo_dbh_t *dbh, pdo_stmt_t *stmt, zval *info);

/* Helper: build TPB flags from handle isolation_level + writable */
static zend_long _pdo_fbird_tpb_flags(pdo_fbird_db_handle *H, int for_autocommit)
{
	zend_long flags = 0;

	/* Access mode */
	flags |= H->writable ? PHP_FBIRD_WRITE : PHP_FBIRD_READ;

	/* Isolation level */
	switch (H->isolation_level) {
		case PDO_FBIRD_TXN_SERIALIZABLE:
			flags |= PHP_FBIRD_CONSISTENCY;
			break;
		case PDO_FBIRD_TXN_REPEATABLE_READ:
			flags |= PHP_FBIRD_CONCURRENCY;
			break;
		case PDO_FBIRD_TXN_READ_COMMITTED:
		default:
			flags |= PHP_FBIRD_COMMITTED | PHP_FBIRD_REC_VERSION;
			break;
	}

	/* Wait mode */
	flags |= PHP_FBIRD_WAIT;

	return flags;
}

/* Helper: start a transaction using master + attachment with TPB */
static void* _pdo_fbt_start(pdo_fbird_db_handle *H)
{
	void *att = fbc_get_attachment(H->fbc_conn);
	zend_long flags = _pdo_fbird_tpb_flags(H, !H->in_manually_transaction);
	unsigned tpb_len = 0;
	unsigned char *tpb = fbxpb_build_tpb(
		IBG(master_instance), flags, 0, &tpb_len, H->status);
	void *trans = fbt_start(IBG(master_instance), att, tpb_len, tpb, H->status);
	fbxpb_free_tpb(tpb);
	return trans;
}

/* {{{ pdo_fbird_handle_closer */
static void pdo_fbird_handle_closer(pdo_dbh_t *dbh)
{
	pdo_fbird_db_handle *H = (pdo_fbird_db_handle *)dbh->driver_data;
	if (!H) return;

	if (H->fbt_trans) {
		fbt_rollback(H->fbt_trans, H->status);
		fbt_free(H->fbt_trans);
		H->fbt_trans = NULL;
	}

	if (H->fbsvc_service) {
		fbsvc_detach(IBG(master_instance), H->fbsvc_service, H->status);
		fbsvc_free(H->fbsvc_service);
		H->fbsvc_service = NULL;
	}

	if (H->fbc_conn) {
		fbc_disconnect(H->fbc_conn, H->status);
		H->fbc_conn = NULL;
	}

	if (H->host)    { efree(H->host);    H->host    = NULL; }
	if (H->charset) { efree(H->charset); H->charset = NULL; }
	if (H->role)    { efree(H->role);    H->role    = NULL; }
	if (H->date_format) { efree(H->date_format); H->date_format = NULL; }
	if (H->time_format) { efree(H->time_format); H->time_format = NULL; }
	if (H->timestamp_format) { efree(H->timestamp_format); H->timestamp_format = NULL; }

	efree(H);
	dbh->driver_data = NULL;
}
/* }}} */

/* {{{ pdo_fbird_handle_preparer */
static bool pdo_fbird_handle_preparer(pdo_dbh_t *dbh, zend_string *sql,
	pdo_stmt_t *stmt, zval *driver_options)
{
	pdo_fbird_db_handle *H = (pdo_fbird_db_handle *)dbh->driver_data;

	pdo_fbird_stmt *S = ecalloc(1, sizeof(pdo_fbird_stmt));
	S->H = H;
	stmt->driver_data = S;
	stmt->methods = &pdo_fbird_stmt_methods;
	/* We support both :name and ? — preprocess :name→? ourselves,
	   then tell PDO we handle named placeholders so it routes
	   named params through param_hook for positional resolution. */
	stmt->supports_placeholders = PDO_PLACEHOLDER_NAMED;

	/* Check for scrollable cursor request */
	if (driver_options) {
		zend_long cursor_type = pdo_attr_lval(driver_options, PDO_ATTR_CURSOR, PDO_CURSOR_FWDONLY);
		S->scrollable = (cursor_type == PDO_CURSOR_SCROLL) ? 1 : 0;
	}

	/* Preprocess: convert :name → ? and build name→position map */
	pdo_fbird_named_param *np = NULL;
	unsigned int np_count = 0;
	zend_string *rewritten = php_firebird_preprocess(
		ZSTR_VAL(sql), ZSTR_LEN(sql), &np, &np_count);

	const char *prepare_sql;
	unsigned prepare_len;
	if (rewritten) {
		prepare_sql = ZSTR_VAL(rewritten);
		prepare_len = (unsigned)ZSTR_LEN(rewritten);
		S->named_params = np;
		S->named_param_count = np_count;
	} else {
		prepare_sql = ZSTR_VAL(sql);
		prepare_len = (unsigned)ZSTR_LEN(sql);
	}

	if (!H->fbt_trans) {
		H->fbt_trans = _pdo_fbt_start(H);
		if (!H->fbt_trans) {
			pdo_fbird_error(dbh);
			efree(S);
			stmt->driver_data = NULL;
			return 0;
		}
	}

	void *att = fbc_get_attachment(H->fbc_conn);
	void *tr  = fbt_get_handle(H->fbt_trans);

	S->fbs_stmt = fbs_prepare(
		IBG(master_instance), att, tr,
		prepare_sql, prepare_len,
		H->dialect, S->status
	);

	if (rewritten) {
		zend_string_release(rewritten);
	}

	if (!S->fbs_stmt) {
		pdo_fbird_stmt_error(stmt);
		if (S->named_params) { efree(S->named_params); }
		efree(S);
		stmt->driver_data = NULL;
		return 0;
	}

	S->out_meta  = fbs_get_output_metadata(IBG(master_instance), S->fbs_stmt, S->status);
	S->out_count = S->out_meta ? fbm_get_count(IBG(master_instance), S->out_meta) : 0;
	if (S->out_meta && S->out_count > 0) {
		unsigned msg_len = fbm_get_message_length(IBG(master_instance), S->out_meta);
		S->out_buf = ecalloc(1, msg_len);
	}

	S->in_meta  = fbs_get_input_metadata(IBG(master_instance), S->fbs_stmt, S->status);
	S->in_count = S->in_meta ? fbm_get_count(IBG(master_instance), S->in_meta) : 0;
	if (S->in_meta && S->in_count > 0) {
		unsigned msg_len = fbm_get_message_length(IBG(master_instance), S->in_meta);
		S->in_buf = ecalloc(1, msg_len);
	}

	stmt->column_count = (int)S->out_count;
	return 1;
}
/* }}} */

/* {{{ pdo_fbird_handle_doer */
static zend_long pdo_fbird_handle_doer(pdo_dbh_t *dbh, const zend_string *sql)
{
	pdo_fbird_db_handle *H = (pdo_fbird_db_handle *)dbh->driver_data;

	if (!H->fbt_trans) {
		H->fbt_trans = _pdo_fbt_start(H);
		if (!H->fbt_trans) {
			pdo_fbird_error(dbh);
			return -1;
		}
	}

	void *att = fbc_get_attachment(H->fbc_conn);
	void *tr  = fbt_get_handle(H->fbt_trans);

	ISC_STATUS_ARRAY st = {0};
	void *fbs = fbs_prepare(IBG(master_instance), att, tr,
		ZSTR_VAL(sql), (unsigned)ZSTR_LEN(sql), H->dialect, st);
	if (!fbs) {
		memcpy(H->status, st, sizeof(ISC_STATUS_ARRAY));
		pdo_fbird_error(dbh);
		return -1;
	}

	int rc = fbs_execute(IBG(master_instance), fbs, tr,
		NULL, NULL, NULL, NULL, st);
	if (!rc) {
		memcpy(H->status, st, sizeof(ISC_STATUS_ARRAY));
		fbs_free(fbs, st);
		pdo_fbird_error(dbh);
		return -1;
	}

	zend_long affected = (zend_long)fbs_get_affected_records(
		IBG(master_instance), fbs, st);
	fbs_free(fbs, st);

	if (H->autocommit && !H->in_manually_transaction) {
		fbt_commit_retaining(H->fbt_trans, H->status);
	}

	return affected >= 0 ? affected : 0;
}
/* }}} */

/* {{{ Transaction methods */
static bool pdo_fbird_handle_begin(pdo_dbh_t *dbh)
{
	pdo_fbird_db_handle *H = (pdo_fbird_db_handle *)dbh->driver_data;

	if (H->fbt_trans) {
		fbt_commit(H->fbt_trans, H->status);
		fbt_free(H->fbt_trans);
		H->fbt_trans = NULL;
	}

	H->in_manually_transaction = 1;
	H->fbt_trans = _pdo_fbt_start(H);
	if (!H->fbt_trans) {
		H->in_manually_transaction = 0;
		pdo_fbird_error(dbh);
		return false;
	}
	return true;
}

static bool pdo_fbird_handle_commit(pdo_dbh_t *dbh)
{
	pdo_fbird_db_handle *H = (pdo_fbird_db_handle *)dbh->driver_data;
	if (!H->fbt_trans) return true;

	int rc = fbt_commit(H->fbt_trans, H->status);
	fbt_free(H->fbt_trans);
	H->fbt_trans = NULL;
	H->in_manually_transaction = 0;

	if (rc != 0) { pdo_fbird_error(dbh); return false; }
	return true;
}

static bool pdo_fbird_handle_rollback(pdo_dbh_t *dbh)
{
	pdo_fbird_db_handle *H = (pdo_fbird_db_handle *)dbh->driver_data;
	if (!H->fbt_trans) return true;

	fbt_rollback(H->fbt_trans, H->status);
	fbt_free(H->fbt_trans);
	H->fbt_trans = NULL;
	H->in_manually_transaction = 0;
	return true;
}
/* }}} */

/* {{{ Service API helpers */

/* Ensure service manager is attached; returns 1 on success, 0 on error */
static int _pdo_fbird_service_ensure_attached(pdo_dbh_t *dbh)
{
	pdo_fbird_db_handle *H = (pdo_fbird_db_handle *)dbh->driver_data;
	if (H->fbsvc_service && fbsvc_is_attached(H->fbsvc_service)) {
		return 1;
	}
	/* Build SPB with user + password */
	char spb[256];
	unsigned short p = 0;
	spb[p++] = isc_spb_version;
	spb[p++] = isc_spb_current_version;

	const char *user = dbh->username ? dbh->username : "SYSDBA";
	size_t ulen = strlen(user);
	if (ulen > 0) {
		spb[p++] = isc_spb_user_name;
		spb[p++] = (char)ulen;
		memcpy(&spb[p], user, ulen);
		p += ulen;
	}
	const char *pass = dbh->password ? dbh->password : "";
	size_t plen = strlen(pass);
	if (plen > 0) {
		spb[p++] = isc_spb_password;
		spb[p++] = (char)plen;
		memcpy(&spb[p], pass, plen);
		p += plen;
	}

	/* Build service name: host:service_mgr or just service_mgr */
	char svc_name[256] = "service_mgr";
	if (H->host && H->host[0]) {
		snprintf(svc_name, sizeof(svc_name), "%s:service_mgr", H->host);
	}

	if (H->fbsvc_service) {
		fbsvc_detach(IBG(master_instance), H->fbsvc_service, H->status);
		fbsvc_free(H->fbsvc_service);
		H->fbsvc_service = NULL;
	}

	H->fbsvc_service = fbsvc_attach(IBG(master_instance), svc_name, p,
		(const unsigned char *)spb, H->status);
	if (!H->fbsvc_service) {
		pdo_fbird_error(dbh);
		return 0;
	}
	return 1;
}

/* Query service for a single text line result (server version, etc.) */
static char *_pdo_fbird_service_query_line(pdo_dbh_t *dbh, char info_action)
{
	pdo_fbird_db_handle *H = (pdo_fbird_db_handle *)dbh->driver_data;
	static char spb[] = { isc_info_svc_timeout, 10, 0, 0, 0 };
	char res_buf[512];

	if (!fbsvc_query(IBG(master_instance), H->fbsvc_service,
			sizeof(spb), (const unsigned char *)spb,
			1, (const unsigned char *)&info_action,
			sizeof(res_buf), (unsigned char *)res_buf, H->status)) {
		return NULL;
	}

	char *result = res_buf;
	while (*result != isc_info_end) {
		switch (*result++) {
			case isc_info_svc_server_version:
			case isc_info_svc_implementation:
			case isc_info_svc_get_env:
			case isc_info_svc_get_env_lock:
			case isc_info_svc_get_env_msg:
			case isc_info_svc_user_dbpath: {
				int len = isc_vax_integer(result, 2);
				char *str = emalloc(len + 1);
				memcpy(str, result + 2, len);
				str[len] = '\0';
				return str;
			}
			default:
				return NULL;
		}
	}
	return NULL;
}

/* Query service for multi-line output (db stats, backup verbose) */
static char *_pdo_fbird_service_query_lines(pdo_dbh_t *dbh)
{
	pdo_fbird_db_handle *H = (pdo_fbird_db_handle *)dbh->driver_data;
	static char spb[] = { isc_info_svc_timeout, 10, 0, 0, 0 };
	char info_action = isc_info_svc_line;
	char res_buf[4096];
	smart_str output = {0};

	for (;;) {
		if (!fbsvc_query(IBG(master_instance), H->fbsvc_service,
				sizeof(spb), (const unsigned char *)spb,
				1, (const unsigned char *)&info_action,
				sizeof(res_buf), (unsigned char *)res_buf, H->status)) {
			smart_str_free(&output);
			return NULL;
		}

		char *result = res_buf;
		int done = 0;
		while (*result != isc_info_end && !done) {
			switch (*result++) {
				case isc_info_svc_line: {
					int len = isc_vax_integer(result, 2);
					if (len == 0) { done = 1; break; }
					result += 2;
					smart_str_appendl(&output, result, len);
					smart_str_appendc(&output, '\n');
					result += len;
					break;
				}
				default:
					done = 1;
					break;
			}
		}
		if (done) break;
	}

	if (output.s) {
		smart_str_0(&output);
		char *ret = estrdup(ZSTR_VAL(output.s));
		smart_str_free(&output);
		return ret;
	}
	return estrdup("");
}

/* Build user SPB for add/modify/delete user */
static int _pdo_fbird_service_user_op(pdo_dbh_t *dbh, char operation, zval *val)
{
	pdo_fbird_db_handle *H = (pdo_fbird_db_handle *)dbh->driver_data;

	if (Z_TYPE_P(val) != IS_ARRAY) {
		pdo_raise_impl_error(dbh, NULL, "HY000",
			"Service user operation requires an array with 'username' key");
		return 0;
	}

	HashTable *ht = Z_ARRVAL_P(val);
	zval *z_user = zend_hash_str_find(ht, "username", sizeof("username") - 1);
	if (!z_user || Z_TYPE_P(z_user) != IS_STRING) {
		pdo_raise_impl_error(dbh, NULL, "HY000",
			"Service user operation requires 'username' string");
		return 0;
	}

	char buf[512];
	unsigned short p = 0;
	buf[p++] = operation;

	/* username (required) */
	size_t ulen = Z_STRLEN_P(z_user);
	buf[p++] = isc_spb_sec_username;
	buf[p++] = (char)ulen;
	buf[p++] = (char)(ulen >> 8);
	memcpy(&buf[p], Z_STRVAL_P(z_user), ulen);
	p += ulen;

	/* password (optional for modify/delete) */
	zval *z_pass = zend_hash_str_find(ht, "password", sizeof("password") - 1);
	if (z_pass && Z_TYPE_P(z_pass) == IS_STRING && Z_STRLEN_P(z_pass) > 0) {
		size_t plen = Z_STRLEN_P(z_pass);
		buf[p++] = isc_spb_sec_password;
		buf[p++] = (char)plen;
		buf[p++] = (char)(plen >> 8);
		memcpy(&buf[p], Z_STRVAL_P(z_pass), plen);
		p += plen;
	}

	/* first_name (optional) */
	zval *z_fn = zend_hash_str_find(ht, "first_name", sizeof("first_name") - 1);
	if (z_fn && Z_TYPE_P(z_fn) == IS_STRING && Z_STRLEN_P(z_fn) > 0) {
		size_t len = Z_STRLEN_P(z_fn);
		buf[p++] = isc_spb_sec_firstname;
		buf[p++] = (char)len;
		buf[p++] = (char)(len >> 8);
		memcpy(&buf[p], Z_STRVAL_P(z_fn), len);
		p += len;
	}

	/* last_name (optional) */
	zval *z_ln = zend_hash_str_find(ht, "last_name", sizeof("last_name") - 1);
	if (z_ln && Z_TYPE_P(z_ln) == IS_STRING && Z_STRLEN_P(z_ln) > 0) {
		size_t len = Z_STRLEN_P(z_ln);
		buf[p++] = isc_spb_sec_lastname;
		buf[p++] = (char)len;
		buf[p++] = (char)(len >> 8);
		memcpy(&buf[p], Z_STRVAL_P(z_ln), len);
		p += len;
	}

	if (!fbsvc_start(IBG(master_instance), H->fbsvc_service,
			p, (const unsigned char *)buf, H->status)) {
		pdo_fbird_error(dbh);
		return 0;
	}
	return 1;
}
/* }}} */

/* {{{ Attribute get/set */
static bool pdo_fbird_handle_set_attribute(pdo_dbh_t *dbh, zend_long attr, zval *val)
{
	pdo_fbird_db_handle *H = (pdo_fbird_db_handle *)dbh->driver_data;
	switch (attr) {
		case PDO_ATTR_AUTOCOMMIT:
			H->autocommit = zval_is_true(val) ? 1 : 0;
			return true;
		case PDO_FBIRD_ATTR_DIALECT:
			H->dialect = (int)zval_get_long(val);
			return true;
		case PDO_FBIRD_ATTR_CHARSET:
			if (H->charset) efree(H->charset);
			H->charset = estrdup(Z_STRVAL_P(val));
			return true;
		case PDO_FBIRD_ATTR_ROLE:
			if (H->role) efree(H->role);
			H->role = estrdup(Z_STRVAL_P(val));
			return true;
		case PDO_FBIRD_ATTR_TRANSACTION_ISOLATION_LEVEL: {
			int level = (int)zval_get_long(val);
			if (level < PDO_FBIRD_TXN_READ_COMMITTED || level > PDO_FBIRD_TXN_SERIALIZABLE) {
				return false;
			}
			H->isolation_level = level;
			return true;
		}
		case PDO_FBIRD_ATTR_WRITABLE_TRANSACTION:
			H->writable = zval_is_true(val) ? 1 : 0;
			return true;
		case PDO_FBIRD_ATTR_DATE_FORMAT:
			if (H->date_format) efree(H->date_format);
			H->date_format = estrdup(Z_STRVAL_P(val));
			return true;
		case PDO_FBIRD_ATTR_TIME_FORMAT:
			if (H->time_format) efree(H->time_format);
			H->time_format = estrdup(Z_STRVAL_P(val));
			return true;
		case PDO_FBIRD_ATTR_TIMESTAMP_FORMAT:
			if (H->timestamp_format) efree(H->timestamp_format);
			H->timestamp_format = estrdup(Z_STRVAL_P(val));
			return true;
		case PDO_FBIRD_ATTR_FETCH_TABLE_NAMES:
			H->fetch_table_names = zval_is_true(val) ? 1 : 0;
			return true;
		case PDO_FBIRD_ATTR_SET_BIND: {
			/* Execute SET BIND statement (Firebird 4+ only) */
			if (Z_TYPE_P(val) != IS_STRING || Z_STRLEN_P(val) == 0) {
				return false;
			}
			char buf[256];
			snprintf(buf, sizeof(buf), "SET BIND OF %s", Z_STRVAL_P(val));
			zend_string *sql = zend_string_init(buf, strlen(buf), 0);
			zend_long rows = pdo_fbird_handle_doer(dbh, sql);
			zend_string_release(sql);
			return (rows != -1) ? true : false;
		}

		/* Service API attributes */
		case PDO_FBIRD_ATTR_SERVICE_ATTACH:
			return _pdo_fbird_service_ensure_attached(dbh) ? true : false;

		case PDO_FBIRD_ATTR_SERVICE_DETACH:
			if (H->fbsvc_service) {
				fbsvc_detach(IBG(master_instance), H->fbsvc_service, H->status);
				fbsvc_free(H->fbsvc_service);
				H->fbsvc_service = NULL;
			}
			return true;

		case PDO_FBIRD_ATTR_SERVICE_BACKUP:
		case PDO_FBIRD_ATTR_SERVICE_RESTORE: {
			if (!_pdo_fbird_service_ensure_attached(dbh)) return false;
			if (Z_TYPE_P(val) != IS_ARRAY) {
				pdo_raise_impl_error(dbh, NULL, "HY000",
					"Backup/restore requires array with 'database' and 'backup_file' keys");
				return false;
			}
			HashTable *ht = Z_ARRVAL_P(val);
			zval *z_db = zend_hash_str_find(ht, "database", sizeof("database") - 1);
			zval *z_bk = zend_hash_str_find(ht, "backup_file", sizeof("backup_file") - 1);
			if (!z_db || Z_TYPE_P(z_db) != IS_STRING || !z_bk || Z_TYPE_P(z_bk) != IS_STRING) {
				pdo_raise_impl_error(dbh, NULL, "HY000",
					"Backup/restore requires 'database' and 'backup_file' string keys");
				return false;
			}
			zval *z_opts = zend_hash_str_find(ht, "options", sizeof("options") - 1);
			zend_long opts = z_opts ? zval_get_long(z_opts) : 0;
			zval *z_verbose = zend_hash_str_find(ht, "verbose", sizeof("verbose") - 1);
			int verbose = z_verbose ? zval_is_true(z_verbose) : 0;

			char operation = (attr == PDO_FBIRD_ATTR_SERVICE_BACKUP)
				? isc_action_svc_backup : isc_action_svc_restore;
			size_t dblen = Z_STRLEN_P(z_db);
			size_t bklen = Z_STRLEN_P(z_bk);
			char spb_buf[512];
			int spb_len = slprintf(spb_buf, sizeof(spb_buf),
				"%c%c%c%c%s%c%c%c%s%c%c%c%c%c",
				operation,
				isc_spb_dbname, (char)dblen, (char)(dblen >> 8), Z_STRVAL_P(z_db),
				isc_spb_bkp_file, (char)bklen, (char)(bklen >> 8), Z_STRVAL_P(z_bk),
				isc_spb_options,
				(char)opts, (char)(opts >> 8), (char)(opts >> 16), (char)(opts >> 24));
			if (verbose) spb_buf[spb_len++] = isc_spb_verbose;

			if (!fbsvc_start(IBG(master_instance), H->fbsvc_service,
					(unsigned short)spb_len, (const unsigned char *)spb_buf, H->status)) {
				pdo_fbird_error(dbh);
				return false;
			}
			return true;
		}

		case PDO_FBIRD_ATTR_SERVICE_ADD_USER:
			if (!_pdo_fbird_service_ensure_attached(dbh)) return false;
			return _pdo_fbird_service_user_op(dbh, isc_action_svc_add_user, val) ? true : false;

		case PDO_FBIRD_ATTR_SERVICE_MODIFY_USER:
			if (!_pdo_fbird_service_ensure_attached(dbh)) return false;
			return _pdo_fbird_service_user_op(dbh, isc_action_svc_modify_user, val) ? true : false;

		case PDO_FBIRD_ATTR_SERVICE_DELETE_USER:
			if (!_pdo_fbird_service_ensure_attached(dbh)) return false;
			return _pdo_fbird_service_user_op(dbh, isc_action_svc_delete_user, val) ? true : false;

		case PDO_FBIRD_ATTR_SERVICE_DB_STATS: {
			if (!_pdo_fbird_service_ensure_attached(dbh)) return false;
			if (Z_TYPE_P(val) != IS_ARRAY) {
				pdo_raise_impl_error(dbh, NULL, "HY000",
					"DB stats requires array with 'database' and 'options' keys");
				return false;
			}
			HashTable *ht = Z_ARRVAL_P(val);
			zval *z_db = zend_hash_str_find(ht, "database", sizeof("database") - 1);
			zval *z_opts = zend_hash_str_find(ht, "options", sizeof("options") - 1);
			if (!z_db || Z_TYPE_P(z_db) != IS_STRING || !z_opts) {
				pdo_raise_impl_error(dbh, NULL, "HY000",
					"DB stats requires 'database' string and 'options' integer");
				return false;
			}
			size_t dblen = Z_STRLEN_P(z_db);
			zend_long action = zval_get_long(z_opts);
			zend_long argument = action;
			action = isc_spb_options;
			char spb_buf[256];
			int spb_len = slprintf(spb_buf, sizeof(spb_buf),
				"%c%c%c%c%s%c%c%c%c%c",
				(char)isc_action_svc_db_stats,
				isc_spb_dbname, (char)dblen, (char)(dblen >> 8), Z_STRVAL_P(z_db),
				(char)action, (char)argument, (char)(argument >> 8),
				(char)(argument >> 16), (char)(argument >> 24));

			if (!fbsvc_start(IBG(master_instance), H->fbsvc_service,
					(unsigned short)spb_len, (const unsigned char *)spb_buf, H->status)) {
				pdo_fbird_error(dbh);
				return false;
			}
			return true;
		}
	}
	return false;
}

static int pdo_fbird_handle_get_attribute(pdo_dbh_t *dbh, zend_long attr, zval *val)
{
	pdo_fbird_db_handle *H = (pdo_fbird_db_handle *)dbh->driver_data;
	switch (attr) {
		case PDO_ATTR_SERVER_VERSION:
		case PDO_ATTR_SERVER_INFO: {
			unsigned v = fbc_get_server_version(H->fbc_conn);
			char buf[32];
			snprintf(buf, sizeof(buf), "%u.0", v / 10);
			ZVAL_STRING(val, buf);
			return 1;
		}
		case PDO_ATTR_CLIENT_VERSION:
			ZVAL_STRING(val, "Firebird OO API");
			return 1;
		case PDO_ATTR_DRIVER_NAME:
			ZVAL_STRING(val, "fbird");
			return 1;
		case PDO_ATTR_AUTOCOMMIT:
			ZVAL_BOOL(val, H->autocommit);
			return 1;
		case PDO_FBIRD_ATTR_DIALECT:
			ZVAL_LONG(val, H->dialect);
			return 1;
		case PDO_FBIRD_ATTR_CHARSET:
			ZVAL_STRING(val, H->charset ? H->charset : "");
			return 1;
		case PDO_FBIRD_ATTR_ROLE:
			ZVAL_STRING(val, H->role ? H->role : "");
			return 1;
		case PDO_ATTR_CONNECTION_STATUS:
			ZVAL_BOOL(val, H->fbc_conn && fbc_is_connected(H->fbc_conn));
			return 1;
		case PDO_FBIRD_ATTR_TRANSACTION_ISOLATION_LEVEL:
			ZVAL_LONG(val, H->isolation_level);
			return 1;
		case PDO_FBIRD_ATTR_WRITABLE_TRANSACTION:
			ZVAL_BOOL(val, H->writable);
			return 1;
		case PDO_FBIRD_ATTR_DATE_FORMAT:
			ZVAL_STRING(val, H->date_format ? H->date_format : "%Y-%m-%d");
			return 1;
		case PDO_FBIRD_ATTR_TIME_FORMAT:
			ZVAL_STRING(val, H->time_format ? H->time_format : "%H:%M:%S");
			return 1;
		case PDO_FBIRD_ATTR_TIMESTAMP_FORMAT:
			ZVAL_STRING(val, H->timestamp_format ? H->timestamp_format : "%Y-%m-%d %H:%M:%S");
			return 1;
		case PDO_FBIRD_ATTR_FETCH_TABLE_NAMES:
			ZVAL_BOOL(val, H->fetch_table_names);
			return 1;
		case PDO_FBIRD_ATTR_SET_BIND:
			/* SET BIND is write-only; return empty string for get */
			ZVAL_STRING(val, "");
			return 1;

		/* Service API get attributes */
		case PDO_FBIRD_ATTR_SERVICE_ATTACH:
			ZVAL_BOOL(val, H->fbsvc_service && fbsvc_is_attached(H->fbsvc_service));
			return 1;

		case PDO_FBIRD_ATTR_SERVICE_SERVER_VERSION: {
			if (!_pdo_fbird_service_ensure_attached(dbh)) {
				ZVAL_FALSE(val);
				return 1;
			}
			char *ver = _pdo_fbird_service_query_line(dbh, isc_info_svc_server_version);
			if (ver) {
				ZVAL_STRING(val, ver);
				efree(ver);
			} else {
				ZVAL_FALSE(val);
			}
			return 1;
		}

		case PDO_FBIRD_ATTR_SERVICE_SERVER_INFO: {
			if (!_pdo_fbird_service_ensure_attached(dbh)) {
				ZVAL_FALSE(val);
				return 1;
			}
			char *info = _pdo_fbird_service_query_line(dbh, isc_info_svc_implementation);
			if (info) {
				ZVAL_STRING(val, info);
				efree(info);
			} else {
				ZVAL_FALSE(val);
			}
			return 1;
		}

		case PDO_FBIRD_ATTR_SERVICE_DB_STATS: {
			/* After setAttribute(DB_STATS, [...]) starts the stats job,
			   getAttribute(DB_STATS) retrieves the output lines */
			if (!H->fbsvc_service || !fbsvc_is_attached(H->fbsvc_service)) {
				ZVAL_FALSE(val);
				return 1;
			}
			char *lines = _pdo_fbird_service_query_lines(dbh);
			if (lines) {
				ZVAL_STRING(val, lines);
				efree(lines);
			} else {
				ZVAL_FALSE(val);
			}
			return 1;
		}
	}
	return 0;
}
/* }}} */

/* {{{ pdo_fbird_check_liveness */
static zend_result pdo_fbird_check_liveness(pdo_dbh_t *dbh)
{
	pdo_fbird_db_handle *H = (pdo_fbird_db_handle *)dbh->driver_data;
	if (!H->fbc_conn || !fbc_is_connected(H->fbc_conn)) {
		return FAILURE;
	}
	/* Real server ping via isc_info roundtrip */
	return fbc_ping(IBG(master_instance), H->fbc_conn, H->status) ? SUCCESS : FAILURE;
}
/* }}} */

/* {{{ pdo_fbird_fetch_error_func */
static void pdo_fbird_fetch_error_func(pdo_dbh_t *dbh, pdo_stmt_t *stmt, zval *info)
{
	pdo_fbird_db_handle *H = (pdo_fbird_db_handle *)dbh->driver_data;
	ISC_STATUS *status = stmt
		? ((pdo_fbird_stmt *)stmt->driver_data)->status
		: H->status;

	long gds_code = 0;
	if (status && status[0] == 1 && status[1] > 0) {
		gds_code = status[1];
	}

	char msg[512] = {0};
	if (status && status[0] == 1 && status[1] > 0) {
		ISC_STATUS *p = status;
		char buf[256];
		while (fb_interpret(buf, sizeof(buf), (const ISC_STATUS **)&p)) {
			if (strlen(msg) + strlen(buf) + 2 < sizeof(msg)) {
				if (msg[0]) strncat(msg, " ", sizeof(msg) - strlen(msg) - 1);
				strncat(msg, buf, sizeof(msg) - strlen(msg) - 1);
			}
		}
	}

	add_next_index_long(info, gds_code);
	add_next_index_string(info, msg[0] ? msg : "Unknown Firebird error");
}
/* }}} */

/* {{{ pdo_fbird_handle_quoter */
static zend_string *pdo_fbird_handle_quoter(pdo_dbh_t *dbh, const zend_string *unquoted,
	enum pdo_param_type paramtype)
{
	const char *src = ZSTR_VAL(unquoted);
	size_t src_len = ZSTR_LEN(unquoted);

	/* Count single quotes to determine output size */
	size_t quote_count = 0;
	for (size_t i = 0; i < src_len; i++) {
		if (src[i] == '\'' ) quote_count++;
		if (src[i] == '\0') {
			/* NULL bytes not allowed in Firebird strings */
			return NULL;
		}
	}

	/* Output: opening quote + doubled quotes + closing quote */
	size_t out_len = src_len + quote_count + 2;
	zend_string *quoted = zend_string_alloc(out_len, 0);
	char *dst = ZSTR_VAL(quoted);

	*dst++ = '\'';
	for (size_t i = 0; i < src_len; i++) {
		if (src[i] == '\'') {
			*dst++ = '\'';
			*dst++ = '\'';
		} else {
			*dst++ = src[i];
		}
	}
	*dst++ = '\'';
	*dst = '\0';

	return quoted;
}
/* }}} */

/* {{{ pdo_fbird_handle_last_id
 * Return last generated value for a sequence/generator.
 * name = sequence name (required for Firebird). */
static zend_string *pdo_fbird_handle_last_id(pdo_dbh_t *dbh, const zend_string *name)
{
	pdo_fbird_db_handle *H = (pdo_fbird_db_handle *)dbh->driver_data;

	if (!name || ZSTR_LEN(name) == 0) {
		/* Firebird has no implicit last-insert-id; sequence name is required */
		return NULL;
	}

	if (ZSTR_LEN(name) > 31) {
		return NULL;
	}

	char query[128];
	snprintf(query, sizeof(query), "SELECT GEN_ID(%s,0) FROM rdb$database", ZSTR_VAL(name));

	void *attachment = fbc_get_attachment(H->fbc_conn);
	void *tr_handle = fbt_get_handle(H->fbt_trans);
	if (!attachment || !tr_handle) {
		return NULL;
	}

	void *stmt = fbs_prepare(IBG(master_instance), attachment, tr_handle,
		query, (unsigned)strlen(query), H->dialect, H->status);
	if (!stmt) {
		return NULL;
	}

	ISC_INT64 result = fbs_execute_singleton_int64(IBG(master_instance), stmt, tr_handle, H->status);

	if (H->status[0] == 1 && H->status[1] != 0) {
		fbs_free(stmt, H->status);
		return NULL;
	}

	fbs_free(stmt, H->status);

	char buf[32];
	int len = snprintf(buf, sizeof(buf), "%lld", (long long)result);
	return zend_string_init(buf, len, 0);
}
/* }}} */

/* {{{ pdo_fbird_dbh_methods */
const struct pdo_dbh_methods pdo_fbird_dbh_methods = {
	pdo_fbird_handle_closer,
	pdo_fbird_handle_preparer,
	pdo_fbird_handle_doer,
	pdo_fbird_handle_quoter,
	pdo_fbird_handle_begin,
	pdo_fbird_handle_commit,
	pdo_fbird_handle_rollback,
	pdo_fbird_handle_set_attribute,
	pdo_fbird_handle_last_id,
	pdo_fbird_fetch_error_func,
	pdo_fbird_handle_get_attribute,
	pdo_fbird_check_liveness,
	NULL, NULL, NULL, NULL,
};
/* }}} */

/* {{{ pdo_fbird_handle_factory */
static int pdo_fbird_handle_factory(pdo_dbh_t *dbh, zval *driver_options)
{
	pdo_fbird_db_handle *H = ecalloc(1, sizeof(pdo_fbird_db_handle));
	dbh->driver_data = H;
	H->dialect    = 3;
	H->autocommit = dbh->auto_commit ? 1 : 0;
	H->in_manually_transaction = 0;
	H->isolation_level = PDO_FBIRD_TXN_READ_COMMITTED;
	H->writable = 1;

	char host[256]    = {0};
	char dbname[1024] = {0};
	char charset[64]  = {0};
	char role[128]    = {0};
	int  dialect      = 3;

	const char *dsn  = dbh->data_source;
	char *dsn_copy   = estrdup(dsn);
	char *saveptr    = NULL;
	char *tok        = strtok_r(dsn_copy, ";", &saveptr);

	while (tok) {
		char *eq = strchr(tok, '=');
		if (eq) {
			*eq = '\0';
			const char *key = tok, *v = eq + 1;
			if      (!strcasecmp(key, "host"))    snprintf(host,    sizeof(host),    "%s", v);
			else if (!strcasecmp(key, "dbname"))  snprintf(dbname,  sizeof(dbname),  "%s", v);
			else if (!strcasecmp(key, "charset")) snprintf(charset, sizeof(charset), "%s", v);
			else if (!strcasecmp(key, "role"))    snprintf(role,    sizeof(role),    "%s", v);
			else if (!strcasecmp(key, "dialect")) dialect = atoi(v);
		}
		tok = strtok_r(NULL, ";", &saveptr);
	}
	efree(dsn_copy);

	H->dialect = dialect;
	if (host[0])    H->host    = estrdup(host);
	if (charset[0]) H->charset = estrdup(charset);
	if (role[0])    H->role    = estrdup(role);

	char connstr[1280] = {0};
	if (host[0]) snprintf(connstr, sizeof(connstr), "%s:%s", host, dbname);
	else         snprintf(connstr, sizeof(connstr), "%s", dbname);

	H->fbc_conn = fbc_connect(
		IBG(master_instance),
		connstr,    strlen(connstr),
		dbh->username  ? dbh->username  : "", dbh->username  ? strlen(dbh->username)  : 0,
		dbh->password  ? dbh->password  : "", dbh->password  ? strlen(dbh->password)  : 0,
		charset[0] ? charset : NULL,           charset[0] ? strlen(charset) : 0,
		role[0]    ? role    : NULL,           role[0]    ? strlen(role)    : 0,
		0,       /* num_buffers */
		dialect,
		-1,      /* force_write: not set */
		H->status
	);

	if (!H->fbc_conn) {
		pdo_fbird_error(dbh);
		if (H->host)    efree(H->host);
		if (H->charset) efree(H->charset);
		if (H->role)    efree(H->role);
		if (H->date_format) efree(H->date_format);
		if (H->time_format) efree(H->time_format);
		if (H->timestamp_format) efree(H->timestamp_format);
		efree(H);
		dbh->driver_data = NULL;
		return 0;
	}

	dbh->methods = &pdo_fbird_dbh_methods;
	dbh->alloc_own_columns = 1;
	return 1;
}
/* }}} */

/* {{{ pdo_fbird_driver */
const pdo_driver_t pdo_fbird_driver = {
	PDO_DRIVER_HEADER(fbird),
	pdo_fbird_handle_factory
};
/* }}} */
