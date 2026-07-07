/* fbird_class_resultset.c - Firebird\ResultSet OOP class */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_firebird.h"
#include "php_fbird_includes.h"
#include "firebird_utils.h"
#include "fbird_class_internal.h"
#include "php_fbird_query_internal.h"

zend_object_handlers fbird_resultset_handlers;

zend_object *fbird_resultset_create(zend_class_entry *ce)
{
	fbird_resultset_obj *intern = zend_object_alloc(sizeof(fbird_resultset_obj), ce);
	intern->query_res = NULL;
	zend_object_std_init(&intern->std, ce);
	object_properties_init(&intern->std, ce);
	intern->std.handlers = &fbird_resultset_handlers;
	return &intern->std;
}

void fbird_resultset_free(zend_object *obj)
{
	fbird_resultset_obj *intern = fbird_resultset_from_obj(obj);
	if (intern->query_res) {
		zend_list_delete(intern->query_res);
		intern->query_res = NULL;
	}
	zend_object_std_dtor(obj);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_resultset_fetch, 0, 0, MAY_BE_ARRAY|MAY_BE_FALSE)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdResultSet, fetch)
{
	ZEND_PARSE_PARAMETERS_NONE();
	fbird_resultset_obj *intern = Z_FBIRD_RESULTSET_P(ZEND_THIS);
	if (!intern->query_res) {
		RETURN_FALSE;
	}
	fbird_query *fb_query = (fbird_query *) intern->query_res->ptr;
	if (!fb_query) { RETURN_FALSE; }
	_php_fbird_fetch_hash_query(fb_query, FBIRD_FETCH_ASSOC, 0, return_value);
}

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

const zend_function_entry fbird_resultset_methods[] = {
	PHP_ME(FirebirdResultSet, fetch, arginfo_fbird_resultset_fetch, ZEND_ACC_PUBLIC)
	PHP_ME(FirebirdResultSet, close, arginfo_fbird_resultset_close, ZEND_ACC_PUBLIC)
	PHP_FE_END
};

zend_resource *fbird_resultset_get_resource(zend_object *obj)
{
	fbird_resultset_obj *intern = fbird_resultset_from_obj(obj);
	return intern ? intern->query_res : NULL;
}

void fbird_setup_resultset_object(zval *return_value, zend_resource *res)
{
	zval_ptr_dtor(return_value);
	object_init_ex(return_value, fbird_resultset_ce);
	fbird_resultset_obj *intern = Z_FBIRD_RESULTSET_P(return_value);
	GC_ADDREF(res);
	intern->query_res = res;
}
