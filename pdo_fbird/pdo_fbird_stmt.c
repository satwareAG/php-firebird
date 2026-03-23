/*
 * pdo_fbird — statement handler (T12, T13)
 * prepare, execute, fetch, describe, param binding
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "ext/pdo/php_pdo.h"
#include "ext/pdo/php_pdo_driver.h"
#include "php_pdo_fbird.h"
#include "php_pdo_fbird_int.h"
#include <ibase.h>
#include <ctype.h>
#include "../firebird_utils.h"
#include "../php_firebird.h"
#include "../php_fbird_includes.h"

/* {{{ php_firebird_preprocess — replace :name with ? and build name→position map */
zend_string *php_firebird_preprocess(const char *sql, size_t sql_len,
	pdo_fbird_named_param **out_params, unsigned int *out_count)
{
	/* Pass 1: count named params and compute output length */
	unsigned int count = 0;
	int in_string = 0;
	int in_comment = 0;
	int in_block = 0; /* inside EXECUTE BLOCK's BEGIN...END */

	for (size_t i = 0; i < sql_len; i++) {
		char c = sql[i];
		if (in_comment) {
			if (c == '*' && i + 1 < sql_len && sql[i + 1] == '/') {
				in_comment = 0; i++;
			}
			continue;
		}
		if (c == '/' && i + 1 < sql_len && sql[i + 1] == '*') {
			in_comment = 1; i++;
			continue;
		}
		if (c == '-' && i + 1 < sql_len && sql[i + 1] == '-') {
			while (i < sql_len && sql[i] != '\n') i++;
			continue;
		}
		if (c == '\'') { in_string = !in_string; continue; }
		if (in_string) continue;

		/* Detect BEGIN keyword (for EXECUTE BLOCK) */
		if (!in_block && (c == 'B' || c == 'b') && i + 5 <= sql_len) {
			if (strncasecmp(sql + i, "BEGIN", 5) == 0 &&
			    (i + 5 >= sql_len || !isalnum((unsigned char)sql[i + 5]))) {
				in_block = 1;
				i += 4;
				continue;
			}
		}

		/* Skip :name inside BEGIN...END block (those are PSQL variables) */
		if (in_block) {
			if ((c == 'E' || c == 'e') && i + 3 <= sql_len) {
				if (strncasecmp(sql + i, "END", 3) == 0 &&
				    (i + 3 >= sql_len || !isalnum((unsigned char)sql[i + 3]))) {
					in_block = 0;
					i += 2;
				}
			}
			continue;
		}

		if (c == ':' && i + 1 < sql_len && (isalpha((unsigned char)sql[i + 1]) || sql[i + 1] == '_')) {
			size_t start = i + 1;
			size_t end = start;
			while (end < sql_len && (isalnum((unsigned char)sql[end]) || sql[end] == '_')) end++;
			count++;
			i = end - 1;
		}
	}

	if (count == 0) {
		*out_params = NULL;
		*out_count = 0;
		return NULL; /* no named params, use original SQL */
	}

	/* Pass 2: build output SQL and param map */
	pdo_fbird_named_param *params = ecalloc(count, sizeof(pdo_fbird_named_param));
	zend_string *out = zend_string_alloc(sql_len + 16, 0); /* generous */
	char *dst = ZSTR_VAL(out);
	unsigned int pidx = 0;
	in_string = 0;
	in_comment = 0;
	in_block = 0;

	for (size_t i = 0; i < sql_len; i++) {
		char c = sql[i];
		if (in_comment) {
			*dst++ = c;
			if (c == '*' && i + 1 < sql_len && sql[i + 1] == '/') {
				in_comment = 0; *dst++ = sql[++i];
			}
			continue;
		}
		if (c == '/' && i + 1 < sql_len && sql[i + 1] == '*') {
			in_comment = 1; *dst++ = c; *dst++ = sql[++i];
			continue;
		}
		if (c == '-' && i + 1 < sql_len && sql[i + 1] == '-') {
			while (i < sql_len && sql[i] != '\n') { *dst++ = sql[i]; i++; }
			if (i < sql_len) *dst++ = sql[i];
			continue;
		}
		if (c == '\'') { in_string = !in_string; *dst++ = c; continue; }
		if (in_string) { *dst++ = c; continue; }

		if (!in_block && (c == 'B' || c == 'b') && i + 5 <= sql_len) {
			if (strncasecmp(sql + i, "BEGIN", 5) == 0 &&
			    (i + 5 >= sql_len || !isalnum((unsigned char)sql[i + 5]))) {
				in_block = 1;
				memcpy(dst, sql + i, 5); dst += 5;
				i += 4;
				continue;
			}
		}

		if (in_block) {
			if ((c == 'E' || c == 'e') && i + 3 <= sql_len) {
				if (strncasecmp(sql + i, "END", 3) == 0 &&
				    (i + 3 >= sql_len || !isalnum((unsigned char)sql[i + 3]))) {
					in_block = 0;
				}
			}
			*dst++ = c;
			continue;
		}

		if (c == ':' && i + 1 < sql_len && (isalpha((unsigned char)sql[i + 1]) || sql[i + 1] == '_')) {
			size_t start = i + 1;
			size_t end = start;
			while (end < sql_len && (isalnum((unsigned char)sql[end]) || sql[end] == '_')) end++;
			params[pidx].name = estrndup(sql + start, end - start);
			params[pidx].position = pidx;
			pidx++;
			*dst++ = '?';
			i = end - 1;
			continue;
		}

		*dst++ = c;
	}
	*dst = '\0';

	size_t final_len = dst - ZSTR_VAL(out);
	out = zend_string_truncate(out, final_len, 0);

	*out_params = params;
	*out_count = pidx;
	return out;
}
/* }}} */

/* Helper: read a null indicator from message buffer */
static int _pdo_fbird_is_null(pdo_fbird_stmt *S, unsigned idx)
{
	if (!S->out_meta) return 0;
	unsigned null_off = fbm_get_null_offset(IBG(master_instance), S->out_meta, idx);
	short null_flag = 0;
	memcpy(&null_flag, S->out_buf + null_off, sizeof(short));
	return (null_flag != 0);
}

/* {{{ pdo_fbird_stmt_dtor */
static int pdo_fbird_stmt_dtor(pdo_stmt_t *stmt)
{
	pdo_fbird_stmt *S = (pdo_fbird_stmt *)stmt->driver_data;
	if (!S) return 1;

	if (S->named_params) {
		for (unsigned int i = 0; i < S->named_param_count; i++) {
			if (S->named_params[i].name) efree(S->named_params[i].name);
		}
		efree(S->named_params);
		S->named_params = NULL;
	}

	if (S->out_buf)  { efree(S->out_buf);  S->out_buf  = NULL; }
	if (S->in_buf)   { efree(S->in_buf);   S->in_buf   = NULL; }

	/* Release metadata first (ref-counted, independent of statement lifetime) */
	if (S->out_meta) { fbm_release(S->out_meta); S->out_meta = NULL; }
	if (S->in_meta)  { fbm_release(S->in_meta);  S->in_meta  = NULL; }

	if (S->fbs_stmt) {
		/* Only close cursor if connection still alive and cursor open */
		if (S->has_rows && S->H && S->H->fbc_conn &&
		    fbc_is_connected(S->H->fbc_conn) &&
		    fbs_is_cursor_open(S->fbs_stmt)) {
			fbs_close_cursor(S->fbs_stmt, S->status);
		}
		fbs_free(S->fbs_stmt, S->status);
		S->fbs_stmt = NULL;
	}

	efree(S);
	stmt->driver_data = NULL;
	return 1;
}
/* }}} */

/* {{{ pdo_fbird_stmt_execute */
static int pdo_fbird_stmt_execute(pdo_stmt_t *stmt)
{
	pdo_fbird_stmt    *S = (pdo_fbird_stmt *)stmt->driver_data;
	pdo_fbird_db_handle *H = S->H;

	/* Close any previously open cursor */
	if (fbs_is_cursor_open(S->fbs_stmt)) {
		fbs_close_cursor(S->fbs_stmt, S->status);
	}
	S->has_rows = 0;

	void *tr = fbt_get_handle(H->fbt_trans);
	unsigned stmt_type = fbs_get_type(IBG(master_instance), S->fbs_stmt, S->status);

	/* SELECT / stored proc with output → open cursor */
	if (stmt_type == isc_info_sql_stmt_select ||
	    stmt_type == isc_info_sql_stmt_select_for_upd ||
	    stmt_type == isc_info_sql_stmt_exec_procedure) {

		int rc = fbs_open_cursor(
			IBG(master_instance), S->fbs_stmt, tr,
			S->in_buf, S->in_meta,
			0, S->status
		);
		if (!rc) {
			pdo_fbird_stmt_error(stmt);
			return 0;
		}
		S->has_rows = 1;
		stmt->row_count = -1; /* unknown for SELECT */
	} else {
		/* DML / DDL */
		int rc = fbs_execute(
			IBG(master_instance), S->fbs_stmt, tr,
			S->in_buf, S->in_meta,
			S->out_buf, S->out_meta,
			S->status
		);
		if (!rc) {
			pdo_fbird_stmt_error(stmt);
			return 0;
		}
		ISC_UINT64 aff = fbs_get_affected_records(
			IBG(master_instance), S->fbs_stmt, S->status);
		stmt->row_count = (zend_long)aff;

		/* Only autocommit for DML/DDL — never for SELECT (cursor still open) */
 	if (H->autocommit && !H->in_manually_transaction) {
 		fbt_commit_retaining(H->fbt_trans, H->status);
 	}
	}
	return 1;
}
/* }}} */

/* {{{ pdo_fbird_stmt_fetch */
static int pdo_fbird_stmt_fetch(pdo_stmt_t *stmt,
	enum pdo_fetch_orientation ori, zend_long offset)
{
	pdo_fbird_stmt *S = (pdo_fbird_stmt *)stmt->driver_data;

	if (!S->has_rows || !S->fbs_stmt) return 0;

	int rc = fbs_fetch(IBG(master_instance), S->fbs_stmt,
		S->out_buf, S->status);

	if (rc == 1) return 1;   /* row fetched */
	if (rc == 0) {           /* end of data */
		S->has_rows = 0;
		fbs_close_cursor(S->fbs_stmt, S->status);
		return 0;
	}
	/* error */
	pdo_fbird_stmt_error(stmt);
	S->has_rows = 0;
	return 0;
}
/* }}} */

/* {{{ pdo_fbird_stmt_describe */
static int pdo_fbird_stmt_describe(pdo_stmt_t *stmt, int colno)
{
	pdo_fbird_stmt *S = (pdo_fbird_stmt *)stmt->driver_data;
	if (!S->out_meta || (unsigned)colno >= S->out_count) return 0;

	struct pdo_column_data *col = &stmt->columns[colno];

	const char *alias = fbm_get_alias(IBG(master_instance), S->out_meta, (unsigned)colno);
	const char *field = fbm_get_field(IBG(master_instance), S->out_meta, (unsigned)colno);
	const char *name  = (alias && alias[0]) ? alias : (field ? field : "");

	col->name        = zend_string_init(name, strlen(name), 0);
	col->maxlen      = fbm_get_length(IBG(master_instance), S->out_meta, (unsigned)colno);
	col->precision   = (zend_long)fbm_get_scale(IBG(master_instance), S->out_meta, (unsigned)colno);

	(void)fbm_get_type(IBG(master_instance), S->out_meta, (unsigned)colno); /* type info available if needed */
	return 1;
}
/* }}} */

/* {{{ pdo_fbird_stmt_get_col */
static int pdo_fbird_stmt_get_col(pdo_stmt_t *stmt, int colno,
	zval *result, enum pdo_param_type *type)
{
	pdo_fbird_stmt *S = (pdo_fbird_stmt *)stmt->driver_data;
	if (!S->out_meta || !S->out_buf || (unsigned)colno >= S->out_count) {
		ZVAL_NULL(result);
		return 1;
	}

	if (_pdo_fbird_is_null(S, (unsigned)colno)) {
		ZVAL_NULL(result);
		return 1;
	}

	unsigned sql_type = fbm_get_type(IBG(master_instance), S->out_meta, (unsigned)colno);
	unsigned offset   = fbm_get_offset(IBG(master_instance), S->out_meta, (unsigned)colno);
	unsigned length   = fbm_get_length(IBG(master_instance), S->out_meta, (unsigned)colno);
	int      scale    = fbm_get_scale(IBG(master_instance), S->out_meta, (unsigned)colno);
	unsigned char *data = S->out_buf + offset;

	switch (sql_type & ~1) {
		case SQL_SHORT: {
			short v; memcpy(&v, data, sizeof(short));
			if (scale < 0) {
				char buf[32]; snprintf(buf, sizeof(buf), "%.*f", -scale, v * pow(10.0, scale));
				ZVAL_STRING(result, buf);
			} else ZVAL_LONG(result, v);
			break;
		}
		case SQL_LONG: {
			ISC_LONG v; memcpy(&v, data, sizeof(ISC_LONG));
			if (scale < 0) {
				char buf[32]; snprintf(buf, sizeof(buf), "%.*f", -scale, v * pow(10.0, scale));
				ZVAL_STRING(result, buf);
			} else ZVAL_LONG(result, v);
			break;
		}
		case SQL_INT64: {
			ISC_INT64 v; memcpy(&v, data, sizeof(ISC_INT64));
			if (scale < 0) {
				char buf[48]; snprintf(buf, sizeof(buf), "%.*f", -scale, v * pow(10.0, scale));
				ZVAL_STRING(result, buf);
			} else ZVAL_LONG(result, (zend_long)v);
			break;
		}
		case SQL_FLOAT: {
			float v; memcpy(&v, data, sizeof(float));
			ZVAL_DOUBLE(result, (double)v);
			break;
		}
		case SQL_DOUBLE:
		case SQL_D_FLOAT: {
			double v; memcpy(&v, data, sizeof(double));
			ZVAL_DOUBLE(result, v);
			break;
		}
		case SQL_BOOLEAN: {
			unsigned char v = *data;
			ZVAL_BOOL(result, v != 0);
			break;
		}
		case SQL_VARYING: {
			/* VARY: first 2 bytes = length, then data */
			unsigned short vlen; memcpy(&vlen, data, sizeof(unsigned short));
			ZVAL_STRINGL(result, (char *)(data + sizeof(unsigned short)), vlen);
			break;
		}
		case SQL_TEXT: {
			/* Fixed CHAR: trim trailing spaces */
			char *s = (char *)data;
			size_t len = length;
			while (len > 0 && s[len - 1] == ' ') len--;
			ZVAL_STRINGL(result, s, len);
			break;
		}
		case SQL_TIMESTAMP:
		case SQL_TYPE_DATE:
		case SQL_TYPE_TIME:
#ifdef SQL_TIMESTAMP_TZ
		case SQL_TIMESTAMP_TZ:
#endif
#ifdef SQL_TIME_TZ
		case SQL_TIME_TZ:
#endif
		{
			/* Return as string via fb_interpret-style formatting — use raw hex for now */
			char buf[64];
			snprintf(buf, sizeof(buf), "(datetime:%u)", sql_type);
			ZVAL_STRING(result, buf);
			break;
		}
		case SQL_BLOB: {
			/* Return blob ID as string for now; full LOB streaming in T16 */
			ISC_QUAD bid; memcpy(&bid, data, sizeof(ISC_QUAD));
			char buf[48];
			snprintf(buf, sizeof(buf), "%08x:%08x", bid.gds_quad_high, bid.gds_quad_low);
			ZVAL_STRING(result, buf);
			break;
		}
		default:
			ZVAL_STRINGL(result, (char *)data, length);
			break;
	}
	return 1;
}
/* }}} */

/* {{{ pdo_fbird_stmt_param_hook — bind positional ? parameters */
static int pdo_fbird_stmt_param_hook(pdo_stmt_t *stmt,
	struct pdo_bound_param_data *param, enum pdo_param_event event_type)
{
	pdo_fbird_stmt *S = (pdo_fbird_stmt *)stmt->driver_data;

	/* Resolve named parameter to positional index.
	   PDO passes names with leading ':', our map stores without. */
	if (event_type == PDO_PARAM_EVT_NORMALIZE && param->name && S->named_params) {
		const char *pname = ZSTR_VAL(param->name);
		if (pname[0] == ':') pname++;
		for (unsigned int i = 0; i < S->named_param_count; i++) {
			if (S->named_params[i].name && strcasecmp(pname, S->named_params[i].name) == 0) {
				param->paramno = S->named_params[i].position;
				return 1;
			}
		}
		return 1;
	}

	if (event_type != PDO_PARAM_EVT_EXEC_PRE) return 1;
	if (!S->in_meta || !S->in_buf) return 1;
	if (param->paramno < 0 || (unsigned)param->paramno >= S->in_count) return 1;

	unsigned idx    = (unsigned)param->paramno;
	unsigned offset = fbm_get_offset(IBG(master_instance), S->in_meta, idx);
	unsigned length = fbm_get_length(IBG(master_instance), S->in_meta, idx);
	unsigned null_off = fbm_get_null_offset(IBG(master_instance), S->in_meta, idx);
	unsigned sql_type = fbm_get_type(IBG(master_instance), S->in_meta, idx);
	unsigned char *dest = S->in_buf + offset;
	short *null_flag    = (short *)(S->in_buf + null_off);

	/* Allow NULL binding: ensure nullable bit is set (sql_type |= 1) */
	sql_type |= 1;

	zval *val = &param->parameter;
	if (!val || Z_TYPE_P(val) == IS_NULL) {
		*null_flag = -1;
		memset(dest, 0, length);
		return 1;
	}
	*null_flag = 0;

	switch (sql_type & ~1) {
		case SQL_SHORT: {
			short v = (short)zval_get_long(val);
			memcpy(dest, &v, sizeof(short));
			break;
		}
		case SQL_LONG: {
			ISC_LONG v = (ISC_LONG)zval_get_long(val);
			memcpy(dest, &v, sizeof(ISC_LONG));
			break;
		}
		case SQL_INT64: {
			ISC_INT64 v = (ISC_INT64)zval_get_long(val);
			memcpy(dest, &v, sizeof(ISC_INT64));
			break;
		}
		case SQL_FLOAT: {
			float v = (float)zval_get_double(val);
			memcpy(dest, &v, sizeof(float));
			break;
		}
		case SQL_DOUBLE:
		case SQL_D_FLOAT: {
			double v = zval_get_double(val);
			memcpy(dest, &v, sizeof(double));
			break;
		}
		case SQL_BOOLEAN: {
			unsigned char v = zval_is_true(val) ? 1 : 0;
			*dest = v;
			break;
		}
		case SQL_VARYING: {
			zend_string *s = zval_get_string(val);
			unsigned short slen = (unsigned short)MIN(ZSTR_LEN(s), length);
			memcpy(dest, &slen, sizeof(unsigned short));
			memcpy(dest + sizeof(unsigned short), ZSTR_VAL(s), slen);
			zend_string_release(s);
			break;
		}
		case SQL_TEXT: {
			zend_string *s = zval_get_string(val);
			size_t slen = MIN(ZSTR_LEN(s), length);
			memcpy(dest, ZSTR_VAL(s), slen);
			if (slen < length) memset(dest + slen, ' ', length - slen);
			zend_string_release(s);
			break;
		}
		default: {
			zend_string *s = zval_get_string(val);
			size_t slen = MIN(ZSTR_LEN(s), length);
			memcpy(dest, ZSTR_VAL(s), slen);
			zend_string_release(s);
			break;
		}
	}
	return 1;
}
/* }}} */

/* {{{ pdo_fbird_stmt_methods */
const struct pdo_stmt_methods pdo_fbird_stmt_methods = {
	pdo_fbird_stmt_dtor,
	pdo_fbird_stmt_execute,
	pdo_fbird_stmt_fetch,
	pdo_fbird_stmt_describe,
	pdo_fbird_stmt_get_col,
	pdo_fbird_stmt_param_hook,
	NULL, /* set_attribute */
	NULL, /* get_attribute */
	NULL, /* get_column_meta */
	NULL, /* next_rowset */
	NULL, /* cursor_closer */
};
/* }}} */
