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
	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

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
	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	if (FBG(sql_code) != 0) {
		RETURN_LONG(FBG(sql_code));
	}
	RETURN_FALSE;
}

PHP_FUNCTION(fbird_sqlstate)
{
	char sqlstate[6]; /* 5 chars + null terminator */

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	/* Check if there is an error to report */
	if (FBG(sql_code) == 0) {
		RETURN_FALSE;
	}

	/* Call fb_sqlstate to get the SQLSTATE code from the status vector */
	fb_sqlstate(sqlstate, IB_STATUS);

	/* fb_sqlstate always returns a 5-character string, with "00000" for success */
	if (sqlstate[0] == '0' && sqlstate[1] == '0' && sqlstate[2] == '0' &&
	    sqlstate[3] == '0' && sqlstate[4] == '0') {
		/* No error state */
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

PHP_FUNCTION(fbird_set_exception_mode)
{
	zend_long mode;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "l", &mode) == FAILURE) {
		return;
	}

	if (mode != FBIRD_EXCEPTION_MODE_SILENT && mode != FBIRD_EXCEPTION_MODE_THROW) {
		php_error_docref(NULL, E_WARNING,
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

	/* Call fb_sqlstate to get the SQLSTATE code from the status vector */
	fb_sqlstate(sqlstate, IB_STATUS);

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
void _php_fbird_error(void)
{
	char *s = FBG(errmsg);
	const ISC_STATUS *statusp = IB_STATUS;
	size_t msg_len;

	FBG(sql_code) = fbu_sqlcode(IB_STATUS);

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

#endif /* HAVE_FIREBIRD */
