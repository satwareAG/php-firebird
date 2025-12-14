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
 * Parameter binding functions.
 *
 * This file contains functions for binding PHP values to Firebird query
 * parameters and safely copying SQLVAR data.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include <ctype.h>

#if HAVE_FIREBIRD

#include "php_firebird.h"
#include "php_fbird_includes.h"
#include "php_fbird_query_internal.h"
#include "php_fbird_query_bind.h"
#include "php_fbird_query_array.h"
#include "firebird_utils.h"

/* Helper function for safer SQLVAR data copying */
int _php_fbird_safe_copy_sqlvar_data(XSQLVAR *dest_var, const XSQLVAR *src_var, int field_index, const char *query_context) /* {{{ */
{
	/* Validate input parameters */
	if (!dest_var || !src_var) {
		_php_fbird_module_error("EXECUTE PROCEDURE: Invalid XSQLVAR pointers for field %d in query: %s",
            field_index, query_context ? query_context : "unknown");
		return FAILURE;
	}

	if (!src_var->sqldata) {
		/*
		 * OO API note:
		 * For EXECUTE PROCEDURE and DML ... RETURNING, the extension can operate
		 * with a message buffer (IMessageMetadata + out_msg_buffer) instead of
		 * legacy SQLDA row buffers. In that mode, sqldata pointers remain NULL.
		 *
		 * Treat missing sqldata as "no legacy row-buffer to copy" (success),
		 * and let fetch logic read from the message buffer.
		 */
		dest_var->sqldata = NULL;
		return SUCCESS;
	}

	/* Verify sqltype consistency between source and destination */
	if (dest_var->sqltype != src_var->sqltype) {
		_php_fbird_module_error("EXECUTE PROCEDURE: sqltype mismatch for field %d (dest=%d, src=%d) in query: %s",
			field_index, dest_var->sqltype, src_var->sqltype, query_context ? query_context : "unknown");
		return FAILURE;
	}

	/* Allocate and copy data based on SQL type with comprehensive bounds checking */
	switch (dest_var->sqltype & ~1) {
		case SQL_TEXT:
			/* Validate field length for TEXT fields */
			if (dest_var->sqllen != src_var->sqllen) {
				_php_fbird_module_error("EXECUTE PROCEDURE: TEXT sqllen mismatch for field %d (dest=%d, src=%d) in query: %s",
					field_index, dest_var->sqllen, src_var->sqllen, query_context ? query_context : "unknown");
				return FAILURE;
			}
			/* ISC_SHORT can't exceed 32767, so > 65535 check is tautologically false - only check < 0 */
			if (dest_var->sqllen < 0) {
				_php_fbird_module_error("EXECUTE PROCEDURE: Invalid TEXT length %d for field %d in query: %s",
					dest_var->sqllen, field_index, query_context ? query_context : "unknown");
				return FAILURE;
			}
			dest_var->sqldata = safe_emalloc(sizeof(char), dest_var->sqllen, 0);
			if (!dest_var->sqldata) {
				_php_fbird_module_error("EXECUTE PROCEDURE: Failed to allocate TEXT data for field %d in query: %s",
                    field_index, query_context ? query_context : "unknown");
				return FAILURE;
			}
			/* Use safer copy with explicit size limit */
			memcpy(dest_var->sqldata, src_var->sqldata, dest_var->sqllen);
			break;

		case SQL_VARYING:
			/* Validate field length for VARCHAR fields */
			if (dest_var->sqllen != src_var->sqllen) {
				_php_fbird_module_error("EXECUTE PROCEDURE: VARCHAR sqllen mismatch for field %d (dest=%d, src=%d) in query: %s",
					field_index, dest_var->sqllen, src_var->sqllen, query_context ? query_context : "unknown");
				return FAILURE;
			}
			if (dest_var->sqllen < 0 || dest_var->sqllen > 65535) {
				_php_fbird_module_error("EXECUTE PROCEDURE: Invalid VARCHAR length %d for field %d in query: %s",
					dest_var->sqllen, field_index, query_context ? query_context : "unknown");
				return FAILURE;
			}
			dest_var->sqldata = safe_emalloc(sizeof(char), dest_var->sqllen + sizeof(short), 0);
			if (!dest_var->sqldata) {
				_php_fbird_module_error("EXECUTE PROCEDURE: Failed to allocate VARCHAR data for field %d in query: %s",
                    field_index, query_context ? query_context : "unknown");
				return FAILURE;
			}
			/* Copy length prefix + data with bounds checking */
			{
				size_t varchar_copy_size = dest_var->sqllen + sizeof(short);
				memcpy(dest_var->sqldata, src_var->sqldata, varchar_copy_size);
			}
			break;

#ifdef SQL_BOOLEAN
		case SQL_BOOLEAN:
			if (src_var->sqllen != sizeof(FB_BOOLEAN)) {
				_php_fbird_module_error("EXECUTE PROCEDURE: Invalid BOOLEAN length %d for field %d", src_var->sqllen, field_index);
				return FAILURE;
			}
			dest_var->sqldata = emalloc(sizeof(FB_BOOLEAN));
			if (!dest_var->sqldata) {
				_php_fbird_module_error("EXECUTE PROCEDURE: Failed to allocate BOOLEAN data for field %d", field_index);
				return FAILURE;
			}
			/* Direct assignment for simple types (safer than memcpy for single values) */
			*(FB_BOOLEAN *)dest_var->sqldata = *(FB_BOOLEAN *)src_var->sqldata;
			break;
#endif

		case SQL_SHORT:
			if (src_var->sqllen != sizeof(short)) {
				_php_fbird_module_error("EXECUTE PROCEDURE: Invalid SHORT length %d for field %d", src_var->sqllen, field_index);
				return FAILURE;
			}
			dest_var->sqldata = emalloc(sizeof(short));
			if (!dest_var->sqldata) {
				_php_fbird_module_error("EXECUTE PROCEDURE: Failed to allocate SHORT data for field %d", field_index);
				return FAILURE;
			}
			*(short *)dest_var->sqldata = *(short *)src_var->sqldata;
			break;

		case SQL_LONG:
			if (src_var->sqllen != sizeof(ISC_LONG)) {
				_php_fbird_module_error("EXECUTE PROCEDURE: Invalid LONG length %d for field %d", src_var->sqllen, field_index);
				return FAILURE;
			}
			dest_var->sqldata = emalloc(sizeof(ISC_LONG));
			if (!dest_var->sqldata) {
				_php_fbird_module_error("EXECUTE PROCEDURE: Failed to allocate LONG data for field %d", field_index);
				return FAILURE;
			}
			*(ISC_LONG *)dest_var->sqldata = *(ISC_LONG *)src_var->sqldata;
			break;

		case SQL_FLOAT:
			if (src_var->sqllen != sizeof(float)) {
				_php_fbird_module_error("EXECUTE PROCEDURE: Invalid FLOAT length %d for field %d", src_var->sqllen, field_index);
				return FAILURE;
			}
			dest_var->sqldata = emalloc(sizeof(float));
			if (!dest_var->sqldata) {
				_php_fbird_module_error("EXECUTE PROCEDURE: Failed to allocate FLOAT data for field %d", field_index);
				return FAILURE;
			}
			*(float *)dest_var->sqldata = *(float *)src_var->sqldata;
			break;

		case SQL_DOUBLE:
			if (src_var->sqllen != sizeof(double)) {
				_php_fbird_module_error("EXECUTE PROCEDURE: Invalid DOUBLE length %d for field %d", src_var->sqllen, field_index);
				return FAILURE;
			}
			dest_var->sqldata = emalloc(sizeof(double));
			if (!dest_var->sqldata) {
				_php_fbird_module_error("EXECUTE PROCEDURE: Failed to allocate DOUBLE data for field %d", field_index);
				return FAILURE;
			}
			*(double *)dest_var->sqldata = *(double *)src_var->sqldata;
			break;

		case SQL_INT64:
			if (src_var->sqllen != sizeof(ISC_INT64)) {
				_php_fbird_module_error("EXECUTE PROCEDURE: Invalid INT64 length %d for field %d", src_var->sqllen, field_index);
				return FAILURE;
			}
			dest_var->sqldata = emalloc(sizeof(ISC_INT64));
			if (!dest_var->sqldata) {
				_php_fbird_module_error("EXECUTE PROCEDURE: Failed to allocate INT64 data for field %d", field_index);
				return FAILURE;
			}
			*(ISC_INT64 *)dest_var->sqldata = *(ISC_INT64 *)src_var->sqldata;
			break;

		case SQL_TIMESTAMP:
			if (src_var->sqllen != sizeof(ISC_TIMESTAMP)) {
				_php_fbird_module_error("EXECUTE PROCEDURE: Invalid TIMESTAMP length %d for field %d", src_var->sqllen, field_index);
				return FAILURE;
			}
			dest_var->sqldata = emalloc(sizeof(ISC_TIMESTAMP));
			if (!dest_var->sqldata) {
				_php_fbird_module_error("EXECUTE PROCEDURE: Failed to allocate TIMESTAMP data for field %d", field_index);
				return FAILURE;
			}
			*(ISC_TIMESTAMP *)dest_var->sqldata = *(ISC_TIMESTAMP *)src_var->sqldata;
			break;

		case SQL_TYPE_DATE:
			if (src_var->sqllen != sizeof(ISC_DATE)) {
				_php_fbird_module_error("EXECUTE PROCEDURE: Invalid DATE length %d for field %d", src_var->sqllen, field_index);
				return FAILURE;
			}
			dest_var->sqldata = emalloc(sizeof(ISC_DATE));
			if (!dest_var->sqldata) {
				_php_fbird_module_error("EXECUTE PROCEDURE: Failed to allocate DATE data for field %d", field_index);
				return FAILURE;
			}
			*(ISC_DATE *)dest_var->sqldata = *(ISC_DATE *)src_var->sqldata;
			break;

		case SQL_TYPE_TIME:
			if (src_var->sqllen != sizeof(ISC_TIME)) {
				_php_fbird_module_error("EXECUTE PROCEDURE: Invalid TIME length %d for field %d", src_var->sqllen, field_index);
				return FAILURE;
			}
			dest_var->sqldata = emalloc(sizeof(ISC_TIME));
			if (!dest_var->sqldata) {
				_php_fbird_module_error("EXECUTE PROCEDURE: Failed to allocate TIME data for field %d", field_index);
				return FAILURE;
			}
			*(ISC_TIME *)dest_var->sqldata = *(ISC_TIME *)src_var->sqldata;
			break;

		case SQL_BLOB:
		case SQL_ARRAY:
			if (src_var->sqllen != sizeof(ISC_QUAD)) {
				_php_fbird_module_error("EXECUTE PROCEDURE: Invalid QUAD length %d for field %d", src_var->sqllen, field_index);
				return FAILURE;
			}
			dest_var->sqldata = emalloc(sizeof(ISC_QUAD));
			if (!dest_var->sqldata) {
				_php_fbird_module_error("EXECUTE PROCEDURE: Failed to allocate QUAD data for field %d", field_index);
				return FAILURE;
			}
			*(ISC_QUAD *)dest_var->sqldata = *(ISC_QUAD *)src_var->sqldata;
			break;

#if FB_API_VER >= 40
		case SQL_TIMESTAMP_TZ:
			if (src_var->sqllen != sizeof(ISC_TIMESTAMP_TZ)) {
				_php_fbird_module_error("EXECUTE PROCEDURE: Invalid TIMESTAMP_TZ length %d for field %d", src_var->sqllen, field_index);
				return FAILURE;
			}
			dest_var->sqldata = emalloc(sizeof(ISC_TIMESTAMP_TZ));
			if (!dest_var->sqldata) {
				_php_fbird_module_error("EXECUTE PROCEDURE: Failed to allocate TIMESTAMP_TZ data for field %d", field_index);
				return FAILURE;
			}
			*(ISC_TIMESTAMP_TZ *)dest_var->sqldata = *(ISC_TIMESTAMP_TZ *)src_var->sqldata;
			break;

		case SQL_TIME_TZ:
			if (src_var->sqllen != sizeof(ISC_TIME_TZ)) {
				_php_fbird_module_error("EXECUTE PROCEDURE: Invalid TIME_TZ length %d for field %d", src_var->sqllen, field_index);
				return FAILURE;
			}
			dest_var->sqldata = emalloc(sizeof(ISC_TIME_TZ));
			if (!dest_var->sqldata) {
				_php_fbird_module_error("EXECUTE PROCEDURE: Failed to allocate TIME_TZ data for field %d", field_index);
				return FAILURE;
			}
			*(ISC_TIME_TZ *)dest_var->sqldata = *(ISC_TIME_TZ *)src_var->sqldata;
			break;
#endif

		default:
			_php_fbird_module_error("EXECUTE PROCEDURE: Unhandled sqltype %d for field %d",
				dest_var->sqltype & ~1, field_index);
			return FAILURE;
	}

	return SUCCESS;
}
/* }}} */

/**
 * Transfer bound XSQLDA values to OO API message buffer.
 *
 * After _php_fbird_bind() populates the XSQLDA structure with PHP values,
 * this function copies those values to the flat message buffer format
 * required by the OO API (fbs_execute, fbs_open_cursor).
 *
 * The OO API uses IMessageMetadata to describe buffer layout:
 * - fbm_get_offset() returns data position for each parameter
 * - fbm_get_null_offset() returns null indicator position
 * - fbm_get_length() returns data length for each parameter
 *
 * @param ib_query Query structure with populated in_sqlda and allocated in_msg_buffer
 * @return SUCCESS or FAILURE
 */
int _php_fbird_xsqlda_to_msg_buffer(fbird_query *ib_query) /* {{{ */
{
	/* Validate prerequisites */
	if (!ib_query->in_msg_buffer || !ib_query->in_metadata || !ib_query->in_sqlda) {
		return SUCCESS; /* Nothing to transfer - no input parameters */
	}

	if (ib_query->in_fields_count == 0) {
		return SUCCESS; /* No parameters */
	}

	void *master = IBG(master_instance);
	if (!master) {
		_php_fbird_module_error("OO API master instance not available");
		return FAILURE;
	}

	/* Transfer each parameter from XSQLDA to message buffer */
	for (int i = 0; i < ib_query->in_fields_count; i++) {
		XSQLVAR *var = &ib_query->in_sqlda->sqlvar[i];

		/* Get offsets from metadata */
		unsigned data_offset = fbm_get_offset(master, ib_query->in_metadata, i);
		unsigned null_offset = fbm_get_null_offset(master, ib_query->in_metadata, i);
		unsigned meta_length = fbm_get_length(master, ib_query->in_metadata, i);
		/* IMPORTANT: use metadata type, not XSQLVAR type.
		 * _php_fbird_bind() may change var->sqltype (e.g., to SQL_TEXT for string fallback),
		 * but the OO API message buffer format must match the original parameter type. */
		unsigned meta_type = fbm_get_type(master, ib_query->in_metadata, i) & ~1;

		/* Set null indicator in message buffer */
		short *null_ptr = (short *)(ib_query->in_msg_buffer + null_offset);
		if (var->sqlind && *var->sqlind == -1) {
			*null_ptr = -1; /* NULL value */
			continue; /* Skip data transfer for NULL values */
		}
		*null_ptr = 0; /* Not NULL */

		/* Get destination pointer in message buffer */
		unsigned char *dest = ib_query->in_msg_buffer + data_offset;

		/* Transfer data based on SQL type */
		if (!var->sqldata) {
			/* No data - shouldn't happen for non-NULL values */
			_php_fbird_module_error("Parameter %d: sqldata is NULL for non-NULL value", i + 1);
			return FAILURE;
		}

		/* Copy data from XSQLDA to message buffer using METADATA type (not var->sqltype)
		 * to ensure correct format for the OO API */
		switch (meta_type) {
			case SQL_TEXT:
				/* Fixed-length character field */
				if ((unsigned)var->sqllen <= meta_length) {
					memcpy(dest, var->sqldata, var->sqllen);
					/* Pad with spaces if needed (TEXT fields are space-padded) */
					if ((unsigned)var->sqllen < meta_length) {
						memset(dest + var->sqllen, ' ', meta_length - var->sqllen);
					}
				} else {
					/* Truncate if source is longer */
					memcpy(dest, var->sqldata, meta_length);
				}
				break;

			case SQL_VARYING:
				/* Variable-length character field: 2-byte length prefix + data
				 *
				 * Handle two cases:
				 * 1. Data is already in VARY format (var->sqltype == SQL_VARYING): has 2-byte length prefix
				 * 2. Data is raw string (var->sqltype == SQL_TEXT): no length prefix, need to add one
				 *
				 * The bind code may convert non-string values to SQL_TEXT format (raw string),
				 * but the OO API expects SQL_VARYING format with length prefix. */
				{
					unsigned var_type = (unsigned)(var->sqltype & ~1);

					if (var_type == SQL_VARYING) {
						/* Data already has VARY format (2-byte length + data) */
						short str_len = *(short *)var->sqldata;
						if ((unsigned)(str_len + sizeof(short)) <= meta_length + sizeof(short)) {
							memcpy(dest, var->sqldata, str_len + sizeof(short));
						} else {
							/* Truncate */
							*(short *)dest = (short)(meta_length);
							memcpy(dest + sizeof(short), var->sqldata + sizeof(short), meta_length);
						}
					} else {
						/* Data is raw string without length prefix (SQL_TEXT format)
						 * Need to convert to VARY format for OO API */
						short str_len = (short)var->sqllen;
						if ((unsigned)str_len > meta_length) {
							str_len = (short)meta_length; /* Truncate */
						}
						/* Write length prefix */
						*(short *)dest = str_len;
						/* Copy string data */
						memcpy(dest + sizeof(short), var->sqldata, str_len);
					}
				}
				break;

			case SQL_SHORT:
				*(short *)dest = *(short *)var->sqldata;
				break;

			case SQL_LONG:
				*(ISC_LONG *)dest = *(ISC_LONG *)var->sqldata;
				break;

			case SQL_INT64:
				*(ISC_INT64 *)dest = *(ISC_INT64 *)var->sqldata;
				break;

			case SQL_FLOAT:
				*(float *)dest = *(float *)var->sqldata;
				break;

			case SQL_DOUBLE:
				*(double *)dest = *(double *)var->sqldata;
				break;

			case SQL_TIMESTAMP:
				*(ISC_TIMESTAMP *)dest = *(ISC_TIMESTAMP *)var->sqldata;
				break;

			case SQL_TYPE_DATE:
				*(ISC_DATE *)dest = *(ISC_DATE *)var->sqldata;
				break;

			case SQL_TYPE_TIME:
				*(ISC_TIME *)dest = *(ISC_TIME *)var->sqldata;
				break;

			case SQL_BLOB:
			case SQL_ARRAY:
				*(ISC_QUAD *)dest = *(ISC_QUAD *)var->sqldata;
				break;

#ifdef SQL_BOOLEAN
			case SQL_BOOLEAN:
				*(FB_BOOLEAN *)dest = *(FB_BOOLEAN *)var->sqldata;
				break;
#endif

#if FB_API_VER >= 40
			case SQL_TIMESTAMP_TZ:
				*(ISC_TIMESTAMP_TZ *)dest = *(ISC_TIMESTAMP_TZ *)var->sqldata;
				break;

			case SQL_TIME_TZ:
				*(ISC_TIME_TZ *)dest = *(ISC_TIME_TZ *)var->sqldata;
				break;

			case SQL_INT128:
			case SQL_DEC16:
			case SQL_DEC34:
				/* Copy the raw bytes - these are fixed-size types */
				memcpy(dest, var->sqldata, meta_length);
				break;
#endif

			default:
				/* For unknown types, try raw copy based on metadata length */
				if (meta_length > 0 && (unsigned)var->sqllen <= meta_length) {
					memcpy(dest, var->sqldata, var->sqllen);
				} else {
					_php_fbird_module_error("Parameter %d: unsupported SQL type %d",
						i + 1, var->sqltype & ~1);
					return FAILURE;
				}
				break;
		}
	}

	return SUCCESS;
}
/* }}} */

int _php_fbird_bind(fbird_query *ib_query, zval *b_vars) /* {{{ */
{
	BIND_BUF *buf = ib_query->bind_buf;
	XSQLDA *sqlda = ib_query->in_sqlda;

	int i, array_cnt = 0, rv = SUCCESS;

	for (i = 0; i < sqlda->sqld; ++i) { /* bound vars */

		zval *b_var = &b_vars[i];
		XSQLVAR *var = &sqlda->sqlvar[i];

		var->sqlind = &buf[i].nullind;
		var->sqldata = (void*)&buf[i].val;

		/* check if a NULL should be inserted */
		switch (Z_TYPE_P(b_var)) {
			int force_null;

			case IS_STRING:

				force_null = 0;

				/* for these types, an empty string can be handled like a NULL value */
				switch (var->sqltype & ~1) {
					case SQL_SHORT:
					case SQL_LONG:
					case SQL_INT64:
					case SQL_FLOAT:
					case SQL_DOUBLE:
					case SQL_TIMESTAMP:
					case SQL_TYPE_DATE:
					case SQL_TYPE_TIME:
#if FB_API_VER >= 40
					case SQL_INT128:
					case SQL_DEC16:
					case SQL_DEC34:
					case SQL_TIMESTAMP_TZ:
					case SQL_TIME_TZ:
#endif
						force_null = (Z_STRLEN_P(b_var) == 0);
						break;
					default:
						break;
				}

				if (! force_null) break;
				/* fall through */

			case IS_NULL:
					buf[i].nullind = -1;

				if ((var->sqltype & ~1) == SQL_ARRAY) ++array_cnt;

				continue;
		}

		/* if we make it to this point, we must provide a value for the parameter */

		buf[i].nullind = 0;

		switch (var->sqltype & ~1) {
			struct tm t;

			case SQL_SHORT:
				{
					zend_long lval = zval_get_long(b_var);
					buf[i].val.sval = (short)lval;
				}
				continue;

			case SQL_LONG:
				{
					zend_long lval = zval_get_long(b_var);
					buf[i].val.lval = (ISC_LONG)lval;
				}
				continue;

			case SQL_INT64:
				{
					zend_long lval = zval_get_long(b_var);
					buf[i].val.i64val = (ISC_INT64)lval;
				}
				continue;

			case SQL_FLOAT:
				{
					double dval = zval_get_double(b_var);
					buf[i].val.fval = (float)dval;
				}
				continue;

			case SQL_DOUBLE:
				{
					double dval = zval_get_double(b_var);
					buf[i].val.dval = dval;
				}
				continue;

			case SQL_TIMESTAMP:
			case SQL_TYPE_DATE:
			case SQL_TYPE_TIME:
				if (Z_TYPE_P(b_var) == IS_LONG) {
					struct tm *res;
					res = php_gmtime_r(&Z_LVAL_P(b_var), &t);
					if (!res) {
						return FAILURE;
					}
				} else {
#ifdef HAVE_STRPTIME
					char *format = INI_STR("fbird.timestampformat");

					convert_to_string(b_var);

					switch (var->sqltype & ~1) {
						case SQL_TYPE_DATE:
							format = INI_STR("fbird.dateformat");
							break;
						case SQL_TYPE_TIME:
							format = INI_STR("fbird.timeformat");
							break;
						default:
							break;
					}
					if (!strptime(Z_STRVAL_P(b_var), format, &t)) {
						/* strptime() cannot handle it, so let IB have a try */
						break;
					}
#else /* ifndef HAVE_STRPTIME */
					break; /* let IB parse it as a string */
#endif
				}

				switch (var->sqltype & ~1) {
					default: /* == case SQL_TIMESTAMP */
						isc_encode_timestamp(&t, &buf[i].val.tsval);
						break;
					case SQL_TYPE_DATE:
						isc_encode_sql_date(&t, &buf[i].val.dtval);
						break;
					case SQL_TYPE_TIME:
						isc_encode_sql_time(&t, &buf[i].val.tmval);
						break;
				}
				continue;

#if FB_API_VER >= 40
			case SQL_TIMESTAMP_TZ:
			case SQL_TIME_TZ:
				/* Timezone types require Firebird 4.0+ master interface */
				if (!IBG(master_instance)) {
					_php_fbird_module_error("Parameter %d: Timezone fields require Firebird 4.0+ client library", i+1);
					rv = FAILURE;
					continue;
				}

				{
					char tz_name[64] = "GMT";  /* Default timezone */
					unsigned year = 1970, month = 1, day = 1;
					unsigned hours = 0, minutes = 0, seconds = 0, fractions = 0;

					if (Z_TYPE_P(b_var) == IS_LONG) {
						/* Unix timestamp - convert to components in UTC */
						struct tm *res;
						res = php_gmtime_r(&Z_LVAL_P(b_var), &t);
						if (!res) {
							_php_fbird_module_error("Parameter %d: Invalid timestamp value", i+1);
							rv = FAILURE;
							continue;
						}
						year = (unsigned)(t.tm_year + 1900);
						month = (unsigned)(t.tm_mon + 1);
						day = (unsigned)t.tm_mday;
						hours = (unsigned)t.tm_hour;
						minutes = (unsigned)t.tm_min;
						seconds = (unsigned)t.tm_sec;
						fractions = 0;
						/* Keep tz_name as "GMT" for unix timestamps */
					} else {
						/* String format: "YYYY-MM-DD HH:MM:SS.FFFF TZ" or "HH:MM:SS.FFFF TZ" */
						convert_to_string(b_var);
						char *str = Z_STRVAL_P(b_var);
						size_t len = Z_STRLEN_P(b_var);

						/* Try to parse timezone from end of string */
						char *tz_start = NULL;
						char *space = strrchr(str, ' ');
						if (space && space > str) {
							/* Check if what follows the last space looks like a timezone */
							char *potential_tz = space + 1;
							if (potential_tz[0] == '+' || potential_tz[0] == '-' ||
								(potential_tz[0] >= 'A' && potential_tz[0] <= 'Z') ||
								(potential_tz[0] >= 'a' && potential_tz[0] <= 'z')) {
								tz_start = potential_tz;
								size_t tz_len = strlen(tz_start);
								if (tz_len > 0 && tz_len < sizeof(tz_name)) {
									strncpy(tz_name, tz_start, sizeof(tz_name) - 1);
									tz_name[sizeof(tz_name) - 1] = '\0';
								}
							}
						}

						/* Parse date/time components using strptime */
						memset(&t, 0, sizeof(t));
						char *parse_end = NULL;

						if ((var->sqltype & ~1) == SQL_TIME_TZ) {
							/* Time only format */
							char *format = INI_STR("fbird.timeformat");
							parse_end = strptime(str, format, &t);
							hours = (unsigned)t.tm_hour;
							minutes = (unsigned)t.tm_min;
							seconds = (unsigned)t.tm_sec;
						} else {
							/* Timestamp format */
							char *format = INI_STR("fbird.timestampformat");
							parse_end = strptime(str, format, &t);
							year = (unsigned)(t.tm_year + 1900);
							month = (unsigned)(t.tm_mon + 1);
							day = (unsigned)t.tm_mday;
							hours = (unsigned)t.tm_hour;
							minutes = (unsigned)t.tm_min;
							seconds = (unsigned)t.tm_sec;
						}

						if (!parse_end) {
							/* strptime failed - let Firebird try to parse it as string */
							break;
						}
					}

					/* Encode using Firebird 4.0+ API */
					var->sqldata = (void*)&buf[i].val;
					if ((var->sqltype & ~1) == SQL_TIME_TZ) {
						if (fbu_encode_time_tz(IBG(master_instance), &buf[i].val.tmtzval,
								hours, minutes, seconds, fractions, tz_name) != 0) {
							_php_fbird_module_error("Parameter %d: Failed to encode TIME WITH TIME ZONE", i+1);
							rv = FAILURE;
							continue;
						}
					} else {
						if (fbu_encode_timestamp_tz(IBG(master_instance), &buf[i].val.tstzval,
								year, month, day, hours, minutes, seconds, fractions, tz_name) != 0) {
							_php_fbird_module_error("Parameter %d: Failed to encode TIMESTAMP WITH TIME ZONE", i+1);
							rv = FAILURE;
							continue;
						}
					}
				}
				continue;
#endif /* FB_API_VER >= 40 */

			case SQL_BLOB:

				convert_to_string(b_var);

				if (Z_STRLEN_P(b_var) != BLOB_ID_LEN ||
					!_php_fbird_string_to_quad(Z_STRVAL_P(b_var), &buf[i].val.qval)) {

					fbird_blob ib_blob = { 0 };
					ib_blob.type = BLOB_INPUT;

					if (isc_create_blob(IB_STATUS, &ib_query->link->handle.db,
							&ib_query->trans->handle.tr, &ib_blob.bl_handle.blob, &ib_blob.bl_qd)) {
						_php_fbird_error();
						return FAILURE;
					}

					if (_php_fbird_blob_add(b_var, &ib_blob) != SUCCESS) {
						return FAILURE;
					}

					if (isc_close_blob(IB_STATUS, &ib_blob.bl_handle.blob)) {
						_php_fbird_error();
						return FAILURE;
					}
					buf[i].val.qval = ib_blob.bl_qd;
				}
				continue;
#ifdef SQL_BOOLEAN
			case SQL_BOOLEAN:

				switch (Z_TYPE_P(b_var)) {
					case IS_LONG:
					case IS_DOUBLE:
					case IS_TRUE:
					case IS_FALSE:
						*(FB_BOOLEAN *)var->sqldata = zend_is_true(b_var) ? FB_TRUE : FB_FALSE;
						break;
					case IS_STRING:
					{
						zend_long lval;
						double dval;

						if ((Z_STRLEN_P(b_var) == 0)) {
							*(FB_BOOLEAN *)var->sqldata = FB_FALSE;
							break;
						}

						switch (is_numeric_string(Z_STRVAL_P(b_var), Z_STRLEN_P(b_var), &lval, &dval, 0)) {
							case IS_LONG:
								*(FB_BOOLEAN *)var->sqldata = (lval != 0) ? FB_TRUE : FB_FALSE;
								break;
							case IS_DOUBLE:
								*(FB_BOOLEAN *)var->sqldata = (dval != 0) ? FB_TRUE : FB_FALSE;
								break;
							default:
								if (!zend_binary_strncasecmp(Z_STRVAL_P(b_var), Z_STRLEN_P(b_var), "true", 4, 4)) {
									*(FB_BOOLEAN *)var->sqldata = FB_TRUE;
								} else if (!zend_binary_strncasecmp(Z_STRVAL_P(b_var), Z_STRLEN_P(b_var), "false", 5, 5)) {
									*(FB_BOOLEAN *)var->sqldata = FB_FALSE;
								} else {
									_php_fbird_module_error("Parameter %d: cannot convert string to boolean", i+1);
									rv = FAILURE;
									continue;
								}
						}
						break;
					}
					case IS_NULL:
						buf[i].nullind = -1;
						break;
					default:
						_php_fbird_module_error("Parameter %d: must be boolean", i+1);
						rv = FAILURE;
						continue;
				}
				var->sqltype = SQL_BOOLEAN;
				continue;
#endif
			case SQL_ARRAY:
				if (Z_TYPE_P(b_var) != IS_ARRAY) {
					convert_to_string(b_var);

					if (Z_STRLEN_P(b_var) != BLOB_ID_LEN ||
						!_php_fbird_string_to_quad(Z_STRVAL_P(b_var), &buf[i].val.qval)) {

						_php_fbird_module_error("Parameter %d: invalid array ID",i+1);
						rv = FAILURE;
					}
				} else {
					/* OO API Only: Store array slice via IAttachment::putSlice() */
					if (!ib_query->link || !ib_query->link->fbc_connection) {
						_php_fbird_module_error("Parameter %d: OO API connection required for array binding", i + 1);
						rv = FAILURE;
						++array_cnt;
						continue;
					}
					if (!ib_query->trans || !ib_query->trans->fbt_transaction) {
						_php_fbird_module_error("Parameter %d: OO API transaction required for array binding", i + 1);
						rv = FAILURE;
						++array_cnt;
						continue;
					}

				/* Convert the PHP array argument into a contiguous element buffer */
				void* attachment_ptr = fbc_get_attachment(ib_query->link->fbc_connection);
				void* transaction_ptr = fbt_get_handle(ib_query->trans->fbt_transaction);

				/* Get table and column names for array lookup.
				 * OO API input metadata doesn't provide relname/sqlname for anonymous params.
				 * If empty, try to parse from SQL (INSERT INTO table (col,...) VALUES (?,...)). */
				char arr_relname[32] = "";
				char arr_sqlname[32] = "";

				if (ib_query->in_sqlda->sqlvar[i].relname_length > 0) {
					strncpy(arr_relname, ib_query->in_sqlda->sqlvar[i].relname, sizeof(arr_relname) - 1);
				}
				if (ib_query->in_sqlda->sqlvar[i].sqlname_length > 0) {
					strncpy(arr_sqlname, ib_query->in_sqlda->sqlvar[i].sqlname, sizeof(arr_sqlname) - 1);
				}

				/* Parse SQL to extract table/column names if not available from metadata */
				if ((arr_relname[0] == '\0' || arr_sqlname[0] == '\0') && ib_query->query) {
					const char *sql = ib_query->query;
					const char *insert_pos, *table_start, *table_end;
					const char *cols_start, *cols_end;

					/* Skip whitespace and find INSERT INTO */
					while (*sql && (*sql == ' ' || *sql == '\t' || *sql == '\n' || *sql == '\r')) sql++;
					insert_pos = sql;

					if (strncasecmp(insert_pos, "INSERT", 6) == 0) {
						insert_pos += 6;
						while (*insert_pos && (*insert_pos == ' ' || *insert_pos == '\t' || *insert_pos == '\n')) insert_pos++;

						if (strncasecmp(insert_pos, "INTO", 4) == 0) {
							insert_pos += 4;
							while (*insert_pos && (*insert_pos == ' ' || *insert_pos == '\t' || *insert_pos == '\n')) insert_pos++;

							/* Extract table name */
							table_start = insert_pos;
							table_end = table_start;
							while (*table_end && *table_end != ' ' && *table_end != '\t' &&
							       *table_end != '\n' && *table_end != '(') {
								table_end++;
							}

							if (arr_relname[0] == '\0' && table_end > table_start) {
								size_t len = table_end - table_start;
								if (len >= sizeof(arr_relname)) len = sizeof(arr_relname) - 1;
								memcpy(arr_relname, table_start, len);
								arr_relname[len] = '\0';
							}

							/* Find column list (col1, col2, ...) */
							cols_start = strchr(table_end, '(');
							if (cols_start) {
								cols_start++; /* skip '(' */
								cols_end = strchr(cols_start, ')');
								if (cols_end && arr_sqlname[0] == '\0') {
									/* Parse column names and find column at position i */
									int col_idx = 0;
									const char *col_ptr = cols_start;

									while (col_ptr < cols_end && col_idx <= i) {
										/* Skip whitespace */
										while (col_ptr < cols_end && (*col_ptr == ' ' || *col_ptr == '\t' || *col_ptr == '\n')) col_ptr++;

										/* Find column name start/end */
										const char *col_name_start = col_ptr;
										while (col_ptr < cols_end && *col_ptr != ',' && *col_ptr != ' ' &&
										       *col_ptr != '\t' && *col_ptr != '\n') {
											col_ptr++;
										}
										const char *col_name_end = col_ptr;

										if (col_idx == i && col_name_end > col_name_start) {
											size_t len = col_name_end - col_name_start;
											if (len >= sizeof(arr_sqlname)) len = sizeof(arr_sqlname) - 1;
											memcpy(arr_sqlname, col_name_start, len);
											arr_sqlname[len] = '\0';
											break;
										}

										/* Skip to next column */
										while (col_ptr < cols_end && (*col_ptr == ' ' || *col_ptr == '\t' || *col_ptr == '\n')) col_ptr++;
										if (col_ptr < cols_end && *col_ptr == ',') {
											col_ptr++;
											col_idx++;
										}
									}
								}
							}
						}
					}
				}

				if (arr_relname[0] == '\0' || arr_sqlname[0] == '\0') {
					_php_fbird_module_error("Parameter %d: cannot determine table/column name for array binding. Use explicit INSERT INTO table (columns...) VALUES (...).", i + 1);
					rv = FAILURE;
					++array_cnt;
					continue;
				}

				/* Firebird stores identifiers UPPERCASE in system tables - convert parsed names */
				for (char *p = arr_relname; *p; p++) *p = toupper((unsigned char)*p);
				for (char *p = arr_sqlname; *p; p++) *p = toupper((unsigned char)*p);

				ISC_ARRAY_DESC ar_desc;
				/* OO API: Query array descriptor from system tables */
				if (fba_lookup_bounds(
						IBG(master_instance),
						attachment_ptr,
						transaction_ptr,
						arr_relname,
						arr_sqlname,
						&ar_desc,
						IB_STATUS
					) != 0) {
					_php_fbird_error();
					rv = FAILURE;
					++array_cnt;
					continue;
				}

					/* Compute element size and SQL type for the slice buffer */
					ISC_LONG elem_size = 0;
					int arr_el_type = SQL_TEXT; /* default for text/unknown */
					switch (ar_desc.array_desc_dtype) {
						case blr_text:
						case blr_text2:
							elem_size = (ISC_LONG)ar_desc.array_desc_length;
							arr_el_type = SQL_TEXT;
							break;
						case blr_varying:
						case blr_varying2:
							elem_size = (ISC_LONG)ar_desc.array_desc_length;
							arr_el_type = SQL_VARYING;
							break;
						case blr_short:
							elem_size = (ISC_LONG)sizeof(short);
							arr_el_type = SQL_SHORT;
							break;
						case blr_long:
							elem_size = (ISC_LONG)sizeof(ISC_LONG);
							arr_el_type = SQL_LONG;
							break;
						case blr_int64:
							elem_size = (ISC_LONG)sizeof(ISC_INT64);
							arr_el_type = SQL_INT64;
							break;
						default:
							_php_fbird_module_error("Parameter %d: unsupported array element dtype %d", i + 1, ar_desc.array_desc_dtype);
							rv = FAILURE;
							++array_cnt;
							continue;
					}

					ISC_LONG elements = 1;
					for (int d = 0; d < ar_desc.array_desc_dimensions; d++) {
						elements *= 1 + ar_desc.array_desc_bounds[d].array_bound_upper - ar_desc.array_desc_bounds[d].array_bound_lower;
					}

					ISC_LONG slice_len = elem_size * elements;
					void* array_data = ecalloc(1, (size_t)slice_len);
					if (FAILURE == _php_fbird_bind_array(b_var, (char*)array_data, (zend_ulong)slice_len, (fbird_array*)&(fbird_array){.ar_desc = ar_desc, .ar_size = slice_len, .el_type = arr_el_type, .el_size = (unsigned short)elem_size}, 0)) {
						_php_fbird_module_error("Parameter %d: failed to bind array argument", i + 1);
						efree(array_data);
						rv = FAILURE;
						++array_cnt;
						continue;
					}

					ISC_QUAD array_id = {0, 0};
					if (fba_put_slice(
							IBG(master_instance),
							attachment_ptr,
							transaction_ptr,
							&array_id,
							&ar_desc,
							array_data,
							slice_len,
							IB_STATUS
						) != 0) {
						_php_fbird_error();
						efree(array_data);
						rv = FAILURE;
						++array_cnt;
						continue;
					}

					buf[i].val.qval = array_id;
					efree(array_data);
				}
				++array_cnt;
				continue;
		} /* switch */

		/* we end up here if none of the switch cases handled the field */
		convert_to_string(b_var);
		var->sqldata = Z_STRVAL_P(b_var);
		var->sqllen	 = (ISC_SHORT)Z_STRLEN_P(b_var);
		var->sqltype = SQL_TEXT;
	} /* for */
	return rv;
}
/* }}} */

#endif /* HAVE_FIREBIRD */
