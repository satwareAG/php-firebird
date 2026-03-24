/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

#ifndef FIREBIRD_UTILS_H
#define FIREBIRD_UTILS_H

#include <ibase.h>
#include <stdint.h>
#include "php_fbird_includes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Firebird 3.0+ OO API Required */
#if !defined(FB_API_VER) || FB_API_VER < 30
#error "This extension requires Firebird 3.0+ (FB_API_VER >= 30). Legacy API is not supported."
#endif

/* fb_ptr_t: Struct wrapper to prevent implicit conversion to int by C compiler.
 * This GUARANTEES 64-bit pointer safety across the C/C++ boundary. */
typedef struct { uintptr_t p; } fb_ptr_t;

/* Typed opaque handle structs for C/C++ boundary type safety.
 * These replace void* in function signatures, catching misuse at compile time.
 * The actual struct definitions live in firebird_utils.cpp (C++ only). */
typedef struct fb_opaque_connection_s fb_opaque_connection_t;
typedef struct fb_opaque_transaction_s fb_opaque_transaction_t;
typedef struct fb_opaque_statement_s fb_opaque_statement_t;
typedef struct fb_opaque_batch_s fb_opaque_batch_t;

/* Bridge Functions - use fb_ptr_t return type to avoid any 32-bit truncation */
fb_ptr_t fbc_get_attachment_safe(fbc_connection_t *connection);
fb_ptr_t fbt_start_safe(
    fbc_master_t *master_ptr,
    fb_ptr_t attachment_ptr,
    unsigned tpb_len,
    const unsigned char* tpb,
    ISC_STATUS* status_vector
);
fb_ptr_t fbt_reconnect_safe(
    fbc_master_t *master_ptr,
    fb_ptr_t attachment_ptr,
    ISC_INT64 trans_id,
    ISC_STATUS* status_vector
);
fb_opaque_transaction_t* fbt_get_handle_safe(fb_ptr_t transaction_ptr);

const char *_fbird_res_type_name(int type);
void fbp_error_ex(long level, const char *msg, ...);
#if PHP_DEBUG
void fbp_dump_buffer(int len, const unsigned char *buffer);
void fbp_dump_buffer_raw(int len, const unsigned char *buffer);
#endif

unsigned fbu_get_client_version(fbc_master_t *master_ptr);
ISC_TIME fbu_encode_time(fbc_master_t *master_ptr, unsigned hours, unsigned minutes,
  unsigned seconds, unsigned fractions);
ISC_DATE fbu_encode_date(fbc_master_t *master_ptr, unsigned year, unsigned month, unsigned day);

long fbu_sqlcode(const ISC_STATUS *status_vector);

ISC_STATUS fba_array_lookup_bounds(ISC_STATUS *status_vector,
    isc_db_handle *db_handle, isc_tr_handle *tr_handle,
    const char *relation_name, const char *field_name,
    ISC_ARRAY_DESC *desc);

ISC_TIMESTAMP fbu_encode_timestamp(fbc_master_t *master_ptr, unsigned year, unsigned month, unsigned day,
    unsigned hours, unsigned minutes, unsigned seconds, unsigned fractions);

void fbu_decode_time(fbc_master_t *master_ptr, ISC_TIME time,
    unsigned* hours, unsigned* minutes, unsigned* seconds, unsigned* fractions);

void fbu_decode_date(fbc_master_t *master_ptr, ISC_DATE date,
    unsigned* year, unsigned* month, unsigned* day);

void fbu_decode_timestamp(fbc_master_t *master_ptr, ISC_TIMESTAMP timestamp,
    unsigned* year, unsigned* month, unsigned* day,
    unsigned* hours, unsigned* minutes, unsigned* seconds, unsigned* fractions);

fb_opaque_connection_t* fbc_connect(
    fbc_master_t *master_ptr,
    const char* database, size_t database_len,
    const char* user, size_t user_len,
    const char* password, size_t password_len,
    const char* charset, size_t charset_len,
    const char* role, size_t role_len,
    int num_buffers,
    int dialect,
    int force_write,
    ISC_STATUS* status_vector
);

void fbc_disconnect(fbc_connection_t *connection, ISC_STATUS* status_vector);
int fbc_drop_database(fbc_connection_t *connection, ISC_STATUS* status_vector);
fbc_connection_t* fbc_create_database(
    fbc_master_t *master_ptr,
    const char* create_sql,
    unsigned dialect,
    ISC_STATUS* status_vector
);

int fbc_is_connected(fbc_connection_t *connection);
int fbc_ping(fbc_master_t *master_ptr, fbc_connection_t *connection, ISC_STATUS* status_vector);
unsigned fbc_get_server_version(fbc_connection_t *connection);

int fbc_get_info(
    fbc_master_t *master_ptr,
    fb_opaque_connection_t *attachment_ptr,
    unsigned items_length,
    const unsigned char* items,
    unsigned buffer_length,
    unsigned char* buffer,
    ISC_STATUS* status_vector
);

int fbt_commit(fbt_transaction_t *transaction, ISC_STATUS* status_vector);
int fbt_rollback(fbt_transaction_t *transaction, ISC_STATUS* status_vector);
int fbt_commit_retaining(fbt_transaction_t *transaction, ISC_STATUS* status_vector);
int fbt_rollback_retaining(fbt_transaction_t *transaction, ISC_STATUS* status_vector);
int fbt_is_active(fbt_transaction_t *transaction);
void fbt_free(fbt_transaction_t *transaction);

int fbt_get_info(
    fbc_master_t *master_ptr,
    fb_opaque_transaction_t *transaction_ptr,
    unsigned items_length,
    const unsigned char* items,
    unsigned buffer_length,
    unsigned char* buffer,
    ISC_STATUS* status_vector
);

int fbt_get_limbo_transactions(
    fbc_master_t *master_ptr,
    fb_opaque_connection_t *attachment_ptr,
    ISC_INT64* trans_ids,
    unsigned max_ids,
    ISC_STATUS* status_vector
);

fb_opaque_statement_t* fbs_prepare(
    fbc_master_t *master_ptr,
    fb_opaque_connection_t *attachment_ptr,
    fb_opaque_transaction_t *transaction_ptr,
    const char* sql,
    unsigned sql_length,
    unsigned dialect,
    ISC_STATUS* status_vector
);

int fbs_execute(
    fbc_master_t *master_ptr,
    fbs_statement_t *statement_ptr,
    fbt_transaction_t *transaction_ptr,
    void *in_msg,
    void *in_metadata,
    void *out_msg,
    void *out_metadata,
    ISC_STATUS* status_vector
);

int fbs_open_cursor(
    fbc_master_t *master_ptr,
    fbs_statement_t *statement_ptr,
    fbt_transaction_t *transaction_ptr,
    void *in_msg,
    void *in_metadata,
    unsigned cursor_flags,
    ISC_STATUS* status_vector
);

int fbs_fetch(
    fbc_master_t *master_ptr,
    fbs_statement_t *statement_ptr,
    void *out_msg,
    ISC_STATUS* status_vector
);

int fbs_close_cursor(fbs_statement_t *statement_ptr, ISC_STATUS* status_vector);
int fbs_free(fbs_statement_t *statement_ptr, ISC_STATUS* status_vector);
unsigned fbs_get_type(fbc_master_t *master_ptr, fbs_statement_t *statement_ptr, ISC_STATUS* status_vector);
ISC_UINT64 fbs_get_affected_records(fbc_master_t *master_ptr, fbs_statement_t *statement_ptr, ISC_STATUS* status_vector);
void* fbs_get_input_metadata(fbc_master_t *master_ptr, fbs_statement_t *statement_ptr, ISC_STATUS* status_vector);
void* fbs_get_output_metadata(fbc_master_t *master_ptr, fbs_statement_t *statement_ptr, ISC_STATUS* status_vector);
void* fbs_get_statement(fbs_statement_t *statement_ptr);
int fbs_set_cursor_name(fbc_master_t *master_ptr, fbs_statement_t *statement_ptr, const char* name, ISC_STATUS* status_vector);

void fbu_int128_to_string(const void* value, int scale, char* buffer, size_t buffer_size);
void fbu_decfloat16_to_string(const void* value, char* buffer, size_t buffer_size);
void fbu_decfloat34_to_string(const void* value, char* buffer, size_t buffer_size);

/* Metadata functions */
const char* fbm_get_field(fbc_master_t *master_ptr, void* metadata_ptr, unsigned index);
const char* fbm_get_relation(fbc_master_t *master_ptr, void* metadata_ptr, unsigned index);

/* Array API Functions */
int fba_lookup_bounds_oo(fbc_master_t *master_ptr, fb_opaque_connection_t *attachment_ptr, fb_opaque_transaction_t *transaction_ptr,
    const char* relation_name, const char* field_name, ISC_ARRAY_DESC *desc, ISC_STATUS* status_vector);
int fba_get_slice_oo(fbc_master_t *master_ptr, fb_opaque_connection_t *attachment_ptr, fb_opaque_transaction_t *transaction_ptr,
    ISC_QUAD *blob_id, ISC_ARRAY_DESC *desc, void *buffer, unsigned buffer_length, ISC_STATUS* status_vector);
int fba_put_slice_oo(fbc_master_t *master_ptr, fb_opaque_connection_t *attachment_ptr, fb_opaque_transaction_t *transaction_ptr,
    ISC_QUAD *blob_id, ISC_ARRAY_DESC *desc, void *buffer, unsigned buffer_length, ISC_STATUS* status_vector);

/* Batch API Functions (Firebird 4.0+) */
#if FB_API_VER >= 40
fb_opaque_batch_t* fbbatch_create(fbc_master_t *master_ptr, fb_opaque_connection_t *attachment_ptr, fb_opaque_transaction_t *transaction_ptr,
    unsigned sql_length, const char* sql, unsigned dialect, unsigned bpb_length,
    const unsigned char* bpb, ISC_STATUS* status_vector);
int fbbatch_add_row(fbc_master_t *master_ptr, fb_opaque_batch_t *batch_ptr, void *row_msg,
    void *row_metadata, ISC_STATUS* status_vector);
int fbbatch_execute(fbc_master_t *master_ptr, fb_opaque_batch_t *batch_ptr, ISC_STATUS* status_vector);
void fbbatch_free(fb_opaque_batch_t *batch_ptr);
int fbbatch_cancel(fb_opaque_batch_t *batch_ptr);
unsigned fbbatch_get_row_count(fb_opaque_batch_t *batch_ptr);
void* fbbatch_get_metadata(fbc_master_t *master_ptr, fb_opaque_batch_t *batch_ptr, ISC_STATUS* status_vector);
void* fbbatch_get_errors(fbc_master_t *master_ptr, fb_opaque_batch_t *batch_ptr, ISC_STATUS* status_vector);
unsigned fbbatch_get_blob_alignment(fbc_master_t *master_ptr, fb_opaque_batch_t *batch_ptr, ISC_STATUS* status_vector);
int fbbatch_append_blob_data(fbc_master_t *master_ptr, fb_opaque_batch_t *batch_ptr, unsigned length,
    const void* buffer, ISC_STATUS* status_vector);
int fbbatch_add_blob_stream(fbc_master_t *master_ptr, fb_opaque_batch_t *batch_ptr, php_stream *stream,
    ISC_STATUS* status_vector);
int fbbatch_set_default_bpb(fbc_master_t *master_ptr, fb_opaque_batch_t *batch_ptr, unsigned bpb_length,
    const unsigned char* bpb, ISC_STATUS* status_vector);
#endif

#ifdef __cplusplus
}
#endif

#endif /* FIREBIRD_UTILS_H */
