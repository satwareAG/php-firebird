/*
 * fbird_class_decfloat.c - Firebird\DecFloat class (FB 4.0+)
 *
 * Container-only class for DECFLOAT(16) and DECFLOAT(34) values.
 * v1 scope: __toString(), toPrecision(), rawBytes()
 * Arithmetic operations tracked in separate issue.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "zend_exceptions.h"
#include "php_firebird.h"
#include "php_fbird_includes.h"
#include "firebird_utils.h"
#include "fbird_class_internal.h"

#if FB_API_VER >= 40

zend_class_entry *fbird_decfloat_ce;

/* Struct: stores raw bytes + precision (16 or 34) */
typedef struct {
	unsigned char  raw_bytes[16];  /* Max: FB_DEC34 = 16 bytes */
	unsigned char  byte_len;       /* 8 for DEC16, 16 for DEC34 */
	unsigned char  precision;      /* 16 or 34 */
	zend_object    std;
} fbird_decfloat_obj;

static inline fbird_decfloat_obj *fbird_decfloat_from_obj(zend_object *obj) {
	return (fbird_decfloat_obj *)((char *)obj - XtOffsetOf(fbird_decfloat_obj, std));
}
#define Z_FBIRD_DECFLOAT_P(zv) fbird_decfloat_from_obj(Z_OBJ_P(zv))

static zend_object_handlers fbird_decfloat_handlers;

/* Property entries for readonly properties */
static zend_object *fbird_decfloat_create(zend_class_entry *ce) {
	fbird_decfloat_obj *intern = zend_object_alloc(sizeof(fbird_decfloat_obj), ce);
	zend_object_std_init(&intern->std, ce);
	object_properties_init(&intern->std, ce);
	return &intern->std;
}

static void fbird_decfloat_free(zend_object *obj) {
	fbird_decfloat_obj *intern = fbird_decfloat_from_obj(obj);
	zend_object_std_dtor(&intern->std);
}

/* }}} */

/* {{{ Construct from string */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_decfloat_fromString, 0, 1, IS_OBJECT, 0)
	ZEND_ARG_TYPE_INFO(0, value, IS_STRING, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdDecFloat, fromString)
{
	zend_string *value;
	ZEND_PARSE_PARAMETERS_START(1, 1);
		Z_PARAM_STR(value);
	ZEND_PARSE_PARAMETERS_END();

	if (!FBG(master_instance)) {
		zend_throw_exception(NULL, "Firebird OO API not available", 0);
		RETURN_THROWS();
	}

	/* Try DEC34 first (wider type, accepts all DEC16 values) */
	FB_DEC34 dec34;
	if (fbu_string_to_decfloat34(FBG(master_instance), ZSTR_VAL(value), &dec34) == 0) {
		object_init_ex(return_value, fbird_decfloat_ce);
		fbird_decfloat_obj *intern = Z_FBIRD_DECFLOAT_P(return_value);
		memcpy(intern->raw_bytes, &dec34, sizeof(FB_DEC34));
		intern->byte_len = sizeof(FB_DEC34);
		intern->precision = 34;
		return;
	}

	/* Fall back to DEC16 */
	FB_DEC16 dec16;
	if (fbu_string_to_decfloat16(FBG(master_instance), ZSTR_VAL(value), &dec16) == 0) {
		object_init_ex(return_value, fbird_decfloat_ce);
		fbird_decfloat_obj *intern = Z_FBIRD_DECFLOAT_P(return_value);
		memcpy(intern->raw_bytes, &dec16, sizeof(FB_DEC16));
		intern->byte_len = sizeof(FB_DEC16);
		intern->precision = 16;
		return;
	}

	zend_throw_exception(NULL, "Failed to parse DECFLOAT value", 0);
	RETURN_THROWS();
}
/* }}} */

/* {{{ __toString */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_decfloat_toString, 0, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdDecFloat, __toString)
{
	ZEND_PARSE_PARAMETERS_NONE();

	fbird_decfloat_obj *intern = Z_FBIRD_DECFLOAT_P(ZEND_THIS);
	if (intern == NULL) RETURN_STRING("");

	if (!FBG(master_instance)) {
		RETURN_STRING("");
	}

	char buf[48];
	int rc;
	if (intern->precision == 16) {
		rc = fbu_decfloat16_to_string(FBG(master_instance), intern->raw_bytes, buf, sizeof(buf));
	} else {
		rc = fbu_decfloat34_to_string(FBG(master_instance), intern->raw_bytes, buf, sizeof(buf));
	}

	if (rc == 0) {
		RETURN_STRING(buf);
	}
	RETURN_STRING("");
}
/* }}} */

/* {{{ toPrecision() - returns precision (16 or 34) */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_decfloat_toPrecision, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdDecFloat, toPrecision)
{
	ZEND_PARSE_PARAMETERS_NONE();
	fbird_decfloat_obj *intern = Z_FBIRD_DECFLOAT_P(ZEND_THIS);
	RETURN_LONG(intern->precision);
}
/* }}} */

/* {{{ rawBytes() - returns raw binary representation */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_decfloat_rawBytes, 0, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdDecFloat, rawBytes)
{
	ZEND_PARSE_PARAMETERS_NONE();
	fbird_decfloat_obj *intern = Z_FBIRD_DECFLOAT_P(ZEND_THIS);
	RETURN_STRINGL((char *)intern->raw_bytes, intern->byte_len);
}
/* }}} */

/* {{{ __construct */
ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_decfloat_construct, 0, 0, 1)
	ZEND_ARG_TYPE_INFO(0, value, IS_STRING, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdDecFloat, __construct)
{
	zend_string *value;
	ZEND_PARSE_PARAMETERS_START(1, 1);
		Z_PARAM_STR(value);
	ZEND_PARSE_PARAMETERS_END();

	if (!FBG(master_instance)) {
		zend_throw_exception(NULL, "Firebird OO API not available", 0);
		return;
	}

	fbird_decfloat_obj *intern = Z_FBIRD_DECFLOAT_P(ZEND_THIS);

	/* Try DEC34 first (wider type) */
	FB_DEC34 dec34;
	if (fbu_string_to_decfloat34(FBG(master_instance), ZSTR_VAL(value), &dec34) == 0) {
		memcpy(intern->raw_bytes, &dec34, sizeof(FB_DEC34));
		intern->byte_len = sizeof(FB_DEC34);
		intern->precision = 34;
		return;
	}

	/* Fall back to DEC16 */
	FB_DEC16 dec16;
	if (fbu_string_to_decfloat16(FBG(master_instance), ZSTR_VAL(value), &dec16) == 0) {
		memcpy(intern->raw_bytes, &dec16, sizeof(FB_DEC16));
		intern->byte_len = sizeof(FB_DEC16);
		intern->precision = 16;
		return;
	}

	zend_throw_exception(NULL, "Failed to parse DECFLOAT value", 0);
}
/* }}} */

/* Method table */
const zend_function_entry fbird_decfloat_methods[] = {
	PHP_ME(FirebirdDecFloat, __construct, arginfo_fbird_decfloat_construct, ZEND_ACC_PUBLIC)
	PHP_ME(FirebirdDecFloat, fromString, arginfo_fbird_decfloat_fromString, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(FirebirdDecFloat, __toString, arginfo_fbird_decfloat_toString, ZEND_ACC_PUBLIC)
	PHP_ME(FirebirdDecFloat, toPrecision, arginfo_fbird_decfloat_toPrecision, ZEND_ACC_PUBLIC)
	PHP_ME(FirebirdDecFloat, rawBytes, arginfo_fbird_decfloat_rawBytes, ZEND_ACC_PUBLIC)
	PHP_FE_END
};

/* Registration (called from fbird_register_classes) */
void fbird_register_decfloat_class(void)
{
	zend_class_entry ce;
	INIT_CLASS_ENTRY(ce, "Firebird\\DecFloat", fbird_decfloat_methods);
	fbird_decfloat_ce = zend_register_internal_class(&ce);
	fbird_decfloat_ce->create_object = fbird_decfloat_create;
	memcpy(&fbird_decfloat_handlers, zend_get_std_object_handlers(),
		sizeof(zend_object_handlers));
	fbird_decfloat_handlers.offset = XtOffsetOf(fbird_decfloat_obj, std);
	fbird_decfloat_handlers.free_obj = fbird_decfloat_free;
}

/* Create a DecFloat object from raw bytes (for internal fetch path) */
void fbird_setup_decfloat_object(zval *return_value, unsigned char precision,
	const void *raw_bytes, unsigned char byte_len)
{
	object_init_ex(return_value, fbird_decfloat_ce);
	fbird_decfloat_obj *intern = Z_FBIRD_DECFLOAT_P(return_value);
	memcpy(intern->raw_bytes, raw_bytes, byte_len);
	intern->byte_len = byte_len;
	intern->precision = precision;
}

#endif /* FB_API_VER >= 40 */
