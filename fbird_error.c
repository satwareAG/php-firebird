/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"

#if HAVE_FIREBIRD

#include "php_ini.h"
#include "zend_exceptions.h"
#include "php_firebird.h"
#include "php_fbird_includes.h"
#include "firebird_utils.h"

PHP_FUNCTION(fbird_errmsg)
{
	zval *link_arg = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|z!", &link_arg) == FAILURE) {
		RETURN_THROWS();
	}

	/* #439: Per-connection error context */
	if (link_arg && Z_TYPE_P(link_arg) == IS_RESOURCE) {
		zend_resource *res = Z_RES_P(link_arg);
		fbird_db_link *link = (fbird_db_link *)zend_fetch_resource(res, "Firebird link", le_link);
		if (!link) link = (fbird_db_link *)zend_fetch_resource(res, "Firebird link", le_plink);
		if (link && link->errmsg[0] != '\0') {
			RETURN_STRING(link->errmsg);
		}
	}

	/* BC: fall back to global error */
	if (FBG(sql_code) != 0) {
		RETURN_STRING(FBG(errmsg));
	}

	RETURN_FALSE;
}

PHP_FUNCTION(fbird_get_client_version)
{
	RETURN_DOUBLE((double)FBG(client_major_version) + (double)FBG(client_minor_version) / 10);
}

PHP_FUNCTION(fbird_get_client_major_version)
{
	RETURN_LONG(FBG(client_major_version));
}

PHP_FUNCTION(fbird_get_client_minor_version)
{
	RETURN_LONG(FBG(client_minor_version));
}

PHP_FUNCTION(fbird_errcode)
{
	zval *link_arg = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|z!", &link_arg) == FAILURE) {
		RETURN_THROWS();
	}

	/* #439: Per-connection error context - check link first */
	if (link_arg && Z_TYPE_P(link_arg) == IS_RESOURCE) {
		zend_resource *res = Z_RES_P(link_arg);
		fbird_db_link *link = (fbird_db_link *)zend_fetch_resource(res, "Firebird link", le_link);
		if (!link) link = (fbird_db_link *)zend_fetch_resource(res, "Firebird link", le_plink);
		if (link && link->errmsg[0] != '\0') {
			RETURN_LONG(isc_sqlcode(link->last_status));
		}
	}

	if (FBG(sql_code) != 0) {
		RETURN_LONG(FBG(sql_code));
	}
	RETURN_FALSE;
}

PHP_FUNCTION(fbird_sqlstate)
{
	char sqlstate[6];
	zval *link_arg = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|z!", &link_arg) == FAILURE) {
		RETURN_THROWS();
	}

	ISC_STATUS *status_to_check = FBG(last_status);

	/* #439: Per-connection error context */
	if (link_arg && Z_TYPE_P(link_arg) == IS_RESOURCE) {
		zend_resource *res = Z_RES_P(link_arg);
		fbird_db_link *link = (fbird_db_link *)zend_fetch_resource(res, "Firebird link", le_link);
		if (!link) link = (fbird_db_link *)zend_fetch_resource(res, "Firebird link", le_plink);
		if (link && link->errmsg[0] != '\0') {
			status_to_check = link->last_status;
		}
	}

	if (status_to_check[0] == 0) {
		RETURN_FALSE;
	}

	fb_sqlstate(sqlstate, status_to_check);

	if (sqlstate[0] == '0' && sqlstate[1] == '0' && sqlstate[2] == '0' &&
	    sqlstate[3] == '0' && sqlstate[4] == '0') {
		RETURN_FALSE;
	}

	RETURN_STRINGL(sqlstate, 5);
}

PHP_FUNCTION(fbird_escape_string)
{
	zend_string *str;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(str)
	ZEND_PARSE_PARAMETERS_END();

	/* Count single quotes to calculate required buffer size */
	size_t quote_count = 0;
	const char *p = ZSTR_VAL(str);
	size_t len = ZSTR_LEN(str);

	for (size_t i = 0; i < len; i++) {
		if (p[i] == '\'') {
			quote_count++;
		}
	}

	/* If no quotes, return copy of original string */
	if (quote_count == 0) {
		RETURN_STR_COPY(str);
	}

	/* Allocate buffer for escaped string (original + extra quotes) */
	zend_string *escaped = zend_string_alloc(len + quote_count, 0);
	char *out = ZSTR_VAL(escaped);

	/* Copy string, doubling single quotes */
	for (size_t i = 0; i < len; i++) {
		if (p[i] == '\'') {
			*out++ = '\'';
			*out++ = '\'';
		} else {
			*out++ = p[i];
		}
	}
	*out = '\0';

	RETURN_NEW_STR(escaped);
}

/* #374: fbird_escape_literal - quote string for SQL literals */
PHP_FUNCTION(fbird_escape_literal)
{
	zend_string *str;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(str)
	ZEND_PARSE_PARAMETERS_END();

	/* Count single quotes for buffer sizing */
	size_t quote_count = 0;
	const char *p = ZSTR_VAL(str);
	size_t len = ZSTR_LEN(str);
	for (size_t i = 0; i < len; i++) {
		if (p[i] == '\'') quote_count++;
	}

	/* Buffer: original + doubled quotes + 2 wrapping quotes + NUL */
	zend_string *escaped = zend_string_alloc(len + quote_count + 2, 0);
	char *out = ZSTR_VAL(escaped);

	*out++ = '\'';
	for (size_t i = 0; i < len; i++) {
		if (p[i] == '\'') {
			*out++ = '\'';
			*out++ = '\'';
		} else {
			*out++ = p[i];
		}
	}
	*out++ = '\'';
	*out = '\0';

	RETURN_NEW_STR(escaped);
}

/* #374: fbird_escape_identifier - quote identifier with double quotes */
PHP_FUNCTION(fbird_escape_identifier)
{
	zend_string *str;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(str)
	ZEND_PARSE_PARAMETERS_END();

	/* Count double quotes for buffer sizing */
	size_t quote_count = 0;
	const char *p = ZSTR_VAL(str);
	size_t len = ZSTR_LEN(str);
	for (size_t i = 0; i < len; i++) {
		if (p[i] == '"') quote_count++;
	}

	/* Buffer: original + doubled quotes + 2 wrapping quotes + NUL */
	zend_string *escaped = zend_string_alloc(len + quote_count + 2, 0);
	char *out = ZSTR_VAL(escaped);

	*out++ = '"';
	for (size_t i = 0; i < len; i++) {
		if (p[i] == '"') {
			*out++ = '"';
			*out++ = '"';
		} else {
			*out++ = p[i];
		}
	}
	*out++ = '"';
	*out = '\0';

	RETURN_NEW_STR(escaped);
}

PHP_FUNCTION(fbird_set_exception_mode)
{
	zend_long mode;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "l", &mode) == FAILURE) {
		return;
	}

	if (mode != FBIRD_EXCEPTION_MODE_SILENT && mode != FBIRD_EXCEPTION_MODE_THROW) {
		_php_fbird_module_error(
			"Invalid mode (use FBIRD_EXCEPTION_MODE_SILENT or FBIRD_EXCEPTION_MODE_THROW)");
		RETURN_FALSE;
	}

	FBG(exception_mode) = (int)mode;
	RETURN_TRUE;
}

PHP_FUNCTION(fbird_get_exception_mode)
{
	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	RETURN_LONG(FBG(exception_mode));
}

PHP_METHOD(FirebirdException, getSqlState)
{
	char sqlstate[6]; /* 5 chars + null terminator */

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	/* Call fb_sqlstate to get the SQLSTATE code from the last error status vector */
	fb_sqlstate(sqlstate, FBG(last_status));

	/* Always return the SQLSTATE (even if "00000" for compatibility) */
	RETURN_STRINGL(sqlstate, 5);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_firebird_exception_getSqlState, 0, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

const zend_function_entry firebird_exception_methods[] = {
	PHP_ME(FirebirdException, getSqlState, arginfo_firebird_exception_getSqlState, ZEND_ACC_PUBLIC)
	PHP_FE_END
};

/* print firebird error and save it for fbird_errmsg() */
void _php_fbird_error(ISC_STATUS *status)
{
	char *s = FBG(errmsg);
	const ISC_STATUS *statusp = status;
	size_t msg_len;

	/* Store the status vector for fbird_sqlstate() */
	memcpy(FBG(last_status), status, sizeof(FBG(last_status)));

	FBG(sql_code) = fbu_sqlcode(status);

	msg_len = strlen(FBG(errmsg));
	while (msg_len < MAX_ERRMSG && fb_interpret(s, MAX_ERRMSG - msg_len - 1, &statusp)) {
		msg_len = strlen(s);
		s[msg_len] = ' ';
		s[msg_len + 1] = '\0';
		msg_len = s - FBG(errmsg) + msg_len + 1;
		s = FBG(errmsg) + msg_len;
	}

	/* Check runtime exception_mode */
	if (FBG(exception_mode) == FBIRD_EXCEPTION_MODE_THROW) {
		zend_throw_exception(firebird_exception_ce, FBG(errmsg), FBG(sql_code));
	} else {
		php_error_docref(NULL, E_WARNING, "%s", FBG(errmsg));
	}
}

/* print php firebird module error and save it for fbird_errmsg() */
void _php_fbird_module_error(const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);

	/* Module errors have no Firebird status vector — clear last_status */
	memset(FBG(last_status), 0, sizeof(FBG(last_status)));

	/* vsnprintf NUL terminates the buf and writes at most n-1 chars+NUL */
	vsnprintf(FBG(errmsg), MAX_ERRMSG, msg, ap);
	va_end(ap);

	FBG(sql_code) = -999; /* no SQL error */

	/* Check runtime exception_mode */
	if (FBG(exception_mode) == FBIRD_EXCEPTION_MODE_THROW) {
		zend_throw_exception(firebird_exception_ce, FBG(errmsg), FBG(sql_code));
	} else {
		php_error_docref(NULL, E_WARNING, "%s", FBG(errmsg));
	}
}

/* #439: Per-connection error context - copy global error to link storage */
void _php_fbird_error_for_link(ISC_STATUS *status, void *link_ptr)
{
	_php_fbird_error(status);
	if (link_ptr) {
		fbird_db_link *link = (fbird_db_link *)link_ptr;
		strncpy(link->errmsg, FBG(errmsg), sizeof(link->errmsg) - 1);
		link->errmsg[sizeof(link->errmsg) - 1] = '\0';
		memcpy(link->last_status, status, sizeof(link->last_status));
	}
}

/* #370: fbird_error_list - return array of all error entries from status vector */
PHP_FUNCTION(fbird_error_list)
{
	zval *link_arg = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|z!", &link_arg) == FAILURE) {
		RETURN_THROWS();
	}

	ISC_STATUS *status = FBG(last_status);

	if (link_arg && Z_TYPE_P(link_arg) == IS_RESOURCE) {
		zend_resource *res = Z_RES_P(link_arg);
		fbird_db_link *link = (fbird_db_link *)zend_fetch_resource(res, "Firebird link", le_link);
		if (!link) link = (fbird_db_link *)zend_fetch_resource(res, "Firebird link", le_plink);
		if (link && link->errmsg[0] != '\0') {
			status = link->last_status;
		}
	}

	if (status[0] == 0) {
		RETURN_FALSE;
	}

	array_init(return_value);

	/* Iterate status vector entries */
	const ISC_STATUS *p = status;
	while (*p != isc_arg_end) {
		char msg[512] = {0};
		if (fb_interpret(msg, sizeof(msg), &p)) {
			add_next_index_string(return_value, msg);
		} else {
			break;
		}
	}
}

/* #371: fbird_error_field - return structured diagnostic fields */
PHP_FUNCTION(fbird_error_field)
{
	zval *link_arg = NULL;
	zend_long field = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|zl", &link_arg, &field) == FAILURE) {
		RETURN_THROWS();
	}

	ISC_STATUS *status = FBG(last_status);

	if (link_arg && Z_TYPE_P(link_arg) == IS_RESOURCE) {
		zend_resource *res = Z_RES_P(link_arg);
		fbird_db_link *link = (fbird_db_link *)zend_fetch_resource(res, "Firebird link", le_link);
		if (!link) link = (fbird_db_link *)zend_fetch_resource(res, "Firebird link", le_plink);
		if (link && link->errmsg[0] != '\0') {
			status = link->last_status;
		}
	}

	if (status[0] == 0) {
		RETURN_FALSE;
	}

	/* If field number specified (1=SQLSTATE, 2=SQLCODE, 3=MESSAGE), return single value */
	if (field > 0) {
		char sqlstate[6];
		fb_sqlstate(sqlstate, status);
		switch (field) {
			case 1: RETURN_STRINGL(sqlstate, 5);
			case 2: RETURN_LONG(isc_sqlcode(status));
			case 3: {
				char errmsg[MAX_ERRMSG] = {0};
				const ISC_STATUS *p = status;
				while (fb_interpret(errmsg + strlen(errmsg), MAX_ERRMSG - strlen(errmsg) - 1, &p)) {
					size_t len = strlen(errmsg);
					if (len < MAX_ERRMSG - 1) { errmsg[len] = ' '; errmsg[len + 1] = '\0'; }
				}
				RETURN_STRING(errmsg);
			}
			default: RETURN_FALSE;
		}
	}

	array_init(return_value);

	char sqlstate[6];
	fb_sqlstate(sqlstate, status);
	add_assoc_string(return_value, "sqlstate", sqlstate);
	add_assoc_long(return_value, "sqlcode", isc_sqlcode(status));

	/* Full error message */
	char errmsg[MAX_ERRMSG] = {0};
	const ISC_STATUS *p = status;
	while (fb_interpret(errmsg + strlen(errmsg), MAX_ERRMSG - strlen(errmsg) - 1, &p)) {
		size_t len = strlen(errmsg);
		if (len < MAX_ERRMSG - 1) {
			errmsg[len] = ' ';
			errmsg[len + 1] = '\0';
		}
	}
	add_assoc_string(return_value, "message", errmsg);
}

#endif /* HAVE_FIREBIRD */
