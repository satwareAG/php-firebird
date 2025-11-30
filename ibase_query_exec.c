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

#if HAVE_IBASE

#include "ext/standard/php_standard.h"
#include "php_interbase.h"
#include "php_ibase_includes.h"
#include "php_ibase_query_internal.h"
#include "firebird_utils.h"

#define ISC_LONG_MIN    INT_MIN
#define ISC_LONG_MAX    INT_MAX

/* Max identifier size for Firebird 3+ (63 chars) but we alloc more for safety */
#define MAX_IDENTIFIER_LEN 255

/* Exported for use in ibase_result.c and other files */
int le_query;

/* Forward declarations */
static int _php_ibase_bind_array(zval *val, char *buf, zend_ulong buf_size, ibase_array *array, int dim);
static int _php_ibase_set_query_info(ibase_query *ib_query);

/* Implementation of _php_ibase_set_query_info */
static int _php_ibase_set_query_info(ibase_query *ib_query) /* {{{ */
{
	char info_req[] = { isc_info_sql_stmt_type };
	char info_buf[20];
	XSQLDA sqlda;

	/* Get statement type */
	if (isc_dsql_sql_info(IB_STATUS, &ib_query->stmt.stmt, sizeof(info_req), info_req, sizeof(info_buf), info_buf)) {
		_php_ibase_error();
		return FAILURE;
	}

	if (info_buf[0] == isc_info_sql_stmt_type) {
		int len = isc_vax_integer(&info_buf[1], 2);
		ib_query->statement_type = isc_vax_integer(&info_buf[3], len);
	} else {
		ib_query->statement_type = isc_info_sql_stmt_select; /* fallback/default */
	}

	/* Get field counts via describe */
	memset(&sqlda, 0, sizeof(XSQLDA));
	sqlda.version = SQLDA_CURRENT_VERSION;
	sqlda.sqln = 0;
	sqlda.sqld = 0;

	if (isc_dsql_describe(IB_STATUS, &ib_query->stmt.stmt, SQLDA_CURRENT_VERSION, &sqlda)) {
		_php_ibase_error();
		return FAILURE;
	}
	ib_query->out_fields_count = sqlda.sqld;

	memset(&sqlda, 0, sizeof(XSQLDA));
	sqlda.version = SQLDA_CURRENT_VERSION;
	sqlda.sqln = 0;
	sqlda.sqld = 0;
	if (isc_dsql_describe_bind(IB_STATUS, &ib_query->stmt.stmt, SQLDA_CURRENT_VERSION, &sqlda)) {
		_php_ibase_error();
		return FAILURE;
	}
	ib_query->in_fields_count = sqlda.sqld;

	return SUCCESS;
}
/* }}} */

/* Helper function for safer SQLVAR data copying */
int _php_ibase_safe_copy_sqlvar_data(XSQLVAR *dest_var, const XSQLVAR *src_var, int field_index, const char *query_context) /* {{{ */
{
	/* Validate input parameters */
	if (!dest_var || !src_var) {
		_php_ibase_module_error("EXECUTE PROCEDURE: Invalid XSQLVAR pointers for field %d in query: %s",
            field_index, query_context ? query_context : "unknown");
		return FAILURE;
	}

	if (!src_var->sqldata) {
		_php_ibase_module_error("EXECUTE PROCEDURE: Source sqldata is NULL for field %d in query: %s",
            field_index, query_context ? query_context : "unknown");
		return FAILURE;
	}

	/* Verify sqltype consistency between source and destination */
	if (dest_var->sqltype != src_var->sqltype) {
		_php_ibase_module_error("EXECUTE PROCEDURE: sqltype mismatch for field %d (dest=%d, src=%d) in query: %s",
			field_index, dest_var->sqltype, src_var->sqltype, query_context ? query_context : "unknown");
		return FAILURE;
	}

	/* Allocate and copy data based on SQL type with comprehensive bounds checking */
	switch (dest_var->sqltype & ~1) {
		case SQL_TEXT:
			/* Validate field length for TEXT fields */
			if (dest_var->sqllen != src_var->sqllen) {
				_php_ibase_module_error("EXECUTE PROCEDURE: TEXT sqllen mismatch for field %d (dest=%d, src=%d) in query: %s",
					field_index, dest_var->sqllen, src_var->sqllen, query_context ? query_context : "unknown");
				return FAILURE;
			}
			if (dest_var->sqllen < 0 || dest_var->sqllen > 65535) {
				_php_ibase_module_error("EXECUTE PROCEDURE: Invalid TEXT length %d for field %d in query: %s",
					dest_var->sqllen, field_index, query_context ? query_context : "unknown");
				return FAILURE;
			}
			dest_var->sqldata = safe_emalloc(sizeof(char), dest_var->sqllen, 0);
			if (!dest_var->sqldata) {
				_php_ibase_module_error("EXECUTE PROCEDURE: Failed to allocate TEXT data for field %d in query: %s",
                    field_index, query_context ? query_context : "unknown");
				return FAILURE;
			}
			/* Use safer copy with explicit size limit */
			memcpy(dest_var->sqldata, src_var->sqldata, dest_var->sqllen);
			break;

		case SQL_VARYING:
			/* Validate field length for VARCHAR fields */
			if (dest_var->sqllen != src_var->sqllen) {
				_php_ibase_module_error("EXECUTE PROCEDURE: VARCHAR sqllen mismatch for field %d (dest=%d, src=%d) in query: %s",
					field_index, dest_var->sqllen, src_var->sqllen, query_context ? query_context : "unknown");
				return FAILURE;
			}
			if (dest_var->sqllen < 0 || dest_var->sqllen > 65535) {
				_php_ibase_module_error("EXECUTE PROCEDURE: Invalid VARCHAR length %d for field %d in query: %s",
					dest_var->sqllen, field_index, query_context ? query_context : "unknown");
				return FAILURE;
			}
			dest_var->sqldata = safe_emalloc(sizeof(char), dest_var->sqllen + sizeof(short), 0);
			if (!dest_var->sqldata) {
				_php_ibase_module_error("EXECUTE PROCEDURE: Failed to allocate VARCHAR data for field %d in query: %s",
                    field_index, query_context ? query_context : "unknown");
				return FAILURE;
			}
			/* Copy length prefix + data with bounds checking */
			size_t varchar_copy_size = dest_var->sqllen + sizeof(short);
			memcpy(dest_var->sqldata, src_var->sqldata, varchar_copy_size);
			break;

#ifdef SQL_BOOLEAN
		case SQL_BOOLEAN:
			if (src_var->sqllen != sizeof(FB_BOOLEAN)) {
				_php_ibase_module_error("EXECUTE PROCEDURE: Invalid BOOLEAN length %d for field %d", src_var->sqllen, field_index);
				return FAILURE;
			}
			dest_var->sqldata = emalloc(sizeof(FB_BOOLEAN));
			if (!dest_var->sqldata) {
				_php_ibase_module_error("EXECUTE PROCEDURE: Failed to allocate BOOLEAN data for field %d", field_index);
				return FAILURE;
			}
			/* Direct assignment for simple types (safer than memcpy for single values) */
			*(FB_BOOLEAN *)dest_var->sqldata = *(FB_BOOLEAN *)src_var->sqldata;
			break;
#endif

		case SQL_SHORT:
			if (src_var->sqllen != sizeof(short)) {
				_php_ibase_module_error("EXECUTE PROCEDURE: Invalid SHORT length %d for field %d", src_var->sqllen, field_index);
				return FAILURE;
			}
			dest_var->sqldata = emalloc(sizeof(short));
			if (!dest_var->sqldata) {
				_php_ibase_module_error("EXECUTE PROCEDURE: Failed to allocate SHORT data for field %d", field_index);
				return FAILURE;
			}
			*(short *)dest_var->sqldata = *(short *)src_var->sqldata;
			break;

		case SQL_LONG:
			if (src_var->sqllen != sizeof(ISC_LONG)) {
				_php_ibase_module_error("EXECUTE PROCEDURE: Invalid LONG length %d for field %d", src_var->sqllen, field_index);
				return FAILURE;
			}
			dest_var->sqldata = emalloc(sizeof(ISC_LONG));
			if (!dest_var->sqldata) {
				_php_ibase_module_error("EXECUTE PROCEDURE: Failed to allocate LONG data for field %d", field_index);
				return FAILURE;
			}
			*(ISC_LONG *)dest_var->sqldata = *(ISC_LONG *)src_var->sqldata;
			break;

		case SQL_FLOAT:
			if (src_var->sqllen != sizeof(float)) {
				_php_ibase_module_error("EXECUTE PROCEDURE: Invalid FLOAT length %d for field %d", src_var->sqllen, field_index);
				return FAILURE;
			}
			dest_var->sqldata = emalloc(sizeof(float));
			if (!dest_var->sqldata) {
				_php_ibase_module_error("EXECUTE PROCEDURE: Failed to allocate FLOAT data for field %d", field_index);
				return FAILURE;
			}
			*(float *)dest_var->sqldata = *(float *)src_var->sqldata;
			break;

		case SQL_DOUBLE:
			if (src_var->sqllen != sizeof(double)) {
				_php_ibase_module_error("EXECUTE PROCEDURE: Invalid DOUBLE length %d for field %d", src_var->sqllen, field_index);
				return FAILURE;
			}
			dest_var->sqldata = emalloc(sizeof(double));
			if (!dest_var->sqldata) {
				_php_ibase_module_error("EXECUTE PROCEDURE: Failed to allocate DOUBLE data for field %d", field_index);
				return FAILURE;
			}
			*(double *)dest_var->sqldata = *(double *)src_var->sqldata;
			break;

		case SQL_INT64:
			if (src_var->sqllen != sizeof(ISC_INT64)) {
				_php_ibase_module_error("EXECUTE PROCEDURE: Invalid INT64 length %d for field %d", src_var->sqllen, field_index);
				return FAILURE;
			}
			dest_var->sqldata = emalloc(sizeof(ISC_INT64));
			if (!dest_var->sqldata) {
				_php_ibase_module_error("EXECUTE PROCEDURE: Failed to allocate INT64 data for field %d", field_index);
				return FAILURE;
			}
			*(ISC_INT64 *)dest_var->sqldata = *(ISC_INT64 *)src_var->sqldata;
			break;

		case SQL_TIMESTAMP:
			if (src_var->sqllen != sizeof(ISC_TIMESTAMP)) {
				_php_ibase_module_error("EXECUTE PROCEDURE: Invalid TIMESTAMP length %d for field %d", src_var->sqllen, field_index);
				return FAILURE;
			}
			dest_var->sqldata = emalloc(sizeof(ISC_TIMESTAMP));
			if (!dest_var->sqldata) {
				_php_ibase_module_error("EXECUTE PROCEDURE: Failed to allocate TIMESTAMP data for field %d", field_index);
				return FAILURE;
			}
			*(ISC_TIMESTAMP *)dest_var->sqldata = *(ISC_TIMESTAMP *)src_var->sqldata;
			break;

		case SQL_TYPE_DATE:
			if (src_var->sqllen != sizeof(ISC_DATE)) {
				_php_ibase_module_error("EXECUTE PROCEDURE: Invalid DATE length %d for field %d", src_var->sqllen, field_index);
				return FAILURE;
			}
			dest_var->sqldata = emalloc(sizeof(ISC_DATE));
			if (!dest_var->sqldata) {
				_php_ibase_module_error("EXECUTE PROCEDURE: Failed to allocate DATE data for field %d", field_index);
				return FAILURE;
			}
			*(ISC_DATE *)dest_var->sqldata = *(ISC_DATE *)src_var->sqldata;
			break;

		case SQL_TYPE_TIME:
			if (src_var->sqllen != sizeof(ISC_TIME)) {
				_php_ibase_module_error("EXECUTE PROCEDURE: Invalid TIME length %d for field %d", src_var->sqllen, field_index);
				return FAILURE;
			}
			dest_var->sqldata = emalloc(sizeof(ISC_TIME));
			if (!dest_var->sqldata) {
				_php_ibase_module_error("EXECUTE PROCEDURE: Failed to allocate TIME data for field %d", field_index);
				return FAILURE;
			}
			*(ISC_TIME *)dest_var->sqldata = *(ISC_TIME *)src_var->sqldata;
			break;

		case SQL_BLOB:
		case SQL_ARRAY:
			if (src_var->sqllen != sizeof(ISC_QUAD)) {
				_php_ibase_module_error("EXECUTE PROCEDURE: Invalid QUAD length %d for field %d", src_var->sqllen, field_index);
				return FAILURE;
			}
			dest_var->sqldata = emalloc(sizeof(ISC_QUAD));
			if (!dest_var->sqldata) {
				_php_ibase_module_error("EXECUTE PROCEDURE: Failed to allocate QUAD data for field %d", field_index);
				return FAILURE;
			}
			*(ISC_QUAD *)dest_var->sqldata = *(ISC_QUAD *)src_var->sqldata;
			break;

#if FB_API_VER >= 40
		case SQL_TIMESTAMP_TZ:
			if (src_var->sqllen != sizeof(ISC_TIMESTAMP_TZ)) {
				_php_ibase_module_error("EXECUTE PROCEDURE: Invalid TIMESTAMP_TZ length %d for field %d", src_var->sqllen, field_index);
				return FAILURE;
			}
			dest_var->sqldata = emalloc(sizeof(ISC_TIMESTAMP_TZ));
			if (!dest_var->sqldata) {
				_php_ibase_module_error("EXECUTE PROCEDURE: Failed to allocate TIMESTAMP_TZ data for field %d", field_index);
				return FAILURE;
			}
			*(ISC_TIMESTAMP_TZ *)dest_var->sqldata = *(ISC_TIMESTAMP_TZ *)src_var->sqldata;
			break;

		case SQL_TIME_TZ:
			if (src_var->sqllen != sizeof(ISC_TIME_TZ)) {
				_php_ibase_module_error("EXECUTE PROCEDURE: Invalid TIME_TZ length %d for field %d", src_var->sqllen, field_index);
				return FAILURE;
			}
			dest_var->sqldata = emalloc(sizeof(ISC_TIME_TZ));
			if (!dest_var->sqldata) {
				_php_ibase_module_error("EXECUTE PROCEDURE: Failed to allocate TIME_TZ data for field %d", field_index);
				return FAILURE;
			}
			*(ISC_TIME_TZ *)dest_var->sqldata = *(ISC_TIME_TZ *)src_var->sqldata;
			break;
#endif

		default:
			_php_ibase_module_error("EXECUTE PROCEDURE: Unhandled sqltype %d for field %d",
				dest_var->sqltype & ~1, field_index);
			return FAILURE;
	}

	return SUCCESS;
}
/* }}} */

static int _php_ibase_bind(ibase_query *ib_query, zval *b_vars) /* {{{ */
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
				}

				if (! force_null) break;

			case IS_NULL:
					buf[i].nullind = -1;

				if ((var->sqltype & ~1) == SQL_ARRAY) ++array_cnt;

				continue;
		}

		/* if we make it to this point, we must provide a value for the parameter */

		buf[i].nullind = 0;

		switch (var->sqltype & ~1) {
			struct tm t;

			case SQL_TIMESTAMP:
			// TODO: case SQL_TIMESTAMP_TZ:
			// TODO: case SQL_TIME_TZ:
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
					char *format = INI_STR("ibase.timestampformat");

					convert_to_string(b_var);

					switch (var->sqltype & ~1) {
						case SQL_TYPE_DATE:
							format = INI_STR("ibase.dateformat");
							break;
						case SQL_TYPE_TIME:
						// TODO: case SQL_TIME_TZ:
							format = INI_STR("ibase.timeformat");
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
					// TODO: case SQL_TIME_TZ:
						isc_encode_sql_time(&t, &buf[i].val.tmval);
						break;
				}
				continue;

			case SQL_BLOB:

				convert_to_string(b_var);

				if (Z_STRLEN_P(b_var) != BLOB_ID_LEN ||
					!_php_ibase_string_to_quad(Z_STRVAL_P(b_var), &buf[i].val.qval)) {

					ibase_blob ib_blob = { 0 };
					ib_blob.type = BLOB_INPUT;

					if (isc_create_blob(IB_STATUS, &ib_query->link->handle.db,
							&ib_query->trans->handle.tr, &ib_blob.bl_handle.blob, &ib_blob.bl_qd)) {
						_php_ibase_error();
						return FAILURE;
					}

					if (_php_ibase_blob_add(b_var, &ib_blob) != SUCCESS) {
						return FAILURE;
					}

					if (isc_close_blob(IB_STATUS, &ib_blob.bl_handle.blob)) {
						_php_ibase_error();
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
									_php_ibase_module_error("Parameter %d: cannot convert string to boolean", i+1);
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
						_php_ibase_module_error("Parameter %d: must be boolean", i+1);
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
						!_php_ibase_string_to_quad(Z_STRVAL_P(b_var), &buf[i].val.qval)) {

						_php_ibase_module_error("Parameter %d: invalid array ID",i+1);
						rv = FAILURE;
					}
			} else {
				/* convert the array data into something IB can understand */
				ibase_array *ar = &ib_query->in_array[array_cnt];
				void *array_data = ecalloc(1, ar->ar_size);
					ISC_QUAD array_id = { 0, 0 };

                    if (FAILURE == _php_ibase_bind_array(b_var, array_data, ar->ar_size,
							ar, 0)) {
						_php_ibase_module_error("Parameter %d: failed to bind array argument", i+1);
						efree(array_data);
						rv = FAILURE;
						continue;
					}

                    /* FIX: Use temporary ISC_LONG for slice length to avoid pointer type mismatch on 64-bit systems */
                    ISC_LONG slice_len = (ISC_LONG)ar->ar_size;

					if (isc_array_put_slice(IB_STATUS, &ib_query->link->handle.db, &ib_query->trans->handle.tr,
							&array_id, &ar->ar_desc, array_data, &slice_len)) {
						_php_ibase_error();
						efree(array_data);
						return FAILURE;
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

void _php_ibase_alloc_xsqlda_vars(XSQLDA *sqlda, ISC_SHORT *nullinds) /* {{{ */
{
	int i;
	XSQLVAR *var;

	if (sqlda) {
		var = sqlda->sqlvar;
		for (i = 0; i < sqlda->sqld; i++, var++) {
			var->sqlind = &nullinds[i];

            /* Allocate sqldata buffer based on sqltype and sqllen */
            size_t code_size = 0;
            switch (var->sqltype & ~1) {
                case SQL_VARYING:
                    code_size = var->sqllen + sizeof(short);
                    break;
                case SQL_TEXT:
                    code_size = var->sqllen;
                    break;
                case SQL_ARRAY:
                case SQL_BLOB:
                    code_size = sizeof(ISC_QUAD);
                    break;
                default:
                    /* For fixed-size types (INTEGER, FLOAT, DATE, TIMESTAMP, BOOLEAN, etc.),
                       sqllen is reliable size. */
                    code_size = var->sqllen;
                    break;
            }

            if (code_size > 0) {
                /* Use ecalloc to zero-initialize the buffer to prevent garbage data */
                var->sqldata = ecalloc(1, code_size);
            } else {
                var->sqldata = NULL;
            }
		}
	}
}
/* }}} */

static void _php_ibase_free_xsqlda(XSQLDA *sqlda) /* {{{ */
{
	int i;
	XSQLVAR *var;

	IBDEBUG("Free XSQLDA?");
	if (sqlda) {
		IBDEBUG("Freeing XSQLDA...");
		var = sqlda->sqlvar;
		for (i = 0; i < sqlda->sqld; i++, var++) {
			efree(var->sqldata);
		}
		efree(sqlda);
	}
}
/* }}} */

static void _php_ibase_free_query(ibase_query *ib_query) /* {{{ */
{
	IBDEBUG("Freeing query...");

	if(ib_query->in_nullind)efree(ib_query->in_nullind);
	if(ib_query->out_nullind)efree(ib_query->out_nullind);
	if(ib_query->bind_buf)efree(ib_query->bind_buf);
	if(ib_query->in_sqlda)efree(ib_query->in_sqlda); // Note to myself: no need for _php_ibase_free_xsqlda()
	if(ib_query->out_sqlda)_php_ibase_free_xsqlda(ib_query->out_sqlda);
	if(ib_query->in_array)efree(ib_query->in_array);
	if(ib_query->out_array)efree(ib_query->out_array);
	if(ib_query->query)efree(ib_query->query);
	if(ib_query->ht_aliases)zend_array_destroy(ib_query->ht_aliases);
	if(ib_query->ht_ind)zend_array_destroy(ib_query->ht_ind);

	efree(ib_query);
}
/* }}} */

static void php_ibase_free_query_rsrc(zend_resource *rsrc) /* {{{ */
{
    ibase_query *ib_query = (ibase_query *)rsrc->ptr;

    if (ib_query != NULL) {
        IBDEBUG("Preparing to free query by dtor...");

        /* If this is a child result, unlink it from the parent's list to prevent
         * use-after-free if the parent is subsequently freed.
         * Note: If we are being freed BY the parent (in the loop below), parent will
         * have already set ib_query->parent = NULL, so this block won't run. */
        if (ib_query->parent) {
            ibase_query **curr = &ib_query->parent->child_head;
            while (*curr) {
                if (*curr == ib_query) {
                    *curr = ib_query->child_next;
                    break;
                }
                curr = &(*curr)->child_next;
            }
            /* Do NOT call zend_list_free on parent - the parent query resource
             * should remain valid for subsequent ibase_execute() calls.
             * The parent's lifetime is controlled by the user, not by child results. */
        }

        /* Invalidate and free any dependent child result resources first so that
         * further use of those results triggers a TypeError as expected by tests. */
        ibase_query *child = ib_query->child_head;
        while (child) {
            ibase_query *next = child->child_next;
            /* Break the back-link to avoid cascading frees */
            child->parent = NULL;
            if (child->res) {
                /* Close child resource which marks it invalid for Zend */
                zend_list_close(child->res);
                child->res = NULL;
            }
            child = next;
        }
        /* Ensure any open cursor/statement is properly closed on the server
         * to avoid -502 (Attempt to reopen an open cursor) on subsequent uses. */
        if (ib_query->stmt.stmt) {
            /* Close open cursor if needed */
            if (ib_query->is_open) {
                IBDEBUG("Closing open cursor in dtor");
                (void) isc_dsql_free_statement(IB_STATUS, &ib_query->stmt.stmt, DSQL_close);
                ib_query->is_open = 0;
                ib_query->has_more_rows = 0;
                /* If this is a child result that reused the parent's statement handle,
                 * mirror the cursor state reset to the parent to avoid double-close
                 * warnings on the next ibase_execute(). */
                if (ib_query->parent) {
                    ib_query->parent->is_open = 0;
                    ib_query->parent->has_more_rows = 0;
                }
            }
            /* Drop the statement handle only if this resource OWNS it.
             * Result clones created for SELECT reuse parent's handle and must NOT drop it. */
            if (ib_query->owns_stmt_handle) {
                (void) isc_dsql_free_statement(IB_STATUS, &ib_query->stmt.stmt, DSQL_drop);
            }
        }
        _php_ibase_free_query(ib_query);
    }
}
/* }}} */

void php_ibase_query_minit(INIT_FUNC_ARGS) /* {{{ */
{
	(void)type;
	le_query = zend_register_list_destructors_ex(php_ibase_free_query_rsrc, NULL,
		LE_QUERY, module_number);
}
/* }}} */

static int _php_ibase_alloc_array(ibase_array **ib_arrayp, XSQLDA *sqlda, /* {{{ */
	fb_safe_handle link, fb_safe_handle trans, unsigned short *array_cnt)
{
	unsigned short i, n;
	ibase_array *ar;
	/* first check if we have any arrays at all */
	for (i = *array_cnt = 0; i < sqlda->sqld; ++i) {
		if ((sqlda->sqlvar[i].sqltype & ~1) == SQL_ARRAY) {
			++*array_cnt;
		}
	}
	if (! *array_cnt) return SUCCESS;

	ar = ecalloc(*array_cnt, sizeof(ibase_array));

	for (i = n = 0; i < sqlda->sqld; ++i) {
		unsigned short dim;
		zend_ulong ar_size = 1;
		XSQLVAR *var = &sqlda->sqlvar[i];

		if ((var->sqltype & ~1) != SQL_ARRAY) {
			 continue;
		}

		ibase_array *a = &ar[n++];
		ISC_ARRAY_DESC *ar_desc = &a->ar_desc;

        /* Fix stack smashing: Copy names to local HEAP buffers to ensure
         * safe access by isc_array_lookup_bounds and avoid stack corruption. */
        /* Increased buffer size for metadata names to support future expansion
         * and avoid truncation warnings */
        char *rname = ecalloc(1, MAX_IDENTIFIER_LEN + 1);
        char *sname = ecalloc(1, MAX_IDENTIFIER_LEN + 1);

		if (!rname || !sname) {
			_php_ibase_module_error("Failed to allocate memory for array names");
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
			_php_ibase_error();
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
			// These are converted to VARCHAR via isc_dpb_set_bind tag at connect
			// blr_dec64
			// blr_dec128
			// blr_int128
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
				 * We use SQL_VARYING to explicitly handle the length prefix.
				 * This ensures proper binary layout (short length + data) is generated
				 * in _php_ibase_bind_array, preventing data corruption or offset errors
				 * that occur if we treat it as SQL_TEXT but allocate extra space.
				 */
				a->el_type = SQL_VARYING;
				a->el_size = ar_desc->array_desc_length + sizeof(short);
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
				_php_ibase_module_error("Unsupported array type %d in relation '%s' column '%s'",
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
             _php_ibase_module_error("Array size exceeds system limits");
             efree(ar);
             return FAILURE;
        }
		a->ar_size = a->el_size * ar_size;
	} /* for column */
	*ib_arrayp = ar;
	return SUCCESS;
}
/* }}} */

/* allocate and prepare query */
static int _php_ibase_prepare(ibase_query **new_query, ibase_db_link *link, /* {{{ */
    ibase_trans *trans, zend_resource *trans_res, char *query)
{
	/* Return FAILURE, if querystring is empty */
	if (*query == '\0') {
		php_error_docref(NULL, E_WARNING, "Querystring empty.");
		return FAILURE;
	}

 ibase_query *ib_query = ecalloc(1, sizeof(ibase_query));
 /* Ensure linkage fields are initialized explicitly for clarity */
 ib_query->parent = NULL;
 ib_query->child_head = NULL;
 ib_query->child_next = NULL;

	ib_query->res = zend_register_resource(ib_query, le_query);
	ib_query->link = link;
	ib_query->trans = trans;
	ib_query->trans_res = trans_res;
 ib_query->dialect = link->dialect;
 ib_query->query = estrdup(query);
 /* This prepared query owns the statement handle and is responsible for
  * dropping it in the resource destructor. */
 ib_query->owns_stmt_handle = 1;

	if (isc_dsql_allocate_statement(IB_STATUS, &link->handle.db, &ib_query->stmt.stmt)) {
		_php_ibase_error();
		goto _php_ibase_alloc_query_error;
	}

	if (isc_dsql_prepare(IB_STATUS, &ib_query->trans->handle.tr, &ib_query->stmt.stmt,
			0, query, link->dialect, NULL)) {
		IBDEBUG("isc_dsql_prepare() failed\n");
		_php_ibase_error();
		goto _php_ibase_alloc_query_error;
	}

	if(_php_ibase_set_query_info(ib_query)){
		goto _php_ibase_alloc_query_error;
	}

	if(ib_query->out_fields_count) {
		ib_query->out_sqlda = (XSQLDA *) emalloc(XSQLDA_LENGTH(ib_query->out_fields_count));
		ib_query->out_sqlda->sqln = ib_query->out_fields_count;
		ib_query->out_sqlda->version = SQLDA_CURRENT_VERSION;

  if (isc_dsql_describe(IB_STATUS, &ib_query->stmt.stmt, SQLDA_CURRENT_VERSION, ib_query->out_sqlda)) {
			IBDEBUG("isc_dsql_describe() failed\n");
			_php_ibase_error();
			goto _php_ibase_alloc_query_error;
		}

		/* assert(ib_query->out_sqlda->sqln == ib_query->out_sqlda->sqld); */
		/* assert(ib_query->out_sqlda->sqld == ib_query->out_fields_count); */

		ib_query->out_nullind = safe_emalloc(sizeof(*ib_query->out_nullind), ib_query->out_sqlda->sqld, 0);
		_php_ibase_alloc_xsqlda_vars(ib_query->out_sqlda, ib_query->out_nullind);
		if (FAILURE == _php_ibase_alloc_array(&ib_query->out_array, ib_query->out_sqlda,
			link->handle, trans->handle, &ib_query->out_array_cnt)) {
			goto _php_ibase_alloc_query_error;
		}
	}

	if(ib_query->in_fields_count) {
		ib_query->in_sqlda = emalloc(XSQLDA_LENGTH(ib_query->in_fields_count));
		ib_query->in_sqlda->sqln = ib_query->in_fields_count;
		ib_query->in_sqlda->version = SQLDA_CURRENT_VERSION;

		if (isc_dsql_describe_bind(IB_STATUS, &ib_query->stmt.stmt, SQLDA_CURRENT_VERSION, ib_query->in_sqlda)) {
			IBDEBUG("isc_dsql_describe_bind() failed\n");
			_php_ibase_error();
			goto _php_ibase_alloc_query_error;
		}

		assert(ib_query->in_sqlda->sqln == ib_query->in_sqlda->sqld);
		assert(ib_query->in_sqlda->sqld == ib_query->in_fields_count);

		ib_query->bind_buf = safe_emalloc(sizeof(BIND_BUF), ib_query->in_sqlda->sqld, 0);
		ib_query->in_nullind = safe_emalloc(sizeof(*ib_query->in_nullind), ib_query->in_sqlda->sqld, 0);
		if (FAILURE == _php_ibase_alloc_array(&ib_query->in_array, ib_query->in_sqlda,
			link->handle, trans->handle, &ib_query->in_array_cnt)) {
			goto _php_ibase_alloc_query_error;
		}
	}

 *new_query = ib_query;

	return SUCCESS;

_php_ibase_alloc_query_error:
	zend_list_delete(ib_query->res);

	return FAILURE;
}
/* }}} */

static int _php_ibase_bind_array(zval *val, char *buf, zend_ulong buf_size, /* {{{ */
	ibase_array *array, int dim)
{
	zval null_val, *pnull_val = &null_val;
	int u_bound = array->ar_desc.array_desc_bounds[dim].array_bound_upper,
		l_bound = array->ar_desc.array_desc_bounds[dim].array_bound_lower,
		dim_len = 1 + u_bound - l_bound;

	ZVAL_NULL(pnull_val);

	if (dim < array->ar_desc.array_desc_dimensions) {
		zend_ulong slice_size = buf_size / dim_len;
		unsigned short i;
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

			if (_php_ibase_bind_array(subval, buf, slice_size, array, dim+1) == FAILURE)
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

			/* no coercion for array types */
			double l;

			convert_to_double(val);

			if (Z_DVAL_P(val) > 0) {
				l = Z_DVAL_P(val) * pow(10, -array->ar_desc.array_desc_scale) + .5;
			} else {
				l = Z_DVAL_P(val) * pow(10, -array->ar_desc.array_desc_scale) - .5;
			}

			switch (array->el_type) {
				case SQL_SHORT:
					if (l > SHRT_MAX || l < SHRT_MIN) {
						_php_ibase_module_error("Array parameter exceeds field width");
						return FAILURE;
					}
					*(short*) buf = (short) l;
					break;
				case SQL_LONG:
					if (l > ISC_LONG_MAX || l < ISC_LONG_MIN) {
						_php_ibase_module_error("Array parameter exceeds field width");
						return FAILURE;
					}
					*(ISC_LONG*) buf = (ISC_LONG) l;
					break;
				case SQL_INT64:
					{
						long double l;

						convert_to_string(val);

						if (!sscanf(Z_STRVAL_P(val), "%Lf", &l)) {
							_php_ibase_module_error("Cannot convert '%s' to long double",
								 Z_STRVAL_P(val));
							return FAILURE;
						}

						if (l > 0) {
							*(ISC_INT64 *) buf = (ISC_INT64) (l * pow(10,
								-array->ar_desc.array_desc_scale) + .5);
						} else {
							*(ISC_INT64 *) buf = (ISC_INT64) (l * pow(10,
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
#if (SIZEOF_ZEND_LONG < 8)
				ISC_INT64 l;
#endif

				case SQL_SHORT:
					convert_to_long(val);
					if (Z_LVAL_P(val) > SHRT_MAX || Z_LVAL_P(val) < SHRT_MIN) {
						_php_ibase_module_error("Array parameter exceeds field width");
						return FAILURE;
					}
					*(short *) buf = (short) Z_LVAL_P(val);
					break;
				case SQL_LONG:
					convert_to_long(val);
#if (SIZEOF_ZEND_LONG > 4)
					if (Z_LVAL_P(val) > ISC_LONG_MAX || Z_LVAL_P(val) < ISC_LONG_MIN) {
						_php_ibase_module_error("Array parameter exceeds field width");
						return FAILURE;
					}
#endif
					*(ISC_LONG *) buf = (ISC_LONG) Z_LVAL_P(val);
					break;
				case SQL_INT64:
#if (SIZEOF_ZEND_LONG >= 8)
					convert_to_long(val);
					*(zend_long *) buf = Z_LVAL_P(val);
#else
					convert_to_string(val);
					if (!sscanf(Z_STRVAL_P(val), "%" LL_MASK "d", &l)) {
						_php_ibase_module_error("Cannot convert '%s' to long integer",
							 Z_STRVAL_P(val));
						return FAILURE;
					} else {
						*(ISC_INT64 *) buf = l;
					}
#endif
					break;
				case SQL_FLOAT:
					convert_to_double(val);
					*(float*) buf = (float) Z_DVAL_P(val);
					break;
#ifdef SQL_BOOLEAN
				case SQL_BOOLEAN:
					convert_to_boolean(val);
					// On Windows error unresolved symbol Z_BVAL_P is thrown, so we use Z_LVAL_P
					*(FB_BOOLEAN*) buf = Z_LVAL_P(val);
					break;
#endif
				case SQL_DOUBLE:
					convert_to_double(val);
					*(double*) buf = Z_DVAL_P(val);
					break;
				case SQL_TIMESTAMP:
				// TODO: case SQL_TIMESTAMP_TZ:
					convert_to_string(val);
#ifdef HAVE_STRPTIME
					strptime(Z_STRVAL_P(val), INI_STR("ibase.timestampformat"), &t);
#else
					n = sscanf(Z_STRVAL_P(val), "%d%*[/]%d%*[/]%d %d%*[:]%d%*[:]%d",
						&t.tm_mon, &t.tm_mday, &t.tm_year, &t.tm_hour, &t.tm_min, &t.tm_sec);

					if (n != 3 && n != 6) {
						_php_ibase_module_error("Invalid date/time format (expected 3 or 6 fields, got %d."
							" Use format 'm/d/Y H:i:s'. You gave '%s')", n, Z_STRVAL_P(val));
						return FAILURE;
					}
					t.tm_year -= 1900;
					t.tm_mon--;
#endif
					isc_encode_timestamp(&t, (ISC_TIMESTAMP * ) buf);
					break;
				case SQL_TYPE_DATE:
					convert_to_string(val);
#ifdef HAVE_STRPTIME
					strptime(Z_STRVAL_P(val), INI_STR("ibase.dateformat"), &t);
#else
					n = sscanf(Z_STRVAL_P(val), "%d%*[/]%d%*[/]%d", &t.tm_mon, &t.tm_mday, &t.tm_year);

					if (n != 3) {
						_php_ibase_module_error("Invalid date format (expected 3 fields, got %d. "
							"Use format 'm/d/Y' You gave '%s')", n, Z_STRVAL_P(val));
						return FAILURE;
					}
					t.tm_year -= 1900;
					t.tm_mon--;
#endif
					isc_encode_sql_date(&t, (ISC_DATE *) buf);
					break;
				case SQL_TYPE_TIME:
				// TODO: case SQL_TIME_TZ:
					convert_to_string(val);
#ifdef HAVE_STRPTIME
					strptime(Z_STRVAL_P(val), INI_STR("ibase.timeformat"), &t);
#else
					n = sscanf(Z_STRVAL_P(val), "%d%*[:]%d%*[:]%d", &t.tm_hour, &t.tm_min, &t.tm_sec);

					if (n != 3) {
						_php_ibase_module_error("Invalid time format (expected 3 fields, got %d. "
							"Use format 'H:i:s'. You gave '%s')", n, Z_STRVAL_P(val));
						return FAILURE;
					}
#endif
					isc_encode_sql_time(&t, (ISC_TIME *) buf);
					break;
				case SQL_VARYING:
					{
						convert_to_string(val);
						size_t str_len = Z_STRLEN_P(val);
						size_t max_len = buf_size - sizeof(short);
						if (str_len > max_len) {
							str_len = max_len;
						}
						*(short *)buf = (short)str_len;
						if (str_len > 0) {
							memcpy(buf + sizeof(short), Z_STRVAL_P(val), str_len);
						}
					}
					break;
				default:
					convert_to_string(val);
					strlcpy(buf, Z_STRVAL_P(val), buf_size);
			}
		}
	}
	return SUCCESS;
}
/* }}} */

static int _php_ibase_exec(INTERNAL_FUNCTION_PARAMETERS, ibase_query *ib_query, zval *args, int bind_n) /* {{{ */
{
	int i, rv = FAILURE;
	static char info_count[] = { isc_info_sql_records };
	char result[512];
	ISC_STATUS isc_result;
	int argc = ib_query->in_fields_count;

	(void)execute_data;

	RESET_ERRMSG;
	RETVAL_FALSE;

	/* Enhanced parameter validation BEFORE Firebird API calls */
	if (bind_n < 0 || argc < 0) {
		php_error_docref(NULL, E_WARNING, "Invalid parameter count: bind_n=%d, argc=%d", bind_n, argc);
		return FAILURE;
	}

	if (bind_n != argc) {
		php_error_docref(NULL, (bind_n < argc) ? E_WARNING : E_NOTICE,
			"Statement expects %d arguments, %d given", argc, bind_n);

		if (bind_n < argc) {
			return FAILURE;
		}
	}

 /* Cursor lifecycle management: before any re-execution, close an open cursor
  * unconditionally to match legacy semantics and avoid -502 reopen errors. */
 if (ib_query->statement_type != isc_info_sql_stmt_exec_procedure && ib_query->is_open) {
     IBDEBUG("Closing open cursor before re-execution");
     /* Be tolerant: silently ignore ALL errors when attempting to close the cursor
      * before re-execution. The cursor may already have been closed by various means
      * (ibase_free_result, transaction commit, EOF reached, etc.) - this is expected
      * and should not generate warnings. We unconditionally reset the is_open flag. */
     (void) isc_dsql_free_statement(IB_STATUS, &ib_query->stmt.stmt, DSQL_close);
     ib_query->is_open = 0;
     ib_query->has_more_rows = 0;
 }

	for (i = 0; i < argc; ++i) {
		SEPARATE_ZVAL(&args[i]);
	}

	switch (ib_query->statement_type) {
		fb_safe_handle tr;
		ibase_tr_list **l;
		ibase_trans *trans;

		case isc_info_sql_stmt_start_trans:

			/* a SET TRANSACTION statement should be executed with a NULL trans handle */
			tr.ptr = NULL;

			if (isc_dsql_execute_immediate(IB_STATUS, &ib_query->link->handle.db, &tr.tr, 0,
					ib_query->query, ib_query->dialect, NULL)) {
				_php_ibase_error();
				goto _php_ibase_ex_error;
			}

			trans = (ibase_trans *) emalloc(sizeof(ibase_trans));
			trans->handle = tr;
			trans->link_cnt = 1;
			trans->affected_rows = 0;
			trans->db_link[0] = ib_query->link;

			if (ib_query->link->tr_list == NULL) {
				ib_query->link->tr_list = (ibase_tr_list *) emalloc(sizeof(ibase_tr_list));
				ib_query->link->tr_list->trans = NULL;
				ib_query->link->tr_list->next = NULL;
			}

			/* link the transaction into the connection-transaction list */
			for (l = &ib_query->link->tr_list; *l != NULL; l = &(*l)->next);
			*l = (ibase_tr_list *) emalloc(sizeof(ibase_tr_list));
			(*l)->trans = trans;
			(*l)->next = NULL;

			RETVAL_RES(zend_register_resource(trans, le_trans));
			Z_TRY_ADDREF_P(return_value);

			return SUCCESS;

		case isc_info_sql_stmt_commit:
		case isc_info_sql_stmt_rollback:

			if (isc_dsql_execute_immediate(IB_STATUS, &ib_query->link->handle.db,
					&ib_query->trans->handle.tr, 0, ib_query->query, ib_query->dialect, NULL)) {
				_php_ibase_error();
				goto _php_ibase_ex_error;
			}

			if (ib_query->trans->handle.tr == 0 && ib_query->trans_res != NULL) {
				/* transaction was released by the query and was a registered resource,
				   so we have to release it */
				zend_list_delete(ib_query->trans_res);
				ib_query->trans_res = NULL;
			}

			RETVAL_TRUE;

			return SUCCESS;

		default:
			RETVAL_FALSE;
	}

	if (ib_query->in_fields_count) { /* has placeholders */
		IBDEBUG("Query wants XSQLDA for input");
		if (_php_ibase_bind(ib_query, args) == FAILURE) {
			IBDEBUG("Could not bind input XSQLDA");
			goto _php_ibase_ex_error;
		}
	}

    /* Execute the statement. For SELECT, this opens the cursor on ib_query->stmt. */
    if (ib_query->statement_type == isc_info_sql_stmt_exec_procedure ||
               ((ib_query->statement_type == isc_info_sql_stmt_insert ||
                 ib_query->statement_type == isc_info_sql_stmt_update ||
                 ib_query->statement_type == isc_info_sql_stmt_delete) &&
                 ib_query->out_sqlda)) {
        /* Use execute2 when output variables are expected (EXECUTE PROCEDURE
         * and DML ... RETURNING). */
        isc_result = isc_dsql_execute2(IB_STATUS, &ib_query->trans->handle.tr,
            &ib_query->stmt.stmt, SQLDA_CURRENT_VERSION, ib_query->in_sqlda, ib_query->out_sqlda);
    } else {
        /* SELECT and DML without RETURNING */
        isc_result = isc_dsql_execute(IB_STATUS, &ib_query->trans->handle.tr,
            &ib_query->stmt.stmt, SQLDA_CURRENT_VERSION, ib_query->in_sqlda);
    }

    if (isc_result) {
        IBDEBUG("Could not execute query");
        _php_ibase_error();
        goto _php_ibase_ex_error;
    }

    ib_query->trans->affected_rows = 0;

    /* For SELECT statements, mark cursor state as open with rows pending. */
    if (ib_query->statement_type == isc_info_sql_stmt_select && ib_query->out_sqlda) {
        ib_query->is_open = 1;
        ib_query->has_more_rows = 1;
    }

	/* Handle result sets for SELECT, EXECUTE PROCEDURE, and DML with RETURNING clauses */
	if (ib_query->out_sqlda) { /* output variables in select, select for update, or RETURNING */

		/* For EXECUTE PROCEDURE and INSERT/UPDATE/DELETE...RETURNING, create independent result resources to avoid
		 * shared state issues where each execution overwrites the previous result.
		 */
  if (ib_query->statement_type == isc_info_sql_stmt_exec_procedure ||
      ib_query->statement_type == isc_info_sql_stmt_insert ||
      ib_query->statement_type == isc_info_sql_stmt_update ||
      ib_query->statement_type == isc_info_sql_stmt_delete) {
			/* Create a new query structure for this specific result */
			ibase_query *result_query = ecalloc(1, sizeof(ibase_query));

			/* Initialize error cleanup flag */
			int cleanup_needed = 1;

			/* Copy essential fields from the original query */
			result_query->link = ib_query->link;
			result_query->trans = ib_query->trans;
			result_query->trans_res = ib_query->trans_res;
   result_query->dialect = ib_query->dialect;
   result_query->statement_type = ib_query->statement_type;
   result_query->out_fields_count = ib_query->out_fields_count;
   result_query->was_result_once = 1;

   /* Reuse the original statement handle for metadata operations.
    * This is safe for EXECUTE PROCEDURE and DML RETURNING because
    * there is no open cursor to conflict with, and it enables
    * alias resolution via the newer Firebird API which requires
    * a valid statement handle. */
   result_query->stmt = ib_query->stmt;
   /* Keep a copy of SQL text for symmetry with SELECT path and
    * potential debug/logging uses in helper routines. */
   if (ib_query->query) {
       result_query->query = estrdup(ib_query->query);
   }

			/* Validate source SQLDA before processing */
			if (ib_query->out_sqlda && ib_query->out_fields_count > 0) {
				/* Validate SQLDA structure integrity */
				if (ib_query->out_sqlda->sqln != ib_query->out_fields_count ||
				    ib_query->out_sqlda->sqld != ib_query->out_fields_count) {
					_php_ibase_module_error("EXECUTE PROCEDURE: Invalid SQLDA structure - sqln=%d, sqld=%d, expected=%d",
						ib_query->out_sqlda->sqln, ib_query->out_sqlda->sqld, ib_query->out_fields_count);
					goto cleanup_result_query;
				}

				/* Allocate SQLDA structure with bounds checking */
				size_t sqlda_size = XSQLDA_LENGTH(ib_query->out_fields_count);
				if (sqlda_size < sizeof(XSQLDA) || ib_query->out_fields_count > 32767) {
					_php_ibase_module_error("EXECUTE PROCEDURE: Invalid field count %d for SQLDA allocation",
						ib_query->out_fields_count);
					goto cleanup_result_query;
				}

				result_query->out_sqlda = (XSQLDA *) emalloc(sqlda_size);
				if (!result_query->out_sqlda) {
					_php_ibase_module_error("EXECUTE PROCEDURE: Failed to allocate SQLDA memory");
					goto cleanup_result_query;
				}

				/* Safe copy of SQLDA header and variable array */
				memcpy(result_query->out_sqlda, ib_query->out_sqlda, sqlda_size);

				/* CRITICAL SAFETY FIX: Clear all sqldata pointers in the copy immediately. */
				for (int i = 0; i < ib_query->out_fields_count; i++) {
					result_query->out_sqlda->sqlvar[i].sqldata = NULL;
				}

				/* Allocate null indicator array with validation */
				result_query->out_nullind = safe_emalloc(sizeof(*result_query->out_nullind),
					ib_query->out_fields_count, 0);
				if (!result_query->out_nullind) {
					_php_ibase_module_error("EXECUTE PROCEDURE: Failed to allocate null indicator array");
					goto cleanup_result_query;
				}

				/* Safe copy of null indicators */
				memcpy(result_query->out_nullind, ib_query->out_nullind,
					sizeof(*result_query->out_nullind) * ib_query->out_fields_count);

				/* Deep copy data for each field using safer copying mechanism */
				for (int i = 0; i < ib_query->out_fields_count; i++) {
					XSQLVAR *orig_var = &ib_query->out_sqlda->sqlvar[i];
					XSQLVAR *result_var = &result_query->out_sqlda->sqlvar[i];

					/* Reset sqldata pointer - will be set by safe copy function */
					result_var->sqldata = NULL;

					/* Use safer copying function with comprehensive validation */
					if (FAILURE == _php_ibase_safe_copy_sqlvar_data(result_var, orig_var, i, ib_query->query)) {
						goto cleanup_result_query;
					}
				}

				/* Update sqlind pointers to point to the new null indicators */
				for (int i = 0; i < ib_query->out_fields_count; i++) {
					if (result_query->out_sqlda->sqlvar[i].sqltype & 1) {
						result_query->out_sqlda->sqlvar[i].sqlind = &result_query->out_nullind[i];
					} else {
						result_query->out_sqlda->sqlvar[i].sqlind = NULL;
					}
				}
			}

   /* Copy input parameter metadata so ibase_num_params()/ibase_param_info()
    * work on the returned result resource (e.g., ibase_query() path). */
   result_query->in_fields_count = ib_query->in_fields_count;
   if (ib_query->in_fields_count > 0 && ib_query->in_sqlda) {
       size_t in_size = XSQLDA_LENGTH(ib_query->in_fields_count);
       result_query->in_sqlda = (XSQLDA *) emalloc(in_size);
       memcpy(result_query->in_sqlda, ib_query->in_sqlda, in_size);
       /* Ensure indicators are not dangling for inputs on the cloned structure */
       for (int i = 0; i < result_query->in_sqlda->sqld; i++) {
           result_query->in_sqlda->sqlvar[i].sqlind = NULL;
           result_query->in_sqlda->sqlvar[i].sqlind = NULL;
       }
   }

   /* Set result flags - EXECUTE PROCEDURE results are immediately available */
   result_query->has_more_rows = 1; /* Data is available for fetching */
   result_query->is_open = 1; /* Result can be fetched once */

   /* Register the result as a new resource - this transfers ownership
    * IMPORTANT: This result does NOT own the statement handle. */
   result_query->owns_stmt_handle = 0;
   result_query->res = zend_register_resource(result_query, le_query);
           if (!result_query->res) {
               _php_ibase_module_error("EXECUTE PROCEDURE: Failed to register result resource");
               goto cleanup_result_query;
           }

            /* Independent result snapshot: DO NOT link as child of parent query. */
            result_query->parent = NULL;

            /* Eagerly load column aliases before clearing the statement handle. */
            if (_php_ibase_alloc_ht_aliases(result_query) == FAILURE) {
                _php_ibase_module_error("EXECUTE PROCEDURE: Failed to allocate aliases");
                goto cleanup_result_query;
            }

            result_query->stmt.ptr = 0; /* Do not reference the handle as it may be freed */

			/* Success - disable cleanup since resource system now owns the memory */
			cleanup_needed = 0;

			RETVAL_RES(result_query->res);
			Z_TRY_ADDREF_P(return_value);

			return SUCCESS;

cleanup_result_query:
			/* Clean up partially allocated result_query on error path */
			if (cleanup_needed && result_query) {
				/* Free field data if partially allocated */
				if (result_query->out_sqlda && result_query->out_fields_count > 0) {
					for (int i = 0; i < result_query->out_fields_count; i++) {
						if (result_query->out_sqlda->sqlvar[i].sqldata) {
							efree(result_query->out_sqlda->sqlvar[i].sqldata);
						}
					}
				}

				/* Free SQLDA structure */
				if (result_query->out_sqlda) {
					efree(result_query->out_sqlda);
				}

				/* Free null indicator array */
				if (result_query->out_nullind) {
					efree(result_query->out_nullind);
				}

				/* Free alias hash table if allocated */
				if (result_query->ht_aliases) {
					zend_array_destroy(result_query->ht_aliases);
				}

				/* Free the result query structure itself */
				efree(result_query);
			}

			/* Propagate error to caller */
			goto _php_ibase_ex_error;
  } else {
            /* SELECT queries: Create independent result data to prevent use-after-free vulnerability */

            /* Create a new query structure for this specific result */
            ibase_query *result_query = ecalloc(1, sizeof(ibase_query));

			/* Initialize error cleanup flag */
			int cleanup_needed = 1;

			/* Copy essential fields from the original query */
			result_query->link = ib_query->link;
			result_query->trans = ib_query->trans;
			result_query->trans_res = ib_query->trans_res;
			result_query->dialect = ib_query->dialect;
			result_query->statement_type = ib_query->statement_type;
			result_query->out_fields_count = ib_query->out_fields_count;
			result_query->was_result_once = 1;

   /* Reuse parent's prepared statement and already-open cursor */
   result_query->stmt = ib_query->stmt;
   result_query->query = estrdup(ib_query->query);

   /* Copy input parameter metadata so ibase_num_params()/ibase_param_info()
    * work on the returned result resource (e.g., ibase_query() path). */
   result_query->in_fields_count = ib_query->in_fields_count;
   if (ib_query->in_fields_count > 0 && ib_query->in_sqlda) {
       size_t in_size = XSQLDA_LENGTH(ib_query->in_fields_count);
       result_query->in_sqlda = (XSQLDA *) emalloc(in_size);
       memcpy(result_query->in_sqlda, ib_query->in_sqlda, in_size);
       /* Input buffer pointers are not needed on the result copy when
        * reusing the same open cursor; keep them NULL to avoid misuse. */
       for (int i = 0; i < result_query->in_sqlda->sqld; i++) {
           result_query->in_sqlda->sqlvar[i].sqlind = NULL;
           result_query->in_sqlda->sqlvar[i].sqldata = NULL;
       }
   }

   /* Create independent copies of result data structures */
   if (ib_query->out_fields_count > 0 && ib_query->out_sqlda) {
                /* Validate source SQLDA before processing */
                if (ib_query->out_sqlda->sqln != ib_query->out_fields_count ||
                    ib_query->out_sqlda->sqld != ib_query->out_fields_count) {
                    _php_ibase_module_error("SELECT: Invalid SQLDA structure - sqln=%d, sqld=%d, expected=%d",
                        ib_query->out_sqlda->sqln, ib_query->out_sqlda->sqld, ib_query->out_fields_count);
                    goto cleanup_select_result_query;
                }

				/* Allocate independent SQLDA structure */
				size_t sqlda_size = XSQLDA_LENGTH(ib_query->out_fields_count);
				result_query->out_sqlda = (XSQLDA *) emalloc(sqlda_size);
				if (!result_query->out_sqlda) {
					_php_ibase_module_error("SELECT: Failed to allocate SQLDA memory");
					goto cleanup_select_result_query;
				}

				/* Safe copy of SQLDA header and variable array */
				memcpy(result_query->out_sqlda, ib_query->out_sqlda, sqlda_size);

				/* CRITICAL SAFETY FIX: Clear all sqldata pointers immediately to prevent
				 * double-free of parent data if allocation loop fails. */
				for (int i = 0; i < ib_query->out_fields_count; i++) {
					result_query->out_sqlda->sqlvar[i].sqldata = NULL;
				}

				/* Allocate independent null indicator array */
				result_query->out_nullind = safe_emalloc(sizeof(*result_query->out_nullind),
					ib_query->out_fields_count, 0);
				if (!result_query->out_nullind) {
					_php_ibase_module_error("SELECT: Failed to allocate null indicator array");
					goto cleanup_select_result_query;
				}

				/* Safe copy of null indicators */
				memcpy(result_query->out_nullind, ib_query->out_nullind,
					sizeof(*result_query->out_nullind) * ib_query->out_fields_count);

				/* Deep copy data for each field using safer copying mechanism */
				for (int i = 0; i < ib_query->out_fields_count; i++) {
					XSQLVAR *orig_var = &ib_query->out_sqlda->sqlvar[i];
					XSQLVAR *result_var = &result_query->out_sqlda->sqlvar[i];

					/* Reset sqldata pointer - will be set by safe copy function */
					result_var->sqldata = NULL;

					/* Use safer copying function with comprehensive validation */
					if (FAILURE == _php_ibase_safe_copy_sqlvar_data(result_var, orig_var, i, ib_query->query)) {
						goto cleanup_select_result_query;
					}
				}

				/* Update sqlind pointers to point to the new null indicators */
				for (int i = 0; i < ib_query->out_fields_count; i++) {
					if (result_query->out_sqlda->sqlvar[i].sqltype & 1) {
						result_query->out_sqlda->sqlvar[i].sqlind = &result_query->out_nullind[i];
					} else {
						result_query->out_sqlda->sqlvar[i].sqlind = NULL;
					}
				}

				/* Copy array metadata if present */
				if (ib_query->out_array_cnt > 0 && ib_query->out_array) {
					result_query->out_array_cnt = ib_query->out_array_cnt;
					result_query->out_array = safe_emalloc(sizeof(ibase_array), ib_query->out_array_cnt, 0);
					if (!result_query->out_array) {
						_php_ibase_module_error("SELECT: Failed to allocate array metadata");
						goto cleanup_select_result_query;
					}
					memcpy(result_query->out_array, ib_query->out_array,
						sizeof(ibase_array) * ib_query->out_array_cnt);
				}
			}

			/* Set result flags - parent query remains open, result has independent data */
			result_query->has_more_rows = 1; /* Data is available for fetching */
			result_query->is_open = 1; /* Result can be fetched */

   /* Register the result as a new resource - this transfers ownership
    * IMPORTANT: This result does NOT own the statement handle. */
   result_query->owns_stmt_handle = 0;
   result_query->res = zend_register_resource(result_query, le_query);
            if (!result_query->res) {
                _php_ibase_module_error("SELECT: Failed to register result resource");
                goto cleanup_select_result_query;
            }

            /* Link this result as a child of the parent prepared query so that
             * freeing the parent can invalidate dependent results (required for
             * use-after-free tests). */
            result_query->parent = ib_query;
            result_query->child_head = NULL;
            result_query->child_next = ib_query->child_head;
            ib_query->child_head = result_query;

   /* NOTE: We do NOT increment parent's refcount. The parent query resource
    * lifetime is controlled by the user, not by child results. This allows
    * multiple ibase_execute() calls on the same prepared query without the
    * query resource becoming invalid after ibase_free_result(). */

   /* Mark cursor state inherited from parent execute */
   result_query->is_open = 1;
   result_query->has_more_rows = 1;

   /* Success - disable cleanup since resource system now owns the memory */
   cleanup_needed = 0;

   RETVAL_RES(result_query->res);
   Z_TRY_ADDREF_P(return_value);

   return SUCCESS;

cleanup_select_result_query:
			/* Clean up partially allocated result_query on error path */
			if (cleanup_needed && result_query) {
				/* Free field data if partially allocated */
				if (result_query->out_sqlda && result_query->out_fields_count > 0) {
					for (int i = 0; i < result_query->out_fields_count; i++) {
						if (result_query->out_sqlda->sqlvar[i].sqldata) {
							efree(result_query->out_sqlda->sqlvar[i].sqldata);
						}
					}
				}

				/* Free SQLDA structure */
				if (result_query->out_sqlda) {
					efree(result_query->out_sqlda);
				}

				/* Free null indicator array */
				if (result_query->out_nullind) {
					efree(result_query->out_nullind);
				}

				/* Free array metadata */
				if (result_query->out_array) {
					efree(result_query->out_array);
				}

				/* Free query string */
				if (result_query->query) {
					efree(result_query->query);
				}

				/* NOTE: Do NOT free result_query->stmt as it's shared with parent */

				/* Free the result query structure itself */
				efree(result_query);
			}

			/* Propagate error to caller */
			goto _php_ibase_ex_error;
		}
	}

	/* Update cursor flags based on statement type and execution result */
	switch (ib_query->statement_type) {

		unsigned long affected_rows;

		case isc_info_sql_stmt_insert:
		case isc_info_sql_stmt_update:
		case isc_info_sql_stmt_delete:
		case isc_info_sql_stmt_exec_procedure:

			if (isc_dsql_sql_info(IB_STATUS, &ib_query->stmt.stmt, sizeof(info_count),
					info_count, sizeof(result), result)) {
				_php_ibase_error();
				goto _php_ibase_ex_error;
			}

			affected_rows = 0;

			if (result[0] == isc_info_sql_records) {
				unsigned i = 3, result_size = isc_vax_integer(&result[1],2);

				while (result[i] != isc_info_end && i < result_size) {
					short len = (short)isc_vax_integer(&result[i+1],2);
					if (result[i] != isc_info_req_select_count) {
						affected_rows += isc_vax_integer(&result[i+3],len);
					}
					i += len+3;
				}
			}

			ib_query->trans->affected_rows = affected_rows;

			if (!ib_query->out_sqlda) { /* no result set is being returned */
				/* Non-SELECT statements without RETURNING clause - no cursor opened */
				ib_query->is_open = 0;
				ib_query->has_more_rows = 0;

				if (affected_rows) {
					RETVAL_LONG(affected_rows);
				} else {
					RETVAL_TRUE;
				}
				break;
			}

			/* DML with RETURNING clause - cursor is opened but handled by result resource */
			ib_query->is_open = 0;
			ib_query->has_more_rows = 0;
			break;

		case isc_info_sql_stmt_select:
			/* SELECT statements - cursor is now open and has potential rows */
			if (ib_query->out_sqlda) {
				ib_query->is_open = 1;
				ib_query->has_more_rows = 1;
			} else {
				/* SELECT without output - unusual but handle */
				ib_query->is_open = 0;
				ib_query->has_more_rows = 0;
			}
			break;

		default:
			/* Other statement types (DDL, etc.) - no cursor */
			ib_query->is_open = 0;
			ib_query->has_more_rows = 0;
			RETVAL_TRUE;
			break;
	}

	rv = SUCCESS;

_php_ibase_ex_error:
	/* Clear cursor flags on any execution error to prevent inconsistent state */
	ib_query->is_open = 0;
	ib_query->has_more_rows = 0;
	return rv;
}
/* }}} */

/* {{{ proto mixed ibase_query([resource link_identifier, [ resource link_identifier, ]] string query [, mixed bind_arg [, mixed bind_arg [, ...]]]) */
PHP_FUNCTION(ibase_query)
{
	zval *args;
	int i, argc = ZEND_NUM_ARGS();
	char *query = NULL;
	ibase_db_link *link = NULL;
	ibase_trans *trans = NULL;
	zval *link_arg = NULL, *trans_arg = NULL;
	zend_resource *trans_res = NULL;
	ibase_query *ib_query;
	int bind_start = 0;
	int explicit_create = 0;

	if (argc < 1) {
		WRONG_PARAM_COUNT;
	}

	args = safe_emalloc(argc, sizeof(zval), 0);
	if (zend_get_parameters_array_ex(argc, args) == FAILURE) {
		efree(args);
		WRONG_PARAM_COUNT;
	}

	/* Flexible argument parsing handling optional params and placeholders */
	i = 0;
	while (i < argc) {
		zval *arg = &args[i];
		ZVAL_DEREF(arg); /* Handle references */

		/* Handle IBASE_CREATE (0) passed as first argument */
		if (i == 0 && Z_TYPE_P(arg) == IS_LONG && Z_LVAL_P(arg) == PHP_IBASE_CREATE) {
			explicit_create = 1;
			i++;
			continue;
		}

		if (Z_TYPE_P(arg) == IS_STRING) {
			query = Z_STRVAL_P(arg);
			bind_start = i + 1;
			break;
		} else if (Z_TYPE_P(arg) == IS_RESOURCE) {
			/* Identify resource type */
			if (!trans && !link) {
				trans = (ibase_trans *)zend_fetch_resource_ex(arg, NULL, le_trans);
				if (trans) {
					trans_arg = arg;
					trans_res = Z_RES_P(trans_arg);
				} else {
					link = (ibase_db_link *)zend_fetch_resource_ex(arg, NULL, le_link);
					if (!link) link = (ibase_db_link *)zend_fetch_resource_ex(arg, NULL, le_plink);
					if (link) link_arg = arg;
				}
			} else if (trans && !link) {
				link = (ibase_db_link *)zend_fetch_resource_ex(arg, NULL, le_link);
				if (!link) link = (ibase_db_link *)zend_fetch_resource_ex(arg, NULL, le_plink);
				if (link) link_arg = arg;
			} else if (link && !trans) {
				trans = (ibase_trans *)zend_fetch_resource_ex(arg, NULL, le_trans);
				if (trans) {
					trans_arg = arg;
					trans_res = Z_RES_P(trans_arg);
				}
			}
		}
		/* Skip non-string, non-resource arguments (e.g. IBASE_CREATE/0 placeholder) */
		i++;
	}

	if (!query) {
		efree(args);
		_php_ibase_module_error("Query argument missing or not a string");
		RETURN_FALSE;
	}

	/* Handle CREATE DATABASE request via IBASE_CREATE flag */
	if (explicit_create) {
		/* Use void* for handles to ensure 64-bit storage on stack, preventing stack smashing
		   if Firebird client writes 64-bit handles to 32-bit isc_db_handle types. */
		void *safe_new_db_handle = NULL;
		void *safe_new_trans_handle = NULL;
		unsigned short dialect = 3; /* Default dialect 3 for new databases */

		if (isc_dsql_execute_immediate(IB_STATUS, (isc_db_handle*)&safe_new_db_handle, (isc_tr_handle*)&safe_new_trans_handle, 0, query, dialect, NULL)) {
			_php_ibase_error();
			efree(args);
			RETURN_FALSE;
		}

		/* Commit the implicit transaction started by CREATE DATABASE to ensure persistence */
		if (safe_new_trans_handle) {
			if (isc_commit_transaction(IB_STATUS, (isc_tr_handle*)&safe_new_trans_handle)) {
				_php_ibase_error();
				/* Note: Database created but commit failed? */
			}
		}

		/* Register the new database connection as a resource */
		if (safe_new_db_handle) {
			link = (ibase_db_link *) ecalloc(1, sizeof(ibase_db_link));
			link->handle.ptr = safe_new_db_handle;
			link->dialect = dialect;
			link->tr_list = NULL;
			link->event_head = NULL;

			RETVAL_RES(zend_register_resource(link, le_link));
		} else {
			RETVAL_TRUE;
		}

		efree(args);
		return;
	}

	/* Resolve Link if missing */
	if (!link && !trans) {
		if (IBG(default_link)) {
			link = (ibase_db_link *)zend_fetch_resource2(IBG(default_link), "InterBase link", le_link, le_plink);
		}

		/* If no link found, fail gracefully */
		if (!link) {
			efree(args);
			_php_ibase_module_error("No default connection");
			RETURN_FALSE;
		}
	} else if (!link && trans) {
		/* If transaction is provided but link is not, infer link from transaction.
		   Transactions must be associated with at least one database connection.
		   We default to the first associated link. */
		if (trans->link_cnt > 0) {
			link = trans->db_link[0];
		} else {
			efree(args);
			_php_ibase_module_error("Transaction has no associated link");
			RETURN_FALSE;
		}
	}

	/* Resolve Transaction if missing */
	if (!trans) {
		if (SUCCESS != _php_ibase_def_trans(link, &trans)) {
			efree(args);
			RETURN_FALSE;
		}
	}

	if (!trans) {
		efree(args);
		_php_ibase_module_error("Could not determine transaction");
		RETURN_FALSE;
	}

	if (FAILURE == _php_ibase_prepare(&ib_query, link, trans, trans_res, query)) {
		efree(args);
		RETURN_FALSE;
	}

	if (FAILURE == _php_ibase_exec(INTERNAL_FUNCTION_PARAM_PASSTHRU, ib_query, &args[bind_start], argc - bind_start)) {
		zend_list_delete(ib_query->res);
		efree(args);
		RETURN_FALSE;
	}

	if (Z_TYPE_P(return_value) != IS_RESOURCE) {
	    zend_list_delete(ib_query->res);
	}

	efree(args);
}
/* }}} */

/* {{{ proto resource ibase_prepare([resource link_identifier, [ resource link_identifier, ]] string query) */
PHP_FUNCTION(ibase_prepare)
{
	zval *args;
	int i, argc = ZEND_NUM_ARGS();
	char *query;
	ibase_db_link *link = NULL;
	ibase_trans *trans = NULL;
	zval *link_arg = NULL, *trans_arg = NULL;
	zend_resource *trans_res = NULL;
	ibase_query *ib_query;

	if (argc < 1) {
		WRONG_PARAM_COUNT;
	}

	args = safe_emalloc(argc, sizeof(zval), 0);
	if (zend_get_parameters_array_ex(argc, args) == FAILURE) {
		efree(args);
		WRONG_PARAM_COUNT;
	}

	/* Parse arguments */
	i = 0;
	if (Z_TYPE(args[i]) == IS_RESOURCE) {
		trans = (ibase_trans *)zend_fetch_resource_ex(&args[i], NULL, le_trans);
		if (trans) {
			trans_arg = &args[i];
			trans_res = Z_RES_P(trans_arg);
			i++;
		} else {
			link = (ibase_db_link *)zend_fetch_resource_ex(&args[i], NULL, le_link);
			if (!link) {
				link = (ibase_db_link *)zend_fetch_resource_ex(&args[i], NULL, le_plink);
			}
			if (link) {
				link_arg = &args[i];
				i++;
			}
		}
	}

	if (i == 1 && i < argc && Z_TYPE(args[i]) == IS_RESOURCE) {
		if (trans) {
			ibase_db_link *l = (ibase_db_link *)zend_fetch_resource_ex(&args[i], NULL, le_link);
			if (!l) l = (ibase_db_link *)zend_fetch_resource_ex(&args[i], NULL, le_plink);
			if (l) {
				link = l;
				link_arg = &args[i];
				i++;
			}
		} else if (link) {
			ibase_trans *t = (ibase_trans *)zend_fetch_resource_ex(&args[i], NULL, le_trans);
			if (t) {
				trans = t;
				trans_arg = &args[i];
				trans_res = Z_RES_P(trans_arg);
				i++;
			}
		}
	}

	if (i < argc && Z_TYPE(args[i]) == IS_STRING) {
		query = Z_STRVAL(args[i]);
	} else {
		efree(args);
		_php_ibase_module_error("Query argument missing or not a string");
		RETURN_FALSE;
	}

	if (!link && !trans) {
		if (IBG(default_link)) {
			link = (ibase_db_link *)zend_fetch_resource2(IBG(default_link), "InterBase link", le_link, le_plink);
		}
		if (!link) {
			efree(args);
			_php_ibase_module_error("No default connection");
			RETURN_FALSE;
		}
	}

	if (!link && trans) {
        /* If transaction is provided but link is not, infer link from transaction.
           Transactions must be associated with at least one database connection.
           We default to the first associated link. */
        if (trans->link_cnt > 0) {
            link = trans->db_link[0];
        } else {
            efree(args);
            _php_ibase_module_error("Transaction has no associated link");
            RETURN_FALSE;
        }
    }

	if (!trans) {
		if (SUCCESS != _php_ibase_def_trans(link, &trans)) {
			efree(args);
			RETURN_FALSE;
		}
	}

	if (FAILURE == _php_ibase_prepare(&ib_query, link, trans, trans_res, query)) {
		efree(args);
		RETURN_FALSE;
	}

	efree(args);
	RETVAL_RES(ib_query->res);
	Z_TRY_ADDREF_P(return_value);
}
/* }}} */

/* {{{ proto mixed ibase_execute(resource query [, mixed bind_arg [, mixed bind_arg [, ...]]]) */
PHP_FUNCTION(ibase_execute)
{
	zval *args;
	int i, argc = ZEND_NUM_ARGS();
	ibase_query *ib_query;

	if (argc < 1) {
		WRONG_PARAM_COUNT;
	}

	args = safe_emalloc(argc, sizeof(zval), 0);
	if (zend_get_parameters_array_ex(argc, args) == FAILURE) {
		efree(args);
		WRONG_PARAM_COUNT;
	}

	ib_query = (ibase_query *)zend_fetch_resource_ex(&args[0], "Firebird/InterBase query", le_query);
	if (!ib_query) {
		efree(args);
		RETURN_FALSE;
	}

	if (FAILURE == _php_ibase_exec(INTERNAL_FUNCTION_PARAM_PASSTHRU, ib_query, &args[1], argc - 1)) {
		efree(args);
		RETURN_FALSE;
	}

	efree(args);
}
/* }}} */

/* {{{ proto bool ibase_free_query(resource query) */
void _php_ibase_free_query_impl(INTERNAL_FUNCTION_PARAMETERS, int as_result)
{
	zval *query_arg;
	ibase_query *ib_query;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &query_arg) == FAILURE) {
		return;
	}

	ib_query = (ibase_query *)zend_fetch_resource_ex(query_arg, "Firebird/InterBase query", le_query);
	if (!ib_query) {
		RETURN_FALSE;
	}

	zend_list_close(Z_RES_P(query_arg));
	RETURN_TRUE;
}

PHP_FUNCTION(ibase_free_query)
{
	_php_ibase_free_query_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ proto int ibase_affected_rows([ resource link_identifier ]) */
PHP_FUNCTION(ibase_affected_rows)
{
	zval *link_arg = NULL;
	ibase_db_link *link = NULL;
	ibase_trans *trans = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|r", &link_arg) == FAILURE) {
		return;
	}

	if (link_arg) {
		link = (ibase_db_link *)zend_fetch_resource2_ex(link_arg, LE_LINK, le_link, le_plink);
	} else {
		if (IBG(default_link)) {
			link = (ibase_db_link *)zend_fetch_resource2(IBG(default_link), "InterBase link", le_link, le_plink);
		}
	}

	if (!link) {
		RETURN_FALSE;
	}

	if (SUCCESS == _php_ibase_def_trans(link, &trans)) {
		RETVAL_LONG(trans->affected_rows);
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

static zval * _php_ibase_hash_to_zval_array(HashTable *ht, int *count)
{
    int n = zend_hash_num_elements(ht);
    if (n == 0) {
        *count = 0;
        return NULL;
    }
    zval *arr = safe_emalloc(n, sizeof(zval), 0);
    int i = 0;
    zval *entry;
    ZEND_HASH_FOREACH_VAL(ht, entry) {
        ZVAL_COPY(&arr[i], entry);
        i++;
    } ZEND_HASH_FOREACH_END();
    *count = n;
    return arr;
}

/* {{{ proto int fbird_execute_statement(resource trans_handle, string query [, array params])
   Execute DML/DDL statement within a specific transaction and return affected rows */
PHP_FUNCTION(fbird_execute_statement)
{
    zval *trans_arg, *params_arg = NULL;
    char *sql;
    size_t sql_len;
    ibase_trans *trans;
    ibase_db_link *link;
    ibase_query *ib_query;
    zval *bind_args = NULL;
    int bind_n = 0;

    RESET_ERRMSG;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "rs|a", &trans_arg, &sql, &sql_len, &params_arg) == FAILURE) {
        return;
    }

    trans = (ibase_trans *)zend_fetch_resource_ex(trans_arg, LE_TRANS, le_trans);
    if (!trans) {
        RETURN_FALSE;
    }
    if (trans->link_cnt > 0) {
        link = trans->db_link[0];
    } else {
        _php_ibase_module_error("Transaction has no associated link");
        RETURN_FALSE;
    }

    if (FAILURE == _php_ibase_prepare(&ib_query, link, trans, Z_RES_P(trans_arg), sql)) {
        RETURN_FALSE;
    }

    if (params_arg) {
        bind_args = _php_ibase_hash_to_zval_array(Z_ARRVAL_P(params_arg), &bind_n);
    }

    if (FAILURE == _php_ibase_exec(INTERNAL_FUNCTION_PARAM_PASSTHRU, ib_query, bind_args, bind_n)) {
        if (bind_args) {
            for (int i=0; i<bind_n; i++) zval_ptr_dtor(&bind_args[i]);
            efree(bind_args);
        }
        zend_list_delete(ib_query->res);
        RETURN_FALSE;
    }

    if (bind_args) {
        for (int i=0; i<bind_n; i++) zval_ptr_dtor(&bind_args[i]);
        efree(bind_args);
    }

    /* Strict return type check */
    if (Z_TYPE_P(return_value) == IS_RESOURCE) {
        zend_throw_error(NULL, "fbird_execute_statement expects a DML/DDL statement, but SELECT was executed. Use fbird_execute_query().");
        zend_list_delete(Z_RES_P(return_value));
        zend_list_delete(ib_query->res);
        RETURN_THROWS();
    }

    /* Cleanup prepared query resource as fbird_execute_statement is one-shot for the user?
       Wait, fbird_execute_statement takes SQL + Params. It prepares, executes, then destroys query handle?
       Yes, similar to ibase_query execution path.
       If the user wants prepared statement reuse, they should use ibase_prepare + ibase_execute.
       fbird_execute_statement is atomic execution.
    */
    zend_list_delete(ib_query->res);

    if (Z_TYPE_P(return_value) == IS_TRUE) {
        RETVAL_LONG(0);
    }
}
/* }}} */

/* {{{ proto resource fbird_execute_query(resource trans_handle, string query [, array params])
   Execute SELECT statement within a specific transaction and return result resource */
PHP_FUNCTION(fbird_execute_query)
{
    zval *trans_arg, *params_arg = NULL;
    char *sql;
    size_t sql_len;
    ibase_trans *trans;
    ibase_db_link *link;
    ibase_query *ib_query;
    zval *bind_args = NULL;
    int bind_n = 0;

    RESET_ERRMSG;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "rs|a", &trans_arg, &sql, &sql_len, &params_arg) == FAILURE) {
        return;
    }

    trans = (ibase_trans *)zend_fetch_resource_ex(trans_arg, LE_TRANS, le_trans);
    if (!trans) RETURN_FALSE;
    if (trans->link_cnt > 0) {
        link = trans->db_link[0];
    } else {
        _php_ibase_module_error("Transaction has no associated link");
        RETURN_FALSE;
    }

    if (FAILURE == _php_ibase_prepare(&ib_query, link, trans, Z_RES_P(trans_arg), sql)) {
        RETURN_FALSE;
    }

    if (params_arg) {
        bind_args = _php_ibase_hash_to_zval_array(Z_ARRVAL_P(params_arg), &bind_n);
    }

    if (FAILURE == _php_ibase_exec(INTERNAL_FUNCTION_PARAM_PASSTHRU, ib_query, bind_args, bind_n)) {
        if (bind_args) {
            for (int i=0; i<bind_n; i++) zval_ptr_dtor(&bind_args[i]);
            efree(bind_args);
        }
        zend_list_delete(ib_query->res);
        RETURN_FALSE;
    }

    if (bind_args) {
        for (int i=0; i<bind_n; i++) zval_ptr_dtor(&bind_args[i]);
        efree(bind_args);
    }

    if (Z_TYPE_P(return_value) != IS_RESOURCE) {
        /* RETURNING queries also return resource if execute2 results are present. */
        zend_throw_error(NULL, "fbird_execute_query expects a SELECT or RETURNING statement.");
        // _php_ibase_exec returns TRUE/LONG for DML.
        zend_list_delete(ib_query->res);
        RETURN_THROWS();
    }

    /* Keep ib_query alive as it holds the statement handle */
    /* But wait, _php_ibase_exec creates a RESULT resource that references ib_query.
       If ib_query is just for execution, we should probably keep it alive managed by the result.
       Actually _php_ibase_exec implementation for SELECT reuses ib_query->stmt.
       And it sets result_query->parent = ib_query.
       So we MUST return the result resource (which is in return_value)
       AND let ib_query be managed.
       Actually, ibase_query implementation returns the result resource but keeps ib_query resource alive?
       Wait, ibase_query deletes ib_query->res ONLY on error.
       So on success, ib_query->res is alive.
       Is it returned? No, return_value is the result_query->res.
       So ib_query (the prepared statement) leaks?
       No, ibase_query is one-shot.
       Let's check ibase_query implementation again.
       It does zend_list_delete(ib_query->res) ONLY on error label.
       If success, it returns.
       So ib_query resource leaks?
       Ah, for SELECT, _php_ibase_exec returns result_query->res.
       result_query->parent = ib_query.
       So ib_query resource must persist for the lifetime of result?
       Yes.
       So we DO NOT delete ib_query->res on success.
    */
}
/* }}} */

/* {{{ proto mixed fbird_execute_auto(resource link_identifier, string query [, array params])
   Execute statement in an autonomous transaction (start -> execute -> commit/rollback) */
PHP_FUNCTION(fbird_execute_auto)
{
    zval *link_arg, *params_arg = NULL;
    char *sql;
    size_t sql_len;
    ibase_db_link *link;
    ibase_trans *trans;
    ibase_query *ib_query;
    zval *bind_args = NULL;
    int bind_n = 0;
    fb_safe_handle tr_handle = {0};
    ISC_STATUS result;

    RESET_ERRMSG;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "rs|a", &link_arg, &sql, &sql_len, &params_arg) == FAILURE) {
        return;
    }

    link = (ibase_db_link *)zend_fetch_resource2_ex(link_arg, LE_LINK, le_link, le_plink);
    if (!link) RETURN_FALSE;

    /* Start autonomous transaction */
    result = isc_start_transaction(IB_STATUS, &tr_handle.tr, 1, &link->handle.db, 0, NULL);
    if (result) {
        _php_ibase_error();
        RETURN_FALSE;
    }

    /* Create temp trans object */
    trans = (ibase_trans *) emalloc(sizeof(ibase_trans));
    trans->handle = tr_handle;
    trans->link_cnt = 1;
    trans->affected_rows = 0;
    trans->db_link[0] = link;
    /* We do NOT register this transaction as a resource because it's strictly local scope */

    /* Prepare */
    if (FAILURE == _php_ibase_prepare(&ib_query, link, trans, NULL, sql)) {
        isc_rollback_transaction(IB_STATUS, &trans->handle.tr);
        efree(trans);
        RETURN_FALSE;
    }

    if (params_arg) {
        bind_args = _php_ibase_hash_to_zval_array(Z_ARRVAL_P(params_arg), &bind_n);
    }

    if (FAILURE == _php_ibase_exec(INTERNAL_FUNCTION_PARAM_PASSTHRU, ib_query, bind_args, bind_n)) {
        if (bind_args) {
            for (int i=0; i<bind_n; i++) zval_ptr_dtor(&bind_args[i]);
            efree(bind_args);
        }

        zend_list_delete(ib_query->res); // Frees statement
        isc_rollback_transaction(IB_STATUS, &trans->handle.tr);
        efree(trans);
        RETURN_FALSE;
    }

    if (bind_args) {
        for (int i=0; i<bind_n; i++) zval_ptr_dtor(&bind_args[i]);
        efree(bind_args);
    }

    /* Check if it returned a resource (SELECT) */
    if (Z_TYPE_P(return_value) == IS_RESOURCE) {
        /* We cannot support returning a cursor from an autonomous transaction
           because we commit immediately below, which would close the cursor. */
        zend_throw_error(NULL, "fbird_execute_auto cannot be used with SELECT statements (cursor would be closed on commit).");
        zend_list_delete(Z_RES_P(return_value));
        zend_list_delete(ib_query->res);
        isc_rollback_transaction(IB_STATUS, &trans->handle.tr);
        efree(trans);
        RETURN_THROWS();
    }

    /* Commit */
    if (isc_commit_transaction(IB_STATUS, &trans->handle.tr)) {
        _php_ibase_error();
        zend_list_delete(ib_query->res);
        efree(trans); // Handle invalid now
        RETURN_FALSE;
    }

    zend_list_delete(ib_query->res);
    efree(trans);

    /* Return value is already set by _php_ibase_exec (TRUE/affected_rows) */
}
/* }}} */

int _php_ibase_fetch_query_res(zval *from, ibase_query **ib_query)
{
	if (Z_TYPE_P(from) != IS_RESOURCE) {
		return 0;
	}
	*ib_query = (ibase_query *)zend_fetch_resource_ex(from, "Firebird/InterBase query", le_query);
	return (*ib_query) ? 1 : 0;
}

#endif /* HAVE_IBASE */
