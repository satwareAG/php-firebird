/*
   +----------------------------------------------------------------------+
   | PHP Version 8                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) The PHP Group                                          |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
 */

/**
 * Array handling functions.
 *
 * This file contains functions for allocating array descriptors and binding
 * PHP arrays to Firebird array format.
 */

/* Enable POSIX extensions for strptime() - must be defined before any includes */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>
#include <string.h>

#include "php.h"
#include "php_ini.h"

#if HAVE_FIREBIRD

#include "php_firebird.h"
#include "php_fbird_includes.h"
#include "php_fbird_query_internal.h"
#include "php_fbird_query_array.h"
#include "php_fbird_query_prepare.h"
#include "firebird_utils.h"

#define ISC_LONG_MIN    INT_MIN
#define ISC_LONG_MAX    INT_MAX

int _php_fbird_alloc_array(fbird_array **ib_arrayp, XSQLDA *sqlda, /* {{{ */
	fb_safe_handle link, fb_safe_handle trans, unsigned short *array_cnt)
{
	unsigned short i, n;
	fbird_array *ar;
	/* first check if we have any arrays at all */
	for (i = *array_cnt = 0; i < sqlda->sqld; ++i) {
		if ((sqlda->sqlvar[i].sqltype & ~1) == SQL_ARRAY) {
			++*array_cnt;
		}
	}
	if (! *array_cnt) return SUCCESS;

	ar = ecalloc(*array_cnt, sizeof(fbird_array));

	for (i = n = 0; i < sqlda->sqld; ++i) {
		unsigned short dim;
		zend_ulong ar_size = 1;
		XSQLVAR *var = &sqlda->sqlvar[i];

		if ((var->sqltype & ~1) != SQL_ARRAY) {
			 continue;
		}

		fbird_array *a = &ar[n++];
		ISC_ARRAY_DESC *ar_desc = &a->ar_desc;

        /* Fix stack smashing: Copy names to local HEAP buffers to ensure
         * safe access by isc_array_lookup_bounds and avoid stack corruption. */
        /* Increased buffer size for metadata names to support future expansion
         * and avoid truncation warnings */
        char *rname = ecalloc(1, MAX_IDENTIFIER_LEN + 1);
        char *sname = ecalloc(1, MAX_IDENTIFIER_LEN + 1);

		if (!rname || !sname) {
			_php_fbird_module_error("Failed to allocate memory for array names");
			if (rname) efree(rname);
			if (sname) efree(sname);
			efree(ar);
			return FAILURE;
		}

        /* Use length fields if available and valid, otherwise safe limit to struct size */
        if (var->relname) {
            int len = var->relname_length;
            if (len > 32) len = 32; /* Cap to XSQLVAR limit */
            if (len > 0) memcpy(rname, var->relname, len);
        }
        if (var->sqlname) {
            int len = var->sqlname_length;
            if (len > 32) len = 32; /* Cap to XSQLVAR limit */
            if (len > 0) memcpy(sname, var->sqlname, len);
        }

		if (isc_array_lookup_bounds(IB_STATUS, &link.db, &trans.tr, rname,
				sname, ar_desc)) {
			_php_fbird_error();
			efree(ar);
            efree(rname);
            efree(sname);
			return FAILURE;
		}
        efree(rname);
        efree(sname);

		switch (ar_desc->array_desc_dtype) {
			case blr_text:
			case blr_text2:
				a->el_type = SQL_TEXT;
				a->el_size = ar_desc->array_desc_length;
				break;
#ifdef SQL_BOOLEAN
				case blr_bool:
					a->el_type = SQL_BOOLEAN;
					a->el_size = sizeof(FB_BOOLEAN);
					break;
#endif
			case blr_short:
				a->el_type = SQL_SHORT;
				a->el_size = sizeof(short);
				break;
			case blr_long:
				a->el_type = SQL_LONG;
				a->el_size = sizeof(ISC_LONG);
				break;
			case blr_float:
				a->el_type = SQL_FLOAT;
				a->el_size = sizeof(float);
				break;
			case blr_double:
				a->el_type = SQL_DOUBLE;
				a->el_size = sizeof(double);
				break;
			case blr_int64:
				a->el_type = SQL_INT64;
				a->el_size = sizeof(ISC_INT64);
				break;
			case blr_timestamp:
				a->el_type = SQL_TIMESTAMP;
				a->el_size = sizeof(ISC_TIMESTAMP);
				break;
			case blr_sql_date:
				a->el_type = SQL_TYPE_DATE;
				a->el_size = sizeof(ISC_DATE);
				break;
			case blr_sql_time:
				a->el_type = SQL_TYPE_TIME;
				a->el_size = sizeof(ISC_TIME);
				break;
#if FB_API_VER >= 40
			/* These are converted to VARCHAR via isc_dpb_set_bind tag at connect */
			/* blr_dec64 */
			/* blr_dec128 */
			/* blr_int128 */
			case blr_sql_time_tz:
				a->el_type = SQL_TIME_TZ;
				a->el_size = sizeof(ISC_TIME_TZ);
				break;
			case blr_timestamp_tz:
				a->el_type = SQL_TIMESTAMP_TZ;
				a->el_size = sizeof(ISC_TIMESTAMP_TZ);
				break;
#endif
			case blr_varying:
			case blr_varying2:
				/*
				 * WORKAROUND: Treat VARCHAR arrays as TEXT (CHAR)
				 * Firebird's isc_array_put_slice/get_slice appear to mishandle SQL_VARYING
				 * stride alignment or format in some versions (PHP buffer corruption).
				 * Treating them as SQL_TEXT logic works reliably:
				 * - Write as blank-padded text (Firebird converts to VARCHAR storage)
				 * - Read as blank-padded text (Firebird converts from VARCHAR)
				 *
				 * We modify the descriptor in place so put_slice sees blr_text.
				 *
				 * NOTE: Previous UTF8 heuristic was REMOVED as it was incorrect.
				 * ISC_ARRAY_DESC has no charset field, so we cannot reliably detect UTF8.
				 * The heuristic (dividing by 4 if length divisible by 4) caused truncation
				 * for non-UTF8 VARCHAR arrays like VARCHAR(1000) with charset NONE.
				 *
				 * For UTF8 databases, users may need to handle character/byte conversion
				 * at the application level if truncation occurs.
				 */
				a->el_type = SQL_TEXT;
				a->el_size = ar_desc->array_desc_length;
				ar_desc->array_desc_dtype = blr_text;
				break;
			case blr_quad:
			case blr_blob_id:
			case blr_cstring:
			case blr_cstring2:
				/**
				 * These types are mentioned as array types in the manual, but I
				 * wouldn't know how to create an array field with any of these
				 * types. I assume these types are not applicable to arrays, and
				 * were mentioned erroneously.
				 */
			default:
				_php_fbird_module_error("Unsupported array type %d in relation '%s' column '%s'",
					ar_desc->array_desc_dtype, var->relname, var->sqlname);
				efree(ar);
				return FAILURE;
		} /* switch array_desc_type */

		/* calculate elements count */
		for (dim = 0; dim < ar_desc->array_desc_dimensions; dim++) {
			ar_size *= 1 + ar_desc->array_desc_bounds[dim].array_bound_upper
				-ar_desc->array_desc_bounds[dim].array_bound_lower;
		}

        /* Safety check for overflow */
        float safe_size = (float)a->el_size * (float)ar_size;
        if (safe_size > (float)ZEND_ULONG_MAX) {
             _php_fbird_module_error("Array size exceeds system limits");
             efree(ar);
             return FAILURE;
        }
		a->ar_size = a->el_size * ar_size;
	} /* for column */
	*ib_arrayp = ar;
	return SUCCESS;
}
/* }}} */

int _php_fbird_bind_array(zval *val, char *buf, zend_ulong buf_size, /* {{{ */
	fbird_array *array, int dim)
{
	zval null_val, *pnull_val = &null_val;
	int u_bound = array->ar_desc.array_desc_bounds[dim].array_bound_upper,
		l_bound = array->ar_desc.array_desc_bounds[dim].array_bound_lower,
		dim_len = 1 + u_bound - l_bound;

	ZVAL_NULL(pnull_val);

	if (dim < array->ar_desc.array_desc_dimensions) {
		zend_ulong slice_size = buf_size / dim_len;
		int i;
		zval *subval = val;

		if (Z_TYPE_P(val) == IS_ARRAY) {
			zend_hash_internal_pointer_reset(Z_ARRVAL_P(val));
		}

		for (i = 0; i < dim_len; ++i) {

			if (Z_TYPE_P(val) == IS_ARRAY &&
				(subval = zend_hash_get_current_data(Z_ARRVAL_P(val))) == NULL)
			{
				subval = pnull_val;
			}

			if (_php_fbird_bind_array(subval, buf, slice_size, array, dim+1) == FAILURE)
			{
				return FAILURE;
			}
			buf += slice_size;

			if (Z_TYPE_P(val) == IS_ARRAY) {
				zend_hash_move_forward(Z_ARRVAL_P(val));
			}
		}

		if (Z_TYPE_P(val) == IS_ARRAY) {
			zend_hash_internal_pointer_reset(Z_ARRVAL_P(val));
		}

	} else {
		/* expect a single value */
		if (Z_TYPE_P(val) == IS_NULL) {
			memset(buf, 0, buf_size);
		} else if (array->ar_desc.array_desc_scale < 0) {

			/* no coercion for array types
			 * Use zval_get_double() to avoid modifying the original zval in-place,
			 * which would corrupt PHP arrays when processing multiple elements. */
			double l;
			double dval = zval_get_double(val);

			if (dval > 0) {
				l = dval * pow(10, -array->ar_desc.array_desc_scale) + .5;
			} else {
				l = dval * pow(10, -array->ar_desc.array_desc_scale) - .5;
			}

			switch (array->el_type) {
				case SQL_SHORT:
					if (l > SHRT_MAX || l < SHRT_MIN) {
						_php_fbird_module_error("Array parameter exceeds field width");
						return FAILURE;
					}
					*(short*) buf = (short) l;
					break;
				case SQL_LONG:
					if (l > ISC_LONG_MAX || l < ISC_LONG_MIN) {
						_php_fbird_module_error("Array parameter exceeds field width");
						return FAILURE;
					}
					*(ISC_LONG*) buf = (ISC_LONG) l;
					break;
				case SQL_INT64:
					{
						long double ld;

						/* Use zval_get_string() to get a copy without modifying the original zval */
						zend_string *str = zval_get_string(val);

						if (!sscanf(ZSTR_VAL(str), "%Lf", &ld)) {
							_php_fbird_module_error("Cannot convert '%s' to long double",
								 ZSTR_VAL(str));
							zend_string_release(str);
							return FAILURE;
						}
						zend_string_release(str);

						if (ld > 0) {
							*(ISC_INT64 *) buf = (ISC_INT64) (ld * pow(10,
								-array->ar_desc.array_desc_scale) + .5);
						} else {
							*(ISC_INT64 *) buf = (ISC_INT64) (ld * pow(10,
								-array->ar_desc.array_desc_scale) - .5);
						}
					}
					break;
			}
		} else {
			struct tm t = { 0 };

			switch (array->el_type) {
#ifndef HAVE_STRPTIME
				unsigned short n;
#endif

				case SQL_SHORT:
					{
						/* Use zval_get_long() to avoid modifying the original zval in-place */
						zend_long lval = zval_get_long(val);
						if (lval > SHRT_MAX || lval < SHRT_MIN) {
							_php_fbird_module_error("Array parameter exceeds field width");
							return FAILURE;
						}
						*(short *) buf = (short) lval;
					}
					break;
				case SQL_LONG:
					{
						/* Use zval_get_long() to avoid modifying the original zval in-place */
						zend_long lval = zval_get_long(val);
#if (SIZEOF_ZEND_LONG > 4)
						if (lval > ISC_LONG_MAX || lval < ISC_LONG_MIN) {
							_php_fbird_module_error("Array parameter exceeds field width");
							return FAILURE;
						}
#endif
						*(ISC_LONG *) buf = (ISC_LONG) lval;
					}
					break;
				case SQL_INT64:
					{
#if (SIZEOF_ZEND_LONG >= 8)
						/* Use zval_get_long() to avoid modifying the original zval in-place */
						zend_long lval = zval_get_long(val);
						*(zend_long *) buf = lval;
#else
						/* Use zval_get_string() to get a copy without modifying the original zval */
						ISC_INT64 l;
						zend_string *str = zval_get_string(val);
						if (!sscanf(ZSTR_VAL(str), "%" LL_MASK "d", &l)) {
							_php_fbird_module_error("Cannot convert '%s' to long integer",
								 ZSTR_VAL(str));
							zend_string_release(str);
							return FAILURE;
						}
						zend_string_release(str);
						*(ISC_INT64 *) buf = l;
#endif
					}
					break;
				case SQL_FLOAT:
					{
						/* Use zval_get_double() to avoid modifying the original zval in-place */
						double dval = zval_get_double(val);
						*(float*) buf = (float) dval;
					}
					break;
#ifdef SQL_BOOLEAN
				case SQL_BOOLEAN:
					/* zend_is_true() does not modify the zval */
					*(FB_BOOLEAN*) buf = zend_is_true(val) ? FB_TRUE : FB_FALSE;
					break;
#endif
				case SQL_DOUBLE:
					{
						/* Use zval_get_double() to avoid modifying the original zval in-place */
						double dval = zval_get_double(val);
						*(double*) buf = dval;
					}
					break;
				case SQL_TIMESTAMP:
				/* TODO: case SQL_TIMESTAMP_TZ: */
					{
						/* Use zval_get_string() to get a copy without modifying the original zval */
						zend_string *str = zval_get_string(val);
#ifdef HAVE_STRPTIME
						strptime(ZSTR_VAL(str), INI_STR("fbird.timestampformat"), &t);
#else
						n = sscanf(ZSTR_VAL(str), "%d%*[/]%d%*[/]%d %d%*[:]%d%*[:]%d",
							&t.tm_mon, &t.tm_mday, &t.tm_year, &t.tm_hour, &t.tm_min, &t.tm_sec);

						if (n != 3 && n != 6) {
							_php_fbird_module_error("Invalid date/time format (expected 3 or 6 fields, got %d."
								" Use format 'm/d/Y H:i:s'. You gave '%s')", n, ZSTR_VAL(str));
							zend_string_release(str);
							return FAILURE;
						}
						t.tm_year -= 1900;
						t.tm_mon--;
#endif
						zend_string_release(str);
						isc_encode_timestamp(&t, (ISC_TIMESTAMP * ) buf);
					}
					break;
				case SQL_TYPE_DATE:
					{
						/* Use zval_get_string() to get a copy without modifying the original zval */
						zend_string *str = zval_get_string(val);
#ifdef HAVE_STRPTIME
						strptime(ZSTR_VAL(str), INI_STR("fbird.dateformat"), &t);
#else
						n = sscanf(ZSTR_VAL(str), "%d%*[/]%d%*[/]%d", &t.tm_mon, &t.tm_mday, &t.tm_year);

						if (n != 3) {
							_php_fbird_module_error("Invalid date format (expected 3 fields, got %d. "
								"Use format 'm/d/Y' You gave '%s')", n, ZSTR_VAL(str));
							zend_string_release(str);
							return FAILURE;
						}
						t.tm_year -= 1900;
						t.tm_mon--;
#endif
						zend_string_release(str);
						isc_encode_sql_date(&t, (ISC_DATE *) buf);
					}
					break;
				case SQL_TYPE_TIME:
				/* TODO: case SQL_TIME_TZ: */
					{
						/* Use zval_get_string() to get a copy without modifying the original zval */
						zend_string *str = zval_get_string(val);
#ifdef HAVE_STRPTIME
						strptime(ZSTR_VAL(str), INI_STR("fbird.timeformat"), &t);
#else
						n = sscanf(ZSTR_VAL(str), "%d%*[:]%d%*[:]%d", &t.tm_hour, &t.tm_min, &t.tm_sec);

						if (n != 3) {
							_php_fbird_module_error("Invalid time format (expected 3 fields, got %d. "
								"Use format 'H:i:s'. You gave '%s')", n, ZSTR_VAL(str));
							zend_string_release(str);
							return FAILURE;
						}
#endif
						zend_string_release(str);
						isc_encode_sql_time(&t, (ISC_TIME *) buf);
					}
					break;
				case SQL_VARYING:
					{
						/* VARCHAR arrays use IBVARY format: 2-byte length prefix + character data.
						 * buf_size = el_size = array_desc_length + sizeof(short)
						 *
						 * Use zval_get_string() to get copy without modifying original zval.
						 * This fixes PHP 8.x issue where convert_to_string() modifies in-place,
						 * causing all array elements to contain the last value */
						zend_string *str = zval_get_string(val);
						size_t str_len = ZSTR_LEN(str);
						size_t max_len = buf_size - sizeof(short);
						if (str_len > max_len) {
							str_len = max_len;
						}
						/* Write length prefix */
						*(short *)buf = (short)str_len;
						/* Copy string data after length prefix */
						if (str_len > 0) {
							memcpy(buf + sizeof(short), ZSTR_VAL(str), str_len);
						}
						zend_string_release(str);
					}
					break;
				default:
					{
						/* Use zval_get_string() to get copy without modifying original zval
						 * This fixes PHP 8.x issue where convert_to_string() modifies in-place,
						 * causing all CHAR array elements to contain the last value */
						zend_string *str = zval_get_string(val);
						size_t copy_len = ZSTR_LEN(str);
						if (copy_len >= buf_size) {
							copy_len = buf_size - 1;
						}
						memcpy(buf, ZSTR_VAL(str), copy_len);
						/* Pad remaining buffer with spaces for CHAR fields (SQL_TEXT) */
						if (copy_len < buf_size) {
							memset(buf + copy_len, ' ', buf_size - copy_len);
						}
						zend_string_release(str);
					}
			}
		}
	}
	return SUCCESS;
}
/* }}} */

#endif /* HAVE_FIREBIRD */
