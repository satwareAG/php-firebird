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
#include "php_firebird.h"
#include "php_fbird_includes.h"
#include "php_fbird_query_internal.h"
#include "firebird_utils.h"

#define ISC_LONG_MIN    INT_MIN
#define ISC_LONG_MAX    INT_MAX

#define FETCH_ROW       1
#define FETCH_ARRAY     2

typedef struct {
	unsigned short vary_length;
	char vary_string[1];
} IBVARY;

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
    setenv("TZ", "UTC", 1);
    tzset();
    time_t ts = mktime(tm);
    /* Restore TZ */
    if (saved) {
        setenv("TZ", saved, 1);
        efree(saved);
    } else {
        unsetenv("TZ");
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
    setenv("TZ", use_tz, 1);
    tzset();
    time_t ts = mktime(tm);
    if (saved) {
        setenv("TZ", saved, 1);
        efree(saved);
    } else {
        unsetenv("TZ");
    }
    tzset();
    return ts;
}

static int _php_fbird_var_zval(zval *val, void *data, int type, int len, /* {{{ */
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
				 * For UTF8 CHAR(N): sqllen = N×4 (max bytes), buffer space-padded
				 * For single-byte CHAR(N): sqllen = N, buffer space-padded
				 *
				 * Problem: We cannot distinguish intentional trailing spaces from
				 * padding using byte-level analysis. For UTF8, we know the declared
				 * character count (N = sqllen/4) and can count exactly that many
				 * UTF8 characters to find the actual data boundary.
				 *
				 * For single-byte charsets, we rtrim spaces which matches
				 * historical SQL CHAR behavior (trailing spaces are insignificant).
				 *
				 * Charset IDs: 4 = UTF8, 59 = UTF8MB4 (Firebird 4+)
				 */
				unsigned char charset_id = (unsigned char)(subtype & 0xFF);

				if (charset_id == 4 || charset_id == 59) {
					/* UTF8/UTF8MB4: count exactly N characters where N = sqllen/4 */
					size_t char_count = (size_t)len / 4;
					size_t actual_len = 0;
					size_t chars = 0;
					const unsigned char *p = (const unsigned char *)data;

					while (actual_len < (size_t)len && chars < char_count) {
						unsigned char c = p[actual_len];
						size_t char_bytes;

						if ((c & 0x80) == 0) {
							char_bytes = 1;  /* ASCII */
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
						chars++;
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
				format = INI_STR("ibase.timeformat");
				fbu_decode_time_tz(IBG(master_instance), (ISC_TIME_TZ *) data, &hours, &minutes, &seconds, &fractions, sizeof(timeZoneBuffer), timeZoneBuffer);
				ISC_TIME time = fbu_encode_time(IBG(master_instance), hours, minutes, seconds, fractions);
				isc_decode_sql_time(&time, &t);
			} else {
				format = INI_STR("ibase.timestampformat");
				fbu_decode_timestamp_tz(IBG(master_instance), (ISC_TIMESTAMP_TZ *) data, &year, &month, &day, &hours, &minutes, &seconds, &fractions, sizeof(timeZoneBuffer), timeZoneBuffer);
				ISC_TIMESTAMP ts;
				ts.timestamp_date = fbu_encode_date(IBG(master_instance), year, month, day);
				ts.timestamp_time = fbu_encode_time(IBG(master_instance), hours, minutes, seconds, fractions);
				isc_decode_timestamp(&ts, &t);
			}

			if (((type & ~1) != SQL_TIME_TZ) && (flag & PHP_IBASE_UNIXTIME)) {
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
			format = INI_STR("ibase.timestampformat");
			isc_decode_timestamp((ISC_TIMESTAMP *) data, &t);
			goto format_date_time;
		case SQL_TYPE_DATE:
			format = INI_STR("ibase.dateformat");
			isc_decode_sql_date((ISC_DATE *) data, &t);
			goto format_date_time;
		case SQL_TYPE_TIME:
			format = INI_STR("ibase.timeformat");
			isc_decode_sql_time((ISC_TIME *) data, &t);

format_date_time:
			/*
			  XXX - Might have to remove this later - seems that isc_decode_date()
			   always sets tm_isdst to 0, sometimes incorrectly (InterBase 6 bug?)
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
                l = strftime(string_data, sizeof(string_data), format, &t);
                ZVAL_STRINGL(val, string_data, l);
            } else if (flag & PHP_IBASE_UNIXTIME) {
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
	} /* switch (type) */
	return SUCCESS;
}
/* }}}	*/

static int _php_fbird_arr_zval(zval *ar_zval, char *data, zend_ulong data_size, /* {{{ */
	fbird_array *ib_array, int dim, size_t flag)
{
	/**
	 * Create multidimension array - recursion function
	 */
	int
		u_bound = ib_array->ar_desc.array_desc_bounds[dim].array_bound_upper,
		l_bound = ib_array->ar_desc.array_desc_bounds[dim].array_bound_lower,
		dim_len = 1 + u_bound - l_bound;
	unsigned short i;

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
		if (FAILURE == _php_fbird_var_zval(ar_zval, data, ib_array->el_type,
				ib_array->ar_desc.array_desc_length, ib_array->ar_desc.array_desc_scale, 0, flag)) {
			return FAILURE;
		}

		/* fix for peculiar handling of VARCHAR arrays;
		   truncate the field to the cstring length */
		if (ib_array->ar_desc.array_desc_dtype == blr_varying ||
			ib_array->ar_desc.array_desc_dtype == blr_varying2) {
			Z_STRLEN_P(ar_zval) = strlen(Z_STRVAL_P(ar_zval));
		}
	}
	return SUCCESS;
}
/* }}} */

static void _php_fbird_fetch_hash(INTERNAL_FUNCTION_PARAMETERS, int fetch_type) /* {{{ */
{
	zval *res_arg, *result;
	zend_long flag = 0;
	zend_long i, array_cnt = 0;
	fbird_query *ib_query;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r|l", &res_arg, &flag)) {
		RETURN_FALSE;
	}

 if(!_php_fbird_fetch_query_res(res_arg, &ib_query)) {
        /* Let Zend validate resource via _php_fbird_fetch_query_res */
        RETURN_FALSE;
    }

	if (ib_query->out_sqlda == NULL || !ib_query->has_more_rows || !ib_query->is_open) {
		RETURN_FALSE;
	}

	assert(ib_query->out_fields_count > 0);

 if (ib_query->statement_type != isc_info_sql_stmt_exec_procedure) {
        /* Treat DML ... RETURNING as a single buffered row (no fetch). */
        int is_buffered_returning = (
            ib_query->out_sqlda &&
            (ib_query->statement_type == isc_info_sql_stmt_insert ||
             ib_query->statement_type == isc_info_sql_stmt_update ||
             ib_query->statement_type == isc_info_sql_stmt_delete) &&
            ib_query->was_result_once
        );
        if (!is_buffered_returning) {
        ISC_STATUS fetch_res = isc_dsql_fetch(IB_STATUS, &ib_query->stmt.stmt, 1, ib_query->out_sqlda);
        if (fetch_res) {
            ib_query->has_more_rows = 0;
            ib_query->is_open = 0;

            /* Check for EOF (100) - do not report error.
             * Also suppress "Invalid cursor reference" (-504, isc_dsql_cursor_err = 335544569)
             * which occurs when fetching from a cursor implicitly closed by a transaction commit.
             */
            int suppress_error = (fetch_res == 100);

            if (!suppress_error && IB_STATUS[0] == 1 && IB_STATUS[1]) {
                /* Suppress specific cursor errors that indicate the cursor was closed
                 * (e.g. by transaction commit) to allow returning FALSE (EOF) cleanly.
                 * 335544569: isc_dsql_cursor_err (SQL -504)
                 * 335544436: Observed error code for "Invalid cursor reference" on some versions
                 * We do NOT suppress 335544332 (isc_bad_stmt_handle) as that implies
                 * usage of a freed/corrupted resource which should warn.
                 */
                if (IB_STATUS[1] == 335544569 || IB_STATUS[1] == 335544436) {
                    suppress_error = 1;
                } else {
                    _php_fbird_error();
                }
            }

            /* Close the cursor. If we suppressed a cursor error, closing might also fail
             * (e.g. cursor already closed -502), so suppress that too. */
            if (isc_dsql_free_statement(IB_STATUS, &ib_query->stmt.stmt, DSQL_close)) {
                /* Check for "Attempt to reclose a closed cursor" (-502)
                 * iso_dsql_cursor_close_err = 335544573 (check this constant?)
                 * Actually -502 is isc_dsql_cursor_open_err usually?
                 * Let's just suppress if we already suppressed the fetch error, OR
                 * if this specific error matches known safe cases.
                 */
                if (!suppress_error) {
                    /* If closing failed, check if it was due to cursor already being closed/invalid.
                     * Suppress these to avoid double-fault noise. */
                    if (IB_STATUS[1] == 335544569
                        || IB_STATUS[1] == 335544436
                        || IB_STATUS[1] == 335544573 /* isc_dsql_cursor_close_err */) {
                        /* Suppress */
                    } else {
                        _php_fbird_error();
                    }
                }
            }

            RETURN_FALSE;
        }
        } else {
            /* Buffered returning: data already in out_sqlda, consume once */
            ib_query->has_more_rows = 0;
            ib_query->is_open = 0;
        }
    } else {
        ib_query->has_more_rows = 0;
        ib_query->is_open = 0;
    }

	assert(ib_query->out_fields_count == ib_query->out_sqlda->sqld);

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

	for(i = 0; i < ib_query->out_fields_count; ++i) {
		XSQLVAR *var = &ib_query->out_sqlda->sqlvar[i];

		// Check if field is NULL using defensive programming
		bool is_null_field = false;

		if (var->sqltype & 1) {
			// Nullable field - check null indicator safely
			if (var->sqlind == NULL) {
				_php_fbird_module_error("NULL indicator missing for nullable field %ld", i);
				goto _php_fbird_fetch_error;
			}
			is_null_field = (*var->sqlind == -1);
		} else {
			// NOT NULL field - should not have null indicator access
			is_null_field = false;
		}

		if (is_null_field) {
			zend_hash_move_forward(ht_ret);
			continue;
		}

		result = zend_hash_get_current_data(ht_ret);
        if (!result) {
            _php_fbird_module_error("Internal error: result array iterator out of sync");
            RETURN_FALSE;
        }

		switch (var->sqltype & ~1) {

			default:
				_php_fbird_var_zval(result, var->sqldata, var->sqltype, var->sqllen,
					var->sqlscale, var->sqlsubtype, flag);
				break;
			case SQL_BLOB:
				if (flag & PHP_IBASE_FETCH_BLOBS) { /* fetch blob contents into hash */

					fbird_blob blob_handle;
					zend_ulong max_len = 0;
					static char bl_items[] = {isc_info_blob_total_length};
					char bl_info[20];
					unsigned short i;

					blob_handle.bl_handle.ptr = 0;
					blob_handle.bl_qd = *(ISC_QUAD *) var->sqldata;

					if (isc_open_blob(IB_STATUS, &ib_query->link->handle.db, &ib_query->trans->handle.tr,
							&blob_handle.bl_handle.blob, &blob_handle.bl_qd)) {
						_php_fbird_error();
						goto _php_fbird_fetch_error;
					}

					if (isc_blob_info(IB_STATUS, &blob_handle.bl_handle.blob, sizeof(bl_items),
							bl_items, sizeof(bl_info), bl_info)) {
						_php_fbird_error();
						goto _php_fbird_fetch_error;
					}

					/* find total length of blob's data */
					for (i = 0; i < sizeof(bl_info); ) {
						unsigned short item_len;
						char item = bl_info[i++];

						if (item == isc_info_end || item == isc_info_truncated ||
							item == isc_info_error || i >= sizeof(bl_info)) {

							_php_fbird_module_error("Could not determine BLOB size (internal error)"
								);
							goto _php_fbird_fetch_error;
						}

						item_len = (unsigned short) isc_vax_integer(&bl_info[i], 2);

						if (item == isc_info_blob_total_length) {
							max_len = isc_vax_integer(&bl_info[i+2], item_len);
							break;
						}
						i += item_len+2;
					}

					if (max_len == 0) {
						ZVAL_STRING(result, "");
					} else if (SUCCESS != _php_fbird_blob_get(result, &blob_handle,
							max_len)) {
						goto _php_fbird_fetch_error;
					}

					if (isc_close_blob(IB_STATUS, &blob_handle.bl_handle.blob)) {
						_php_fbird_error();
						goto _php_fbird_fetch_error;
					}

				} else { /* blob id only */
					ISC_QUAD bl_qd = *(ISC_QUAD *) var->sqldata;
					ZVAL_NEW_STR(result, _php_fbird_quad_to_string(bl_qd));
				}
				break;
			case SQL_ARRAY:
				if (flag & PHP_IBASE_FETCH_ARRAYS) { /* array can be *huge* so only fetch if asked */
					ISC_QUAD ar_qd = *(ISC_QUAD *) var->sqldata;
					fbird_array *ib_array = &ib_query->out_array[array_cnt++];
					/* Use local copy of size - isc_array_get_slice modifies its size parameter
					 * to reflect actual bytes fetched, which corrupts ar_size for recursive use */
					ISC_LONG fetch_size = ib_array->ar_size;
					/* Use ecalloc to zero-initialize - check if corruption is from uninitialized memory */
					void *ar_data = ecalloc(1, (size_t)fetch_size);

				/* Fetch a fresh array descriptor to ensure we have correct metadata.
				 * The stored descriptor might have stale data. */
				ISC_ARRAY_DESC fresh_desc;
				char rname[64] = {0}, sname[64] = {0};
				/* Get table/column name from the XSQLVAR - need to find the original var */
				if (var->relname_length > 0 && var->relname_length < 64) {
					memcpy(rname, var->relname, var->relname_length);
				}
				if (var->sqlname_length > 0 && var->sqlname_length < 64) {
					memcpy(sname, var->sqlname, var->sqlname_length);
				}
				if (isc_array_lookup_bounds(IB_STATUS, &ib_query->link->handle.db,
						&ib_query->trans->handle.tr, rname, sname, &fresh_desc)) {
					_php_fbird_error();
					efree(ar_data);
					goto _php_fbird_fetch_error;
				}
				/* WORKAROUND: If we treated this as SQL_TEXT in alloc_array (for VARCHAR),
				 * we must tell get_slice to return text (not varying structure).
				 * Keep the length as-is - Firebird reports it correctly. */
				if (ib_array->el_type == SQL_TEXT &&
					(fresh_desc.array_desc_dtype == blr_varying || fresh_desc.array_desc_dtype == blr_varying2)) {
					fresh_desc.array_desc_dtype = blr_text;
				}

				if (isc_array_get_slice(IB_STATUS, &ib_query->link->handle.db,
						&ib_query->trans->handle.tr, &ar_qd, &fresh_desc,
						ar_data, &fetch_size)) {
					_php_fbird_error();
					efree(ar_data);
					goto _php_fbird_fetch_error;
				}

					/* Use ORIGINAL ar_size for recursive processing (structure size),
					 * not the potentially modified fetch_size */
					if (FAILURE == _php_fbird_arr_zval(result, ar_data, ib_array->ar_size, ib_array,
							0, flag)) {
						efree(ar_data);
						goto _php_fbird_fetch_error;
					}
					efree(ar_data);

				} else { /* blob id only */
					ISC_QUAD ar_qd = *(ISC_QUAD *) var->sqldata;
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
/* }}} */

/* {{{ proto fbird_fetch_row(resource result [, int fetch_flags])
   Fetch a row  from the results of a query */
PHP_FUNCTION(fbird_fetch_row)
{
	_php_fbird_fetch_hash(INTERNAL_FUNCTION_PARAM_PASSTHRU, FETCH_ROW);
}
/* }}} */

/* {{{ proto fbird_fetch_assoc(resource result [, int fetch_flags])
   Fetch a row  from the results of a query */
PHP_FUNCTION(fbird_fetch_assoc)
{
	_php_fbird_fetch_hash(INTERNAL_FUNCTION_PARAM_PASSTHRU, FETCH_ARRAY);
}
/* }}} */

/* {{{ proto fbird_fetch_object(resource result [, int fetch_flags])
   Fetch a object from the results of a query */
PHP_FUNCTION(fbird_fetch_object)
{
	_php_fbird_fetch_hash(INTERNAL_FUNCTION_PARAM_PASSTHRU, FETCH_ARRAY);

	if (Z_TYPE_P(return_value) == IS_ARRAY) {
		convert_to_object(return_value);
	}
}
/* }}} */

/* {{{ proto fbird_name_result(resource result, string name)
   Assign a name to a result for use with ... WHERE CURRENT OF <name> statements */
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

	if(!_php_fbird_fetch_query_res(result_arg, &ib_query)) {
		RETURN_FALSE;
	}

	if (isc_dsql_set_cursor_name(IB_STATUS, &ib_query->stmt.stmt, name_arg, 0)) {
		_php_fbird_error();
		RETURN_FALSE;
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool fbird_free_result(resource result)
   Free the memory used by a result */
PHP_FUNCTION(fbird_free_result)
{
	_php_fbird_free_query_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */

#endif /* HAVE_FIREBIRD */
