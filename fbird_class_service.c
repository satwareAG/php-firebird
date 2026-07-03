/* fbird_class_service.c - Firebird\Service OOP class */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "zend_exceptions.h"
#include "php_firebird.h"
#include "php_fbird_includes.h"
#include "firebird_utils.h"
#include "fbird_class_internal.h"

zend_object_handlers fbird_service_handlers;

zend_object *fbird_service_create_obj(zend_class_entry *ce)
{
	fbird_service_obj *intern = zend_object_alloc(sizeof(fbird_service_obj), ce);
	intern->svc_res = NULL;
	intern->fbsvc = NULL;
	zend_object_std_init(&intern->std, ce);
	object_properties_init(&intern->std, ce);
	intern->std.handlers = &fbird_service_handlers;
	return &intern->std;
}

void fbird_service_free_obj(zend_object *obj)
{
	fbird_service_obj *intern = fbird_service_from_obj(obj);
	if (intern->svc_res) {
		intern->svc_res = NULL;
		intern->fbsvc = NULL;
		zend_object_std_dtor(obj);
		return;
	}
	if (intern->fbsvc) {
		ISC_STATUS sv[20];
		fbsvc_detach(IBG(master_instance), intern->fbsvc, sv);
		fbsvc_free(intern->fbsvc);
		intern->fbsvc = NULL;
	}
	zend_object_std_dtor(obj);
}

zend_resource *fbird_service_get_resource(zend_object *obj)
{
	fbird_service_obj *intern = fbird_service_from_obj(obj);
	return intern ? intern->svc_res : NULL;
}

void fbird_setup_service_object(zval *return_value, zend_resource *res)
{
	zval_ptr_dtor(return_value);
	object_init_ex(return_value, fbird_service_ce);
	fbird_service_obj *intern = Z_FBIRD_SERVICE_P(return_value);
	intern->svc_res = res;
	intern->fbsvc = res && res->ptr ? ((fbird_service *)res->ptr)->fbsvc_service : NULL;
}

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

	if (user_len > 255) {
		zend_throw_exception(fbird_service_exception_ce,
			"Username exceeds maximum SPB length of 255 bytes", 0);
		RETURN_THROWS();
	}
	if (pass_len > 255) {
		zend_throw_exception(fbird_service_exception_ce,
			"Password exceeds maximum SPB length of 255 bytes", 0);
		RETURN_THROWS();
	}
	if (6 + user_len + pass_len > 256) {
		zend_throw_exception(fbird_service_exception_ce,
			"Combined credentials exceed SPB buffer capacity", 0);
		RETURN_THROWS();
	}
	if (host_len > 256 - 13) {
		zend_throw_exception(fbird_service_exception_ce,
			"Hostname exceeds maximum length", 0);
		RETURN_THROWS();
	}

	fbird_service_obj *intern = Z_FBIRD_SERVICE_P(ZEND_THIS);
	if (intern->svc_res && intern->svc_res->ptr) {
		intern->fbsvc = ((fbird_service *)intern->svc_res->ptr)->fbsvc_service;
	}

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

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_service_detach, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdService, detach)
{
	ZEND_PARSE_PARAMETERS_NONE();
	fbird_service_obj *intern = Z_FBIRD_SERVICE_P(ZEND_THIS);
	if (intern->svc_res) {
		zend_list_delete(intern->svc_res);
		intern->svc_res = NULL;
		intern->fbsvc = NULL;
		return;
	}
	if (intern->fbsvc) {
		ISC_STATUS sv[20];
		fbsvc_detach(IBG(master_instance), intern->fbsvc, sv);
		fbsvc_free(intern->fbsvc);
		intern->fbsvc = NULL;
	}
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_service_isAttached, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdService, isAttached)
{
	ZEND_PARSE_PARAMETERS_NONE();
	fbird_service_obj *intern = Z_FBIRD_SERVICE_P(ZEND_THIS);
	if (intern->svc_res && intern->svc_res->ptr) {
		intern->fbsvc = ((fbird_service *)intern->svc_res->ptr)->fbsvc_service;
	}
	RETURN_BOOL(intern->fbsvc && fbsvc_is_attached(intern->fbsvc));
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_service_getServerVersion, 0, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdService, getServerVersion)
{
	ZEND_PARSE_PARAMETERS_NONE();
	fbird_service_obj *intern = Z_FBIRD_SERVICE_P(ZEND_THIS);
	if (intern->svc_res && intern->svc_res->ptr) {
		intern->fbsvc = ((fbird_service *)intern->svc_res->ptr)->fbsvc_service;
	}
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
	const char *end = res_buf + sizeof(res_buf);
	if (result < end && *result == isc_info_svc_server_version) {
		if (result + 3 > end) {
			RETURN_STRING("");
		}
		int len = isc_vax_integer(result + 1, 2);
		if (len < 0 || result + 3 + len > end) {
			RETURN_STRING("");
		}
		RETURN_STRINGL(result + 3, len);
	}
	RETURN_STRING("");
}

const zend_function_entry fbird_service_methods[] = {
	PHP_ME(FirebirdService, __construct,      arginfo_fbird_service_construct,      ZEND_ACC_PUBLIC)
	PHP_ME(FirebirdService, detach,           arginfo_fbird_service_detach,         ZEND_ACC_PUBLIC)
	PHP_ME(FirebirdService, isAttached,       arginfo_fbird_service_isAttached,     ZEND_ACC_PUBLIC)
	PHP_ME(FirebirdService, getServerVersion, arginfo_fbird_service_getServerVersion, ZEND_ACC_PUBLIC)
	PHP_FE_END
};
