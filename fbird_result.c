/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

/* Enable GNU features (includes X/Open for strptime, BSD for strlcpy/tm_zone) */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <ctype.h>
#include <string.h>

#include "php.h"
#include "php_ini.h"

#if HAVE_FIREBIRD

#include "ext/standard/php_standard.h"
#include "ext/date/php_date.h"
#include "php_firebird.h"

/* PHP 8.4 compatibility: zend_call_method_with_1_params was removed.
 * Use direct object initialization instead for DateTimeImmutable. */
#include "php_fbird_includes.h"
#include "php_fbird_query_internal.h"
#include "Zend/zend_smart_str.h"
#include "firebird_utils.h"

#define ISC_LONG_MIN    INT_MIN
#define ISC_LONG_MAX    INT_MAX

#define FETCH_ROW       1
#define FETCH_ARRAY     2

typedef struct {
	unsigned short vary_length;
	char vary_string[1];
} IBVARY;

/* Windows-compatible setenv/unsetenv wrappers */
#ifdef PHP_WIN32
static inline int fbird_setenv(const char *name, const char *value) {
    return _putenv_s(name, value);
}
static inline int fbird_unsetenv(const char *name) {
    return _putenv_s(name, "");
}
#else
static inline int fbird_setenv(const char *name, const char *value) {
    return setenv(name, value, 1);
}
static inline int fbird_unsetenv(const char *name) {
    return unsetenv(name);
}
#endif

/* Portable UTC epoch conversion helper */
time_t fbird_timegm_portable(struct tm *tm)
{
#ifdef HAVE_TIMEGM
    return timegm(tm);
#else
    /* Save current TZ */
    char *old_tz = getenv("TZ");
    char *saved = NULL;
    if (old_tz) {
        /* use Zend allocator for consistency with the rest of the extension */
        saved = estrdup(old_tz);
    }
    /* Set UTC and apply */
    fbird_setenv("TZ", "UTC");
    tzset();
    time_t ts = mktime(tm);
    /* Restore TZ */
    if (saved) {
        fbird_setenv("TZ", saved);
        efree(saved);
    } else {
        fbird_unsetenv("TZ");
    }
    tzset();
    return ts;
#endif
}

/* Convert struct tm to epoch using a specific timezone name */
time_t fbird_mktime_with_tz(struct tm *tm, const char *tz)
{
    const char *use_tz = (tz && tz[0]) ? tz : "UTC";
    char *old_tz_env = getenv("TZ");
    char *saved = NULL;
    if (old_tz_env) {
        saved = estrdup(old_tz_env);
    }
    fbird_setenv("TZ", use_tz);
    tzset();
    time_t ts = mktime(tm);
    if (saved) {
        fbird_setenv("TZ", saved);
        efree(saved);
    } else {
        fbird_unsetenv("TZ");
    }
    tzset();
    return ts;
}

static int _php_fbird_var_zval(zval *val, void *data, int type, int len,
    int scale, int subtype, size_t flag)
{
	static ISC_INT64 const scales[] = { 1, 10, 100, 1000,
		10000,
		100000,
		1000000,
		10000000,
		100000000,
		1000000000,
		LL_LIT(10000000000),
		LL_LIT(100000000000),
		LL_LIT(1000000000000),
		LL_LIT(10000000000000),
		LL_LIT(100000000000000),
		LL_LIT(1000000000000000),
		LL_LIT(10000000000000000),
		LL_LIT(100000000000000000),
		LL_LIT(1000000000000000000)
	};

	/* Move variable declarations to function scope */
	unsigned short l;
	zend_long n;
	/* Increased buffer to handle deep timezones + timestamp string */
	char string_data[512] = {0}; /* Initialize to prevent uninitialized access */
	struct tm t;
    char *format;

	switch (type & ~1) {

		case SQL_VARYING:
			{
				/* VARCHAR: length is actual data length from vary_length,
				 * no padding - just use the data as-is */
				len = ((IBVARY *) data)->vary_length;
				data = ((IBVARY *) data)->vary_string;
				ZVAL_STRINGL(val, (char*)data, len);
			}
			break;
		case SQL_TEXT:
			{
				/* CHAR(N) field handling for multi-byte character sets:
				 *
				 * For UTF8 CHAR(N): buffer contains actual UTF8 data + NUL padding
				 * For single-byte CHAR(N): buffer is space-padded to declared length
				 *
				 * UTF8 approach: Read all valid UTF8 characters until NUL or buffer end.
				 * This preserves intentional trailing spaces from the original data.
				 *
				 * For single-byte charsets, we rtrim spaces which matches
				 * historical SQL CHAR behavior (trailing spaces are insignificant).
				 *
				 * Charset IDs: 4 = UTF8, 59 = UTF8MB4 (Firebird 4+)
				 */
				unsigned char charset_id = (unsigned char)(subtype & 0xFF);

				if (charset_id == 4 || charset_id == 59) {
					/* UTF8/UTF8MB4: read all valid UTF8 chars until NUL or buffer end.
					 * Trailing spaces are preserved as they may be intentional data.
					 * NUL bytes indicate padding, not data. */
					size_t actual_len = 0;
					const unsigned char *p = (const unsigned char *)data;

					while (actual_len < (size_t)len) {
						unsigned char c = p[actual_len];
						size_t char_bytes;

						/* Stop at NUL - marks end of actual data */
						if (c == 0) {
							break;
						}

						if ((c & 0x80) == 0) {
							char_bytes = 1;  /* ASCII (including space 0x20) */
						} else if ((c & 0xE0) == 0xC0) {
							char_bytes = 2;  /* 2-byte UTF8 */
						} else if ((c & 0xF0) == 0xE0) {
							char_bytes = 3;  /* 3-byte UTF8 (e.g., €) */
						} else if ((c & 0xF8) == 0xF0) {
							char_bytes = 4;  /* 4-byte UTF8 */
						} else {
							char_bytes = 1;  /* Invalid UTF8, treat as single byte */
						}

						/* Safety: don't read past buffer */
						if (actual_len + char_bytes > (size_t)len) {
							break;
						}

						actual_len += char_bytes;
					}

					ZVAL_STRINGL(val, (char*)data, actual_len);
				} else {
					/* Single-byte or other charset: use strnlen + rtrim */
					size_t actual_len = strnlen((char*)data, len);
					if (actual_len == (size_t)len) {
						while (actual_len > 0 && ((unsigned char*)data)[actual_len - 1] == ' ') {
							actual_len--;
						}
					}
					ZVAL_STRINGL(val, (char*)data, actual_len);
				}
			}
			break;
#ifdef SQL_BOOLEAN
		case SQL_BOOLEAN:
			ZVAL_BOOL(val, *(FB_BOOLEAN *) data);
			break;
#endif
		case SQL_SHORT:
			n = *(short *) data;
			goto _sql_long;
		case SQL_INT64:
#if (SIZEOF_ZEND_LONG >= 8)
			n = *(zend_long *) data;
			goto _sql_long;
#else
			if (scale == 0) {
				l = slprintf(string_data, sizeof(string_data), "%" LL_MASK "d", *(ISC_INT64 *) data);
				ZVAL_STRINGL(val,string_data,l);
			} else {
				ISC_INT64 n = *(ISC_INT64 *) data, f = scales[-scale];

				if (n >= 0) {
					l = slprintf(string_data, sizeof(string_data), "%" LL_MASK "d.%0*" LL_MASK "d", n / f, -scale, n % f);
				} else if (n <= -f) {
					l = slprintf(string_data, sizeof(string_data), "%" LL_MASK "d.%0*" LL_MASK "d", n / f, -scale, -n % f);
				 } else {
					l = slprintf(string_data, sizeof(string_data), "-0.%0*" LL_MASK "d", -scale, -n % f);
				}
				ZVAL_STRINGL(val,string_data,l);
			}
			break;
#endif
		case SQL_LONG:
			n = *(ISC_LONG *) data;
		_sql_long:
			if (scale == 0) {
				ZVAL_LONG(val,n);
			} else {
				zend_long f = (zend_long) scales[-scale];

				if (n >= 0) {
					l = slprintf(string_data, sizeof(string_data), ZEND_LONG_FMT ".%0*" ZEND_LONG_FMT_SPEC, n / f, -scale,  n % f);
				} else if (n <= -f) {
					l = slprintf(string_data, sizeof(string_data), ZEND_LONG_FMT ".%0*" ZEND_LONG_FMT_SPEC, n / f, -scale,  -n % f);
				} else {
					l = slprintf(string_data, sizeof(string_data), "-0.%0*" ZEND_LONG_FMT_SPEC, -scale, -n % f);
				}
				ZVAL_STRINGL(val, string_data, l);
			}
			break;
		case SQL_FLOAT:
			ZVAL_DOUBLE(val, *(float *) data);
			break;
		case SQL_DOUBLE:
			ZVAL_DOUBLE(val, *(double *) data);
			break;
#if FB_API_VER >= 40
		// These are converted to VARCHAR via isc_dpb_set_bind tag at connect
		// case SQL_DEC16:
		// case SQL_DEC34:
		// case SQL_INT128:
		case SQL_TIME_TZ:
		case SQL_TIMESTAMP_TZ:
			// Should be converted to VARCHAR via isc_dpb_set_bind tag at
			// connect if fbclient does not have fb_get_master_instance().
			// Assert this just in case.
			if(!IBG(master_instance)) {
				_php_fbird_module_error("Timezone fields require Firebird 4.0+ master instance");
				return FAILURE;
			}

			/* Increased buffer for Firebird deep/concatenated timezones */
			char timeZoneBuffer[64] = {0};
			unsigned year, month, day, hours, minutes, seconds, fractions;

			if((type & ~1) == SQL_TIME_TZ){
				format = INI_STR("fbird.timeformat");
				fbu_decode_time_tz(IBG(master_instance), (ISC_TIME_TZ *) data, &hours, &minutes, &seconds, &fractions, sizeof(timeZoneBuffer), timeZoneBuffer);
				/* OO API: Populate struct tm directly from decoded components */
				memset(&t, 0, sizeof(t));
				t.tm_hour = (int)hours;
				t.tm_min = (int)minutes;
				t.tm_sec = (int)seconds;
				/* fractions (10000ths of second) not representable in struct tm */
			} else {
				format = INI_STR("fbird.timestampformat");
				fbu_decode_timestamp_tz(IBG(master_instance), (ISC_TIMESTAMP_TZ *) data, &year, &month, &day, &hours, &minutes, &seconds, &fractions, sizeof(timeZoneBuffer), timeZoneBuffer);
				/* OO API: Populate struct tm directly from decoded components */
				memset(&t, 0, sizeof(t));
				t.tm_year = (int)year - 1900;  /* struct tm years since 1900 */
				t.tm_mon = (int)month - 1;     /* struct tm months 0-11 */
				t.tm_mday = (int)day;
				t.tm_hour = (int)hours;
				t.tm_min = (int)minutes;
				t.tm_sec = (int)seconds;
				/* fractions (10000ths of second) not representable in struct tm */
			}

			if (((type & ~1) != SQL_TIME_TZ) && (flag & PHP_FBIRD_UNIXTIME)) {
				ZVAL_LONG(val, fbird_mktime_with_tz(&t, timeZoneBuffer));
			} else {
				char timeBuf[80] = {0};
				l = strftime(timeBuf, sizeof(timeBuf), format, &t);
				if (l == 0) {
					return FAILURE;
				}

				/* Safe checking for truncation */
				int tz_len_int = snprintf(string_data, sizeof(string_data), "%s %s", timeBuf, timeZoneBuffer);
				if (tz_len_int < 0 || (size_t)tz_len_int >= sizeof(string_data)) {
					_php_fbird_module_error("Timezone string truncated");
					return FAILURE;
				}
				ZVAL_STRINGL(val, string_data, (size_t)tz_len_int);
			}
			break;
#endif
		case SQL_TIMESTAMP:
			format = INI_STR("fbird.timestampformat");
			{
				/* OO API: Use fbu_decode_timestamp() instead of legacy isc_decode_timestamp() */
				unsigned ts_year, ts_month, ts_day, ts_hours, ts_minutes, ts_seconds, ts_fractions;
				fbu_decode_timestamp(IBG(master_instance), (ISC_TIMESTAMP *) data,
					&ts_year, &ts_month, &ts_day, &ts_hours, &ts_minutes, &ts_seconds, &ts_fractions);
				memset(&t, 0, sizeof(t));
				t.tm_year = (int)ts_year - 1900;  /* struct tm years since 1900 */
				t.tm_mon = (int)ts_month - 1;     /* struct tm months 0-11 */
				t.tm_mday = (int)ts_day;
				t.tm_hour = (int)ts_hours;
				t.tm_min = (int)ts_minutes;
				t.tm_sec = (int)ts_seconds;
				/* fractions (10000ths of second) not representable in struct tm */
			}
			goto format_date_time;
		case SQL_TYPE_DATE:
			format = INI_STR("fbird.dateformat");
			{
				/* OO API: Use fbu_decode_date() instead of legacy isc_decode_sql_date() */
				unsigned d_year, d_month, d_day;
				fbu_decode_date(IBG(master_instance), *(ISC_DATE *) data, &d_year, &d_month, &d_day);
				memset(&t, 0, sizeof(t));
				t.tm_year = (int)d_year - 1900;
				t.tm_mon = (int)d_month - 1;
				t.tm_mday = (int)d_day;
			}
			goto format_date_time;
		case SQL_TYPE_TIME:
			format = INI_STR("fbird.timeformat");
			{
				/* OO API: Use fbu_decode_time() instead of legacy isc_decode_sql_time() */
				unsigned t_hours, t_minutes, t_seconds, t_fractions;
				fbu_decode_time(IBG(master_instance), *(ISC_TIME *) data, &t_hours, &t_minutes, &t_seconds, &t_fractions);
				memset(&t, 0, sizeof(t));
				t.tm_hour = (int)t_hours;
				t.tm_min = (int)t_minutes;
				t.tm_sec = (int)t_seconds;
				/* fractions (10000ths of second) not representable in struct tm */
			}

format_date_time:
			/*
			 * Setting tm_isdst = -1 tells mktime() to determine DST status automatically.
			 * This is the correct fix for the InterBase 6 bug where isc_decode_date()
			 * always set tm_isdst to 0, causing incorrect time formatting during DST.
			 * Modern Firebird still doesn't set this field, so this workaround remains.
			 */
			t.tm_isdst = -1;
#if HAVE_STRUCT_TM_TM_ZONE
			t.tm_zone = tzname[0];
#endif
			/* Skip unix-time conversion entirely for TIME/TIME_TZ types as they have no date component */
#ifdef SQL_TIME_TZ
   if (((type & ~1) == SQL_TYPE_TIME) || ((type & ~1) == SQL_TIME_TZ)) {
#else
   if ((type & ~1) == SQL_TYPE_TIME) {
#endif
                /* TIME/TIME_TZ: Skip unix conversion, always return formatted string */
                if (flag & PHP_FBIRD_FETCH_DATE_OBJ) {
                    /* Return DateTimeImmutable for TIME type - use php_date_instantiate() */
                    char iso_str[32];
                    snprintf(iso_str, sizeof(iso_str), "1970-01-01 %02d:%02d:%02d",
                        t.tm_hour, t.tm_min, t.tm_sec);
                    php_date_instantiate(php_date_get_immutable_ce(), val);
                    php_date_initialize(Z_PHPDATE_P(val), iso_str, strlen(iso_str), NULL, NULL, PHP_DATE_INIT_FORMAT);
                } else {
                    l = strftime(string_data, sizeof(string_data), format, &t);
                    ZVAL_STRINGL(val, string_data, l);
                }
            } else if (flag & PHP_FBIRD_FETCH_DATE_OBJ) {
                /* TIMESTAMP/DATE: Return DateTimeImmutable object - use php_date_instantiate() */
                char iso_str[32];
                snprintf(iso_str, sizeof(iso_str), "%04d-%02d-%02d %02d:%02d:%02d",
                    t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                    t.tm_hour, t.tm_min, t.tm_sec);
                php_date_instantiate(php_date_get_immutable_ce(), val);
                php_date_initialize(Z_PHPDATE_P(val), iso_str, strlen(iso_str), NULL, NULL, PHP_DATE_INIT_FORMAT);
            } else if (flag & PHP_FBIRD_UNIXTIME) {
                /* TIMESTAMP/DATE: Deterministic behavior — convert to epoch
                 * using PHP's configured timezone (date.timezone). This avoids
                 * dependence on the host OS timezone. */
                const char *php_tz = INI_STR("date.timezone");
                time_t timestamp = fbird_mktime_with_tz(&t, php_tz);
                ZVAL_LONG(val, timestamp);
            } else {
                l = strftime(string_data, sizeof(string_data), format, &t);
                ZVAL_STRINGL(val, string_data, l);
            }
            break;
		default:
			break;
	} /* switch (type) */
	return SUCCESS;
}

static int _php_fbird_arr_zval(zval *ar_zval, char *data, zend_ulong data_size,
	fbird_array *ib_array, int dim, size_t flag)
{
	/**
	 * Create multidimension array - recursion function
	 */
	int
		u_bound = ib_array->ar_desc.array_desc_bounds[dim].array_bound_upper,
		l_bound = ib_array->ar_desc.array_desc_bounds[dim].array_bound_lower,
		dim_len = 1 + u_bound - l_bound;
	int i;

	if (dim < ib_array->ar_desc.array_desc_dimensions) { /* array again */
		zend_ulong slice_size = data_size / dim_len;

		array_init(ar_zval);

		for (i = 0; i < dim_len; ++i) {
			zval slice_zval;

			/* recursion here */
			if (FAILURE == _php_fbird_arr_zval(&slice_zval, data, slice_size, ib_array, dim + 1,
					flag)) {
				return FAILURE;
			}
			data += slice_size;

			/* add_index_zval transfers ownership of the zval to the array.
			 * Do NOT call zval_ptr_dtor after this - it would free the value
			 * that was just added, causing all array elements to contain
			 * the last (use-after-free) value. This was the root cause of
			 * the bug where all CHAR array elements returned the last value. */
			add_index_zval(ar_zval, l_bound + i, &slice_zval);
		}
	} else { /* data at last */
		/* For arrays, subtype info is not readily available in ar_desc.
		 * Pass 0 for subtype which will use the single-byte rtrim logic.
		 * This is acceptable as array CHAR fields are less common and
		 * historical behavior is preserved. */

		if (ib_array->el_type == SQL_VARYING) {
			/* VARCHAR array workaround: Data is null-terminated string (cstring format).
			 * See fb_array.hpp for explanation of dtype_cstring bug workaround.
			 * Element size = declared_length + 1 (for null terminator).
			 * Read the string using strlen() to find the actual length. */
			size_t str_len = strnlen(data, ib_array->el_size - 1);
			ZVAL_STRINGL(ar_zval, data, str_len);
		} else {
			/* For other types, use the standard conversion function */
			zend_long element_length = ib_array->ar_desc.array_desc_length;

			if (FAILURE == _php_fbird_var_zval(ar_zval, data, ib_array->el_type,
					element_length, ib_array->ar_desc.array_desc_scale, 0, flag)) {
				return FAILURE;
			}
		}

		/* Native VARCHAR array support:
		 * _php_fbird_var_zval(SQL_VARYING) already uses IBVARY.vary_length,
		 * so no extra truncation is needed here.
		 */
	}
	return SUCCESS;
}

static void _php_fbird_fetch_hash(INTERNAL_FUNCTION_PARAMETERS, int fetch_type)
{
	zval *res_arg, *result;
	zend_long flag = 0;
	zend_long i, array_cnt = 0;
	fbird_query *ib_query;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r|l", &res_arg, &flag)) {
		RETURN_FALSE;
	}

	/* Validate first argument is a query resource with proper error messages */
	FBIRD_VALIDATE_QUERY_EX(res_arg, 1, ib_query);
	if (!ib_query) {
		RETURN_FALSE;
	}

	/* Pure OO API: Check message buffer instead of XSQLDA */
	if (ib_query->out_metadata == NULL || ib_query->out_msg_buffer == NULL ||
		!ib_query->has_more_rows || !ib_query->is_open) {
		RETURN_FALSE;
	}

	assert(ib_query->out_fields_count > 0);

	/*
	 * Pure OO API Fetch Path
	 *
	 * Uses fbs_fetch() with message buffer for data retrieval.
	 * Data is extracted from message buffer using OO metadata helpers.
	 */
	if (ib_query->statement_type != isc_info_sql_stmt_exec_procedure) {
		/* Check for buffered RETURNING - data already in buffer from execute */
		int is_buffered_returning = (
			ib_query->out_msg_buffer &&
			(ib_query->statement_type == isc_info_sql_stmt_insert ||
			 ib_query->statement_type == isc_info_sql_stmt_update ||
			 ib_query->statement_type == isc_info_sql_stmt_delete) &&
			ib_query->was_result_once
		);

		if (!is_buffered_returning) {
			/* OO API fetch via fbs_fetch() with message buffer */
			if (!ib_query->fbs_statement || !fbs_is_cursor_open(ib_query->fbs_statement)) {
				_php_fbird_module_error("OO API cursor not open"); /* LCOV_EXCL_LINE */
				RETURN_FALSE;
			}

			int fetch_result = fbs_fetch(
				IBG(master_instance),
				ib_query->fbs_statement,
				ib_query->out_msg_buffer,
				IB_STATUS
			);

			if (fetch_result == 0) {
				/* End of data */
				ib_query->has_more_rows = 0;
				ib_query->is_open = 0;
				fbs_close_cursor(ib_query->fbs_statement, IB_STATUS);
				RETURN_FALSE;
			} else if (fetch_result == -1) {
				/* Error */
				ib_query->has_more_rows = 0;
				ib_query->is_open = 0;
				_php_fbird_error(); /* LCOV_EXCL_LINE */
				fbs_close_cursor(ib_query->fbs_statement, IB_STATUS);
				RETURN_FALSE;
			}
			/* fetch_result == 1: row fetched into out_msg_buffer */
		} else {
			/* Buffered returning: data already in buffer, consume once */
			ib_query->has_more_rows = 0;
			ib_query->is_open = 0;
		}
	} else {
		/* EXEC PROCEDURE - single row already buffered */
		ib_query->has_more_rows = 0;
		ib_query->is_open = 0;
	}

	HashTable *ht_ret;
	if(!(fetch_type & FETCH_ROW)) {
		if(!ib_query->ht_aliases){
			if(_php_fbird_alloc_ht_aliases(ib_query)){
				_php_fbird_error();
				RETURN_FALSE;
			}
		}
		ht_ret = zend_array_dup(ib_query->ht_aliases);
	} else {
		if(!ib_query->ht_ind)_php_fbird_alloc_ht_ind(ib_query);
		ht_ret = zend_array_dup(ib_query->ht_ind);
	}

	/*
	 * Pure OO API Data Extraction from Message Buffer
	 *
	 * Uses fbm_* helpers to get field metadata and extract data from the
	 * message buffer that was populated by fbs_fetch().
	 */
	for(i = 0; i < ib_query->out_fields_count; ++i) {
		/* Get field metadata via OO API */
		unsigned field_offset = fbm_get_offset(IBG(master_instance), ib_query->out_metadata, (unsigned)i);
		unsigned null_offset = fbm_get_null_offset(IBG(master_instance), ib_query->out_metadata, (unsigned)i);
		unsigned field_type = fbm_get_type(IBG(master_instance), ib_query->out_metadata, (unsigned)i);
		unsigned field_length = fbm_get_length(IBG(master_instance), ib_query->out_metadata, (unsigned)i);
		int field_scale = fbm_get_scale(IBG(master_instance), ib_query->out_metadata, (unsigned)i);
		unsigned field_subtype = fbm_get_subtype(IBG(master_instance), ib_query->out_metadata, (unsigned)i);

		/* Get data and null indicator pointers from message buffer */
		unsigned char *msg_buffer = (unsigned char *)ib_query->out_msg_buffer;
		void *field_data = msg_buffer + field_offset;
		ISC_SHORT *null_indicator = (ISC_SHORT *)(msg_buffer + null_offset);

		/* Check if field is NULL */
		bool is_null_field = (*null_indicator != 0);

		if (is_null_field) {
			zend_hash_move_forward(ht_ret);
			continue;
		}

		result = zend_hash_get_current_data(ht_ret);
		if (!result) {
			_php_fbird_module_error("Internal error: result array iterator out of sync");
			RETURN_FALSE;
		}

		/* Map OO type to legacy SQL_* type for _php_fbird_var_zval compatibility */
		int sql_type = field_type;

		switch (field_type) {
			default:
				_php_fbird_var_zval(result, field_data, sql_type, field_length,
					field_scale, field_subtype, flag);
				break;
			case SQL_BLOB:
				if (flag & PHP_FBIRD_FETCH_BLOBS) { /* fetch blob contents into hash */

					fbird_blob blob_handle;
					zend_ulong max_len = 0;

					memset(&blob_handle, 0, sizeof(blob_handle));
					blob_handle.type = BLOB_OUTPUT;
					blob_handle.bl_qd = *(ISC_QUAD *)field_data;
					blob_handle.fbb_blob = NULL;

					if (!ib_query->link || !ib_query->link->fbc_connection) {
						_php_fbird_module_error("OO API connection required to fetch BLOB contents");
						goto _php_fbird_fetch_error;
					}
					if (!ib_query->trans || !ib_query->trans->fbt_transaction) {
						_php_fbird_module_error("OO API transaction required to fetch BLOB contents"); /* LCOV_EXCL_LINE */
						goto _php_fbird_fetch_error;
					}

					void *attachment_ptr = fbc_get_attachment(ib_query->link->fbc_connection);
					void *transaction_ptr = fbt_get_handle(ib_query->trans->fbt_transaction);
					if (!attachment_ptr || !transaction_ptr) {
						_php_fbird_module_error("Invalid OO API attachment/transaction for BLOB fetch");
						goto _php_fbird_fetch_error;
					}

					blob_handle.fbb_blob = fbb_open(
						IBG(master_instance),
						attachment_ptr,
						transaction_ptr,
						&blob_handle.bl_qd,
						0,
						NULL,
						IB_STATUS
					);
					if (!blob_handle.fbb_blob) {
						_php_fbird_error();
						goto _php_fbird_fetch_error;
					}

					/* Keep legacy handle pointer in sync for blob helpers. */
					blob_handle.bl_handle.ptr = fbb_get_handle(blob_handle.fbb_blob);

					/* Determine total length via getInfo so we can allocate exact buffer. */
					static unsigned char bl_items[] = { isc_info_blob_total_length };
					unsigned char bl_info[32];

					if (fbb_get_info(
							IBG(master_instance),
							blob_handle.fbb_blob,
							sizeof(bl_items),
							bl_items,
							sizeof(bl_info),
							bl_info,
							IB_STATUS
						) == 0) {
						_php_fbird_error(); /* LCOV_EXCL_LINE */
						goto _php_fbird_fetch_error;
					}

					for (unsigned j = 0; j < sizeof(bl_info); ) {
						unsigned short item_len;
						unsigned char item = bl_info[j++];

						if (item == isc_info_end || item == isc_info_truncated ||
							item == isc_info_error || j >= sizeof(bl_info)) {
							_php_fbird_module_error("Could not determine BLOB size (internal error)");
							goto _php_fbird_fetch_error;
						}

						item_len = (unsigned short)isc_vax_integer((char *)&bl_info[j], 2);

						if (item == isc_info_blob_total_length) {
							max_len = (zend_ulong)isc_vax_integer((char *)&bl_info[j + 2], item_len);
							break;
						}
						j += item_len + 2;
					}

					if (max_len == 0) {
						ZVAL_STRING(result, "");
					} else {
						/*
						 * SAFETY: _php_fbird_blob_get() expects max_len bytes of data plus a trailing NUL.
						 * It uses zend_string_alloc(max_len, ...) semantics (len+1) internally.
						 *
						 * If getInfo reports total_length == max_len, we must ensure the reader side
						 * has room for the terminator and does not overwrite Zend heap metadata.
						 */
						if (SUCCESS != _php_fbird_blob_get(result, &blob_handle, max_len)) {
							goto _php_fbird_fetch_error;
						}
					}

					/* fbb_close returns 1 on success, 0 on error */
					if (fbb_close(IBG(master_instance), blob_handle.fbb_blob, IB_STATUS) == 0) {
						_php_fbird_error(); /* LCOV_EXCL_LINE */
						goto _php_fbird_fetch_error;
					}
					fbb_free(blob_handle.fbb_blob);
					blob_handle.fbb_blob = NULL;
					blob_handle.bl_handle.ptr = 0;

				} else { /* blob id only */
					ISC_QUAD bl_qd = *(ISC_QUAD *) field_data;
					ZVAL_NEW_STR(result, _php_fbird_quad_to_string(bl_qd));
				}
				break;
			case SQL_ARRAY:
				if (flag & PHP_FBIRD_FETCH_ARRAYS) { /* array can be *huge* so only fetch if asked */
					ISC_QUAD ar_qd = *(ISC_QUAD *) field_data;

					/* OO API-only: do NOT use ib_query->out_array.
					 * In the OO fetch path we don't populate XSQLDA-backed out_array descriptors,
					 * so dereferencing it can crash. Build a temporary fbird_array from metadata
					 * for each fetched array column. */
					fbird_array local_array;
					memset(&local_array, 0, sizeof(local_array));

					ISC_ARRAY_DESC fresh_desc;
					char rname[64] = {0}, sname[64] = {0};

					/* Get table/column name from OO metadata */
					const char *relation_name = fbm_get_relation(IBG(master_instance), ib_query->out_metadata, (unsigned)i);
					const char *field_name = fbm_get_field(IBG(master_instance), ib_query->out_metadata, (unsigned)i);
					if (relation_name && strlen(relation_name) < sizeof(rname)) {
						strncpy(rname, relation_name, sizeof(rname) - 1);
					}
					if (field_name && strlen(field_name) < sizeof(sname)) {
						strncpy(sname, field_name, sizeof(sname) - 1);
					}

					void* attachment_ptr = fbc_get_attachment(ib_query->link->fbc_connection);
					void* transaction_ptr = fbt_get_handle(ib_query->trans->fbt_transaction);
					if (!attachment_ptr || !transaction_ptr) {
						_php_fbird_module_error("OO API array fetch requires attachment+transaction handles");
						goto _php_fbird_fetch_error;
					}

					if (fba_lookup_bounds(IBG(master_instance), attachment_ptr, transaction_ptr,
							rname, sname, &fresh_desc, IB_STATUS) != 0) {
						_php_fbird_error(); /* LCOV_EXCL_LINE */
						goto _php_fbird_fetch_error;
					}

					/* Build a temporary fbird_array descriptor from fresh_desc */
					local_array.ar_desc = fresh_desc;

					switch (fresh_desc.array_desc_dtype) {
						case blr_text:
						case blr_text2:
							local_array.el_type = SQL_TEXT;
							local_array.el_size = fresh_desc.array_desc_length;
							break;
						case blr_short:
							local_array.el_type = SQL_SHORT;
							local_array.el_size = sizeof(short);
							break;
						case blr_long:
							local_array.el_type = SQL_LONG;
							local_array.el_size = sizeof(ISC_LONG);
							break;
						case blr_int64:
							local_array.el_type = SQL_INT64;
							local_array.el_size = sizeof(ISC_INT64);
							break;
						case blr_float:
							local_array.el_type = SQL_FLOAT;
							local_array.el_size = sizeof(float);
							break;
						case blr_double:
							local_array.el_type = SQL_DOUBLE;
							local_array.el_size = sizeof(double);
							break;
						case blr_timestamp:
							local_array.el_type = SQL_TIMESTAMP;
							local_array.el_size = sizeof(ISC_TIMESTAMP);
							break;
						case blr_sql_date:
							local_array.el_type = SQL_TYPE_DATE;
							local_array.el_size = sizeof(ISC_DATE);
							break;
						case blr_sql_time:
							local_array.el_type = SQL_TYPE_TIME;
							local_array.el_size = sizeof(ISC_TIME);
							break;
					case blr_varying:
					case blr_varying2:
						/* VARCHAR array workaround: Use null-terminated strings.
						 * See fb_array.hpp for explanation of dtype_cstring bug workaround.
						 * Element size = declared_length + 1 (for null terminator). */
						local_array.el_type = SQL_VARYING;
						local_array.el_size = fresh_desc.array_desc_length + 1;
						break;
						default:
							_php_fbird_module_error("Unsupported array dtype %d", fresh_desc.array_desc_dtype);
							goto _php_fbird_fetch_error;
					}

					/* Calculate total array size */
					zend_ulong ar_size = 1;
					for (unsigned short dim = 0; dim < local_array.ar_desc.array_desc_dimensions; dim++) {
						ar_size *= 1 + local_array.ar_desc.array_desc_bounds[dim].array_bound_upper -
							local_array.ar_desc.array_desc_bounds[dim].array_bound_lower;
					}
					local_array.ar_size = (ISC_LONG)(local_array.el_size * ar_size);

					ISC_LONG fetch_size = local_array.ar_size;
					void *ar_data = ecalloc(1, (size_t)fetch_size);

					if (fba_get_slice(IBG(master_instance), attachment_ptr, transaction_ptr,
							&ar_qd, &local_array.ar_desc, ar_data, &fetch_size, IB_STATUS) != 0) {
						_php_fbird_error(); /* LCOV_EXCL_LINE */
						efree(ar_data);
						goto _php_fbird_fetch_error;
					}

					if (FAILURE == _php_fbird_arr_zval(result, ar_data, local_array.ar_size, &local_array,
							0, flag)) {
						efree(ar_data);
						goto _php_fbird_fetch_error;
					}
					efree(ar_data);

				} else { /* array id only */
					ISC_QUAD ar_qd = *(ISC_QUAD *) field_data;
					ZVAL_NEW_STR(result, _php_fbird_quad_to_string(ar_qd));
				}
				break;
			_php_fbird_fetch_error:
				RETURN_FALSE;
		} /* switch */

		zend_hash_move_forward(ht_ret);
	}

	RETVAL_ARR(ht_ret);
}

PHP_FUNCTION(fbird_fetch_row)
{
	_php_fbird_fetch_hash(INTERNAL_FUNCTION_PARAM_PASSTHRU, FETCH_ROW);
}

PHP_FUNCTION(fbird_fetch_assoc)
{
	_php_fbird_fetch_hash(INTERNAL_FUNCTION_PARAM_PASSTHRU, FETCH_ARRAY);
}

PHP_FUNCTION(fbird_fetch_object)
{
	_php_fbird_fetch_hash(INTERNAL_FUNCTION_PARAM_PASSTHRU, FETCH_ARRAY);

	if (Z_TYPE_P(return_value) == IS_ARRAY) {
		convert_to_object(return_value);
	}
}

PHP_FUNCTION(fbird_name_result)
{
	zval *result_arg;
	char *name_arg;
	size_t name_arg_len;
	fbird_query *ib_query;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "rs", &result_arg, &name_arg, &name_arg_len) == FAILURE) {
		return;
	}

	/* Validate first argument is a query resource with proper error messages */
	FBIRD_VALIDATE_QUERY_EX(result_arg, 1, ib_query);
	if (!ib_query) {
		RETURN_FALSE;
	}

	/* OO API Only: Use fbs_set_cursor_name() for positioned updates */
	if (!ib_query->fbs_statement) {
		_php_fbird_module_error("fbird_name_result() requires OO API statement (fbs_statement required)");
		RETURN_FALSE;
	}

	if (!fbs_set_cursor_name(IBG(master_instance), ib_query->fbs_statement, name_arg, IB_STATUS)) {
		_php_fbird_error(); /* LCOV_EXCL_LINE */
		RETURN_FALSE;
	}

	RETURN_TRUE;
}

PHP_FUNCTION(fbird_free_result)
{
	_php_fbird_free_query_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}

#endif /* HAVE_FIREBIRD */
