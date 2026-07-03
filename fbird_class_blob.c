/* fbird_class_blob.c - Firebird\Blob OOP class */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "zend_exceptions.h"
#include "php_firebird.h"
#include "php_fbird_includes.h"
#include "firebird_utils.h"
#include "fbird_class_internal.h"

zend_object_handlers fbird_blob_handlers;

zend_object *fbird_blob_create_obj(zend_class_entry *ce)
{
	fbird_blob_obj *intern = zend_object_alloc(sizeof(fbird_blob_obj), ce);
	intern->fbb_wrap = NULL;
	intern->blob_res = NULL;
	memset(&intern->blob_id, 0, sizeof(ISC_QUAD));
	zend_object_std_init(&intern->std, ce);
	object_properties_init(&intern->std, ce);
	intern->std.handlers = &fbird_blob_handlers;
	return &intern->std;
}

void fbird_blob_free_obj(zend_object *obj)
{
	fbird_blob_obj *intern = fbird_blob_from_obj(obj);
	if (intern->fbb_wrap) {
		ISC_STATUS sv[20];
		fbb_cancel(IBG(master_instance), intern->fbb_wrap, sv);
		fbb_free(intern->fbb_wrap);
		intern->fbb_wrap = NULL;
	}
	intern->blob_res = NULL;
	zend_object_std_dtor(obj);
}

zend_resource *fbird_blob_get_resource(zend_object *obj)
{
	fbird_blob_obj *intern = fbird_blob_from_obj(obj);
	return intern ? intern->blob_res : NULL;
}

void fbird_setup_blob_object(zval *return_value, zend_resource *res)
{
	zval_ptr_dtor(return_value);
	object_init_ex(return_value, fbird_blob_ce);
	fbird_blob_obj *intern = Z_FBIRD_BLOB_P(return_value);
	GC_ADDREF(res);
	intern->blob_res = res;
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_blob_create, 0, 2, MAY_BE_OBJECT)
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
	void *trans_ptr = NULL;
	if (tr->fbt_trans && fbt_is_active(tr->fbt_trans)) {
		trans_ptr = tr->fbt_trans;
	} else if (tr->trans_res && tr->trans_res->type > 0) {
		fbird_transaction *res_tr = (fbird_transaction *)tr->trans_res->ptr;
		if (res_tr && res_tr->fbt_transaction && fbt_is_active(res_tr->fbt_transaction)) {
			trans_ptr = res_tr->fbt_transaction;
		}
	}
	if (!trans_ptr) {
		zend_throw_exception(fbird_connection_exception_ce, "Transaction not active", 0);
		RETURN_THROWS();
	}

	object_init_ex(return_value, fbird_blob_ce);
	fbird_blob_obj *blob = Z_FBIRD_BLOB_P(return_value);

	ISC_STATUS sv[20];
	blob->fbb_wrap = fbb_create(IBG(master_instance),
		fbc_get_attachment(link->fbc_connection),
		fbt_get_handle(trans_ptr),
		&blob->blob_id, 0, NULL, sv);

	if (!blob->fbb_wrap) {
		_php_fbird_error();
		zend_throw_exception(fbird_query_exception_ce, "Failed to create blob", 0);
		RETURN_THROWS();
	}
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_blob_open, 0, 3, MAY_BE_OBJECT)
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
	void *trans_ptr = NULL;
	if (tr->fbt_trans && fbt_is_active(tr->fbt_trans)) {
		trans_ptr = tr->fbt_trans;
	} else if (tr->trans_res && tr->trans_res->type > 0) {
		fbird_transaction *res_tr = (fbird_transaction *)tr->trans_res->ptr;
		if (res_tr && res_tr->fbt_transaction && fbt_is_active(res_tr->fbt_transaction)) {
			trans_ptr = res_tr->fbt_transaction;
		}
	}
	if (!trans_ptr) {
		zend_throw_exception(fbird_connection_exception_ce, "Transaction not active", 0);
		RETURN_THROWS();
	}

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
		fbt_get_handle(trans_ptr),
		&blob_id, 0, NULL, sv);

	if (!blob->fbb_wrap) {
		_php_fbird_error();
		zend_throw_exception(fbird_query_exception_ce, "Failed to open blob", 0);
		RETURN_THROWS();
	}
}

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

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_blob_read, 0, 1, MAY_BE_STRING|MAY_BE_FALSE)
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

const zend_function_entry fbird_blob_methods[] = {
	PHP_ME(FirebirdBlob, create, arginfo_fbird_blob_create, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(FirebirdBlob, open,   arginfo_fbird_blob_open,   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(FirebirdBlob, write,  arginfo_fbird_blob_write,  ZEND_ACC_PUBLIC)
	PHP_ME(FirebirdBlob, read,   arginfo_fbird_blob_read,   ZEND_ACC_PUBLIC)
	PHP_ME(FirebirdBlob, close,  arginfo_fbird_blob_close,  ZEND_ACC_PUBLIC)
	PHP_ME(FirebirdBlob, getId,  arginfo_fbird_blob_getId,  ZEND_ACC_PUBLIC)
	PHP_FE_END
};
