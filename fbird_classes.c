/*
 * fbird_classes.c — Layer 2 OOP class registrations for php-firebird
 *
 * Phase B1: Exception sub-hierarchy
 *   Firebird\ConnectionException extends Firebird\Exception
 *   Firebird\QueryException      extends Firebird\Exception
 *   Firebird\ServiceException    extends Firebird\Exception
 *
 * Phase B2: Firebird\Connection
 *   __construct(string $db, string $user, string $password, ...)
 *   close(), isConnected(), ping()
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "zend_exceptions.h"
#include "php_firebird.h"
#include "php_fbird_includes.h"
#include "firebird_utils.h"
#include "fbird_classes.h"

/* -----------------------------------------------------------------------
 * B1: Sub-exception class entries
 * --------------------------------------------------------------------- */
zend_class_entry *fbird_connection_exception_ce;
zend_class_entry *fbird_query_exception_ce;
zend_class_entry *fbird_service_exception_ce;

/* -----------------------------------------------------------------------
 * B2: Firebird\Connection internal object
 * --------------------------------------------------------------------- */
zend_class_entry    *fbird_connection_ce;
static zend_object_handlers fbird_connection_handlers;

typedef struct {
	zend_resource *conn_res;   /* fbird_connect() resource — owns the connection */
	zend_object    std;
} fbird_connection_obj;

static inline fbird_connection_obj *fbird_connection_from_obj(zend_object *obj)
{
	return (fbird_connection_obj *)((char *)obj - XtOffsetOf(fbird_connection_obj, std));
}

#define Z_FBIRD_CONNECTION_P(zv) fbird_connection_from_obj(Z_OBJ_P(zv))

static zend_object *fbird_connection_create(zend_class_entry *ce)
{
	fbird_connection_obj *intern = zend_object_alloc(sizeof(fbird_connection_obj), ce);
	intern->conn_res = NULL;
	zend_object_std_init(&intern->std, ce);
	object_properties_init(&intern->std, ce);
	intern->std.handlers = &fbird_connection_handlers;
	return &intern->std;
}

static void fbird_connection_free(zend_object *obj)
{
	fbird_connection_obj *intern = fbird_connection_from_obj(obj);
	/* conn_res is a weak reference — EG(regular_list) owns it, don't delete */
	intern->conn_res = NULL;
	zend_object_std_dtor(obj);
}

/* Firebird\Connection::__construct(string $db, string $user, string $pass
 *                                  [, string $charset = ''
 *                                  [, int $buffers = 0
 *                                  [, int $dialect = 3
 *                                  [, string $role = '']]]]) */
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

	/* Use fbird_connect() to get a resource — it handles caching, persistent
	 * connections, and owns the fbc_connection lifecycle. */
	zval fn, retval;
	zval args[7];
	ZVAL_STRING(&fn, "fbird_connect");
	ZVAL_STRINGL(&args[0], db,      db_len);
	ZVAL_STRINGL(&args[1], user,    user_len);
	ZVAL_STRINGL(&args[2], pass,    pass_len);
	ZVAL_STRINGL(&args[3], charset, charset_len);
	ZVAL_LONG(&args[4], buffers);
	ZVAL_LONG(&args[5], dialect ? dialect : 3);
	ZVAL_STRINGL(&args[6], role,    role_len);
	call_user_function(NULL, NULL, &fn, &retval, 7, args);
	zval_ptr_dtor(&fn);
	for (int i = 0; i < 7; i++) zval_ptr_dtor(&args[i]);

	if (Z_TYPE(retval) != IS_RESOURCE) {
		zval_ptr_dtor(&retval);
		zend_throw_exception(fbird_connection_exception_ce,
			"Failed to connect to Firebird database", 0);
		return;
	}

	/* Store as weak reference — EG(regular_list) owns the resource lifetime */
	intern->conn_res = Z_RES(retval);
	zval_ptr_dtor(&retval); /* drops our zval ref but resource stays in regular_list */

}

/* Firebird\Connection::close(): void */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_connection_close, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdConnection, close)
{
	ZEND_PARSE_PARAMETERS_NONE();
	fbird_connection_obj *intern = Z_FBIRD_CONNECTION_P(ZEND_THIS);
	if (intern->conn_res) {
		/* Close the resource (marks it invalid, triggers destructor) */
		zend_list_close(intern->conn_res);
 	intern->conn_res = NULL;
	}
}

/* Firebird\Connection::isConnected(): bool */
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

/* Firebird\Connection::ping(): bool */
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

/* -----------------------------------------------------------------------
 * B3: Firebird\Transaction internal object
 * --------------------------------------------------------------------- */
zend_class_entry    *fbird_transaction_ce;
static zend_object_handlers fbird_transaction_handlers;

typedef struct {
	void        *fbt_trans;  /* fb::Transaction* from fbt_start() */
	zend_object  std;
} fbird_transaction_obj;

static inline fbird_transaction_obj *fbird_transaction_from_obj(zend_object *obj)
{
	return (fbird_transaction_obj *)((char *)obj - XtOffsetOf(fbird_transaction_obj, std));
}

#define Z_FBIRD_TRANSACTION_P(zv) fbird_transaction_from_obj(Z_OBJ_P(zv))

static zend_object *fbird_transaction_create(zend_class_entry *ce)
{
	fbird_transaction_obj *intern = zend_object_alloc(sizeof(fbird_transaction_obj), ce);
	intern->fbt_trans = NULL;
	zend_object_std_init(&intern->std, ce);
	object_properties_init(&intern->std, ce);
	intern->std.handlers = &fbird_transaction_handlers;
	return &intern->std;
}

static void fbird_transaction_free(zend_object *obj)
{
	fbird_transaction_obj *intern = fbird_transaction_from_obj(obj);
	if (intern->fbt_trans) {
		ISC_STATUS sv[20];
		fbt_rollback(intern->fbt_trans, sv);
		fbt_free(intern->fbt_trans);
		intern->fbt_trans = NULL;
	}
	zend_object_std_dtor(obj);
}

/* Firebird\Transaction::commit(): void */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_transaction_commit, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdTransaction, commit)
{
	ZEND_PARSE_PARAMETERS_NONE();
	fbird_transaction_obj *intern = Z_FBIRD_TRANSACTION_P(ZEND_THIS);
	if (intern->fbt_trans) {
		ISC_STATUS sv[20];
		fbt_commit(intern->fbt_trans, sv);
		fbt_free(intern->fbt_trans);
		intern->fbt_trans = NULL;
	}
}

/* Firebird\Transaction::rollback(): void */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_transaction_rollback, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdTransaction, rollback)
{
	ZEND_PARSE_PARAMETERS_NONE();
	fbird_transaction_obj *intern = Z_FBIRD_TRANSACTION_P(ZEND_THIS);
	if (intern->fbt_trans) {
		ISC_STATUS sv[20];
		fbt_rollback(intern->fbt_trans, sv);
		fbt_free(intern->fbt_trans);
		intern->fbt_trans = NULL;
	}
}

/* Firebird\Transaction::isActive(): bool */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_transaction_isActive, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdTransaction, isActive)
{
	ZEND_PARSE_PARAMETERS_NONE();
	fbird_transaction_obj *intern = Z_FBIRD_TRANSACTION_P(ZEND_THIS);
	RETURN_BOOL(intern->fbt_trans && fbt_is_active(intern->fbt_trans));
}

static const zend_function_entry fbird_transaction_methods[] = {
	PHP_ME(FirebirdTransaction, commit,    arginfo_fbird_transaction_commit,    ZEND_ACC_PUBLIC)
	PHP_ME(FirebirdTransaction, rollback,  arginfo_fbird_transaction_rollback,  ZEND_ACC_PUBLIC)
	PHP_ME(FirebirdTransaction, isActive,  arginfo_fbird_transaction_isActive,  ZEND_ACC_PUBLIC)
	PHP_FE_END
};

/* Firebird\Connection::beginTransaction(): Firebird\Transaction */
ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_connection_beginTransaction, 0, 0, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdConnection, beginTransaction)
{
	ZEND_PARSE_PARAMETERS_NONE();
	fbird_connection_obj *intern = Z_FBIRD_CONNECTION_P(ZEND_THIS);
	if (!intern->conn_res || intern->conn_res->type <= 0) {
		zend_throw_exception(fbird_connection_exception_ce, "Not connected", 0);
		RETURN_THROWS();
	}
	fbird_db_link *link = (fbird_db_link *)intern->conn_res->ptr;
	if (!link || !link->fbc_connection || !fbc_is_connected(link->fbc_connection)) {
		zend_throw_exception(fbird_connection_exception_ce, "Not connected", 0);
		RETURN_THROWS();
	}

	ISC_STATUS sv[20];
	void *trans = fbt_start(IBG(master_instance),
		fbc_get_attachment(link->fbc_connection),
		0, NULL, sv);
	if (!trans) {
		_php_fbird_error();
		zend_throw_exception(fbird_connection_exception_ce, "Failed to start transaction", 0);
		RETURN_THROWS();
	}

	object_init_ex(return_value, fbird_transaction_ce);
	fbird_transaction_obj *tr = Z_FBIRD_TRANSACTION_P(return_value);
	tr->fbt_trans = trans;
}

/* prepare() and B4 classes are defined below; methods table follows after */

/* -----------------------------------------------------------------------
 * B4: Firebird\Statement and Firebird\ResultSet
 *
 * Both wrap a zend_resource* (existing query resource) and delegate
 * to the existing fbird_execute / fbird_fetch_assoc PHP functions.
 * --------------------------------------------------------------------- */
zend_class_entry    *fbird_statement_ce;
zend_class_entry    *fbird_resultset_ce;
static zend_object_handlers fbird_statement_handlers;
static zend_object_handlers fbird_resultset_handlers;

typedef struct {
	zend_resource *query_res; /* result of fbird_prepare() */
	zend_object    std;
} fbird_statement_obj;

typedef struct {
	zend_resource *query_res; /* result of fbird_execute() */
	zend_object    std;
} fbird_resultset_obj;

static inline fbird_statement_obj *fbird_statement_from_obj(zend_object *obj)
{
	return (fbird_statement_obj *)((char *)obj - XtOffsetOf(fbird_statement_obj, std));
}

static inline fbird_resultset_obj *fbird_resultset_from_obj(zend_object *obj)
{
	return (fbird_resultset_obj *)((char *)obj - XtOffsetOf(fbird_resultset_obj, std));
}

#define Z_FBIRD_STATEMENT_P(zv) fbird_statement_from_obj(Z_OBJ_P(zv))
#define Z_FBIRD_RESULTSET_P(zv) fbird_resultset_from_obj(Z_OBJ_P(zv))

static zend_object *fbird_statement_create(zend_class_entry *ce)
{
	fbird_statement_obj *intern = zend_object_alloc(sizeof(fbird_statement_obj), ce);
	intern->query_res = NULL;
	zend_object_std_init(&intern->std, ce);
	object_properties_init(&intern->std, ce);
	intern->std.handlers = &fbird_statement_handlers;
	return &intern->std;
}

static zend_object *fbird_resultset_create(zend_class_entry *ce)
{
	fbird_resultset_obj *intern = zend_object_alloc(sizeof(fbird_resultset_obj), ce);
	intern->query_res = NULL;
	zend_object_std_init(&intern->std, ce);
	object_properties_init(&intern->std, ce);
	intern->std.handlers = &fbird_resultset_handlers;
	return &intern->std;
}

static void fbird_statement_free(zend_object *obj)
{
	fbird_statement_obj *intern = fbird_statement_from_obj(obj);
	if (intern->query_res) {
		zend_list_delete(intern->query_res);
		intern->query_res = NULL;
	}
	zend_object_std_dtor(obj);
}

static void fbird_resultset_free(zend_object *obj)
{
	fbird_resultset_obj *intern = fbird_resultset_from_obj(obj);
	if (intern->query_res) {
		zend_list_delete(intern->query_res);
		intern->query_res = NULL;
	}
	zend_object_std_dtor(obj);
}

/* Helper: call a named PHP function with zval args, return result in retval */
static int fbird_call_fn(const char *fname, zval *args, int argc, zval *retval)
{
	zval fn;
	ZVAL_STRING(&fn, fname);
	int ret = call_user_function(NULL, NULL, &fn, retval, argc, args);
	zval_ptr_dtor(&fn);
	return ret;
}

/* Firebird\ResultSet::fetch(): array|false */
ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_resultset_fetch, 0, 0, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdResultSet, fetch)
{
	ZEND_PARSE_PARAMETERS_NONE();
	fbird_resultset_obj *intern = Z_FBIRD_RESULTSET_P(ZEND_THIS);
	if (!intern->query_res) {
		RETURN_FALSE;
	}
	zval res_zv, retval;
	ZVAL_RES(&res_zv, intern->query_res);
	GC_ADDREF(intern->query_res);
	fbird_call_fn("fbird_fetch_assoc", &res_zv, 1, &retval);
	zval_ptr_dtor(&res_zv);
	ZVAL_COPY_VALUE(return_value, &retval);
}

/* Firebird\ResultSet::close(): void */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_resultset_close, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdResultSet, close)
{
	ZEND_PARSE_PARAMETERS_NONE();
	fbird_resultset_obj *intern = Z_FBIRD_RESULTSET_P(ZEND_THIS);
	if (intern->query_res) {
		zend_list_delete(intern->query_res);
		intern->query_res = NULL;
	}
}

static const zend_function_entry fbird_resultset_methods[] = {
	PHP_ME(FirebirdResultSet, fetch, arginfo_fbird_resultset_fetch, ZEND_ACC_PUBLIC)
	PHP_ME(FirebirdResultSet, close, arginfo_fbird_resultset_close, ZEND_ACC_PUBLIC)
	PHP_FE_END
};

/* Firebird\Statement::execute(Firebird\Transaction $tr): Firebird\ResultSet */
ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_statement_execute, 0, 0, 1)
	ZEND_ARG_OBJ_INFO(0, transaction, Firebird\\Transaction, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdStatement, execute)
{
	zval *tr_zv;
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJECT_OF_CLASS(tr_zv, fbird_transaction_ce)
	ZEND_PARSE_PARAMETERS_END();

	fbird_statement_obj *intern = Z_FBIRD_STATEMENT_P(ZEND_THIS);
	if (!intern->query_res) {
		zend_throw_exception(fbird_query_exception_ce, "Statement not prepared", 0);
		RETURN_THROWS();
	}

	/* Call fbird_execute($query_res) */
	zval res_zv, retval;
	ZVAL_RES(&res_zv, intern->query_res);
	GC_ADDREF(intern->query_res);
	fbird_call_fn("fbird_execute", &res_zv, 1, &retval);
	zval_ptr_dtor(&res_zv);

	if (Z_TYPE(retval) == IS_FALSE) {
		zval_ptr_dtor(&retval);
		zend_throw_exception(fbird_query_exception_ce, "Failed to execute statement", 0);
		RETURN_THROWS();
	}

	/* Return a ResultSet wrapping the same query resource */
	object_init_ex(return_value, fbird_resultset_ce);
	fbird_resultset_obj *rs = Z_FBIRD_RESULTSET_P(return_value);
	if (Z_TYPE(retval) == IS_RESOURCE) {
		rs->query_res = Z_RES(retval);
		GC_ADDREF(rs->query_res);
		zval_ptr_dtor(&retval);
	} else {
		/* Non-SELECT: reuse the prepared statement resource for fetch (returns false) */
		rs->query_res = intern->query_res;
		GC_ADDREF(rs->query_res);
		zval_ptr_dtor(&retval);
	}
}

static const zend_function_entry fbird_statement_methods[] = {
	PHP_ME(FirebirdStatement, execute, arginfo_fbird_statement_execute, ZEND_ACC_PUBLIC)
	PHP_FE_END
};

/* Firebird\Connection::prepare(string $sql, Firebird\Transaction $tr): Firebird\Statement */
ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_connection_prepare, 0, 0, 2)
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
	if (!tr->fbt_trans || !fbt_is_active(tr->fbt_trans)) {
		zend_throw_exception(fbird_connection_exception_ce, "Transaction not active", 0);
		RETURN_THROWS();
	}

	if (!conn->conn_res) {
		zend_throw_exception(fbird_connection_exception_ce,
			"No connection resource available for prepare", 0);
		RETURN_THROWS();
	}

	/* Get a transaction resource from the Firebird\Transaction object.
	 * We need to start a matching resource-based transaction on the same conn_res.
	 * Use fbird_trans() with the conn_res to get a compatible tr_res. */
	zval conn_zv, tr_res_zv, retval;
	ZVAL_RES(&conn_zv, conn->conn_res);
	GC_ADDREF(conn->conn_res);

	/* Start a resource-based transaction on the connection */
	fbird_call_fn("fbird_trans", &conn_zv, 1, &tr_res_zv);
	zval_ptr_dtor(&conn_zv);

	if (Z_TYPE(tr_res_zv) != IS_RESOURCE) {
		zval_ptr_dtor(&tr_res_zv);
		zend_throw_exception(fbird_query_exception_ce,
			"Failed to start transaction for prepare", 0);
		RETURN_THROWS();
	}

	/* Call fbird_prepare($conn_res, $tr_res, $sql) */
	zval prep_args[3];
	ZVAL_RES(&prep_args[0], conn->conn_res);
	GC_ADDREF(conn->conn_res);
	ZVAL_COPY(&prep_args[1], &tr_res_zv);
	ZVAL_STRINGL(&prep_args[2], sql, sql_len);
	fbird_call_fn("fbird_prepare", prep_args, 3, &retval);
	for (int i = 0; i < 3; i++) zval_ptr_dtor(&prep_args[i]);
	zval_ptr_dtor(&tr_res_zv);

	if (Z_TYPE(retval) != IS_RESOURCE) {
		zval_ptr_dtor(&retval);
		zend_throw_exception(fbird_query_exception_ce, "Failed to prepare statement", 0);
		RETURN_THROWS();
	}

	object_init_ex(return_value, fbird_statement_ce);
	fbird_statement_obj *stmt = Z_FBIRD_STATEMENT_P(return_value);
	stmt->query_res = Z_RES(retval);
	GC_ADDREF(stmt->query_res);
	zval_ptr_dtor(&retval);
}

/* -----------------------------------------------------------------------
 * B5: Firebird\Blob
 * --------------------------------------------------------------------- */
zend_class_entry    *fbird_blob_ce;
static zend_object_handlers fbird_blob_handlers;

typedef struct {
	void        *fbb_wrap;   /* BlobWrapper* from fbb_create/fbb_open */
	ISC_QUAD     blob_id;    /* blob ID (set after create/close) */
	zend_object  std;
} fbird_blob_obj;

static inline fbird_blob_obj *fbird_blob_from_obj(zend_object *obj)
{
	return (fbird_blob_obj *)((char *)obj - XtOffsetOf(fbird_blob_obj, std));
}

#define Z_FBIRD_BLOB_P(zv) fbird_blob_from_obj(Z_OBJ_P(zv))

static zend_object *fbird_blob_create_obj(zend_class_entry *ce)
{
	fbird_blob_obj *intern = zend_object_alloc(sizeof(fbird_blob_obj), ce);
	intern->fbb_wrap = NULL;
	memset(&intern->blob_id, 0, sizeof(ISC_QUAD));
	zend_object_std_init(&intern->std, ce);
	object_properties_init(&intern->std, ce);
	intern->std.handlers = &fbird_blob_handlers;
	return &intern->std;
}

static void fbird_blob_free_obj(zend_object *obj)
{
	fbird_blob_obj *intern = fbird_blob_from_obj(obj);
	if (intern->fbb_wrap) {
		ISC_STATUS sv[20];
		fbb_cancel(IBG(master_instance), intern->fbb_wrap, sv);
		fbb_free(intern->fbb_wrap);
		intern->fbb_wrap = NULL;
	}
	zend_object_std_dtor(obj);
}

/* Helper: get fbird_db_link from Firebird\Connection object */
static fbird_db_link *fbird_get_link_from_conn(zval *conn_zv)
{
	fbird_connection_obj *conn = fbird_connection_from_obj(Z_OBJ_P(conn_zv));
	if (!conn->conn_res || conn->conn_res->type <= 0) return NULL;
	return (fbird_db_link *)conn->conn_res->ptr;
}

/* Firebird\Blob::create(Connection $conn, Transaction $tr): Blob */
ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_blob_create, 0, 0, 2)
	ZEND_ARG_OBJ_INFO(0, connection,  Firebird\\Connection,  0)
	ZEND_ARG_OBJ_INFO(0, transaction, Firebird\\Transaction, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdBlob, create)
{
	zval *conn_zv, *tr_zv;
	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_OBJECT_OF_CLASS(conn_zv, fbird_connection_ce)
		Z_PARAM_OBJECT_OF_CLASS(tr_zv,   fbird_transaction_ce)
	ZEND_PARSE_PARAMETERS_END();

	fbird_db_link *link = fbird_get_link_from_conn(conn_zv);
	if (!link || !link->fbc_connection) {
		zend_throw_exception(fbird_connection_exception_ce, "Not connected", 0);
		RETURN_THROWS();
	}
	fbird_transaction_obj *tr = Z_FBIRD_TRANSACTION_P(tr_zv);
	if (!tr->fbt_trans || !fbt_is_active(tr->fbt_trans)) {
		zend_throw_exception(fbird_connection_exception_ce, "Transaction not active", 0);
		RETURN_THROWS();
	}

	object_init_ex(return_value, fbird_blob_ce);
	fbird_blob_obj *blob = Z_FBIRD_BLOB_P(return_value);

	ISC_STATUS sv[20];
	blob->fbb_wrap = fbb_create(IBG(master_instance),
		fbc_get_attachment(link->fbc_connection),
		fbt_get_handle(tr->fbt_trans),
		&blob->blob_id, 0, NULL, sv);

	if (!blob->fbb_wrap) {
		_php_fbird_error();
		zend_throw_exception(fbird_query_exception_ce, "Failed to create blob", 0);
		RETURN_THROWS();
	}
}

/* Firebird\Blob::open(Connection $conn, Transaction $tr, string $id): Blob */
ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_blob_open, 0, 0, 3)
	ZEND_ARG_OBJ_INFO(0, connection,  Firebird\\Connection,  0)
	ZEND_ARG_OBJ_INFO(0, transaction, Firebird\\Transaction, 0)
	ZEND_ARG_TYPE_INFO(0, blobId, IS_STRING, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdBlob, open)
{
	zval *conn_zv, *tr_zv;
	char *id_str;
	size_t id_len;
	ZEND_PARSE_PARAMETERS_START(3, 3)
		Z_PARAM_OBJECT_OF_CLASS(conn_zv, fbird_connection_ce)
		Z_PARAM_OBJECT_OF_CLASS(tr_zv,   fbird_transaction_ce)
		Z_PARAM_STRING(id_str, id_len)
	ZEND_PARSE_PARAMETERS_END();

	fbird_db_link *link = fbird_get_link_from_conn(conn_zv);
	if (!link || !link->fbc_connection) {
		zend_throw_exception(fbird_connection_exception_ce, "Not connected", 0);
		RETURN_THROWS();
	}
	fbird_transaction_obj *tr = Z_FBIRD_TRANSACTION_P(tr_zv);
	if (!tr->fbt_trans || !fbt_is_active(tr->fbt_trans)) {
		zend_throw_exception(fbird_connection_exception_ce, "Transaction not active", 0);
		RETURN_THROWS();
	}

	/* Parse blob ID from hex string "XXXXXXXX:XXXXXXXX" */
	ISC_QUAD blob_id;
	if (id_len < 17 || sscanf(id_str, "%08x:%08x",
			(unsigned *)&blob_id.gds_quad_high,
			(unsigned *)&blob_id.gds_quad_low) != 2) {
		zend_throw_exception(fbird_query_exception_ce, "Invalid blob ID format", 0);
		RETURN_THROWS();
	}

	object_init_ex(return_value, fbird_blob_ce);
	fbird_blob_obj *blob = Z_FBIRD_BLOB_P(return_value);
	blob->blob_id = blob_id;

	ISC_STATUS sv[20];
	blob->fbb_wrap = fbb_open(IBG(master_instance),
		fbc_get_attachment(link->fbc_connection),
		fbt_get_handle(tr->fbt_trans),
		&blob_id, 0, NULL, sv);

	if (!blob->fbb_wrap) {
		_php_fbird_error();
		zend_throw_exception(fbird_query_exception_ce, "Failed to open blob", 0);
		RETURN_THROWS();
	}
}

/* Firebird\Blob::write(string $data): void */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_blob_write, 0, 1, IS_VOID, 0)
	ZEND_ARG_TYPE_INFO(0, data, IS_STRING, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdBlob, write)
{
	char *data;
	size_t data_len;
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STRING(data, data_len)
	ZEND_PARSE_PARAMETERS_END();

	fbird_blob_obj *intern = Z_FBIRD_BLOB_P(ZEND_THIS);
	if (!intern->fbb_wrap) {
		zend_throw_exception(fbird_query_exception_ce, "Blob not open", 0);
		RETURN_THROWS();
	}
	ISC_STATUS sv[20];
	if (!fbb_put_segment(IBG(master_instance), intern->fbb_wrap,
			(unsigned)data_len, data, sv)) {
		_php_fbird_error();
		zend_throw_exception(fbird_query_exception_ce, "Failed to write blob segment", 0);
	}
}

/* Firebird\Blob::read(int $length): string|false */
ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_blob_read, 0, 0, 1)
	ZEND_ARG_TYPE_INFO(0, length, IS_LONG, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdBlob, read)
{
	zend_long length;
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_LONG(length)
	ZEND_PARSE_PARAMETERS_END();

	fbird_blob_obj *intern = Z_FBIRD_BLOB_P(ZEND_THIS);
	if (!intern->fbb_wrap) {
		RETURN_FALSE;
	}

	zend_string *buf = zend_string_alloc((size_t)length, 0);
	unsigned actual = 0;
	ISC_STATUS sv[20];
	int rc = fbb_get_segment(IBG(master_instance), intern->fbb_wrap,
		(unsigned)length, ZSTR_VAL(buf), &actual, sv);

	if (rc == -1) {
		zend_string_efree(buf);
		RETURN_FALSE;
	}
	ZSTR_LEN(buf) = actual;
	ZSTR_VAL(buf)[actual] = '\0';
	RETURN_STR(buf);
}

/* Firebird\Blob::close(): void */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_blob_close, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdBlob, close)
{
	ZEND_PARSE_PARAMETERS_NONE();
	fbird_blob_obj *intern = Z_FBIRD_BLOB_P(ZEND_THIS);
	if (intern->fbb_wrap) {
		ISC_STATUS sv[20];
		fbb_get_blob_id(intern->fbb_wrap, &intern->blob_id);
		fbb_close(IBG(master_instance), intern->fbb_wrap, sv);
		fbb_free(intern->fbb_wrap);
		intern->fbb_wrap = NULL;
	}
}

/* Firebird\Blob::getId(): string */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_blob_getId, 0, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdBlob, getId)
{
	ZEND_PARSE_PARAMETERS_NONE();
	fbird_blob_obj *intern = Z_FBIRD_BLOB_P(ZEND_THIS);
	char id_str[20];
	snprintf(id_str, sizeof(id_str), "%08x:%08x",
		(unsigned)intern->blob_id.gds_quad_high,
		(unsigned)intern->blob_id.gds_quad_low);
	RETURN_STRING(id_str);
}

static const zend_function_entry fbird_blob_methods[] = {
	PHP_ME(FirebirdBlob, create, arginfo_fbird_blob_create, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(FirebirdBlob, open,   arginfo_fbird_blob_open,   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(FirebirdBlob, write,  arginfo_fbird_blob_write,  ZEND_ACC_PUBLIC)
	PHP_ME(FirebirdBlob, read,   arginfo_fbird_blob_read,   ZEND_ACC_PUBLIC)
	PHP_ME(FirebirdBlob, close,  arginfo_fbird_blob_close,  ZEND_ACC_PUBLIC)
	PHP_ME(FirebirdBlob, getId,  arginfo_fbird_blob_getId,  ZEND_ACC_PUBLIC)
	PHP_FE_END
};

/* -----------------------------------------------------------------------
 * B6: Firebird\Service
 * --------------------------------------------------------------------- */
zend_class_entry    *fbird_service_ce;
static zend_object_handlers fbird_service_handlers;

typedef struct {
	void        *fbsvc;   /* fbsvc_service pointer from fbsvc_attach() */
	zend_object  std;
} fbird_service_obj;

static inline fbird_service_obj *fbird_service_from_obj(zend_object *obj)
{
	return (fbird_service_obj *)((char *)obj - XtOffsetOf(fbird_service_obj, std));
}

#define Z_FBIRD_SERVICE_P(zv) fbird_service_from_obj(Z_OBJ_P(zv))

static zend_object *fbird_service_create_obj(zend_class_entry *ce)
{
	fbird_service_obj *intern = zend_object_alloc(sizeof(fbird_service_obj), ce);
	intern->fbsvc = NULL;
	zend_object_std_init(&intern->std, ce);
	object_properties_init(&intern->std, ce);
	intern->std.handlers = &fbird_service_handlers;
	return &intern->std;
}

static void fbird_service_free_obj(zend_object *obj)
{
	fbird_service_obj *intern = fbird_service_from_obj(obj);
	if (intern->fbsvc) {
		ISC_STATUS sv[20];
		fbsvc_detach(IBG(master_instance), intern->fbsvc, sv);
		fbsvc_free(intern->fbsvc);
		intern->fbsvc = NULL;
	}
	zend_object_std_dtor(obj);
}

/* Firebird\Service::__construct(string $host, string $user, string $pass) */
ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_service_construct, 0, 0, 3)
	ZEND_ARG_TYPE_INFO(0, host,     IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, username, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, password, IS_STRING, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdService, __construct)
{
	char *host, *user, *pass;
	size_t host_len, user_len, pass_len;
	ZEND_PARSE_PARAMETERS_START(3, 3)
		Z_PARAM_STRING(host, host_len)
		Z_PARAM_STRING(user, user_len)
		Z_PARAM_STRING(pass, pass_len)
	ZEND_PARSE_PARAMETERS_END();

	fbird_service_obj *intern = Z_FBIRD_SERVICE_P(ZEND_THIS);

	/* Build SPB: user + password */
	char buf[256];
	int buf_len = 0;
	buf[buf_len++] = isc_spb_version;
	buf[buf_len++] = isc_spb_current_version;
	buf[buf_len++] = isc_spb_user_name;
	buf[buf_len++] = (char)user_len;
	memcpy(buf + buf_len, user, user_len); buf_len += user_len;
	buf[buf_len++] = isc_spb_password;
	buf[buf_len++] = (char)pass_len;
	memcpy(buf + buf_len, pass, pass_len); buf_len += pass_len;

	/* Build service location: host:service_mgr */
	char loc[256];
	if (host_len > 0)
		snprintf(loc, sizeof(loc), "%s:service_mgr", host);
	else
		snprintf(loc, sizeof(loc), "%s", "service_mgr");

	ISC_STATUS sv[20];
	intern->fbsvc = fbsvc_attach(IBG(master_instance), loc,
		buf_len, (const unsigned char *)buf, sv);

	if (!intern->fbsvc) {
		_php_fbird_error();
		zend_throw_exception(fbird_service_exception_ce,
			"Failed to attach to Firebird service manager", 0);
	}
}

/* Firebird\Service::detach(): void */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_service_detach, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdService, detach)
{
	ZEND_PARSE_PARAMETERS_NONE();
	fbird_service_obj *intern = Z_FBIRD_SERVICE_P(ZEND_THIS);
	if (intern->fbsvc) {
		ISC_STATUS sv[20];
		fbsvc_detach(IBG(master_instance), intern->fbsvc, sv);
		fbsvc_free(intern->fbsvc);
		intern->fbsvc = NULL;
	}
}

/* Firebird\Service::isAttached(): bool */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_service_isAttached, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdService, isAttached)
{
	ZEND_PARSE_PARAMETERS_NONE();
	fbird_service_obj *intern = Z_FBIRD_SERVICE_P(ZEND_THIS);
	RETURN_BOOL(intern->fbsvc && fbsvc_is_attached(intern->fbsvc));
}

/* Firebird\Service::getServerVersion(): string */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_service_getServerVersion, 0, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdService, getServerVersion)
{
	ZEND_PARSE_PARAMETERS_NONE();
	fbird_service_obj *intern = Z_FBIRD_SERVICE_P(ZEND_THIS);
	if (!intern->fbsvc) {
		zend_throw_exception(fbird_service_exception_ce, "Not attached", 0);
		RETURN_THROWS();
	}

	static char spb[] = { isc_info_svc_timeout, 10, 0, 0, 0 };
	char info_action = isc_info_svc_server_version;
	char res_buf[256];
	ISC_STATUS sv[20];

	if (!fbsvc_query(IBG(master_instance), intern->fbsvc,
			sizeof(spb), (const unsigned char *)spb,
			1, (const unsigned char *)&info_action,
			sizeof(res_buf), (unsigned char *)res_buf, sv)) {
		_php_fbird_error();
		RETURN_STRING("");
	}

	char *result = res_buf;
	if (*result == isc_info_svc_server_version) {
		int len = isc_vax_integer(result + 1, 2);
		RETURN_STRINGL(result + 3, len);
	}
	RETURN_STRING("");
}

static const zend_function_entry fbird_service_methods[] = {
	PHP_ME(FirebirdService, __construct,      arginfo_fbird_service_construct,      ZEND_ACC_PUBLIC)
	PHP_ME(FirebirdService, detach,           arginfo_fbird_service_detach,         ZEND_ACC_PUBLIC)
	PHP_ME(FirebirdService, isAttached,       arginfo_fbird_service_isAttached,     ZEND_ACC_PUBLIC)
	PHP_ME(FirebirdService, getServerVersion, arginfo_fbird_service_getServerVersion, ZEND_ACC_PUBLIC)
	PHP_FE_END
};

/* Define connection methods table here — after all methods are declared */
static const zend_function_entry fbird_connection_methods[] = {
	PHP_ME(FirebirdConnection, __construct,      arginfo_fbird_connection_construct,        ZEND_ACC_PUBLIC)
	PHP_ME(FirebirdConnection, close,            arginfo_fbird_connection_close,            ZEND_ACC_PUBLIC)
	PHP_ME(FirebirdConnection, isConnected,      arginfo_fbird_connection_isConnected,      ZEND_ACC_PUBLIC)
	PHP_ME(FirebirdConnection, ping,             arginfo_fbird_connection_ping,             ZEND_ACC_PUBLIC)
	PHP_ME(FirebirdConnection, beginTransaction, arginfo_fbird_connection_beginTransaction, ZEND_ACC_PUBLIC)
	PHP_ME(FirebirdConnection, prepare,          arginfo_fbird_connection_prepare,          ZEND_ACC_PUBLIC)
	PHP_FE_END
};

/* -----------------------------------------------------------------------
 * Registration entry point called from PHP_MINIT_FUNCTION(fbird)
 * --------------------------------------------------------------------- */
void fbird_register_classes(void)
{
	zend_class_entry ce;

	/* B1: sub-exceptions */
	INIT_CLASS_ENTRY(ce, "Firebird\\ConnectionException", NULL);
	fbird_connection_exception_ce = zend_register_internal_class_ex(&ce, firebird_exception_ce);

	INIT_CLASS_ENTRY(ce, "Firebird\\QueryException", NULL);
	fbird_query_exception_ce = zend_register_internal_class_ex(&ce, firebird_exception_ce);

	INIT_CLASS_ENTRY(ce, "Firebird\\ServiceException", NULL);
	fbird_service_exception_ce = zend_register_internal_class_ex(&ce, firebird_exception_ce);

	/* B2: Firebird\Connection */
	INIT_CLASS_ENTRY(ce, "Firebird\\Connection", fbird_connection_methods);
	fbird_connection_ce = zend_register_internal_class(&ce);
	fbird_connection_ce->create_object = fbird_connection_create;

	memcpy(&fbird_connection_handlers, zend_get_std_object_handlers(),
		sizeof(zend_object_handlers));
	fbird_connection_handlers.offset    = XtOffsetOf(fbird_connection_obj, std);
	fbird_connection_handlers.free_obj  = fbird_connection_free;

	/* B3: Firebird\Transaction */
	INIT_CLASS_ENTRY(ce, "Firebird\\Transaction", fbird_transaction_methods);
	fbird_transaction_ce = zend_register_internal_class(&ce);
	fbird_transaction_ce->create_object = fbird_transaction_create;

	memcpy(&fbird_transaction_handlers, zend_get_std_object_handlers(),
		sizeof(zend_object_handlers));
	fbird_transaction_handlers.offset    = XtOffsetOf(fbird_transaction_obj, std);
	fbird_transaction_handlers.free_obj  = fbird_transaction_free;

	/* B4: Firebird\Statement */
	INIT_CLASS_ENTRY(ce, "Firebird\\Statement", fbird_statement_methods);
	fbird_statement_ce = zend_register_internal_class(&ce);
	fbird_statement_ce->create_object = fbird_statement_create;

	memcpy(&fbird_statement_handlers, zend_get_std_object_handlers(),
		sizeof(zend_object_handlers));
	fbird_statement_handlers.offset    = XtOffsetOf(fbird_statement_obj, std);
	fbird_statement_handlers.free_obj  = fbird_statement_free;

	/* B4: Firebird\ResultSet */
	INIT_CLASS_ENTRY(ce, "Firebird\\ResultSet", fbird_resultset_methods);
	fbird_resultset_ce = zend_register_internal_class(&ce);
	fbird_resultset_ce->create_object = fbird_resultset_create;

	memcpy(&fbird_resultset_handlers, zend_get_std_object_handlers(),
		sizeof(zend_object_handlers));
	fbird_resultset_handlers.offset    = XtOffsetOf(fbird_resultset_obj, std);
	fbird_resultset_handlers.free_obj  = fbird_resultset_free;

	/* B5: Firebird\Blob */
	INIT_CLASS_ENTRY(ce, "Firebird\\Blob", fbird_blob_methods);
	fbird_blob_ce = zend_register_internal_class(&ce);
	fbird_blob_ce->create_object = fbird_blob_create_obj;

	memcpy(&fbird_blob_handlers, zend_get_std_object_handlers(),
		sizeof(zend_object_handlers));
	fbird_blob_handlers.offset    = XtOffsetOf(fbird_blob_obj, std);
	fbird_blob_handlers.free_obj  = fbird_blob_free_obj;

	/* B6: Firebird\Service */
	INIT_CLASS_ENTRY(ce, "Firebird\\Service", fbird_service_methods);
	fbird_service_ce = zend_register_internal_class(&ce);
	fbird_service_ce->create_object = fbird_service_create_obj;

	memcpy(&fbird_service_handlers, zend_get_std_object_handlers(),
		sizeof(zend_object_handlers));
	fbird_service_handlers.offset    = XtOffsetOf(fbird_service_obj, std);
	fbird_service_handlers.free_obj  = fbird_service_free_obj;
}
