/* fbird_class_statement.c - Firebird\Statement OOP class */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "zend_exceptions.h"
#include "php_firebird.h"
#include "php_fbird_includes.h"
#include "firebird_utils.h"
#include "fbird_class_internal.h"
#include "php_fbird_query_internal.h"

zend_object_handlers fbird_statement_handlers;

zend_object *fbird_statement_create(zend_class_entry *ce)
{
	fbird_statement_obj *intern = zend_object_alloc(sizeof(fbird_statement_obj), ce);
	intern->query_res = NULL;
	zend_object_std_init(&intern->std, ce);
	object_properties_init(&intern->std, ce);
	intern->std.handlers = &fbird_statement_handlers;
	return &intern->std;
}

void fbird_statement_free(zend_object *obj)
{
	fbird_statement_obj *intern = fbird_statement_from_obj(obj);
	if (intern->query_res) {
		zend_list_delete(intern->query_res);
		intern->query_res = NULL;
	}
	zend_object_std_dtor(obj);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_statement_execute, 0, 1, MAY_BE_OBJECT)
	ZEND_ARG_OBJ_INFO(0, transaction, Firebird\\Transaction, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdStatement, execute)
{
	zval *tr_zv;
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJECT_OF_CLASS(tr_zv, fbird_transaction_ce)
	ZEND_PARSE_PARAMETERS_END();

	fbird_statement_obj *intern = Z_FBIRD_STATEMENT_P(ZEND_THIS);
	if (!intern->query_res || intern->query_res->type <= 0) {
		zend_throw_exception(fbird_query_exception_ce, "Statement not prepared or freed", 0);
		RETURN_THROWS();
	}

	fbird_query *ib_query = (fbird_query *)intern->query_res->ptr;
	if (!ib_query) {
		zend_throw_exception(fbird_query_exception_ce, "Statement has no query data", 0);
		RETURN_THROWS();
	}

	RETVAL_FALSE;
	if (FAILURE == _php_fbird_exec(INTERNAL_FUNCTION_PARAM_PASSTHRU, ib_query, NULL, 0)) {
		zend_throw_exception(fbird_query_exception_ce, "Failed to execute statement", 0);
		RETURN_THROWS();
	}

	if (Z_TYPE_P(return_value) == IS_RESOURCE &&
	    Z_RES_TYPE_P(return_value) == le_query) {
		fbird_setup_resultset_object(return_value, Z_RES_P(return_value));
	} else if (Z_TYPE_P(return_value) != IS_RESOURCE) {
		object_init_ex(return_value, fbird_resultset_ce);
		fbird_resultset_obj *rs = Z_FBIRD_RESULTSET_P(return_value);
		rs->query_res = intern->query_res;
		GC_ADDREF(rs->query_res);
	}
}

const zend_function_entry fbird_statement_methods[] = {
	PHP_ME(FirebirdStatement, execute, arginfo_fbird_statement_execute, ZEND_ACC_PUBLIC)
	PHP_FE_END
};

zend_resource *fbird_statement_get_resource(zend_object *obj)
{
	fbird_statement_obj *intern = fbird_statement_from_obj(obj);
	return intern ? intern->query_res : NULL;
}

void fbird_setup_statement_object(zval *return_value, zend_resource *res)
{
	zval_ptr_dtor(return_value);
	object_init_ex(return_value, fbird_statement_ce);
	fbird_statement_obj *intern = Z_FBIRD_STATEMENT_P(return_value);
	GC_ADDREF(res);
	intern->query_res = res;
}
