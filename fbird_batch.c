/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"

#if HAVE_FIREBIRD

#include "php_ini.h"
#include "php_firebird.h"
#include "fbird_classes.h"
#include "php_fbird_includes.h"
#include "php_fbird_batch.h"
#include "fbird_datetime.h"
#include "firebird_utils.h"

#if FB_API_VER >= 40
void _php_fbird_free_batch(zend_resource *rsrc)
{
	ISC_STATUS status[256];
	fbird_batch *batch = (fbird_batch *)rsrc->ptr;

	FBDEBUG("Cleaning up batch resource...");

#ifndef PHP_WIN32
	/* Fork-safety check (Issue #22): Skip cleanup if we're in a forked child */
	if (FBG(init_pid) != 0 && getpid() != FBG(init_pid)) {
		FBDEBUG("Skipping batch cleanup in forked child process");
		/* Only free memory allocated by child, not Firebird handles */
		if (batch->in_msg_buffer != NULL) {
			efree(batch->in_msg_buffer);
		}
		efree(batch);
		return;
	}
#endif

	/* Cancel and close the batch if still open */
	if (batch->fbbatch_wrapper != NULL) {
		FBDEBUG("Canceling unexecuted batch...");
		fbbatch_cancel(FBG(master_instance), batch->fbbatch_wrapper, status);
		fbbatch_close(FBG(master_instance), batch->fbbatch_wrapper, status);
		batch->fbbatch_wrapper = NULL;
	}

	/* Free the input message buffer if allocated */
	if (batch->in_msg_buffer != NULL) {
		efree(batch->in_msg_buffer);
		batch->in_msg_buffer = NULL;
	}

	/* Release metadata reference if held */
	if (batch->in_metadata != NULL) {
		fbm_release(batch->in_metadata);
		batch->in_metadata = NULL;
	}

	/* Release strong reference to query resource (Issue #185).
	 * This prevents the IStatement* from being freed while the batch
	 * still holds a pointer to it. */
	if (batch->query_res != NULL) {
		zend_list_delete(batch->query_res);
		batch->query_res = NULL;
	}

	efree(batch);
}
#endif /* FB_API_VER >= 40 */

#if FB_API_VER >= 40
/* Dual-bridge helper: extract fbird_batch from either a Firebird\BatchHandle
 * object or a legacy resource. Returns NULL on failure. */
static fbird_batch *fbird_batch_from_zval(zval *zv)
{
	if (Z_TYPE_P(zv) == IS_OBJECT) {
		return fbird_batch_get_ptr(Z_OBJ_P(zv));
	}
	return (fbird_batch *)zend_fetch_resource_ex(zv, LE_BATCH, le_batch);
}
#endif

/*
 * Custom INI display callback for password fields.
 * Displays "********" instead of the actual password value in phpinfo().
 * Note: This pattern is common across extensions; consider proposing for Zend API.
 */

#if FB_API_VER >= 40
/* IBatch API Functions (Firebird 4.0+ Bulk Operations) */
PHP_FUNCTION(fbird_batch_create)
{
	ISC_STATUS status[256];
	zval *query_arg, *trans_arg = NULL;
	fbird_query *fb_query;
	fbird_transaction *trans = NULL;
	fbird_batch *fb_batch;
	void *stmt_ptr;
	void *batch_wrapper;
	void *metadata;
	unsigned msg_length;

	RESET_ERRMSG;

	/* M3: "z|z!" instead of "r|r!" to accept Firebird\ResultSet/Transaction objects */
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z|z!", &query_arg, &trans_arg) == FAILURE) {
		return;
	}

	FBIRD_VALIDATE_QUERY_EX(query_arg, 1, fb_query);
	if (!fb_query) {
		RETURN_FALSE;
	}

	if (!fb_query->fbs_statement) {
		_php_fbird_module_error("Query has no OO API statement handle");
		RETURN_FALSE;
	}

	/* Get transaction - either from parameter or from query's default */
	if (trans_arg != NULL) {
		FBIRD_VALIDATE_TRANS_EX(trans_arg, 2, trans);
		if (!trans) {
			RETURN_FALSE;
		}
	} else {
		trans = fb_query->trans;
	}

	if (!trans || !trans->fbt_transaction) {
		_php_fbird_module_error("No valid transaction for batch operation");
		RETURN_FALSE;
	}

	/* Get raw IStatement pointer */
	stmt_ptr = fbs_get_statement(fb_query->fbs_statement);
	if (!stmt_ptr) {
		_php_fbird_module_error("Failed to get statement handle");
		RETURN_FALSE;
	}

	/* Guard: batch operations require input parameters (Issue #180).
	 * Calling createBatch() with msg_length=0 causes SIGFPE in libfbclient
	 * (division by zero when computing row capacity from buffer size). */
	{
		unsigned input_count = fbs_get_input_count(
			FBG(master_instance), fb_query->fbs_statement, status);
		if (input_count == 0) {
			_php_fbird_module_error("fbird_batch_create(): statement has no "
				"input parameters; batch operations require parameterized "
				"statements (e.g., INSERT ... VALUES (?, ?))");
			RETURN_FALSE;
		}
	}

	/* Create batch with default buffer size */
	batch_wrapper = fbbatch_create(FBG(master_instance), stmt_ptr, 0, status);
	if (!batch_wrapper) {
		_php_fbird_error(status);
		RETURN_FALSE;
	}

	/* Get input metadata for building message buffers */
	metadata = fbbatch_get_metadata(FBG(master_instance), batch_wrapper, status);
	if (!metadata) {
		fbbatch_close(FBG(master_instance), batch_wrapper, status);
		_php_fbird_error(status);
		RETURN_FALSE;
	}

	msg_length = fbm_get_message_length(FBG(master_instance), metadata);

	/* Allocate batch structure */
	fb_batch = (fbird_batch *)ecalloc(1, sizeof(fbird_batch));
	fb_batch->fbbatch_wrapper = batch_wrapper;
	fb_batch->trans = trans;
	fb_batch->query = fb_query;
	/* M3: query_arg may be a Firebird\ResultSet object or a legacy resource */
	if (Z_TYPE_P(query_arg) == IS_OBJECT) {
		fb_batch->query_res = fbird_resultset_get_resource(Z_OBJ_P(query_arg));
	} else {
		fb_batch->query_res = Z_RES_P(query_arg);
	}
	if (fb_batch->query_res) {
		GC_ADDREF(fb_batch->query_res);
	}
	fb_batch->in_metadata = metadata;
	fb_batch->in_msg_length = msg_length;
	fb_batch->in_msg_buffer = emalloc(msg_length);
	memset(fb_batch->in_msg_buffer, 0, msg_length);

	fbird_setup_batch_object(return_value, fb_batch);
}

/*
 * Shared batch-add implementation used by both the procedural
 * fbird_batch_add() and the OOP BatchHandle::add() method.
 *
 * Validates the batch handle, metadata, and argument count, then binds
 * each parameter to the message buffer and calls fbbatch_add().
 *
 * Returns SUCCESS on success, FAILURE on error (error message set via
 * _php_fbird_module_error or _php_fbird_error).
 */
int fbird_batch_add_impl(fbird_batch *fb_batch, zval *args, int argc)
{
	ISC_STATUS status[256];

	RESET_ERRMSG;

	if (!fb_batch || !fb_batch->fbbatch_wrapper) {
		_php_fbird_module_error("Invalid batch resource");
		return FAILURE;
	}

	/* Get master interface for metadata operations */
	void *master = FBG(master_instance);
	if (!master) {
		_php_fbird_module_error("fbird_batch_add() requires Firebird 3.0+ OO API master interface");
		return FAILURE;
	}

	/* Validate metadata and buffer are available */
	if (!fb_batch->in_metadata || !fb_batch->in_msg_buffer || fb_batch->in_msg_length == 0) {
		_php_fbird_module_error("fbird_batch_add() batch has no input metadata or buffer");
		return FAILURE;
	}

	/* Get parameter count from metadata */
	unsigned param_count = fbm_get_count(master, fb_batch->in_metadata);

	/* Validate argument count matches parameter count */
	if ((unsigned)argc != param_count) {
		_php_fbird_module_error("fbird_batch_add() expects %u parameters, %d given", param_count, argc);
		return FAILURE;
	}

	/* Clear message buffer before populating */
	memset(fb_batch->in_msg_buffer, 0, fb_batch->in_msg_length);

	/* Bind each parameter to the message buffer */
	for (unsigned i = 0; i < param_count; i++) {
		zval *b_var = &args[i];

		/* Get metadata info for this parameter */
		unsigned sql_type = fbm_get_type(master, fb_batch->in_metadata, i) & ~1;
		unsigned data_offset = fbm_get_offset(master, fb_batch->in_metadata, i);
		unsigned null_offset = fbm_get_null_offset(master, fb_batch->in_metadata, i);
		unsigned field_length = fbm_get_length(master, fb_batch->in_metadata, i);
		int sql_scale = fbm_get_scale(master, fb_batch->in_metadata, i);

		/* Get pointers to data and null indicator in message buffer */
		unsigned char *data_ptr = (unsigned char *)fb_batch->in_msg_buffer + data_offset;
		short *null_ptr = (short *)((unsigned char *)fb_batch->in_msg_buffer + null_offset);

		/* Handle NULL values */
		int is_null = 0;
		switch (Z_TYPE_P(b_var)) {
			case IS_NULL:
				is_null = 1;
				break;
			case IS_STRING:
				/* Empty string treated as NULL for numeric/date types */
				if (Z_STRLEN_P(b_var) == 0) {
					switch (sql_type) {
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
							is_null = 1;
							break;
						default:
							break;
					}
				}
				break;
			default:
				break;
		}

		if (is_null) {
			*null_ptr = -1;
			continue;
		}

		/* Not NULL */
		*null_ptr = 0;

		/* Convert PHP value to Firebird format based on SQL type */
		switch (sql_type) {
			case SQL_SHORT: {
				if (sql_scale < 0) {
					/* NUMERIC/DECIMAL with scale */
					double dval = zval_get_double(b_var);
					double factor = pow(10.0, (double)(-sql_scale));
					long long scaled = llround(dval * factor);
					if (scaled < SHRT_MIN || scaled > SHRT_MAX) {
						_php_fbird_module_error("Parameter %u: scaled value out of range for SHORT", i + 1);
						return FAILURE;
					}
					*(short *)data_ptr = (short)scaled;
				} else {
					zend_long lval = zval_get_long(b_var);
					*(short *)data_ptr = (short)lval;
				}
				break;
			}

			case SQL_LONG: {
				if (sql_scale < 0) {
					double dval = zval_get_double(b_var);
					double factor = pow(10.0, (double)(-sql_scale));
					long long scaled = llround(dval * factor);
					if (scaled < INT_MIN || scaled > INT_MAX) {
						_php_fbird_module_error("Parameter %u: scaled value out of range for LONG", i + 1);
						return FAILURE;
					}
					*(ISC_LONG *)data_ptr = (ISC_LONG)scaled;
				} else {
					zend_long lval = zval_get_long(b_var);
					*(ISC_LONG *)data_ptr = (ISC_LONG)lval;
				}
				break;
			}

			case SQL_INT64: {
				if (sql_scale < 0) {
					double dval = zval_get_double(b_var);
					double factor = pow(10.0, (double)(-sql_scale));
					*(ISC_INT64 *)data_ptr = (ISC_INT64)llround(dval * factor);
				} else {
					zend_long lval = zval_get_long(b_var);
					*(ISC_INT64 *)data_ptr = (ISC_INT64)lval;
				}
				break;
			}

			case SQL_FLOAT: {
				double dval = zval_get_double(b_var);
				*(float *)data_ptr = (float)dval;
				break;
			}

			case SQL_DOUBLE: {
				double dval = zval_get_double(b_var);
				*(double *)data_ptr = dval;
				break;
			}

			case SQL_TEXT: {
				/* Fixed-length CHAR field */
				convert_to_string(b_var);
				size_t str_len = Z_STRLEN_P(b_var);
				if (str_len > field_length) {
					str_len = field_length;
				}
				memcpy(data_ptr, Z_STRVAL_P(b_var), str_len);
				/* Pad with spaces for CHAR type */
				if (str_len < field_length) {
					memset(data_ptr + str_len, ' ', field_length - str_len);
				}
				break;
			}

			case SQL_VARYING: {
				/* VARCHAR: 2-byte length prefix + data */
				convert_to_string(b_var);
				size_t str_len = Z_STRLEN_P(b_var);
				if (str_len > field_length) {
					str_len = field_length;
				}
				*(short *)data_ptr = (short)str_len;
				memcpy(data_ptr + sizeof(short), Z_STRVAL_P(b_var), str_len);
				break;
			}

			case SQL_TIMESTAMP:
			case SQL_TYPE_DATE:
			case SQL_TYPE_TIME: {
				if (Z_TYPE_P(b_var) == IS_LONG) {
					/* Unix timestamp */
					struct tm t;
					time_t ts = (time_t)Z_LVAL_P(b_var);
					struct tm *res = php_gmtime_r(&ts, &t);
					if (!res) {
						_php_fbird_module_error("Parameter %u: invalid timestamp value", i + 1);
						return FAILURE;
					}
					switch (sql_type) {
						case SQL_TIMESTAMP:
							*(ISC_TIMESTAMP *)data_ptr = fbu_encode_timestamp(master,
								(unsigned)(t.tm_year + 1900), (unsigned)(t.tm_mon + 1),
								(unsigned)t.tm_mday, (unsigned)t.tm_hour,
								(unsigned)t.tm_min, (unsigned)t.tm_sec, 0);
							break;
						case SQL_TYPE_DATE:
							*(ISC_DATE *)data_ptr = fbu_encode_date(master,
								(unsigned)(t.tm_year + 1900), (unsigned)(t.tm_mon + 1),
								(unsigned)t.tm_mday);
							break;
						case SQL_TYPE_TIME:
							*(ISC_TIME *)data_ptr = fbu_encode_time(master,
								(unsigned)t.tm_hour, (unsigned)t.tm_min,
								(unsigned)t.tm_sec, 0);
							break;
					}
				} else {
					/* Parse string date/time */
					convert_to_string(b_var);
					fbird_datetime_components dt;
					int parsed = 0;
					switch (sql_type) {
						case SQL_TYPE_DATE:
							parsed = fbird_parse_date(Z_STRVAL_P(b_var), &dt);
							if (parsed) {
								*(ISC_DATE *)data_ptr = fbu_encode_date(master, dt.year, dt.month, dt.day);
							}
							break;
						case SQL_TYPE_TIME:
							parsed = fbird_parse_time(Z_STRVAL_P(b_var), &dt);
							if (parsed) {
								*(ISC_TIME *)data_ptr = fbu_encode_time(master, dt.hours, dt.minutes, dt.seconds, dt.fractions);
							}
							break;
						default: /* SQL_TIMESTAMP */
							parsed = fbird_parse_timestamp(Z_STRVAL_P(b_var), &dt);
							if (parsed) {
								*(ISC_TIMESTAMP *)data_ptr = fbu_encode_timestamp(master,
									dt.year, dt.month, dt.day, dt.hours, dt.minutes, dt.seconds, dt.fractions);
							}
							break;
					}
					if (!parsed) {
						_php_fbird_module_error("Parameter %u: invalid date/time string '%s'", i + 1, Z_STRVAL_P(b_var));
						return FAILURE;
					}
				}
				break;
			}

#if FB_API_VER >= 40
			case SQL_TIMESTAMP_TZ:
			case SQL_TIME_TZ: {
				fbird_datetime_components dt;
				fbird_datetime_init(&dt);
				strncpy(dt.timezone, "GMT", sizeof(dt.timezone) - 1);

				if (Z_TYPE_P(b_var) == IS_LONG) {
					struct tm t;
					time_t ts = (time_t)Z_LVAL_P(b_var);
					struct tm *res = php_gmtime_r(&ts, &t);
					if (!res) {
						_php_fbird_module_error("Parameter %u: invalid timestamp value", i + 1);
						return FAILURE;
					}
					dt.year = (unsigned)(t.tm_year + 1900);
					dt.month = (unsigned)(t.tm_mon + 1);
					dt.day = (unsigned)t.tm_mday;
					dt.hours = (unsigned)t.tm_hour;
					dt.minutes = (unsigned)t.tm_min;
					dt.seconds = (unsigned)t.tm_sec;
				} else {
					convert_to_string(b_var);
					int parsed = (sql_type == SQL_TIME_TZ)
						? fbird_parse_time(Z_STRVAL_P(b_var), &dt)
						: fbird_parse_timestamp(Z_STRVAL_P(b_var), &dt);
					if (!parsed) {
						_php_fbird_module_error("Parameter %u: invalid date/time string", i + 1);
						return FAILURE;
					}
					if (!dt.has_timezone) {
						strncpy(dt.timezone, "GMT", sizeof(dt.timezone) - 1);
					}
				}

				if (sql_type == SQL_TIME_TZ) {
					if (fbu_encode_time_tz(master, (ISC_TIME_TZ *)data_ptr,
							dt.hours, dt.minutes, dt.seconds, dt.fractions, dt.timezone) != 0) {
						_php_fbird_module_error("Parameter %u: failed to encode TIME WITH TIME ZONE", i + 1);
						return FAILURE;
					}
				} else {
					if (fbu_encode_timestamp_tz(master, (ISC_TIMESTAMP_TZ *)data_ptr,
							dt.year, dt.month, dt.day, dt.hours, dt.minutes, dt.seconds,
							dt.fractions, dt.timezone) != 0) {
						_php_fbird_module_error("Parameter %u: failed to encode TIMESTAMP WITH TIME ZONE", i + 1);
						return FAILURE;
					}
				}
				break;
			}
#endif

#ifdef SQL_BOOLEAN
			case SQL_BOOLEAN: {
				FB_BOOLEAN bval;
				switch (Z_TYPE_P(b_var)) {
					case IS_TRUE:
						bval = FB_TRUE;
						break;
					case IS_FALSE:
						bval = FB_FALSE;
						break;
					case IS_LONG:
					case IS_DOUBLE:
						bval = zend_is_true(b_var) ? FB_TRUE : FB_FALSE;
						break;
					case IS_STRING:
						if (Z_STRLEN_P(b_var) == 0) {
							bval = FB_FALSE;
						} else if (!zend_binary_strncasecmp(Z_STRVAL_P(b_var), Z_STRLEN_P(b_var), "true", 4, 4)) {
							bval = FB_TRUE;
						} else if (!zend_binary_strncasecmp(Z_STRVAL_P(b_var), Z_STRLEN_P(b_var), "false", 5, 5)) {
							bval = FB_FALSE;
						} else {
							zend_long lval;
							double dval;
							switch (is_numeric_string(Z_STRVAL_P(b_var), Z_STRLEN_P(b_var), &lval, &dval, 0)) {
								case IS_LONG:
									bval = (lval != 0) ? FB_TRUE : FB_FALSE;
									break;
								case IS_DOUBLE:
									bval = (dval != 0) ? FB_TRUE : FB_FALSE;
									break;
								default:
									_php_fbird_module_error("Parameter %u: cannot convert string to boolean", i + 1);
									return FAILURE;
							}
						}
						break;
					default:
						bval = zend_is_true(b_var) ? FB_TRUE : FB_FALSE;
						break;
				}
				*(FB_BOOLEAN *)data_ptr = bval;
				break;
			}
#endif

			case SQL_BLOB: {
				/* BLOB ID as hex string "HHHHHHHH:LLLL" (13 characters)
				 * Format: 8 hex digits (high 32-bit), colon, 4 hex digits (low 16-bit)
				 * Example: "74292B00:7FFC"
				 */
				convert_to_string(b_var);

				if (Z_STRLEN_P(b_var) == BLOB_ID_LEN &&
					_php_fbird_string_to_quad(Z_STRVAL_P(b_var), (ISC_QUAD *)data_ptr)) {
					/* Valid BLOB ID parsed and written to message buffer */
					break;
				}

				/* Invalid BLOB ID format */
				_php_fbird_module_error("Parameter %u: BLOB must be passed as blob ID (use fbird_batch_add_blob() or fbird_blob_create())", i + 1);
				return FAILURE;
			}

			default:
				_php_fbird_module_error("Parameter %u: unsupported SQL type %u for batch binding", i + 1, sql_type);
				return FAILURE;
		}
	}

	/* Add the populated message buffer to the batch */
	if (fbbatch_add(master, fb_batch->fbbatch_wrapper, 1, fb_batch->in_msg_buffer, status) != 1) {
		_php_fbird_error(status);
		return FAILURE;
	}

	return SUCCESS;
}

PHP_FUNCTION(fbird_batch_add)
{
	zval *batch_arg;
	zval *args = NULL;
	int argc = 0;
	fbird_batch *fb_batch;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z*", &batch_arg, &args, &argc) == FAILURE) {
		return;
	}

	fb_batch = fbird_batch_from_zval(batch_arg);

	if (fbird_batch_add_impl(fb_batch, args, argc) == FAILURE) {
		RETURN_FALSE;
	}

	RETURN_TRUE;
}

PHP_FUNCTION(fbird_batch_execute)
{
	ISC_STATUS status[256];
	zval *batch_arg;
	fbird_batch *fb_batch;
	void *trans_ptr;
	unsigned total_processed = 0;
	unsigned error_count = 0;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &batch_arg) == FAILURE) {
		return;
	}

	fb_batch = fbird_batch_from_zval(batch_arg);
	if (!fb_batch || !fb_batch->fbbatch_wrapper) {
		_php_fbird_module_error("Invalid batch resource");
		RETURN_FALSE;
	}

	if (!fb_batch->trans || !fb_batch->trans->fbt_transaction) {
		_php_fbird_module_error("Batch has no valid transaction");
		RETURN_FALSE;
	}

	trans_ptr = fbt_get_handle(fb_batch->trans->fbt_transaction);
	if (!trans_ptr) {
		_php_fbird_module_error("Failed to get transaction handle");
		RETURN_FALSE;
	}

	if (!fbbatch_execute(FBG(master_instance), fb_batch->fbbatch_wrapper, trans_ptr,
			&total_processed, &error_count, status)) {
		_php_fbird_error(status);
		RETURN_FALSE;
	}

	/* Close the batch after execution */
	fbbatch_close(FBG(master_instance), fb_batch->fbbatch_wrapper, status);
	fb_batch->fbbatch_wrapper = NULL;

	/* Calculate success_count from total_processed - error_count */
	unsigned success_count = (total_processed >= error_count) ? (total_processed - error_count) : 0;

	array_init(return_value);
	add_assoc_long(return_value, "total_processed", total_processed);
	add_assoc_long(return_value, "success_count", success_count);
	add_assoc_long(return_value, "error_count", error_count);
}

PHP_FUNCTION(fbird_batch_cancel)
{
	ISC_STATUS status[256];
	zval *batch_arg;
	fbird_batch *fb_batch;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &batch_arg) == FAILURE) {
		return;
	}

	fb_batch = fbird_batch_from_zval(batch_arg);
	if (!fb_batch || !fb_batch->fbbatch_wrapper) {
		_php_fbird_module_error("Invalid batch resource");
		RETURN_FALSE;
	}

	if (!fbbatch_cancel(FBG(master_instance), fb_batch->fbbatch_wrapper, status)) {
		_php_fbird_error(status);
		RETURN_FALSE;
	}

	fbbatch_close(FBG(master_instance), fb_batch->fbbatch_wrapper, status);
	fb_batch->fbbatch_wrapper = NULL;

	RETURN_TRUE;
}

PHP_FUNCTION(fbird_batch_add_blob)
{
	ISC_STATUS status[256];
	zval *batch_arg;
	char *data;
	size_t data_len;
	zend_long blob_type = 0;
	fbird_batch *fb_batch;
	ISC_QUAD blob_id;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "zs|l", &batch_arg, &data, &data_len, &blob_type) == FAILURE) {
		return;
	}

	fb_batch = fbird_batch_from_zval(batch_arg);
	if (!fb_batch || !fb_batch->fbbatch_wrapper) {
		_php_fbird_module_error("Invalid batch resource");
		RETURN_FALSE;
	}

	/* Call C++ wrapper to add BLOB to batch */
	if (fbbatch_add_blob(FBG(master_instance), fb_batch->fbbatch_wrapper,
			(unsigned)data_len, data, &blob_id, 0, NULL, status) == 0) {
		_php_fbird_error(status);
		RETURN_FALSE;
	}

	/* Convert BLOB ID to hex string for PHP */
	RETURN_NEW_STR(_php_fbird_quad_to_string(blob_id));
}

PHP_FUNCTION(fbird_batch_register_blob)
{
	ISC_STATUS status[256];
	zval *batch_arg;
	char *blob_id_str;
	size_t blob_id_len;
	fbird_batch *fb_batch;
	ISC_QUAD existing_blob, batch_blob_id;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "zs", &batch_arg, &blob_id_str, &blob_id_len) == FAILURE) {
		return;
	}

	fb_batch = fbird_batch_from_zval(batch_arg);
	if (!fb_batch || !fb_batch->fbbatch_wrapper) {
		_php_fbird_module_error("Invalid batch resource");
		RETURN_FALSE;
	}

	/* Validate and convert BLOB ID string to ISC_QUAD */
	if (blob_id_len != BLOB_ID_LEN || !_php_fbird_string_to_quad(blob_id_str, &existing_blob)) {
		_php_fbird_module_error("Invalid BLOB ID format (expected %d character hex string)", BLOB_ID_LEN);
		RETURN_FALSE;
	}

	/* Call C++ wrapper to register BLOB in batch */
	if (fbbatch_register_blob(FBG(master_instance), fb_batch->fbbatch_wrapper,
			&existing_blob, &batch_blob_id, status) == 0) {
		_php_fbird_error(status);
		RETURN_FALSE;
	}

	/* Convert batch BLOB ID to hex string for PHP */
	RETURN_NEW_STR(_php_fbird_quad_to_string(batch_blob_id));
}

/* {{{ proto int|false fbird_batch_get_blob_alignment(resource batch)
   Returns the BLOB alignment requirement for this batch in bytes, or false on error.
   The alignment value is needed when embedding inline BLOBs via fbird_batch_add_blob(). */
PHP_FUNCTION(fbird_batch_get_blob_alignment)
{
	ISC_STATUS status[256];
	zval *batch_arg;
	fbird_batch *fb_batch;
	unsigned alignment;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &batch_arg) == FAILURE) {
		return;
	}

	fb_batch = fbird_batch_from_zval(batch_arg);
	if (!fb_batch || !fb_batch->fbbatch_wrapper) {
		_php_fbird_module_error("Invalid batch resource");
		RETURN_FALSE;
	}

	alignment = fbbatch_get_blob_alignment(FBG(master_instance), fb_batch->fbbatch_wrapper, status);
	if (alignment == 0) {
		_php_fbird_error(status);
		RETURN_FALSE;
	}

	RETURN_LONG((zend_long)alignment);
}

PHP_FUNCTION(fbird_batch_append_blob_data)
{
	ISC_STATUS status[256];
	zval *batch_arg;
	char *data;
	size_t data_len;
	fbird_batch *fb_batch;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "zs", &batch_arg, &data, &data_len) == FAILURE) {
		return;
	}

	fb_batch = fbird_batch_from_zval(batch_arg);
	if (!fb_batch || !fb_batch->fbbatch_wrapper) {
		_php_fbird_module_error("Invalid batch resource");
		RETURN_FALSE;
	}

	if (!fbbatch_append_blob_data(FBG(master_instance), fb_batch->fbbatch_wrapper,
			(unsigned)data_len, data, status)) {
		_php_fbird_error(status);
		RETURN_FALSE;
	}

	RETURN_TRUE;
}

PHP_FUNCTION(fbird_batch_add_blob_stream)
{
	ISC_STATUS status[256];
	zval *batch_arg;
	char *data;
	size_t data_len;
	fbird_batch *fb_batch;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "zs", &batch_arg, &data, &data_len) == FAILURE) {
		return;
	}

	fb_batch = fbird_batch_from_zval(batch_arg);
	if (!fb_batch || !fb_batch->fbbatch_wrapper) {
		_php_fbird_module_error("Invalid batch resource");
		RETURN_FALSE;
	}

	if (!fbbatch_add_blob_stream(FBG(master_instance), fb_batch->fbbatch_wrapper,
			(unsigned)data_len, data, status)) {
		_php_fbird_error(status);
		RETURN_FALSE;
	}

	RETURN_TRUE;
}

PHP_FUNCTION(fbird_batch_set_default_bpb)
{
	ISC_STATUS status[256];
	zval *batch_arg;
	char *bpb;
	size_t bpb_len;
	fbird_batch *fb_batch;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "zs", &batch_arg, &bpb, &bpb_len) == FAILURE) {
		return;
	}

	fb_batch = fbird_batch_from_zval(batch_arg);
	if (!fb_batch || !fb_batch->fbbatch_wrapper) {
		_php_fbird_module_error("Invalid batch resource");
		RETURN_FALSE;
	}

	if (!fbbatch_set_default_bpb(FBG(master_instance), fb_batch->fbbatch_wrapper,
			(unsigned)bpb_len, (const unsigned char *)bpb, status)) {
		_php_fbird_error(status);
		RETURN_FALSE;
	}

	RETURN_TRUE;
}

#endif /* FB_API_VER >= 40 */

#endif /* HAVE_FIREBIRD */
