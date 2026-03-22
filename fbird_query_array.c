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

int _php_fbird_alloc_array(fbird_array **ib_arrayp, XSQLDA *sqlda,
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

		if (fba_array_lookup_bounds(IB_STATUS, &link.db, &trans.tr, rname,
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
				/* VARCHAR array workaround for Firebird dtype_cstring bug:
				 *
				 * Firebird's sdl_desc() sets dtype_cstring for blr_varying, but internal
				 * array storage uses IBVARY format. This mismatch corrupts data during
				 * getSlice (length preserved, character data zeroed).
				 *
				 * WORKAROUND: Use blr_cstring in SDL (see fb_array.hpp) and format
				 * user buffer as null-terminated strings instead of IBVARY.
				 * Element size = declared_length + 1 (for null terminator).
				 */
				a->el_type = SQL_VARYING;
				a->el_size = ar_desc->array_desc_length + 1;  /* +1 for null terminator */
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
							*(ISC_TIMESTAMP *)buf = fbu_encode_timestamp(IBG(master_instance),
								dt.year, dt.month, dt.day,
								dt.hours, dt.minutes, dt.seconds,
								dt.fractions);
						} else {
							/* Parsing failed - encode zero timestamp via OO API */
							*(ISC_TIMESTAMP *)buf = fbu_encode_timestamp(IBG(master_instance), 0, 0, 0, 0, 0, 0, 0);
						}
						zend_string_release(str);
					}
					break;
#if FB_API_VER >= 40
				case SQL_TIMESTAMP_TZ:
					{
						/* Timezone types in arrays require Firebird 4.0+ master interface */
						if (!IBG(master_instance)) {
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

						if (fbu_encode_timestamp_tz(IBG(master_instance), (ISC_TIMESTAMP_TZ *)buf,
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
							*(ISC_DATE *)buf = fbu_encode_date(IBG(master_instance),
								dt.year, dt.month, dt.day);
						} else {
							/* Parsing failed - encode zero date via OO API */
							*(ISC_DATE *)buf = fbu_encode_date(IBG(master_instance), 0, 0, 0);
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
							*(ISC_TIME *)buf = fbu_encode_time(IBG(master_instance),
								dt.hours, dt.minutes, dt.seconds,
								dt.fractions);
						} else {
							/* Parsing failed - encode zero time via OO API */
							*(ISC_TIME *)buf = fbu_encode_time(IBG(master_instance), 0, 0, 0, 0);
						}
						zend_string_release(str);
					}
					break;
#if FB_API_VER >= 40
				case SQL_TIME_TZ:
					{
						/* Timezone types in arrays require Firebird 4.0+ master interface */
						if (!IBG(master_instance)) {
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

						if (fbu_encode_time_tz(IBG(master_instance), (ISC_TIME_TZ *)buf,
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
