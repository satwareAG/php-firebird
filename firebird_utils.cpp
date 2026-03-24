/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

#include "php.h"

#if HAVE_FIREBIRD

#include "firebird_utils.h"
#include "firebird_utils_internal.h"

#include <string>
#include <cstring>
#include <cstdio>

/* Opaque struct definitions (C++ only, forward-declared in firebird_utils.h) */
struct fb_opaque_connection_s {
	Firebird::IAttachment* att;
};
struct fb_opaque_transaction_s {
	Firebird::ITransaction* trans;
};
struct fb_opaque_statement_s {
	Firebird::IStatement* stmt;
};
struct fb_opaque_batch_s {
	Firebird::IBatch* batch;
	Firebird::IMessageMetadata* metadata;
	~fb_opaque_batch_s() {
		if (metadata) { metadata->release(); metadata = nullptr; }
	}
};

/* ============================================================================
 * Bridge functions (signatures from firebird_utils.h)
 * ========================================================================== */

extern "C" fb_ptr_t fbc_get_attachment_safe(fbc_connection_t *connection)
{
	(void)connection;
	fb_ptr_t r = {0};
	/* TODO: extract att from fbc_connection_t PHP wrapper */
	return r;
}

extern "C" fb_ptr_t fbt_start_safe(
	fbc_master_t *master_ptr,
	fb_ptr_t attachment_ptr,
	unsigned tpb_len,
	const unsigned char* tpb,
	ISC_STATUS* status_vector)
{
	(void)master_ptr; (void)attachment_ptr; (void)tpb_len; (void)tpb; (void)status_vector;
	fb_ptr_t r = {0};
	/* TODO: FBC_MASTER(master_ptr), cast attachment_ptr.p to IAttachment* */
	return r;
}

extern "C" fb_ptr_t fbt_reconnect_safe(
	fbc_master_t *master_ptr,
	fb_ptr_t attachment_ptr,
	ISC_INT64 trans_id,
	ISC_STATUS* status_vector)
{
	(void)master_ptr; (void)attachment_ptr; (void)trans_id; (void)status_vector;
	fb_ptr_t r = {0};
	/* TODO: FBC_MASTER(master_ptr), cast attachment_ptr.p to IAttachment* */
	return r;
}

extern "C" fb_opaque_transaction_t* fbt_get_handle_safe(fb_ptr_t transaction_ptr)
{
	(void)transaction_ptr;
	/* TODO: cast transaction_ptr.p to ITransaction*, wrap in opaque */
	return nullptr;
}

/* ============================================================================
 * Connection functions (fbc_connection_t* = PHP wrapper - stubs)
 * ========================================================================== */

extern "C" fb_opaque_connection_t* fbc_connect(
	fbc_master_t *master_ptr,
	const char* database, size_t database_len,
	const char* user, size_t user_len,
	const char* password, size_t password_len,
	const char* charset, size_t charset_len,
	const char* role, size_t role_len,
	int num_buffers,
	int dialect,
	int force_write,
	ISC_STATUS* status_vector)
{
	(void)master_ptr; (void)database; (void)database_len;
	(void)user; (void)user_len; (void)password; (void)password_len;
	(void)charset; (void)charset_len; (void)role; (void)role_len;
	(void)num_buffers; (void)dialect; (void)force_write; (void)status_vector;
	/* TODO: implement via FBC_MASTER(master_ptr)->createDatabase() */
	return nullptr;
}

extern "C" void fbc_disconnect(fbc_connection_t *connection, ISC_STATUS* status_vector)
{
	(void)connection; (void)status_vector;
	/* TODO: extract att from PHP wrapper, call att->detach() */
}

extern "C" int fbc_drop_database(fbc_connection_t *connection, ISC_STATUS* status_vector)
{
	(void)connection; (void)status_vector;
	/* TODO: extract att from PHP wrapper */
	return 0;
}

extern "C" fbc_connection_t* fbc_create_database(
	fbc_master_t *master_ptr,
	const char* create_sql,
	unsigned dialect,
	ISC_STATUS* status_vector)
{
	(void)master_ptr; (void)create_sql; (void)dialect; (void)status_vector;
	/* TODO: implement via FBC_MASTER(master_ptr)->createDatabase() */
	return nullptr;
}

extern "C" int fbc_is_connected(fbc_connection_t *connection)
{
	(void)connection;
	/* TODO: check PHP wrapper state */
	return 0;
}

extern "C" int fbc_ping(fbc_master_t *master_ptr, fbc_connection_t *connection, ISC_STATUS* status_vector)
{
	(void)master_ptr; (void)connection; (void)status_vector;
	/* TODO: extract att from PHP wrapper, call att->ping() */
	return 0;
}

extern "C" unsigned fbc_get_server_version(fbc_connection_t *connection)
{
	(void)connection;
	/* TODO: extract att from PHP wrapper */
	return 0;
}

/* ============================================================================
 * Connection info (fb_opaque_connection_t* - direct att access)
 * ========================================================================== */

extern "C" int fbc_get_info(
	fbc_master_t *master_ptr,
	fb_opaque_connection_t *attachment_ptr,
	unsigned items_length,
	const unsigned char* items,
	unsigned buffer_length,
	unsigned char* buffer,
	ISC_STATUS* status_vector)
{
	(void)master_ptr;
	if (!attachment_ptr || !attachment_ptr->att) return 0;
	return attachment_ptr->att->getInfo(items_length, items, buffer_length,
		reinterpret_cast<char*>(buffer), status_vector);
}

/* ============================================================================
 * Transaction functions
 * ========================================================================== */

extern "C" int fbt_commit(fbt_transaction_t *transaction, ISC_STATUS* status_vector)
{
	(void)transaction; (void)status_vector;
	/* TODO: extract ITransaction* from PHP wrapper */
	return 0;
}

extern "C" int fbt_rollback(fbt_transaction_t *transaction, ISC_STATUS* status_vector)
{
	(void)transaction; (void)status_vector;
	/* TODO: extract ITransaction* from PHP wrapper */
	return 0;
}

extern "C" int fbt_commit_retaining(fbt_transaction_t *transaction, ISC_STATUS* status_vector)
{
	(void)transaction; (void)status_vector;
	/* TODO: extract ITransaction* from PHP wrapper */
	return 0;
}

extern "C" int fbt_rollback_retaining(fbt_transaction_t *transaction, ISC_STATUS* status_vector)
{
	(void)transaction; (void)status_vector;
	/* TODO: extract ITransaction* from PHP wrapper */
	return 0;
}

extern "C" int fbt_is_active(fbt_transaction_t *transaction)
{
	(void)transaction;
	/* TODO: extract ITransaction* from PHP wrapper */
	return 0;
}

extern "C" void fbt_free(fbt_transaction_t *transaction)
{
	(void)transaction;
	/* TODO: extract and release ITransaction* from PHP wrapper */
}

extern "C" int fbt_get_info(
	fbc_master_t *master_ptr,
	fb_opaque_transaction_t *transaction_ptr,
	unsigned items_length,
	const unsigned char* items,
	unsigned buffer_length,
	unsigned char* buffer,
	ISC_STATUS* status_vector)
{
	(void)master_ptr;
	if (!transaction_ptr || !transaction_ptr->trans) return 0;
	return transaction_ptr->trans->getInfo(items_length, items, buffer_length,
		reinterpret_cast<char*>(buffer), status_vector);
}

extern "C" int fbt_get_limbo_transactions(
	fbc_master_t *master_ptr,
	fb_opaque_connection_t *attachment_ptr,
	ISC_INT64* trans_ids,
	unsigned max_ids,
	ISC_STATUS* status_vector)
{
	(void)master_ptr; (void)trans_ids; (void)max_ids; (void)status_vector;
	if (!attachment_ptr || !attachment_ptr->att) return 0;
	/* TODO: getLimboTransactions returns count+vectors, need to convert to trans_ids */
	unsigned count = 0;
	ISC_UCHAR** vectors = nullptr;
	int rc = attachment_ptr->att->getLimboTransactions(&count, &vectors, status_vector);
	return rc;
}

/* ============================================================================
 * Statement functions
 * ========================================================================== */

extern "C" fb_opaque_statement_t* fbs_prepare(
	fbc_master_t *master_ptr,
	fb_opaque_connection_t *attachment_ptr,
	fb_opaque_transaction_t *transaction_ptr,
	const char* sql,
	unsigned sql_length,
	unsigned dialect,
	ISC_STATUS* status_vector)
{
	(void)master_ptr;
	if (!attachment_ptr || !attachment_ptr->att) return nullptr;
	Firebird::IStatement* stmt = attachment_ptr->att->prepare(status_vector,
		transaction_ptr ? transaction_ptr->trans : nullptr,
		dialect, sql, sql_length,
		Firebird::IStatement::FLAG_PREPARE_METADATA, nullptr, nullptr);
	if (!stmt) return nullptr;
	auto* opaque = new fb_opaque_statement_s();
	opaque->stmt = stmt;
	return opaque;
}

extern "C" int fbs_execute(
	fbc_master_t *master_ptr,
	fbs_statement_t *statement_ptr,
	fbt_transaction_t *transaction_ptr,
	void *in_msg,
	void *in_metadata,
	void *out_msg,
	void *out_metadata,
	ISC_STATUS* status_vector)
{
	(void)master_ptr; (void)statement_ptr; (void)transaction_ptr;
	(void)in_msg; (void)in_metadata; (void)out_msg; (void)out_metadata;
	(void)status_vector;
	/* TODO: extract IStatement* and ITransaction* from PHP wrappers */
	return 0;
}

extern "C" int fbs_open_cursor(
	fbc_master_t *master_ptr,
	fbs_statement_t *statement_ptr,
	fbt_transaction_t *transaction_ptr,
	void *in_msg,
	void *in_metadata,
	unsigned cursor_flags,
	ISC_STATUS* status_vector)
{
	(void)master_ptr; (void)statement_ptr; (void)transaction_ptr;
	(void)in_msg; (void)in_metadata; (void)cursor_flags; (void)status_vector;
	/* TODO: extract IStatement* and ITransaction* from PHP wrappers */
	return 0;
}

extern "C" int fbs_fetch(
	fbc_master_t *master_ptr,
	fbs_statement_t *statement_ptr,
	void *out_msg,
	ISC_STATUS* status_vector)
{
	(void)master_ptr; (void)statement_ptr; (void)out_msg; (void)status_vector;
	/* TODO: extract IStatement* from PHP wrapper */
	return 0;
}

extern "C" int fbs_close_cursor(fbs_statement_t *statement_ptr, ISC_STATUS* status_vector)
{
	(void)statement_ptr; (void)status_vector;
	/* TODO: extract IStatement* from PHP wrapper */
	return 0;
}

extern "C" int fbs_free(fbs_statement_t *statement_ptr, ISC_STATUS* status_vector)
{
	(void)statement_ptr; (void)status_vector;
	/* TODO: extract IStatement* from PHP wrapper, release */
	return 0;
}

extern "C" unsigned fbs_get_type(fbc_master_t *master_ptr, fbs_statement_t *statement_ptr, ISC_STATUS* status_vector)
{
	(void)master_ptr; (void)statement_ptr; (void)status_vector;
	/* TODO: extract IStatement* from PHP wrapper */
	return 0;
}

extern "C" ISC_UINT64 fbs_get_affected_records(fbc_master_t *master_ptr, fbs_statement_t *statement_ptr, ISC_STATUS* status_vector)
{
	(void)master_ptr; (void)statement_ptr; (void)status_vector;
	/* TODO: extract IStatement* from PHP wrapper */
	return 0;
}

extern "C" void* fbs_get_input_metadata(fbc_master_t *master_ptr, fbs_statement_t *statement_ptr, ISC_STATUS* status_vector)
{
	(void)master_ptr; (void)statement_ptr; (void)status_vector;
	/* TODO: extract IStatement* from PHP wrapper */
	return nullptr;
}

extern "C" void* fbs_get_output_metadata(fbc_master_t *master_ptr, fbs_statement_t *statement_ptr, ISC_STATUS* status_vector)
{
	(void)master_ptr; (void)statement_ptr; (void)status_vector;
	/* TODO: extract IStatement* from PHP wrapper */
	return nullptr;
}

extern "C" void* fbs_get_statement(fbs_statement_t *statement_ptr)
{
	(void)statement_ptr;
	/* TODO: extract IStatement* from PHP wrapper */
	return nullptr;
}

extern "C" int fbs_set_cursor_name(fbc_master_t *master_ptr, fbs_statement_t *statement_ptr, const char* name, ISC_STATUS* status_vector)
{
	(void)master_ptr; (void)statement_ptr; (void)name; (void)status_vector;
	/* TODO: extract IStatement* from PHP wrapper */
	return 0;
}

/* ============================================================================
 * Timestamp / numeric conversion
 * ========================================================================== */

extern "C" void fbu_decode_timestamp(fbc_master_t *master_ptr, ISC_TIMESTAMP timestamp,
	unsigned* year, unsigned* month, unsigned* day,
	unsigned* hours, unsigned* minutes, unsigned* seconds, unsigned* fractions)
{
	(void)master_ptr;
	tm times;
	isc_decode_timestamp(&timestamp, &times);
	if (year) *year = (unsigned)(times.tm_year + 1900);
	if (month) *month = (unsigned)(times.tm_mon + 1);
	if (day) *day = (unsigned)times.tm_mday;
	if (hours) *hours = (unsigned)times.tm_hour;
	if (minutes) *minutes = (unsigned)times.tm_min;
	if (seconds) *seconds = (unsigned)times.tm_sec;
	if (fractions) *fractions = timestamp.timestamp_time % 10000;
}

extern "C" void fbu_int128_to_string(const void* value, int scale, char* buffer, size_t buffer_size)
{
	(void)scale;
	Firebird::Int128 tmp;
	memcpy(&tmp, value, sizeof(tmp));
	std::string s = tmp.toString();
	strncpy(buffer, s.c_str(), buffer_size - 1);
	buffer[buffer_size - 1] = '\0';
}

extern "C" void fbu_decfloat16_to_string(const void* value, char* buffer, size_t buffer_size)
{
	Firebird::Decimal16 tmp;
	memcpy(&tmp, value, sizeof(tmp));
	std::string s = tmp.toString();
	strncpy(buffer, s.c_str(), buffer_size - 1);
	buffer[buffer_size - 1] = '\0';
}

extern "C" void fbu_decfloat34_to_string(const void* value, char* buffer, size_t buffer_size)
{
	Firebird::Decimal34 tmp;
	memcpy(&tmp, value, sizeof(tmp));
	std::string s = tmp.toString();
	strncpy(buffer, s.c_str(), buffer_size - 1);
	buffer[buffer_size - 1] = '\0';
}

/* ============================================================================
 * Metadata functions
 * ========================================================================== */

extern "C" const char* fbm_get_field(fbc_master_t *master_ptr, void* metadata_ptr, unsigned index)
{
	(void)master_ptr; (void)metadata_ptr; (void)index;
	/* TODO: cast to IMessageMetadata*, call getField() */
	return nullptr;
}

extern "C" const char* fbm_get_relation(fbc_master_t *master_ptr, void* metadata_ptr, unsigned index)
{
	(void)master_ptr; (void)metadata_ptr; (void)index;
	/* TODO: cast to IMessageMetadata*, call getRelation() */
	return nullptr;
}

/* ============================================================================
 * Array functions (legacy fba_array_lookup_bounds + OO variants)
 * ========================================================================== */

extern "C" ISC_STATUS fba_array_lookup_bounds(ISC_STATUS *status_vector,
	isc_db_handle *db_handle, isc_tr_handle *tr_handle,
	const char *relation_name, const char *field_name,
	ISC_ARRAY_DESC *desc)
{
	(void)status_vector; (void)db_handle; (void)tr_handle;
	(void)relation_name; (void)field_name; (void)desc;
	/* Legacy API - not implemented for FB 3.0+ */
	return 0;
}

extern "C" int fba_lookup_bounds_oo(fbc_master_t *master_ptr,
	fb_opaque_connection_t *attachment_ptr,
	fb_opaque_transaction_t *transaction_ptr,
	const char* relation_name, const char* field_name,
	ISC_ARRAY_DESC *desc, ISC_STATUS* status_vector)
{
	(void)master_ptr; (void)transaction_ptr;
	(void)relation_name; (void)field_name;
	if (!attachment_ptr || !attachment_ptr->att) return 0;
	return attachment_ptr->att->arrayLookupBounds(status_vector, nullptr, desc);
}

extern "C" int fba_get_slice_oo(fbc_master_t *master_ptr,
	fb_opaque_connection_t *attachment_ptr,
	fb_opaque_transaction_t *transaction_ptr,
	ISC_QUAD *blob_id, ISC_ARRAY_DESC *desc,
	void *buffer, unsigned buffer_length, ISC_STATUS* status_vector)
{
	(void)master_ptr; (void)transaction_ptr;
	size_t len = buffer_length;
	if (!attachment_ptr || !attachment_ptr->att) return 0;
	return attachment_ptr->att->arrayGetSlice(status_vector, nullptr, blob_id, desc, buffer, &len);
}

extern "C" int fba_put_slice_oo(fbc_master_t *master_ptr,
	fb_opaque_connection_t *attachment_ptr,
	fb_opaque_transaction_t *transaction_ptr,
	ISC_QUAD *blob_id, ISC_ARRAY_DESC *desc,
	void *buffer, unsigned buffer_length, ISC_STATUS* status_vector)
{
	(void)master_ptr; (void)transaction_ptr;
	size_t len = buffer_length;
	if (!attachment_ptr || !attachment_ptr->att) return 0;
	return attachment_ptr->att->arrayPutSlice(status_vector, nullptr, blob_id, desc, buffer, &len);
}

/* ============================================================================
 * Events functions
 * ========================================================================== */

extern "C" int fbe_wait_for_event_oo(fb_opaque_connection_t *attachment_ptr,
	unsigned length, const unsigned char* events, ISC_STATUS* status_vector)
{
	if (!attachment_ptr || !attachment_ptr->att) return 0;
	return attachment_ptr->att->queueEvents(status_vector, nullptr, length, events,
		nullptr, nullptr, nullptr);
}

/* ============================================================================
 * Batch functions (Firebird 4.0+ - signatures from firebird_utils.h)
 * ========================================================================== */

#if FB_API_VER >= 40

extern "C" fb_opaque_batch_t* fbbatch_create(fbc_master_t *master_ptr,
	fb_opaque_connection_t *attachment_ptr,
	fb_opaque_transaction_t *transaction_ptr,
	unsigned sql_length, const char* sql, unsigned dialect,
	unsigned bpb_length, const unsigned char* bpb,
	ISC_STATUS* status_vector)
{
	(void)master_ptr;
	if (!attachment_ptr || !attachment_ptr->att) return nullptr;
	Firebird::IStatement* stmt = attachment_ptr->att->prepare(status_vector,
		transaction_ptr ? transaction_ptr->trans : nullptr,
		dialect, sql, sql_length,
		Firebird::IStatement::FLAG_PREPARE_METADATA, nullptr, nullptr);
	if (!stmt) return nullptr;
	Firebird::IBatch* batch = stmt->createBatch(status_vector, bpb_length, bpb);
	stmt->release();
	if (!batch) return nullptr;
	auto* opaque = new fb_opaque_batch_s();
	opaque->batch = batch;
	opaque->metadata = nullptr;
	return opaque;
}

extern "C" int fbbatch_add_row(fbc_master_t *master_ptr,
	fb_opaque_batch_t *batch_ptr, void *row_msg,
	void *row_metadata, ISC_STATUS* status_vector)
{
	(void)master_ptr; (void)row_metadata;
	if (!batch_ptr || !batch_ptr->batch) return 0;
	return batch_ptr->batch->add(1,
		static_cast<const unsigned char*>(row_msg), status_vector);
}

extern "C" int fbbatch_execute(fbc_master_t *master_ptr,
	fb_opaque_batch_t *batch_ptr, ISC_STATUS* status_vector)
{
	(void)master_ptr;
	if (!batch_ptr || !batch_ptr->batch) return 0;
	unsigned total = 0, errors = 0;
	Firebird::ITransaction* tx_out = batch_ptr->batch->execute(
		status_vector, nullptr, &total, &errors);
	return tx_out ? 1 : 0;
}

extern "C" void fbbatch_free(fb_opaque_batch_t *batch_ptr)
{
	if (!batch_ptr) return;
	if (batch_ptr->batch) { batch_ptr->batch->release(); batch_ptr->batch = nullptr; }
	delete batch_ptr;
}

extern "C" int fbbatch_cancel(fb_opaque_batch_t *batch_ptr)
{
	if (!batch_ptr || !batch_ptr->batch) return 0;
	ISC_STATUS status[20] = {};
	batch_ptr->batch->cancel(status);
	return 1;
}

extern "C" unsigned fbbatch_get_row_count(fb_opaque_batch_t *batch_ptr)
{
	if (!batch_ptr || !batch_ptr->batch) return 0;
	ISC_STATUS status[20] = {};
	return (unsigned)batch_ptr->batch->getRowCount(status);
}

extern "C" void* fbbatch_get_metadata(fbc_master_t *master_ptr,
	fb_opaque_batch_t *batch_ptr, ISC_STATUS* status_vector)
{
	(void)master_ptr;
	if (!batch_ptr || !batch_ptr->batch) return nullptr;
	if (!batch_ptr->metadata) {
		batch_ptr->metadata = batch_ptr->batch->getMetadata(status_vector);
	}
	return static_cast<void*>(batch_ptr->metadata);
}

extern "C" void* fbbatch_get_errors(fbc_master_t *master_ptr,
	fb_opaque_batch_t *batch_ptr, ISC_STATUS* status_vector)
{
	(void)master_ptr;
	if (!batch_ptr || !batch_ptr->batch) return nullptr;
	return static_cast<void*>(batch_ptr->batch->getBatchErrors(status_vector));
}

extern "C" unsigned fbbatch_get_blob_alignment(fbc_master_t *master_ptr,
	fb_opaque_batch_t *batch_ptr, ISC_STATUS* status_vector)
{
	(void)master_ptr;
	if (!batch_ptr || !batch_ptr->batch) return 0;
	return batch_ptr->batch->getBlobAlignment(status_vector);
}

extern "C" int fbbatch_append_blob_data(fbc_master_t *master_ptr,
	fb_opaque_batch_t *batch_ptr, unsigned length,
	const void* buffer, ISC_STATUS* status_vector)
{
	(void)master_ptr;
	if (!batch_ptr || !batch_ptr->batch) return 0;
	batch_ptr->batch->appendBlobData(status_vector, length,
		static_cast<const unsigned char*>(buffer));
	return 1;
}

extern "C" int fbbatch_add_blob_stream(fbc_master_t *master_ptr,
	fb_opaque_batch_t *batch_ptr, php_stream *stream,
	ISC_STATUS* status_vector)
{
	(void)master_ptr; (void)batch_ptr; (void)stream; (void)status_vector;
	/* TODO: read from php_stream, feed to addBlobStream */
	return 0;
}

extern "C" int fbbatch_set_default_bpb(fbc_master_t *master_ptr,
	fb_opaque_batch_t *batch_ptr, unsigned bpb_length,
	const unsigned char* bpb, ISC_STATUS* status_vector)
{
	(void)master_ptr;
	if (!batch_ptr || !batch_ptr->batch) return 0;
	batch_ptr->batch->setDefaultBpb(status_vector, bpb_length, bpb);
	return 1;
}

#endif /* FB_API_VER >= 40 */

#endif /* HAVE_FIREBIRD */