/* fbird_class_connection.c - Firebird\Connection OOP class */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "zend_exceptions.h"
#include "php_firebird.h"
#include "php_fbird_includes.h"
#include "firebird_utils.h"
#include "fbird_class_internal.h"
#include "php_fbird_query_prepare.h"
#include "php_fbird_connection.h"
#include "php_fbird_query_internal.h"

zend_object_handlers fbird_connection_handlers;

zend_object *fbird_connection_create(zend_class_entry *ce)
{
	fbird_connection_obj *intern = zend_object_alloc(sizeof(fbird_connection_obj), ce);
	intern->conn_res = NULL;
	zend_object_std_init(&intern->std, ce);
	object_properties_init(&intern->std, ce);
	intern->std.handlers = &fbird_connection_handlers;
	return &intern->std;
}

/* Ref-release path shared by FirebirdConnection::close() and object
 * destruction (#576).
 *
 * The object OWNS one reference on the resource entry. Destruction only
 * drops that ref: while FBG(default_link) (or a userland resource zval)
 * holds another ref, the link stays open - legacy ext/interbase semantics
 * relied on by fbird_connect() callers that ignore the return value.
 * When the last ref drops, list_entry_destructor fires the link dtor.
 *
 * explicit_close (user called close()) additionally closes the server link
 * immediately and releases the default_link ref if we are the default. */
/* Ref-release path shared by FirebirdConnection::close() and object
 * destruction (#576).
 *
 * The object OWNS one reference on the resource entry. Destruction only
 * drops that ref: while FBG(default_link) (or a userland resource zval)
 * holds another ref, the link stays open - legacy ext/interbase semantics
 * relied on by fbird_connect() callers that ignore the return value.
 * When the last ref drops, list_entry_destructor fires the link dtor.
 *
 * explicit_close (user called close()) additionally closes the server link
 * immediately and releases the default_link ref if we are the default. */
static void fbird_connection_obj_release(fbird_connection_obj *intern, bool explicit_close)
{
	if (!intern->conn_res) {
		return;
	}
	if (explicit_close) {
		if (!FBG(in_mshutdown) && FBG(default_link) == intern->conn_res) {
			FBG(default_link) = NULL;
			zend_list_delete(intern->conn_res); /* drop default_link ref */
		}
		zend_list_close(intern->conn_res);     /* fire link dtor (defunct) */
	}
	zend_list_delete(intern->conn_res);        /* drop object's owned ref */
	intern->conn_res = NULL;
}

void fbird_connection_free(zend_object *obj)
{
	fbird_connection_obj *intern = fbird_connection_from_obj(obj);
	fbird_connection_obj_release(intern, false);
	zend_object_std_dtor(obj);
}

static int fbird_call_fn(const char *fname, zval *args, int argc, zval *retval)
{
	zval fn;
	ZVAL_STRING(&fn, fname);
	int ret = call_user_function(NULL, NULL, &fn, retval, argc, args);
	zval_ptr_dtor(&fn);
	return ret;
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_connection_construct, 0, 0, 3)
	ZEND_ARG_TYPE_INFO(0, database, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, username, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, password, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, charset,  IS_STRING, 1)
	ZEND_ARG_TYPE_INFO(0, buffers,  IS_LONG,   1)
	ZEND_ARG_TYPE_INFO(0, dialect,  IS_LONG,   1)
	ZEND_ARG_TYPE_INFO(0, role,     IS_STRING, 1)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdConnection, __construct)
{
	char *db, *user, *pass, *charset = "", *role = "";
	size_t db_len, user_len, pass_len, charset_len = 0, role_len = 0;
	zend_long buffers = 0, dialect = 3;

	ZEND_PARSE_PARAMETERS_START(3, 7)
		Z_PARAM_STRING(db,      db_len)
		Z_PARAM_STRING(user,    user_len)
		Z_PARAM_STRING(pass,    pass_len)
		Z_PARAM_OPTIONAL
		Z_PARAM_STRING(charset, charset_len)
		Z_PARAM_LONG(buffers)
		Z_PARAM_LONG(dialect)
		Z_PARAM_STRING(role,    role_len)
	ZEND_PARSE_PARAMETERS_END();

	fbird_connection_obj *intern = Z_FBIRD_CONNECTION_P(ZEND_THIS);

	RESET_ERRMSG;
	zend_resource *res = _php_fbird_connect_link(
		db,      db_len,
		user,    user_len,
		pass,    pass_len,
		charset, charset_len,
		buffers, dialect ? dialect : 3,
		role,    role_len,
		0 /* flags */, 0 /* non-persistent */);
	if (!res) {
		zend_throw_exception(fbird_connection_exception_ce,
			"Failed to connect to Firebird database", 0);
		return;
	}
	/* Issue #576: the object OWNS this reference (the "caller" ref from
	 * _php_fbird_connect_link). The previous borrow (GC_DELREF here) tied the
	 * resource lifetime to FBG(default_link): the next fbird_connect() drops
	 * that ref (zend_list_delete in _php_fbird_connect_link), the entry is
	 * efreed, and any later isConnected()/ping()/prepare() on this object
	 * dereferenced freed memory (heap-use-after-free under ASAN, SIGSEGV in
	 * the doctrine-firebird-driver suite). Mirror FirebirdStatement's owned
	 * ref: keep it here, drop it in close()/fbird_connection_free(). */
	intern->conn_res = res;
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_connection_close, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdConnection, close)
{
	ZEND_PARSE_PARAMETERS_NONE();
	fbird_connection_obj *intern = Z_FBIRD_CONNECTION_P(ZEND_THIS);
	fbird_connection_obj_release(intern, true);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_connection_isConnected, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdConnection, isConnected)
{
	ZEND_PARSE_PARAMETERS_NONE();
	fbird_connection_obj *intern = Z_FBIRD_CONNECTION_P(ZEND_THIS);
	if (!intern->conn_res || intern->conn_res->type <= 0) {
		RETURN_FALSE;
	}
	fbird_db_link *link = (fbird_db_link *)intern->conn_res->ptr;
	RETURN_BOOL(link && link->fbc_connection && fbc_is_connected(link->fbc_connection));
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_connection_ping, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdConnection, ping)
{
	ZEND_PARSE_PARAMETERS_NONE();
	fbird_connection_obj *intern = Z_FBIRD_CONNECTION_P(ZEND_THIS);
	if (!intern->conn_res || intern->conn_res->type <= 0) {
		RETURN_FALSE;
	}
	fbird_db_link *link = (fbird_db_link *)intern->conn_res->ptr;
	RETURN_BOOL(link && link->fbc_connection && fbc_is_connected(link->fbc_connection));
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_connection_beginTransaction, 0, 0, MAY_BE_OBJECT)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdConnection, beginTransaction)
{
	ZEND_PARSE_PARAMETERS_NONE();
	fbird_connection_obj *intern = Z_FBIRD_CONNECTION_P(ZEND_THIS);
	if (!intern->conn_res || intern->conn_res->type <= 0) {
		zend_throw_exception(fbird_connection_exception_ce, "Not connected", 0);
		RETURN_THROWS();
	}

	zval conn_zv;
	ZVAL_RES(&conn_zv, intern->conn_res);
	GC_ADDREF(intern->conn_res);
	fbird_call_fn("fbird_trans", &conn_zv, 1, return_value);
	zval_ptr_dtor(&conn_zv);

	if (Z_TYPE_P(return_value) != IS_OBJECT) {
		zend_throw_exception(fbird_connection_exception_ce, "Failed to start transaction", 0);
		RETURN_THROWS();
	}
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_connection_prepare, 0, 2, MAY_BE_OBJECT)
	ZEND_ARG_TYPE_INFO(0, sql, IS_STRING, 0)
	ZEND_ARG_OBJ_INFO(0, transaction, Firebird\\Transaction, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdConnection, prepare)
{
	char *sql;
	size_t sql_len;
	zval *tr_zv;
	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_STRING(sql, sql_len)
		Z_PARAM_OBJECT_OF_CLASS(tr_zv, fbird_transaction_ce)
	ZEND_PARSE_PARAMETERS_END();

	fbird_connection_obj *conn = Z_FBIRD_CONNECTION_P(ZEND_THIS);
	if (!conn->conn_res || conn->conn_res->type <= 0) {
		zend_throw_exception(fbird_connection_exception_ce, "Not connected", 0);
		RETURN_THROWS();
	}

	fbird_transaction_obj *tr = Z_FBIRD_TRANSACTION_P(tr_zv);

	zend_resource *tr_res = NULL;
	if (tr->trans_res && tr->trans_res->type > 0) {
		tr_res = tr->trans_res;
	} else if (tr->fbt_trans && fbt_is_active(tr->fbt_trans)) {
		zend_throw_exception(fbird_connection_exception_ce,
			"OOP-native transactions are not supported with prepare(); use beginTransaction()", 0);
		RETURN_THROWS();
	} else {
		zend_throw_exception(fbird_connection_exception_ce, "Transaction not active", 0);
		RETURN_THROWS();
	}

	fbird_db_link *link = (fbird_db_link *)conn->conn_res->ptr;
	fbird_transaction *trans = (fbird_transaction *)tr_res->ptr;
	fbird_query *fb_query = NULL;

	if (FAILURE == _php_fbird_prepare(&fb_query, link, trans, tr_res, sql)) {
		zend_throw_exception(fbird_query_exception_ce, "Failed to prepare statement", 0);
		RETURN_THROWS();
	}

	fbird_setup_statement_object(return_value, fb_query->res);
}

#if FB_API_VER >= 40
/* {{{ Statement/Session Timeout Methods (Firebird 4.0+) */

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_connection_setStatementTimeout, 0, 1, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, milliseconds, IS_LONG, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdConnection, setStatementTimeout)
{
	zend_long ms;
	ZEND_PARSE_PARAMETERS_START(1, 1);
		Z_PARAM_LONG(ms);
	ZEND_PARSE_PARAMETERS_END();

	if (ms < 0) {
		zend_argument_value_error(1, "must be non-negative, " ZEND_LONG_FMT " given", ms);
		RETURN_THROWS();
	}

	fbird_connection_obj *intern = Z_FBIRD_CONNECTION_P(ZEND_THIS);
	if (!intern->conn_res || intern->conn_res->type <= 0) RETURN_FALSE;
	fbird_db_link *link = (fbird_db_link *)intern->conn_res->ptr;
	if (!link || !link->fbc_connection) RETURN_FALSE;

	ISC_STATUS_ARRAY status;
	RETURN_BOOL(fbc_set_statement_timeout(link->fbc_connection, (unsigned int)ms, status) == 0);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_connection_getStatementTimeout, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdConnection, getStatementTimeout)
{
	ZEND_PARSE_PARAMETERS_NONE();
	fbird_connection_obj *intern = Z_FBIRD_CONNECTION_P(ZEND_THIS);
	if (!intern->conn_res || intern->conn_res->type <= 0) RETURN_LONG(0);
	fbird_db_link *link = (fbird_db_link *)intern->conn_res->ptr;
	if (!link || !link->fbc_connection) RETURN_LONG(0);

	RETURN_LONG((zend_long)fbc_get_statement_timeout(link->fbc_connection));
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_connection_setIdleTimeout, 0, 1, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, seconds, IS_LONG, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdConnection, setIdleTimeout)
{
	zend_long sec;
	ZEND_PARSE_PARAMETERS_START(1, 1);
		Z_PARAM_LONG(sec);
	ZEND_PARSE_PARAMETERS_END();

	if (sec < 0) {
		zend_argument_value_error(1, "must be non-negative, " ZEND_LONG_FMT " given", sec);
		RETURN_THROWS();
	}

	fbird_connection_obj *intern = Z_FBIRD_CONNECTION_P(ZEND_THIS);
	if (!intern->conn_res || intern->conn_res->type <= 0) RETURN_FALSE;
	fbird_db_link *link = (fbird_db_link *)intern->conn_res->ptr;
	if (!link || !link->fbc_connection) RETURN_FALSE;

	ISC_STATUS_ARRAY status;
	RETURN_BOOL(fbc_set_idle_timeout(link->fbc_connection, (unsigned int)sec, status) == 0);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_connection_getIdleTimeout, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdConnection, getIdleTimeout)
{
	ZEND_PARSE_PARAMETERS_NONE();
	fbird_connection_obj *intern = Z_FBIRD_CONNECTION_P(ZEND_THIS);
	if (!intern->conn_res || intern->conn_res->type <= 0) RETURN_LONG(0);
	fbird_db_link *link = (fbird_db_link *)intern->conn_res->ptr;
	if (!link || !link->fbc_connection) RETURN_LONG(0);

	RETURN_LONG((zend_long)fbc_get_idle_timeout(link->fbc_connection));
}

#endif /* FB_API_VER >= 40 */

const zend_function_entry fbird_connection_methods[] = {
	PHP_ME(FirebirdConnection, __construct,      arginfo_fbird_connection_construct,        ZEND_ACC_PUBLIC)
	PHP_ME(FirebirdConnection, close,            arginfo_fbird_connection_close,            ZEND_ACC_PUBLIC)
	PHP_ME(FirebirdConnection, isConnected,      arginfo_fbird_connection_isConnected,      ZEND_ACC_PUBLIC)
	PHP_ME(FirebirdConnection, ping,             arginfo_fbird_connection_ping,             ZEND_ACC_PUBLIC)
	PHP_ME(FirebirdConnection, beginTransaction, arginfo_fbird_connection_beginTransaction, ZEND_ACC_PUBLIC)
	PHP_ME(FirebirdConnection, prepare,          arginfo_fbird_connection_prepare,          ZEND_ACC_PUBLIC)
#if FB_API_VER >= 40
	PHP_ME(FirebirdConnection, setStatementTimeout, arginfo_fbird_connection_setStatementTimeout, ZEND_ACC_PUBLIC)
	PHP_ME(FirebirdConnection, getStatementTimeout, arginfo_fbird_connection_getStatementTimeout, ZEND_ACC_PUBLIC)
	PHP_ME(FirebirdConnection, setIdleTimeout,      arginfo_fbird_connection_setIdleTimeout,      ZEND_ACC_PUBLIC)
	PHP_ME(FirebirdConnection, getIdleTimeout,      arginfo_fbird_connection_getIdleTimeout,      ZEND_ACC_PUBLIC)
#endif
	PHP_FE_END
};

zend_resource *fbird_connection_get_resource(zend_object *obj)
{
	fbird_connection_obj *intern = fbird_connection_from_obj(obj);
	return intern ? intern->conn_res : NULL;
}

void fbird_setup_connection_object(zval *return_value, zend_resource *res)
{
	zval_ptr_dtor(return_value);
	object_init_ex(return_value, fbird_connection_ce);
	fbird_connection_obj *intern = Z_FBIRD_CONNECTION_P(return_value);
	/* Issue #576: the object OWNS a reference (was: raw weak pointer, with
	 * default_link as sole owner). The next fbird_connect() to the same or
	 * another database drops the default_link ref; the entry was efreed while
	 * this object still pointed at it -> heap-use-after-free in isConnected()
	 * et al. Callers compensate: _php_fbird_connect drops the caller ref it
	 * got from _php_fbird_connect_link; fbird_connection.c:~837 took none. */
	GC_ADDREF(res);
	intern->conn_res = res;
}

fbird_db_link *fbird_get_link_from_conn(zval *conn_zv)
{
	fbird_connection_obj *conn = fbird_connection_from_obj(Z_OBJ_P(conn_zv));
	if (!conn->conn_res || conn->conn_res->type <= 0) return NULL;
	return (fbird_db_link *)conn->conn_res->ptr;
}
