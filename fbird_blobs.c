/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"

#if HAVE_FIREBIRD

#include "php_firebird.h"
#include "php_fbird_includes.h"
#include "firebird_utils.h"

#define BLOB_CLOSE		1
#define BLOB_CANCEL		2

/* Stream Wrapper Definitions */
typedef struct {
	fbird_blob *ib_blob;
} fbird_blob_stream_data;

static ssize_t fbird_blob_stream_write(php_stream *stream, const char *buf, size_t count)
{
	fbird_blob_stream_data *data = (fbird_blob_stream_data *)stream->abstract;
	fbird_blob *ib_blob = data->ib_blob;
	size_t total_written = 0;
	unsigned chunk_size;

	if (!ib_blob || !ib_blob->fbb_blob) {
		return 0;
	}

	/*
	 * Firebird 3.0+ OO API Blob Write (Stream)
	 *
	 * Uses IBlob::putSegment() via fbb_put_segment() wrapper.
	 * Note: Firebird 3.0+ is required - compile-time enforced in php_fbird_includes.h
	 */
	while (count > 0) {
		chunk_size = count > USHRT_MAX ? USHRT_MAX : (unsigned)count;

		if (fbb_put_segment(IBG(master_instance), ib_blob->fbb_blob, chunk_size, buf, IB_STATUS) == 0) {
			/* Error handling */
			return total_written; /* Return what we managed to write */
		}

		buf += chunk_size;
		count -= chunk_size;
		total_written += chunk_size;
	}

	return total_written;
}

static ssize_t fbird_blob_stream_read(php_stream *stream, char *buf, size_t count)
{
	fbird_blob_stream_data *data = (fbird_blob_stream_data *)stream->abstract;
	fbird_blob *ib_blob = data->ib_blob;
	size_t total_read = 0;
	unsigned actual_len;
	int result;

	if (!ib_blob || !ib_blob->fbb_blob) {
		return 0;
	}

	/*
	 * Firebird 3.0+ OO API Blob Read (Stream)
	 *
	 * Uses IBlob::getSegment() via fbb_get_segment() wrapper.
	 * Note: Firebird 3.0+ is required - compile-time enforced in php_fbird_includes.h
	 */
	while (count > 0) {
		unsigned chunk_size = count > USHRT_MAX ? USHRT_MAX : (unsigned)count;

		result = fbb_get_segment(
			IBG(master_instance),
			ib_blob->fbb_blob,
			chunk_size,
			buf,
			&actual_len,
			IB_STATUS
		);

		if (result >= 0 && actual_len > 0) {
			/* Success - got some data */
			buf += actual_len;
			count -= actual_len;
			total_read += actual_len;
		}
		if (result == 1) {
			/* EOF */
			break;
		}
		if (result < 0) {
			/* Error */
			break;
		}
	}

	return total_read;
}

static int fbird_blob_stream_close(php_stream *stream, int close_handle)
{
	fbird_blob_stream_data *data = (fbird_blob_stream_data *)stream->abstract;
	fbird_blob *ib_blob = data->ib_blob;

	/*
	 * Firebird 3.0+ OO API Blob Close (Stream)
	 *
	 * Uses IBlob::close() via fbb_close() wrapper.
	 * Note: Firebird 3.0+ is required - compile-time enforced in php_fbird_includes.h
	 */
	if (ib_blob) {
		if (ib_blob->fbb_blob) {
			/* fbb_close returns 1 on success, 0 on error */
			if (fbb_close(IBG(master_instance), ib_blob->fbb_blob, IB_STATUS) == 0) {
				_php_fbird_error();
			}
			fbb_free(ib_blob->fbb_blob);
			ib_blob->fbb_blob = NULL;
		}
		efree(ib_blob);
		data->ib_blob = NULL;
	}
	efree(data);
	return 0;
}

static int fbird_blob_stream_flush(php_stream *stream)
{
	/* Firebird blobs are written immediately via put_segment, no explicit flush needed */
	return 0;
}

static const php_stream_ops fbird_blob_stream_ops = {
	fbird_blob_stream_write,
	fbird_blob_stream_read,
	fbird_blob_stream_close,
	fbird_blob_stream_flush,
	"fbird_blob",
	NULL, /* seek not supported for sequential blobs */
	NULL, /* cast */
	NULL, /* stat */
	NULL  /* set_option */
};

#define PARSE_PARAMETERS \
	switch (ZEND_NUM_ARGS()) { \
		default: \
			WRONG_PARAM_COUNT; \
		case 1: \
			if (FAILURE == zend_parse_parameters(1, "s", &blob_id, &blob_id_len)) { \
				RETURN_FALSE; \
			} \
			break; \
		case 2: \
			if (FAILURE == zend_parse_parameters(2, "rs", &link, &blob_id, &blob_id_len)) { \
				RETURN_FALSE; \
			} \
			break; \
	} \

/* BPB for stream-mode blob opening (enables seeking) */
static const unsigned char stream_bpb[] = {
	isc_bpb_version1,
	isc_bpb_type, 1, isc_bpb_type_stream
};

static int le_blob;

static void _php_fbird_free_blob(zend_resource *rsrc)
{
	fbird_blob *ib_blob = (fbird_blob *)rsrc->ptr;

	/*
	 * Firebird 3.0+ OO API Blob Cancel (Resource Cleanup)
	 *
	 * Uses IBlob::cancel() via fbb_cancel() wrapper.
	 * Note: Firebird 3.0+ is required - compile-time enforced in php_fbird_includes.h
	 */
	if (ib_blob->fbb_blob != NULL) { /* blob open */
		/* fbb_cancel returns 1 on success, 0 on error */
		if (fbb_cancel(IBG(master_instance), ib_blob->fbb_blob, IB_STATUS) == 0) {
			/* If the blob handle is invalid (e.g. transaction committed/rolled back),
			 * we can safely ignore the error as there's nothing to cancel/close. */
			if (IB_STATUS[1] != isc_bad_segstr_handle) {
				_php_fbird_module_error("You can lose data. Close any blob after reading from or "
					"writing to it. Use fbird_blob_close() before calling fbird_close()");
			}
		}
		fbb_free(ib_blob->fbb_blob);
		ib_blob->fbb_blob = NULL;
	}
	efree(ib_blob);
}

void php_fbird_blobs_minit(INIT_FUNC_ARGS)
{
	le_blob = zend_register_list_destructors_ex(_php_fbird_free_blob, NULL,
		LE_BLOB, module_number);
}

int _php_fbird_string_to_quad(char const *id, ISC_QUAD *qd)
{
	/* Parse format "HHHHHHHH:LLLL" (8 hex digits : 4 hex digits)
	 * Example: "74292B00:7FFC"
	 */
	unsigned int high_part;
	unsigned short low_part;

	if (sscanf(id, "%x:%hx", &high_part, &low_part) == 2) {
		qd->gds_quad_high = (ISC_LONG)high_part;
		qd->gds_quad_low = (ISC_USHORT)low_part;
		return 1;
	}

	return 0;
}

zend_string *_php_fbird_quad_to_string(ISC_QUAD const qd)
{
	/* Format: "HHHHHHHH:LLLL" (8 hex digits : 4 hex digits) for batch API compatibility
	 * Example: "74292B00:7FFC" (13 characters total) */
	return strpprintf(0, "%08x:%04hx", qd.gds_quad_high, (unsigned short)qd.gds_quad_low);
}

typedef struct {
	ISC_LONG  max_segment;
	ISC_LONG  num_segments;
	ISC_LONG  total_length;
	int       bl_stream;
} FBIRD_BLOBINFO;

int _php_fbird_blob_get(zval *return_value, fbird_blob *ib_blob, zend_ulong max_len)
{
	/* Safety check: verify blob handle is valid before any operation */
	if (!ib_blob || !ib_blob->fbb_blob) {
		_php_fbird_module_error("BLOB handle is invalid or has been closed");
		return FAILURE;
	}

	if (ib_blob->bl_qd.gds_quad_high || ib_blob->bl_qd.gds_quad_low) { /*not null ?*/

		zend_string *bl_data;
		zend_ulong cur_len;

		/*
		 * Allocate buffer with space for a trailing NUL.
		 * zend_string_alloc() allocates (len + 1) bytes for ZSTR_VAL().
		 *
		 * This prevents a 1-byte heap overwrite when cur_len reaches max_len
		 * and we write the terminator at ZSTR_VAL(bl_data)[cur_len].
		 */
		bl_data = zend_string_alloc(max_len, 0);

		/*
		 * Firebird 3.0+ OO API Blob Read
		 *
		 * Uses IBlob::getSegment() via fbb_get_segment() wrapper.
		 * Return codes from fbb_get_segment:
		 *   0 = success, more data available
		 *   1 = EOF (end of blob, no more data)
		 *   2 = partial segment (data returned, more in current segment)
		 *  -1 = error
		 *
		 * Note: Legacy isc_segstr_eof/isc_segment codes are NOT used by OO API.
		 */
		int result;
		unsigned actual_len = 0;

		for (cur_len = 0; cur_len < max_len; ) {
			unsigned chunk_size = (max_len - cur_len) > USHRT_MAX ? USHRT_MAX
				: (unsigned)(max_len - cur_len);

			actual_len = 0;  /* Reset before each call */
			result = fbb_get_segment(
				IBG(master_instance),
				ib_blob->fbb_blob,
				chunk_size,
				&ZSTR_VAL(bl_data)[cur_len],
				&actual_len,
				IB_STATUS
			);

			/* Handle OO API return codes */
			if (result == 1) {
				/* EOF - end of blob reached, this is normal completion */
				break;
			}
			if (result < 0) {
				/* Error */
				zend_string_free(bl_data);
				_php_fbird_error();
				return FAILURE;
			}
			/* result == 0 (success) or result == 2 (partial segment): data was read */
			if (actual_len == 0) {
				/* No data returned despite success code - treat as EOF */
				break;
			}
			/* Safety: cap actual_len to prevent buffer overflow if Firebird misbehaves */
			if (actual_len > chunk_size) {
				actual_len = chunk_size;
			}
			cur_len += actual_len;
		}

		ZSTR_VAL(bl_data)[cur_len] = '\0';
		ZSTR_LEN(bl_data) = cur_len;
		RETVAL_NEW_STR(bl_data);
	} else { /* null blob */
		RETVAL_EMPTY_STRING(); /* empty string */
	}
	return SUCCESS;
}

int _php_fbird_blob_add(zval *string_arg, fbird_blob *ib_blob)
{
	zend_ulong put_cnt = 0, rem_cnt;
	zend_string *str;

	/* Safety check: verify blob handle is valid before any operation */
	if (!ib_blob || !ib_blob->fbb_blob) {
		_php_fbird_module_error("BLOB handle is invalid or has been closed");
		return FAILURE;
	}

	str = zval_get_string(string_arg);

	/*
	 * Firebird 3.0+ OO API Blob Write
	 *
	 * Uses IBlob::putSegment() via fbb_put_segment() wrapper.
	 * Note: Firebird 3.0+ is required - compile-time enforced in php_fbird_includes.h
	 */
	for (rem_cnt = ZSTR_LEN(str); rem_cnt > 0; ) {
		unsigned chunk_size = rem_cnt > USHRT_MAX ? USHRT_MAX : (unsigned)rem_cnt;

		/* fbb_put_segment returns 1 on success, 0 on error */
		if (fbb_put_segment(IBG(master_instance), ib_blob->fbb_blob, chunk_size,
				&ZSTR_VAL(str)[put_cnt], IB_STATUS) == 0) {
			_php_fbird_error();
			zend_string_release(str);
			return FAILURE;
		}
		put_cnt += chunk_size;
		rem_cnt -= chunk_size;
	}
	zend_string_release(str);
	return SUCCESS;
}

static int _php_fbird_blob_info_oo(void *fbb_blob, FBIRD_BLOBINFO *bl_info)
{
	/*
	 * Firebird 3.0+ OO API Blob Info
	 *
	 * Uses IBlob::getInfo() via fbb_get_info() wrapper.
	 * Note: Firebird 3.0+ is required - compile-time enforced in php_fbird_includes.h
	 */
	static unsigned char bl_items[] = {
		isc_info_blob_num_segments,
		isc_info_blob_max_segment,
		isc_info_blob_total_length,
		isc_info_blob_type
	};

	unsigned char bl_inf[sizeof(zend_long)*8];
	unsigned char *p;

	bl_info->max_segment = 0;
	bl_info->num_segments = 0;
	bl_info->total_length = 0;
	bl_info->bl_stream = 0;

	/* fbb_get_info returns 1 on success, 0 on error */
	if (fbb_get_info(IBG(master_instance), fbb_blob, sizeof(bl_items), bl_items,
			sizeof(bl_inf), bl_inf, IB_STATUS) == 0) {
		_php_fbird_error();
		return FAILURE;
	}

	for (p = bl_inf; *p != isc_info_end && p < bl_inf + sizeof(bl_inf);) {
		unsigned short item_len;
		int item = *p++;

		item_len = (unsigned short)isc_vax_integer((char *)p, 2);
		p += 2;
		switch (item) {
			case isc_info_blob_num_segments:
				bl_info->num_segments = isc_vax_integer((char *)p, item_len);
				break;
			case isc_info_blob_max_segment:
				bl_info->max_segment = isc_vax_integer((char *)p, item_len);
				break;
			case isc_info_blob_total_length:
				bl_info->total_length = isc_vax_integer((char *)p, item_len);
				break;
			case isc_info_blob_type:
				bl_info->bl_stream = isc_vax_integer((char *)p, item_len);
				break;
			case isc_info_truncated:
			case isc_info_error:  /* hmm. don't think so...*/
				_php_fbird_module_error("PHP module internal error");
				return FAILURE;
			default:
				break;
		} /* switch */
		p += item_len;
	} /* for */
	return SUCCESS;
}

PHP_FUNCTION(fbird_blob_create)
{
	zval *link = NULL;
	fbird_db_link *ib_link;
	fbird_transaction *trans = NULL;
	fbird_blob *ib_blob;

	RESET_ERRMSG;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS(), "|r", &link)) {
		RETURN_FALSE;
	}

	PHP_FBIRD_LINK_TRANS(link, ib_link, trans);

	ib_blob = (fbird_blob *) emalloc(sizeof(fbird_blob));
	ib_blob->type = BLOB_INPUT;
	ib_blob->fbb_blob = NULL;  /* Phase 6: explicit fbb_blob init */

	/*
	 * Firebird 3.0+ OO API Blob Creation
	 *
	 * Uses IBlob interface via fbb_create() wrapper.
	 * Note: Firebird 3.0+ is required - compile-time enforced in php_fbird_includes.h
	 */
	void *trans_handle = fbt_get_handle(trans->fbt_transaction);
	ib_blob->fbb_blob = fbb_create(
		IBG(master_instance),
		fbc_get_attachment(ib_link->fbc_connection),
		trans_handle,
		&ib_blob->bl_qd,
		0, NULL,  /* No BPB */
		IB_STATUS
	);
	if (ib_blob->fbb_blob == NULL) {
		_php_fbird_error();
		efree(ib_blob);
		RETURN_FALSE;
	}

	RETVAL_RES(zend_register_resource(ib_blob, le_blob));
}

PHP_FUNCTION(fbird_blob_create_seekable)
{
	zval *link = NULL;
	fbird_db_link *ib_link;
	fbird_transaction *trans = NULL;
	fbird_blob *ib_blob;

	RESET_ERRMSG;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS(), "|r", &link)) {
		RETURN_FALSE;
	}

	PHP_FBIRD_LINK_TRANS(link, ib_link, trans);

	ib_blob = (fbird_blob *) emalloc(sizeof(fbird_blob));
	ib_blob->type = BLOB_INPUT;
	ib_blob->fbb_blob = NULL;

	/*
	 * Firebird 3.0+ OO API Blob Creation (Stream Mode)
	 *
	 * Uses IBlob interface via fbb_create() wrapper with stream BPB.
	 * Stream mode enables seeking via fbird_blob_seek().
	 */
	void *trans_handle = fbt_get_handle(trans->fbt_transaction);
	ib_blob->fbb_blob = fbb_create(
		IBG(master_instance),
		fbc_get_attachment(ib_link->fbc_connection),
		trans_handle,
		&ib_blob->bl_qd,
		sizeof(stream_bpb), stream_bpb,  /* Stream BPB for seek support */
		IB_STATUS
	);
	if (ib_blob->fbb_blob == NULL) {
		_php_fbird_error();
		efree(ib_blob);
		RETURN_FALSE;
	}

	RETVAL_RES(zend_register_resource(ib_blob, le_blob));
}

PHP_FUNCTION(fbird_blob_open)
{
	char *blob_id;
	size_t blob_id_len;
	zval *link = NULL;
	fbird_db_link *ib_link;
	fbird_transaction *trans = NULL;
	fbird_blob *ib_blob;

	RESET_ERRMSG;
	PARSE_PARAMETERS;

	PHP_FBIRD_LINK_TRANS(link, ib_link, trans);

	ib_blob = (fbird_blob *) emalloc(sizeof(fbird_blob));
	ib_blob->type = BLOB_OUTPUT;
	ib_blob->fbb_blob = NULL;  /* Phase 6: explicit fbb_blob init */

	do {
		if (! _php_fbird_string_to_quad(blob_id, &ib_blob->bl_qd)) {
			_php_fbird_module_error("String is not a BLOB ID");
			break;
		}

		/*
		 * Firebird 3.0+ OO API Blob Open
		 *
		 * Uses IBlob interface via fbb_open() wrapper.
		 * Note: Firebird 3.0+ is required - compile-time enforced in php_fbird_includes.h
		 */
		void *trans_handle = fbt_get_handle(trans->fbt_transaction);
		ib_blob->fbb_blob = fbb_open(
			IBG(master_instance),
			fbc_get_attachment(ib_link->fbc_connection),
			trans_handle,
			&ib_blob->bl_qd,
			0, NULL,  /* No BPB */
			IB_STATUS
		);
		if (ib_blob->fbb_blob == NULL) {
			_php_fbird_error();
			break;
		}

		RETVAL_RES(zend_register_resource(ib_blob, le_blob));
		return;

	} while (0);

	efree(ib_blob);
	RETURN_FALSE;
}

PHP_FUNCTION(fbird_blob_add)
{
	zval *blob_arg, *string_arg;
	fbird_blob *ib_blob;

	RESET_ERRMSG;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS(), "rz", &blob_arg, &string_arg)) {
		return;
	}

	ib_blob = (fbird_blob *)zend_fetch_resource_ex(blob_arg, NULL, le_blob);

	if (!ib_blob) {
		RETURN_FALSE;
	}

	if (ib_blob->type != BLOB_INPUT) {
		_php_fbird_module_error("BLOB is not open for input");
		RETURN_FALSE;
	}

	if (_php_fbird_blob_add(string_arg, ib_blob) != SUCCESS) {
		RETURN_FALSE;
	}
	RETURN_TRUE;
}

PHP_FUNCTION(fbird_blob_get)
{
	zval *blob_arg;
	zend_ulong len_arg;
	fbird_blob *ib_blob;

	RESET_ERRMSG;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS(), "rl", &blob_arg, &len_arg)) {
		return;
	}

	ib_blob = (fbird_blob *)zend_fetch_resource_ex(blob_arg, LE_BLOB, le_blob);

	if (!ib_blob) {
		RETURN_FALSE;
	}

	if (ib_blob->type != BLOB_OUTPUT) {
		_php_fbird_module_error("BLOB is not open for output");
		RETURN_FALSE;
	}

	if (_php_fbird_blob_get(return_value, ib_blob, len_arg) != SUCCESS) {
		RETURN_FALSE;
	}
}

static void _php_fbird_blob_end(INTERNAL_FUNCTION_PARAMETERS, int bl_end)
{
	zval *blob_arg;
	fbird_blob *ib_blob;

	RESET_ERRMSG;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS(), "r", &blob_arg)) {
		return;
	}

	ib_blob = (fbird_blob *)zend_fetch_resource_ex(blob_arg, NULL, le_blob);

	if (!ib_blob) {
		RETURN_FALSE;
	}

	if (bl_end == BLOB_CLOSE) { /* return id here */

		/*
		 * Firebird 3.0+ OO API Blob Close
		 *
		 * Uses IBlob::close() via fbb_close() wrapper.
		 * Note: Firebird 3.0+ is required - compile-time enforced in php_fbird_includes.h
		 */
		if (ib_blob->bl_qd.gds_quad_high || ib_blob->bl_qd.gds_quad_low) { /*not null ?*/
			/* fbb_close returns 1 on success, 0 on error */
			if (fbb_close(IBG(master_instance), ib_blob->fbb_blob, IB_STATUS) == 0) {
				_php_fbird_error();
				RETURN_FALSE;
			}
		}
		fbb_free(ib_blob->fbb_blob);
		ib_blob->fbb_blob = NULL;

		RETVAL_NEW_STR(_php_fbird_quad_to_string(ib_blob->bl_qd));
	} else { /* discard created blob */
		/*
		 * Firebird 3.0+ OO API Blob Cancel
		 *
		 * Uses IBlob::cancel() via fbb_cancel() wrapper.
		 * Note: Firebird 3.0+ is required - compile-time enforced in php_fbird_includes.h
		 */
		/* fbb_cancel returns 1 on success, 0 on error */
		if (fbb_cancel(IBG(master_instance), ib_blob->fbb_blob, IB_STATUS) == 0) {
			_php_fbird_error();
			RETURN_FALSE;
		}
		fbb_free(ib_blob->fbb_blob);
		ib_blob->fbb_blob = NULL;

		RETVAL_TRUE;
	}
	/*
	 * Do NOT destroy the resource here.
	 *
	 * The userland zval holding this resource is still alive, and PHP will
	 * attempt to release it later. Using zend_list_delete() here can cause:
	 * - resource ID reuse while a zval still references it
	 * - double-dtor paths
	 * - heap corruption (observed in Issue #10 reproduction)
	 *
	 * Instead, close the resource so it becomes invalid for further use,
	 * while letting normal zval lifetime management free it exactly once.
	 */
	zend_list_close(Z_RES_P(blob_arg));
}

PHP_FUNCTION(fbird_blob_close)
{
	_php_fbird_blob_end(INTERNAL_FUNCTION_PARAM_PASSTHRU, BLOB_CLOSE);
}

PHP_FUNCTION(fbird_blob_cancel)
{
	_php_fbird_blob_end(INTERNAL_FUNCTION_PARAM_PASSTHRU, BLOB_CANCEL);
}

PHP_FUNCTION(fbird_blob_info)
{
	char *blob_id = NULL;
	size_t blob_id_len;
	zval *link = NULL, *arg1 = NULL;
	fbird_db_link *ib_link;
	fbird_transaction *trans = NULL;
	fbird_blob ib_blob = { {0}, BLOB_INPUT, {0, 0}, NULL };  /* Phase 6: explicit fbb_blob init */
	FBIRD_BLOBINFO bl_info;
	php_stream *stream = NULL;
	fbird_blob *ext_blob = NULL;

	RESET_ERRMSG;

	// Handle different call signatures: (link, id/stream) or (id/stream)
	if (ZEND_NUM_ARGS() == 1) {
		if (zend_parse_parameters(1, "z", &arg1) == FAILURE) RETURN_FALSE;
	} else if (ZEND_NUM_ARGS() == 2) {
		if (zend_parse_parameters(2, "rz", &link, &arg1) == FAILURE) RETURN_FALSE;
	} else {
		WRONG_PARAM_COUNT;
	}

	// Check if arg1 is a stream or blob resource
	if (arg1 && Z_TYPE_P(arg1) == IS_RESOURCE) {
		stream = (php_stream *)zend_fetch_resource_ex(arg1, NULL, php_file_le_stream());
		if (stream && stream->ops == &fbird_blob_stream_ops) {
			fbird_blob_stream_data *data = (fbird_blob_stream_data *)stream->abstract;
			if (data && data->ib_blob) {
				ext_blob = data->ib_blob;
				ib_blob = *ext_blob; // Copy struct content including handle and quad
			}
		} else {
			ext_blob = (fbird_blob *)zend_fetch_resource_ex(arg1, NULL, le_blob);
			if (ext_blob) {
				ib_blob = *ext_blob;
			}
		}
	} else if (arg1 && Z_TYPE_P(arg1) == IS_STRING) {
		blob_id = Z_STRVAL_P(arg1);
		blob_id_len = Z_STRLEN_P(arg1);
	} else {
		// Invalid argument type
		php_error_docref(NULL, E_WARNING, "Expected blob ID string or blob stream resource");
		RETURN_FALSE;
	}

	if (ext_blob) {
		/*
		 * Firebird 3.0+ OO API Blob Info (from stream)
		 *
		 * Uses IBlob::getInfo() via fbb_get_info() wrapper.
		 */
		if (_php_fbird_blob_info_oo(ext_blob->fbb_blob, &bl_info)) {
			RETURN_FALSE;
		}
	} else {
		// Using a blob ID string
		if (!link) {
			// Fetch default link if not provided
			link = IBG(default_link) ? (zval *)IBG(default_link) : NULL;
		}
		PHP_FBIRD_LINK_TRANS(link, ib_link, trans);

		if (!blob_id || ! _php_fbird_string_to_quad(blob_id, &ib_blob.bl_qd)) {
			_php_fbird_module_error("Unrecognized BLOB ID");
			RETURN_FALSE;
		}

		if (ib_blob.bl_qd.gds_quad_high || ib_blob.bl_qd.gds_quad_low) { /* not null ? */
			/*
			 * Firebird 3.0+ OO API Blob Open/Info/Close
			 *
			 * Uses IBlob interface via fbb_open(), fbb_get_info(), fbb_close() wrappers.
			 */
			void *trans_handle = fbt_get_handle(trans->fbt_transaction);
			ib_blob.fbb_blob = fbb_open(
				IBG(master_instance),
				fbc_get_attachment(ib_link->fbc_connection),
				trans_handle,
				&ib_blob.bl_qd,
				0, NULL,
				IB_STATUS
			);
			if (!ib_blob.fbb_blob) {
				_php_fbird_error();
				RETURN_FALSE;
			}

			if (_php_fbird_blob_info_oo(ib_blob.fbb_blob, &bl_info)) {
				fbb_free(ib_blob.fbb_blob);
				RETURN_FALSE;
			}
			if (fbb_close(IBG(master_instance), ib_blob.fbb_blob, IB_STATUS) == 0) {
				fbb_free(ib_blob.fbb_blob);
				_php_fbird_error();
				RETURN_FALSE;
			}
			fbb_free(ib_blob.fbb_blob);
			ib_blob.fbb_blob = NULL;
		} else { /* null blob */
			bl_info.max_segment = 0;
			bl_info.num_segments = 0;
			bl_info.total_length = 0;
			bl_info.bl_stream = 0;
		}
	}

	array_init(return_value);

	add_index_long(return_value, 0, bl_info.total_length);
 	add_assoc_long(return_value, "length", bl_info.total_length);

	add_index_long(return_value, 1, bl_info.num_segments);
 	add_assoc_long(return_value, "numseg", bl_info.num_segments);

	add_index_long(return_value, 2, bl_info.max_segment);
 	add_assoc_long(return_value, "maxseg", bl_info.max_segment);

	add_index_bool(return_value, 3, bl_info.bl_stream);
 	add_assoc_bool(return_value, "stream", bl_info.bl_stream);

	add_index_bool(return_value, 4, (!ib_blob.bl_qd.gds_quad_high && !ib_blob.bl_qd.gds_quad_low));
 	add_assoc_bool(return_value, "isnull", (!ib_blob.bl_qd.gds_quad_high && !ib_blob.bl_qd.gds_quad_low));

	// Add Blob ID to output
	zend_string *str_id = _php_fbird_quad_to_string(ib_blob.bl_qd);
	add_assoc_str(return_value, "id", str_id);
}

PHP_FUNCTION(fbird_blob_echo)
{
	char *blob_id;
	size_t blob_id_len;
	zval *link = NULL;
	fbird_db_link *ib_link;
	fbird_transaction *trans = NULL;
	fbird_blob ib_blob_id = { {0}, BLOB_OUTPUT, {0, 0}, NULL };  /* Phase 6: explicit fbb_blob init */
	char bl_data[FBIRD_BLOB_SEG];
	unsigned actual_len;
	int result;

	RESET_ERRMSG;
	PARSE_PARAMETERS;

	PHP_FBIRD_LINK_TRANS(link, ib_link, trans);

	if (! _php_fbird_string_to_quad(blob_id, &ib_blob_id.bl_qd)) {
		_php_fbird_module_error("Unrecognized BLOB ID");
		RETURN_FALSE;
	}

	do {
		/*
		 * Firebird 3.0+ OO API Blob Echo
		 *
		 * Uses IBlob interface via fbb_open(), fbb_get_segment(), fbb_close() wrappers.
		 */
		void *trans_handle = fbt_get_handle(trans->fbt_transaction);
		ib_blob_id.fbb_blob = fbb_open(
			IBG(master_instance),
			fbc_get_attachment(ib_link->fbc_connection),
			trans_handle,
			&ib_blob_id.bl_qd,
			0, NULL,
			IB_STATUS
		);
		if (!ib_blob_id.fbb_blob) {
			break;
		}

		/* Read and output blob segments */
		do {
			result = fbb_get_segment(
				IBG(master_instance),
				ib_blob_id.fbb_blob,
				sizeof(bl_data),
				bl_data,
				&actual_len,
				IB_STATUS
			);
			if (result >= 0 && actual_len > 0) {
				PHPWRITE(bl_data, actual_len);
			}
		} while (result == 0 || result == 2);  /* 0 = success with more, 2 = segment */

		if (result < 0 && result != 1) {  /* 1 = EOF is OK */
			fbb_close(IBG(master_instance), ib_blob_id.fbb_blob, IB_STATUS);
			fbb_free(ib_blob_id.fbb_blob);
			break;
		}

		if (fbb_close(IBG(master_instance), ib_blob_id.fbb_blob, IB_STATUS) == 0) {
			fbb_free(ib_blob_id.fbb_blob);
			break;
		}
		fbb_free(ib_blob_id.fbb_blob);
		RETURN_TRUE;
	} while (0);

	_php_fbird_error();
	RETURN_FALSE;
}

PHP_FUNCTION(fbird_blob_import)
{
	zval *link = NULL, *file;
	int size;
	unsigned b;
	fbird_blob ib_blob = { {0}, 0, {0, 0}, NULL };  /* Phase 6: explicit fbb_blob init */
	fbird_db_link *ib_link;
	fbird_transaction *trans = NULL;
	char bl_data[FBIRD_BLOB_SEG];
	php_stream *stream;

	RESET_ERRMSG;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS(), "r|r",
			(ZEND_NUM_ARGS()-1) ? &link : &file, &file)) {
		RETURN_FALSE;
	}

	PHP_FBIRD_LINK_TRANS(link, ib_link, trans);

	php_stream_from_zval(stream, file);

	do {
		/*
		 * Firebird 3.0+ OO API Blob Import
		 *
		 * Uses IBlob interface via fbb_create(), fbb_put_segment(), fbb_close() wrappers.
		 */
		void *trans_handle = fbt_get_handle(trans->fbt_transaction);
		ib_blob.fbb_blob = fbb_create(
			IBG(master_instance),
			fbc_get_attachment(ib_link->fbc_connection),
			trans_handle,
			&ib_blob.bl_qd,
			0, NULL,
			IB_STATUS
		);
		if (!ib_blob.fbb_blob) {
			break;
		}

		for (size = 0; (b = (unsigned)php_stream_read(stream, bl_data, sizeof(bl_data))) > 0; size += b) {
			/* fbb_put_segment returns 1 on success, 0 on error */
			if (fbb_put_segment(IBG(master_instance), ib_blob.fbb_blob, b, bl_data, IB_STATUS) == 0) {
				fbb_cancel(IBG(master_instance), ib_blob.fbb_blob, IB_STATUS);
				fbb_free(ib_blob.fbb_blob);
				break;
			}
		}

		/* fbb_close returns 1 on success, 0 on error */
		if (fbb_close(IBG(master_instance), ib_blob.fbb_blob, IB_STATUS) == 0) {
			fbb_free(ib_blob.fbb_blob);
			break;
		}
		fbb_free(ib_blob.fbb_blob);
		RETURN_NEW_STR(_php_fbird_quad_to_string(ib_blob.bl_qd));
	} while (0);

	_php_fbird_error();
	RETURN_FALSE;
}

PHP_FUNCTION(fbird_blob_create_stream)
{
	zval *link = NULL;
	fbird_db_link *ib_link;
	fbird_transaction *trans = NULL;
	fbird_blob *ib_blob;
	fbird_blob_stream_data *data;
	php_stream *stream;

	RESET_ERRMSG;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS(), "|r", &link)) {
		RETURN_FALSE;
	}

	PHP_FBIRD_LINK_TRANS(link, ib_link, trans);

	ib_blob = (fbird_blob *) emalloc(sizeof(fbird_blob));
	ib_blob->type = BLOB_INPUT;
	ib_blob->fbb_blob = NULL;  /* Phase 6: explicit fbb_blob init */

	/*
	 * Firebird 3.0+ OO API Blob Create (Stream)
	 *
	 * Uses IBlob interface via fbb_create() wrapper.
	 */
	void *trans_handle = fbt_get_handle(trans->fbt_transaction);
	ib_blob->fbb_blob = fbb_create(
		IBG(master_instance),
		fbc_get_attachment(ib_link->fbc_connection),
		trans_handle,
		&ib_blob->bl_qd,
		0, NULL,
		IB_STATUS
	);
	if (!ib_blob->fbb_blob) {
		_php_fbird_error();
		efree(ib_blob);
		RETURN_FALSE;
	}

	data = emalloc(sizeof(fbird_blob_stream_data));
	data->ib_blob = ib_blob;

	stream = php_stream_alloc(&fbird_blob_stream_ops, data, NULL, "w");
	if (!stream) {
		fbb_cancel(IBG(master_instance), ib_blob->fbb_blob, IB_STATUS);
		fbb_free(ib_blob->fbb_blob);
		efree(ib_blob);
		efree(data);
		RETURN_FALSE;
	}

	php_stream_to_zval(stream, return_value);
}

PHP_FUNCTION(fbird_blob_open_stream)
{
	char *blob_id;
	size_t blob_id_len;
	zval *link = NULL;
	fbird_db_link *ib_link;
	fbird_transaction *trans = NULL;
	fbird_blob *ib_blob;
	fbird_blob_stream_data *data;
	php_stream *stream;

	RESET_ERRMSG;
	PARSE_PARAMETERS;

	PHP_FBIRD_LINK_TRANS(link, ib_link, trans);

	ib_blob = (fbird_blob *) emalloc(sizeof(fbird_blob));
	ib_blob->type = BLOB_OUTPUT;
	ib_blob->fbb_blob = NULL;  /* Phase 6: explicit fbb_blob init */

	if (! _php_fbird_string_to_quad(blob_id, &ib_blob->bl_qd)) {
		_php_fbird_module_error("String is not a BLOB ID");
		efree(ib_blob);
		RETURN_FALSE;
	}

	/*
	 * Firebird 3.0+ OO API Blob Open (Stream)
	 *
	 * Uses IBlob interface via fbb_open() wrapper.
	 */
	void *trans_handle = fbt_get_handle(trans->fbt_transaction);
	ib_blob->fbb_blob = fbb_open(
		IBG(master_instance),
		fbc_get_attachment(ib_link->fbc_connection),
		trans_handle,
		&ib_blob->bl_qd,
		0, NULL,
		IB_STATUS
	);
	if (!ib_blob->fbb_blob) {
		_php_fbird_error();
		efree(ib_blob);
		RETURN_FALSE;
	}

	data = emalloc(sizeof(fbird_blob_stream_data));
	data->ib_blob = ib_blob;

	stream = php_stream_alloc(&fbird_blob_stream_ops, data, NULL, "r");
	if (!stream) {
		fbb_close(IBG(master_instance), ib_blob->fbb_blob, IB_STATUS);
		fbb_free(ib_blob->fbb_blob);
		efree(ib_blob);
		efree(data);
		RETURN_FALSE;
	}

	php_stream_to_zval(stream, return_value);
}

PHP_FUNCTION(fbird_blob_open_seekable)
{
	char *blob_id;
	size_t blob_id_len;
	zval *link = NULL;
	fbird_db_link *ib_link;
	fbird_transaction *trans = NULL;
	fbird_blob *ib_blob;

	RESET_ERRMSG;
	PARSE_PARAMETERS;

	PHP_FBIRD_LINK_TRANS(link, ib_link, trans);

	ib_blob = (fbird_blob *) emalloc(sizeof(fbird_blob));
	ib_blob->type = BLOB_OUTPUT;
	ib_blob->fbb_blob = NULL;

	do {
		if (! _php_fbird_string_to_quad(blob_id, &ib_blob->bl_qd)) {
			_php_fbird_module_error("String is not a BLOB ID");
			break;
		}

		/*
		 * Firebird 3.0+ OO API Blob Open (Stream Mode)
		 *
		 * Uses IBlob interface via fbb_open() wrapper with stream BPB.
		 * Stream mode enables seeking via isc_seek_blob/IBlob::seek().
		 */
		void *trans_handle = fbt_get_handle(trans->fbt_transaction);
		ib_blob->fbb_blob = fbb_open(
			IBG(master_instance),
			fbc_get_attachment(ib_link->fbc_connection),
			trans_handle,
			&ib_blob->bl_qd,
			sizeof(stream_bpb), stream_bpb,  /* Stream BPB for seek support */
			IB_STATUS
		);
		if (ib_blob->fbb_blob == NULL) {
			_php_fbird_error();
			break;
		}

		RETVAL_RES(zend_register_resource(ib_blob, le_blob));
		return;

	} while (0);

	efree(ib_blob);
	RETURN_FALSE;
}

PHP_FUNCTION(fbird_blob_seek)
{
	zval *blob_arg;
	zend_long offset;
	zend_long whence = 0; /* Default: SEEK_SET */
	fbird_blob *ib_blob;
	int result_position = 0;

	RESET_ERRMSG;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS(), "rl|l", &blob_arg, &offset, &whence)) {
		RETURN_FALSE;
	}

	/* Validate whence parameter */
	if (whence < 0 || whence > 2) {
		_php_fbird_module_error("Invalid seek mode: must be 0 (SET), 1 (CUR), or 2 (END)");
		RETURN_FALSE;
	}

	ib_blob = (fbird_blob *)zend_fetch_resource_ex(blob_arg, LE_BLOB, le_blob);

	if (!ib_blob) {
		RETURN_FALSE;
	}

	/* Safety check: verify blob handle is valid */
	if (!ib_blob->fbb_blob) {
		_php_fbird_module_error("BLOB handle is invalid or has been closed");
		RETURN_FALSE;
	}

	/*
	 * Firebird 3.0+ OO API Blob Seek
	 *
	 * Uses IBlob::seek() via fbb_seek() wrapper.
	 * Note: Only works on stream blobs (created with isc_bpb_type_stream).
	 * Segmented blobs will fail with isc_bad_segstr_type error.
	 */
	if (fbb_seek(IBG(master_instance), ib_blob->fbb_blob, (int)whence, (int)offset, &result_position, IB_STATUS) == 0) {
		_php_fbird_error();
		RETURN_FALSE;
	}

	RETURN_LONG(result_position);
}

#endif /* HAVE_FIREBIRD */
