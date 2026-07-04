/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

/**
 * Array handling functions.
 *
 * This file contains functions for allocating array descriptors and binding
 * PHP arrays to Firebird array format.
 */

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
#include "fbird_datetime.h"

#define ISC_LONG_MIN    INT_MIN
#define ISC_LONG_MAX    INT_MAX

int _php_fbird_bind_array(zval *val, char *buf, zend_ulong buf_size,
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

		/*
		 * Use external HashPosition for iteration instead of the HashTable's
		 * internal pointer. The internal-pointer functions (_reset, _move_forward)
		 * write to ht->nInternalPointer, which SIGSEGVs when opcache.protect_memory=1
		 * marks the shared memory page as read-only (e.g., JIT-cached arrays).
		 *
		 * The _ex variants use a stack-local HashPosition and never write to the
		 * HashTable structure. This is the PHP 8.1+ recommended pattern.
		 */
		HashPosition pos;
		bool is_array = (Z_TYPE_P(val) == IS_ARRAY);
		if (is_array) {
			zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(val), &pos);
		}

		for (i = 0; i < dim_len; ++i) {

			if (is_array &&
				(subval = zend_hash_get_current_data_ex(Z_ARRVAL_P(val), &pos)) == NULL)
			{
				subval = pnull_val;
			}

			if (_php_fbird_bind_array(subval, buf, slice_size, array, dim+1) == FAILURE)
			{
				return FAILURE;
			}
			buf += slice_size;

			if (is_array) {
				zend_hash_move_forward_ex(Z_ARRVAL_P(val), &pos);
			}
		}

		/* No reset needed: we used an external position, not ht->nInternalPointer. */

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
			switch (array->el_type) {
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
					{
						/* Cross-platform timestamp parsing using fbird_datetime utilities */
						zend_string *str = zval_get_string(val);
						fbird_datetime_components dt;

						if (fbird_parse_timestamp(ZSTR_VAL(str), &dt)) {
							/* Use OO API encoding */
							*(ISC_TIMESTAMP *)buf = fbu_encode_timestamp(FBG(master_instance),
								dt.year, dt.month, dt.day,
								dt.hours, dt.minutes, dt.seconds,
								dt.fractions);
						} else {
							/* Parsing failed - encode zero timestamp via OO API */
							*(ISC_TIMESTAMP *)buf = fbu_encode_timestamp(FBG(master_instance), 0, 0, 0, 0, 0, 0, 0);
						}
						zend_string_release(str);
					}
					break;
#if FB_API_VER >= 40
				case SQL_TIMESTAMP_TZ:
					{
						/* Timezone types in arrays require Firebird 4.0+ master interface */
						if (!FBG(master_instance)) {
							_php_fbird_module_error("TIMESTAMP WITH TIME ZONE arrays require Firebird 4.0+ client library");
							return FAILURE;
						}

						/* Cross-platform timestamp+timezone parsing */
						zend_string *str = zval_get_string(val);
						fbird_datetime_components dt;
						fbird_datetime_init(&dt);
						strncpy(dt.timezone, "GMT", sizeof(dt.timezone) - 1);

						if (fbird_parse_timestamp(ZSTR_VAL(str), &dt)) {
							/* If no timezone was parsed, use default GMT */
							if (!dt.has_timezone) {
								strncpy(dt.timezone, "GMT", sizeof(dt.timezone) - 1);
							}
						}

						zend_string_release(str);

						if (fbu_encode_timestamp_tz(FBG(master_instance), (ISC_TIMESTAMP_TZ *)buf,
								dt.year, dt.month, dt.day,
								dt.hours, dt.minutes, dt.seconds, dt.fractions, dt.timezone) != 0) {
							_php_fbird_module_error("Failed to encode TIMESTAMP WITH TIME ZONE array element");
							return FAILURE;
						}
					}
					break;
#endif
				case SQL_TYPE_DATE:
					{
						/* Cross-platform date parsing using fbird_datetime utilities */
						zend_string *str = zval_get_string(val);
						fbird_datetime_components dt;

						if (fbird_parse_date(ZSTR_VAL(str), &dt)) {
							/* Use OO API encoding */
							*(ISC_DATE *)buf = fbu_encode_date(FBG(master_instance),
								dt.year, dt.month, dt.day);
						} else {
							/* Parsing failed - encode zero date via OO API */
							*(ISC_DATE *)buf = fbu_encode_date(FBG(master_instance), 0, 0, 0);
						}
						zend_string_release(str);
					}
					break;
				case SQL_TYPE_TIME:
					{
						/* Cross-platform time parsing using fbird_datetime utilities */
						zend_string *str = zval_get_string(val);
						fbird_datetime_components dt;

						if (fbird_parse_time(ZSTR_VAL(str), &dt)) {
							/* Use OO API encoding */
							*(ISC_TIME *)buf = fbu_encode_time(FBG(master_instance),
								dt.hours, dt.minutes, dt.seconds,
								dt.fractions);
						} else {
							/* Parsing failed - encode zero time via OO API */
							*(ISC_TIME *)buf = fbu_encode_time(FBG(master_instance), 0, 0, 0, 0);
						}
						zend_string_release(str);
					}
					break;
#if FB_API_VER >= 40
				case SQL_TIME_TZ:
					{
						/* Timezone types in arrays require Firebird 4.0+ master interface */
						if (!FBG(master_instance)) {
							_php_fbird_module_error("TIME WITH TIME ZONE arrays require Firebird 4.0+ client library");
							return FAILURE;
						}

						/* Cross-platform time+timezone parsing */
						zend_string *str = zval_get_string(val);
						fbird_datetime_components dt;
						fbird_datetime_init(&dt);
						strncpy(dt.timezone, "GMT", sizeof(dt.timezone) - 1);

						if (fbird_parse_time(ZSTR_VAL(str), &dt)) {
							/* If no timezone was parsed, use default GMT */
							if (!dt.has_timezone) {
								strncpy(dt.timezone, "GMT", sizeof(dt.timezone) - 1);
							}
						}

						zend_string_release(str);

						if (fbu_encode_time_tz(FBG(master_instance), (ISC_TIME_TZ *)buf,
								dt.hours, dt.minutes, dt.seconds, dt.fractions, dt.timezone) != 0) {
							_php_fbird_module_error("Failed to encode TIME WITH TIME ZONE array element");
							return FAILURE;
						}
					}
					break;
#endif
				case SQL_VARYING:
					{
						/* VARCHAR array workaround: Use null-terminated strings.
						 * buf_size = el_size = array_desc_length + 1 (for null terminator)
						 *
						 * See fb_array.hpp for explanation of dtype_cstring bug workaround.
						 *
						 * Use zval_get_string() to get copy without modifying original zval.
						 * This fixes PHP 8.x issue where convert_to_string() modifies in-place,
						 * causing all array elements to contain the last value */
						zend_string *str = zval_get_string(val);
						size_t str_len = ZSTR_LEN(str);
						size_t max_len = buf_size - 1;  /* Reserve space for null terminator */
						if (str_len > max_len) {
							str_len = max_len;
						}
						/* Copy string data */
						if (str_len > 0) {
							memcpy(buf, ZSTR_VAL(str), str_len);
						}
						/* Null-terminate */
						buf[str_len] = '\0';
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

#endif /* HAVE_FIREBIRD */
