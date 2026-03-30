/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

/**
 * firebird_utils_typed.h - Type-safe wrapper header for firebird_utils.h
 *
 * This header wraps the void* API in firebird_utils.h with forward-declared
 * opaque struct types, providing compile-time type safety without modifying
 * the frozen firebird_utils.h / firebird_utils.cpp files.
 *
 * Design: each typed struct contains a single void* member `p` so that
 * existing raw pointers can be wrapped with zero runtime overhead.
 * Inline wrappers delegate directly to the underlying void* API.
 *
 * Migration path:
 *   Before: fbc_disconnect(raw_ptr, sv);
 *   After:  fbc_connection_t conn = { raw_ptr };
 *           fbc_disconnect_typed(&conn, sv);
 *
 * v10.0.0 - void* elimination (spec-v10-void-star-elimination.md)
 *
 * NOTE: The following functions are intentionally NOT wrapped because they
 * do not take opaque handle void* parameters and need no type-safety shim:
 *   fbu_sqlcode()           - takes const ISC_STATUS*, no handle
 *   fba_array_lookup_bounds() - uses typed isc_db_handle*/isc_tr_handle* directly
 *   fbe_event_block()       - takes unsigned char** buffers only
 *   fbe_event_free()        - takes unsigned char* buffer only
 *   fbe_event_counts()      - takes buffer pointers only
 *   fbxpb_free_tpb()        - takes unsigned char* buffer only
 *   fbbatch_free_errors()   - takes struct pointer only
 *   fbbatch_free_result()   - takes struct pointer only
 */

#ifndef FIREBIRD_UTILS_TYPED_H
#define FIREBIRD_UTILS_TYPED_H

#include "firebird_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Opaque typed structs
 * Each struct wraps a single void* pointer. Callers construct them from
 * the raw pointer returned by the underlying fbc_*/fbt_*/fbs_*/fbb_*/etc.
 * functions and pass &typed_var to the type-safe wrappers below.
 * ========================================================================= */

/** Connection wrapper (fb::Connection*) */
typedef struct fbc_connection_t  { void *p; } fbc_connection_t;

/** Transaction wrapper (fb::Transaction*) */
typedef struct fbt_transaction_t { void *p; } fbt_transaction_t;

/** Statement wrapper (fb::StatementWrapper*) */
typedef struct fbs_statement_t   { void *p; } fbs_statement_t;

/** Blob wrapper (fb::BlobWrapper*) */
typedef struct fbb_blob_t        { void *p; } fbb_blob_t;

/** IMaster pointer (obtained from fb_get_master_interface()) */
typedef struct fbc_master_t      { void *p; } fbc_master_t;

/** IAttachment raw pointer (from fbc_get_attachment()) */
typedef struct fbc_attachment_t  { void *p; } fbc_attachment_t;

/** ITransaction raw pointer (from fbt_get_handle()) */
typedef struct fbt_handle_t      { void *p; } fbt_handle_t;

/** IMessageMetadata pointer (from fbs_get_{input,output}_metadata()) */
typedef struct fbm_metadata_t    { void *p; } fbm_metadata_t;

/** Events wrapper (fb::EventsWrapper* from fbe_queue()) */
typedef struct fbe_events_t      { void *p; } fbe_events_t;

/** Service wrapper (fb::ServiceWrapper* from fbsvc_attach()) */
typedef struct fbsvc_service_t   { void *p; } fbsvc_service_t;

#if FB_API_VER >= 40
/** Batch wrapper (from fbbatch_create()) */
typedef struct fbbatch_batch_t   { void *p; } fbbatch_batch_t;
#endif

/* =========================================================================
 * Private helper macro (undefined at end of header)
 * ========================================================================= */
#define _FBTP(x)  ((x) ? (x)->p : NULL)

/* =========================================================================
 * Type-safe inline wrappers - Utility / datetime functions (fbu_*)
 * ========================================================================= */

static inline unsigned fbu_get_client_version_typed(fbc_master_t *master) {
    return fbu_get_client_version(_FBTP(master));
}

static inline ISC_TIME fbu_encode_time_typed(fbc_master_t *master,
        unsigned hours, unsigned minutes, unsigned seconds, unsigned fractions) {
    return fbu_encode_time(_FBTP(master), hours, minutes, seconds, fractions);
}

static inline ISC_DATE fbu_encode_date_typed(fbc_master_t *master,
        unsigned year, unsigned month, unsigned day) {
    return fbu_encode_date(_FBTP(master), year, month, day);
}

static inline ISC_TIMESTAMP fbu_encode_timestamp_typed(fbc_master_t *master,
        unsigned year, unsigned month, unsigned day,
        unsigned hours, unsigned minutes, unsigned seconds, unsigned fractions) {
    return fbu_encode_timestamp(_FBTP(master), year, month, day,
                                hours, minutes, seconds, fractions);
}

static inline void fbu_decode_time_typed(fbc_master_t *master, ISC_TIME time,
        unsigned *hours, unsigned *minutes, unsigned *seconds, unsigned *fractions) {
    fbu_decode_time(_FBTP(master), time, hours, minutes, seconds, fractions);
}

static inline void fbu_decode_date_typed(fbc_master_t *master, ISC_DATE date,
        unsigned *year, unsigned *month, unsigned *day) {
    fbu_decode_date(_FBTP(master), date, year, month, day);
}

static inline void fbu_decode_timestamp_typed(fbc_master_t *master,
        const ISC_TIMESTAMP *ts,
        unsigned *year, unsigned *month, unsigned *day,
        unsigned *hours, unsigned *minutes, unsigned *seconds, unsigned *fractions) {
    fbu_decode_timestamp(_FBTP(master), ts,
                         year, month, day, hours, minutes, seconds, fractions);
}

static inline int fbu_insert_field_info_typed(
        fbc_master_t *master, ISC_STATUS *status,
        int is_outvar, int num, zval *into_array,
        fbs_statement_t *stmt) {
    return fbu_insert_field_info(_FBTP(master), status, is_outvar, num,
                                 into_array, _FBTP(stmt));
}

static inline int fbu_insert_aliases_typed(
        fbc_master_t *master, ISC_STATUS *status,
        fbird_query *ib_query, fbs_statement_t *stmt) {
    return fbu_insert_aliases(_FBTP(master), status, ib_query, _FBTP(stmt));
}

/* =========================================================================
 * Type-safe inline wrappers - Connection functions (fbc_*)
 * ========================================================================= */

static inline fbc_connection_t fbc_connect_typed(
        fbc_master_t *master,
        const char *database, size_t db_len,
        const char *user, size_t user_len,
        const char *password, size_t password_len,
        const char *charset, size_t charset_len,
        const char *role, size_t role_len,
        int num_buffers, int dialect, int force_write,
        ISC_STATUS *status_vector) {
    fbc_connection_t c;
    c.p = fbc_connect(_FBTP(master),
                      database, db_len, user, user_len,
                      password, password_len, charset, charset_len,
                      role, role_len,
                      num_buffers, dialect, force_write, status_vector);
    return c;
}

static inline int fbc_disconnect_typed(fbc_connection_t *conn, ISC_STATUS *sv) {
    return fbc_disconnect(_FBTP(conn), sv);
}

static inline int fbc_drop_database_typed(fbc_connection_t *conn, ISC_STATUS *sv) {
    return fbc_drop_database(_FBTP(conn), sv);
}

static inline fbc_connection_t fbc_create_database_typed(
        fbc_master_t *master, const char *create_sql,
        unsigned dialect, ISC_STATUS *sv) {
    fbc_connection_t c;
    c.p = fbc_create_database(_FBTP(master), create_sql, dialect, sv);
    return c;
}

static inline int fbc_is_connected_typed(fbc_connection_t *conn) {
    return fbc_is_connected(_FBTP(conn));
}

static inline int fbc_ping_typed(fbc_master_t *master, fbc_connection_t *conn,
        ISC_STATUS *sv) {
    return fbc_ping(_FBTP(master), _FBTP(conn), sv);
}

static inline fbc_attachment_t fbc_get_attachment_typed(fbc_connection_t *conn) {
    fbc_attachment_t a;
    a.p = fbc_get_attachment(_FBTP(conn));
    return a;
}

static inline unsigned fbc_get_server_version_typed(fbc_connection_t *conn) {
    return fbc_get_server_version(_FBTP(conn));
}

static inline int fbc_get_info_typed(
        fbc_master_t *master, fbc_attachment_t *att,
        unsigned items_length, const unsigned char *items,
        unsigned buffer_length, unsigned char *buffer,
        ISC_STATUS *sv) {
    return fbc_get_info(_FBTP(master), _FBTP(att),
                        items_length, items, buffer_length, buffer, sv);
}

/* =========================================================================
 * Type-safe inline wrappers - Transaction functions (fbt_*)
 * ========================================================================= */

static inline fbt_transaction_t fbt_start_typed(
        fbc_master_t *master, fbc_attachment_t *att,
        unsigned tpb_len, const unsigned char *tpb,
        ISC_STATUS *sv) {
    fbt_transaction_t t;
    t.p = fbt_start(_FBTP(master), _FBTP(att), tpb_len, tpb, sv);
    return t;
}

static inline int fbt_commit_typed(fbt_transaction_t *trans, ISC_STATUS *sv) {
    return fbt_commit(_FBTP(trans), sv);
}

static inline int fbt_rollback_typed(fbt_transaction_t *trans, ISC_STATUS *sv) {
    return fbt_rollback(_FBTP(trans), sv);
}

static inline int fbt_commit_retaining_typed(fbt_transaction_t *trans, ISC_STATUS *sv) {
    return fbt_commit_retaining(_FBTP(trans), sv);
}

static inline int fbt_rollback_retaining_typed(fbt_transaction_t *trans, ISC_STATUS *sv) {
    return fbt_rollback_retaining(_FBTP(trans), sv);
}

static inline int fbt_is_active_typed(fbt_transaction_t *trans) {
    return fbt_is_active(_FBTP(trans));
}

static inline fbt_handle_t fbt_get_handle_typed(fbt_transaction_t *trans) {
    fbt_handle_t h;
    h.p = fbt_get_handle(_FBTP(trans));
    return h;
}

static inline void fbt_free_typed(fbt_transaction_t *trans) {
    fbt_free(_FBTP(trans));
}

static inline int fbt_get_info_typed(
        fbc_master_t *master, fbt_handle_t *trans_handle,
        unsigned items_length, const unsigned char *items,
        unsigned buffer_length, unsigned char *buffer,
        ISC_STATUS *sv) {
    return fbt_get_info(_FBTP(master), _FBTP(trans_handle),
                        items_length, items, buffer_length, buffer, sv);
}

static inline int fbt_get_limbo_transactions_typed(
        fbc_master_t *master, fbc_attachment_t *att,
        ISC_INT64 *trans_ids, unsigned max_ids, ISC_STATUS *sv) {
    return fbt_get_limbo_transactions(_FBTP(master), _FBTP(att),
                                      trans_ids, max_ids, sv);
}

static inline fbt_transaction_t fbt_reconnect_typed(
        fbc_master_t *master, fbc_attachment_t *att,
        ISC_INT64 trans_id, ISC_STATUS *sv) {
    fbt_transaction_t t;
    t.p = fbt_reconnect(_FBTP(master), _FBTP(att), trans_id, sv);
    return t;
}

/* =========================================================================
 * Type-safe inline wrappers - Statement functions (fbs_*)
 * ========================================================================= */

static inline fbs_statement_t fbs_prepare_typed(
        fbc_master_t *master, fbc_attachment_t *att, fbt_handle_t *trans_handle,
        const char *sql, unsigned sql_length, unsigned dialect,
        ISC_STATUS *sv) {
    fbs_statement_t s;
    s.p = fbs_prepare(_FBTP(master), _FBTP(att), _FBTP(trans_handle),
                      sql, sql_length, dialect, sv);
    return s;
}

static inline int fbs_execute_typed(
        fbc_master_t *master, fbs_statement_t *stmt, fbt_handle_t *trans_handle,
        void *in_msg, void *in_metadata,
        void *out_msg, void *out_metadata,
        ISC_STATUS *sv) {
    return fbs_execute(_FBTP(master), _FBTP(stmt), _FBTP(trans_handle),
                       in_msg, in_metadata, out_msg, out_metadata, sv);
}

static inline int fbs_open_cursor_typed(
        fbc_master_t *master, fbs_statement_t *stmt, fbt_handle_t *trans_handle,
        void *in_msg, void *in_metadata, unsigned cursor_flags,
        ISC_STATUS *sv) {
    return fbs_open_cursor(_FBTP(master), _FBTP(stmt), _FBTP(trans_handle),
                           in_msg, in_metadata, cursor_flags, sv);
}

static inline int fbs_fetch_typed(
        fbc_master_t *master, fbs_statement_t *stmt,
        void *out_msg, ISC_STATUS *sv) {
    return fbs_fetch(_FBTP(master), _FBTP(stmt), out_msg, sv);
}

static inline int fbs_fetch_prior_typed(
        fbc_master_t *master, fbs_statement_t *stmt,
        void *out_msg, ISC_STATUS *sv) {
    return fbs_fetch_prior(_FBTP(master), _FBTP(stmt), out_msg, sv);
}

static inline int fbs_fetch_first_typed(
        fbc_master_t *master, fbs_statement_t *stmt,
        void *out_msg, ISC_STATUS *sv) {
    return fbs_fetch_first(_FBTP(master), _FBTP(stmt), out_msg, sv);
}

static inline int fbs_fetch_last_typed(
        fbc_master_t *master, fbs_statement_t *stmt,
        void *out_msg, ISC_STATUS *sv) {
    return fbs_fetch_last(_FBTP(master), _FBTP(stmt), out_msg, sv);
}

static inline int fbs_fetch_absolute_typed(
        fbc_master_t *master, fbs_statement_t *stmt,
        int position, void *out_msg, ISC_STATUS *sv) {
    return fbs_fetch_absolute(_FBTP(master), _FBTP(stmt), position, out_msg, sv);
}

static inline int fbs_fetch_relative_typed(
        fbc_master_t *master, fbs_statement_t *stmt,
        int offset, void *out_msg, ISC_STATUS *sv) {
    return fbs_fetch_relative(_FBTP(master), _FBTP(stmt), offset, out_msg, sv);
}

static inline int fbs_close_cursor_typed(fbs_statement_t *stmt, ISC_STATUS *sv) {
    return fbs_close_cursor(_FBTP(stmt), sv);
}

static inline int fbs_free_typed(fbs_statement_t *stmt, ISC_STATUS *sv) {
    return fbs_free(_FBTP(stmt), sv);
}

static inline unsigned fbs_get_type_typed(
        fbc_master_t *master, fbs_statement_t *stmt, ISC_STATUS *sv) {
    return fbs_get_type(_FBTP(master), _FBTP(stmt), sv);
}

static inline ISC_UINT64 fbs_get_affected_records_typed(
        fbc_master_t *master, fbs_statement_t *stmt, ISC_STATUS *sv) {
    return fbs_get_affected_records(_FBTP(master), _FBTP(stmt), sv);
}

static inline fbm_metadata_t fbs_get_input_metadata_typed(
        fbc_master_t *master, fbs_statement_t *stmt, ISC_STATUS *sv) {
    fbm_metadata_t m;
    m.p = fbs_get_input_metadata(_FBTP(master), _FBTP(stmt), sv);
    return m;
}

static inline fbm_metadata_t fbs_get_output_metadata_typed(
        fbc_master_t *master, fbs_statement_t *stmt, ISC_STATUS *sv) {
    fbm_metadata_t m;
    m.p = fbs_get_output_metadata(_FBTP(master), _FBTP(stmt), sv);
    return m;
}

static inline unsigned fbs_get_input_count_typed(
        fbc_master_t *master, fbs_statement_t *stmt, ISC_STATUS *sv) {
    return fbs_get_input_count(_FBTP(master), _FBTP(stmt), sv);
}

static inline unsigned fbs_get_output_count_typed(
        fbc_master_t *master, fbs_statement_t *stmt, ISC_STATUS *sv) {
    return fbs_get_output_count(_FBTP(master), _FBTP(stmt), sv);
}

static inline void *fbs_get_statement_typed(fbs_statement_t *stmt) {
    return fbs_get_statement(_FBTP(stmt));
}

static inline int fbs_is_prepared_typed(fbs_statement_t *stmt) {
    return fbs_is_prepared(_FBTP(stmt));
}

static inline int fbs_is_cursor_open_typed(fbs_statement_t *stmt) {
    return fbs_is_cursor_open(_FBTP(stmt));
}

static inline int fbs_set_cursor_name_typed(
        fbc_master_t *master, fbs_statement_t *stmt,
        const char *cursor_name, ISC_STATUS *sv) {
    return fbs_set_cursor_name(_FBTP(master), _FBTP(stmt), cursor_name, sv);
}

static inline ISC_INT64 fbs_execute_singleton_int64_typed(
        fbc_master_t *master, fbs_statement_t *stmt,
        fbt_handle_t *trans_handle, ISC_STATUS *sv) {
    return fbs_execute_singleton_int64(_FBTP(master), _FBTP(stmt),
                                       _FBTP(trans_handle), sv);
}

/* =========================================================================
 * Type-safe inline wrappers - Blob functions (fbb_*)
 * ========================================================================= */

static inline fbb_blob_t fbb_create_typed(
        fbc_master_t *master, fbc_attachment_t *att, fbt_handle_t *trans_handle,
        ISC_QUAD *blob_id,
        unsigned bpb_length, const unsigned char *bpb,
        ISC_STATUS *sv) {
    fbb_blob_t b;
    b.p = fbb_create(_FBTP(master), _FBTP(att), _FBTP(trans_handle),
                     blob_id, bpb_length, bpb, sv);
    return b;
}

static inline fbb_blob_t fbb_open_typed(
        fbc_master_t *master, fbc_attachment_t *att, fbt_handle_t *trans_handle,
        const ISC_QUAD *blob_id,
        unsigned bpb_length, const unsigned char *bpb,
        ISC_STATUS *sv) {
    fbb_blob_t b;
    b.p = fbb_open(_FBTP(master), _FBTP(att), _FBTP(trans_handle),
                   blob_id, bpb_length, bpb, sv);
    return b;
}

static inline int fbb_put_segment_typed(
        fbc_master_t *master, fbb_blob_t *blob,
        unsigned length, const void *buffer, ISC_STATUS *sv) {
    return fbb_put_segment(_FBTP(master), _FBTP(blob), length, buffer, sv);
}

static inline int fbb_get_segment_typed(
        fbc_master_t *master, fbb_blob_t *blob,
        unsigned buffer_length, void *buffer,
        unsigned *actual_length, ISC_STATUS *sv) {
    return fbb_get_segment(_FBTP(master), _FBTP(blob),
                           buffer_length, buffer, actual_length, sv);
}

static inline int fbb_close_typed(
        fbc_master_t *master, fbb_blob_t *blob, ISC_STATUS *sv) {
    return fbb_close(_FBTP(master), _FBTP(blob), sv);
}

static inline int fbb_seek_typed(
        fbc_master_t *master, fbb_blob_t *blob,
        int whence, int offset, int *result_position, ISC_STATUS *sv) {
    return fbb_seek(_FBTP(master), _FBTP(blob), whence, offset, result_position, sv);
}

static inline int fbb_cancel_typed(
        fbc_master_t *master, fbb_blob_t *blob, ISC_STATUS *sv) {
    return fbb_cancel(_FBTP(master), _FBTP(blob), sv);
}

static inline int fbb_get_info_typed(
        fbc_master_t *master, fbb_blob_t *blob,
        unsigned items_length, const unsigned char *items,
        unsigned buffer_length, unsigned char *buffer,
        ISC_STATUS *sv) {
    return fbb_get_info(_FBTP(master), _FBTP(blob),
                        items_length, items, buffer_length, buffer, sv);
}

static inline void fbb_get_blob_id_typed(fbb_blob_t *blob, ISC_QUAD *blob_id) {
    fbb_get_blob_id(_FBTP(blob), blob_id);
}

static inline int fbb_is_open_typed(fbb_blob_t *blob) {
    return fbb_is_open(_FBTP(blob));
}

static inline void *fbb_get_handle_typed(fbb_blob_t *blob) {
    return fbb_get_handle(_FBTP(blob));
}

static inline void fbb_free_typed(fbb_blob_t *blob) {
    fbb_free(_FBTP(blob));
}

/* =========================================================================
 * Type-safe inline wrappers - Event functions (fbe_*)
 * ========================================================================= */

static inline fbe_events_t fbe_queue_typed(
        fbc_master_t *master, fbc_attachment_t *att,
        unsigned length, const unsigned char *events,
        ISC_STATUS *sv) {
    fbe_events_t e;
    e.p = fbe_queue(_FBTP(master), _FBTP(att), length, events, sv);
    return e;
}

static inline int fbe_cancel_typed(
        fbc_master_t *master, fbe_events_t *events, ISC_STATUS *sv) {
    return fbe_cancel(_FBTP(master), _FBTP(events), sv);
}

static inline int fbe_has_event_fired_typed(fbe_events_t *events) {
    return fbe_has_event_fired(_FBTP(events));
}

static inline void fbe_reset_event_fired_typed(fbe_events_t *events) {
    fbe_reset_event_fired(_FBTP(events));
}

static inline const unsigned char *fbe_get_event_data_typed(
        fbe_events_t *events, unsigned *length) {
    return fbe_get_event_data(_FBTP(events), length);
}

static inline int fbe_is_queued_typed(fbe_events_t *events) {
    return fbe_is_queued(_FBTP(events));
}

static inline void fbe_free_typed(fbe_events_t *events) {
    fbe_free(_FBTP(events));
}

static inline ISC_STATUS fbe_wait_for_event_oo_typed(
        ISC_STATUS *sv, fbc_attachment_t *att,
        unsigned short buffer_length,
        unsigned char *event_buffer, unsigned char *result_buffer) {
    return fbe_wait_for_event_oo(sv, _FBTP(att),
                                 buffer_length, event_buffer, result_buffer);
}

/* =========================================================================
 * Type-safe inline wrappers - Service functions (fbsvc_*)
 * ========================================================================= */

static inline fbsvc_service_t fbsvc_attach_typed(
        fbc_master_t *master,
        const char *service_name,
        unsigned spb_length, const unsigned char *spb,
        ISC_STATUS *sv) {
    fbsvc_service_t s;
    s.p = fbsvc_attach(_FBTP(master), service_name, spb_length, spb, sv);
    return s;
}

static inline int fbsvc_detach_typed(
        fbc_master_t *master, fbsvc_service_t *svc, ISC_STATUS *sv) {
    return fbsvc_detach(_FBTP(master), _FBTP(svc), sv);
}

static inline int fbsvc_start_typed(
        fbc_master_t *master, fbsvc_service_t *svc,
        unsigned spb_length, const unsigned char *spb, ISC_STATUS *sv) {
    return fbsvc_start(_FBTP(master), _FBTP(svc), spb_length, spb, sv);
}

static inline int fbsvc_query_typed(
        fbc_master_t *master, fbsvc_service_t *svc,
        unsigned send_length, const unsigned char *send_items,
        unsigned recv_length, const unsigned char *recv_items,
        unsigned buffer_length, unsigned char *buffer,
        ISC_STATUS *sv) {
    return fbsvc_query(_FBTP(master), _FBTP(svc),
                       send_length, send_items,
                       recv_length, recv_items,
                       buffer_length, buffer, sv);
}

static inline int fbsvc_is_attached_typed(fbsvc_service_t *svc) {
    return fbsvc_is_attached(_FBTP(svc));
}

static inline void fbsvc_free_typed(fbsvc_service_t *svc) {
    fbsvc_free(_FBTP(svc));
}

/* =========================================================================
 * Type-safe inline wrappers - Metadata functions (fbm_*)
 * ========================================================================= */

static inline unsigned fbm_get_message_length_typed(
        fbc_master_t *master, fbm_metadata_t *meta) {
    return fbm_get_message_length(_FBTP(master), _FBTP(meta));
}

static inline unsigned fbm_get_count_typed(
        fbc_master_t *master, fbm_metadata_t *meta) {
    return fbm_get_count(_FBTP(master), _FBTP(meta));
}

static inline unsigned fbm_get_offset_typed(
        fbc_master_t *master, fbm_metadata_t *meta, unsigned index) {
    return fbm_get_offset(_FBTP(master), _FBTP(meta), index);
}

static inline unsigned fbm_get_null_offset_typed(
        fbc_master_t *master, fbm_metadata_t *meta, unsigned index) {
    return fbm_get_null_offset(_FBTP(master), _FBTP(meta), index);
}

static inline unsigned fbm_get_type_typed(
        fbc_master_t *master, fbm_metadata_t *meta, unsigned index) {
    return fbm_get_type(_FBTP(master), _FBTP(meta), index);
}

static inline unsigned fbm_get_subtype_typed(
        fbc_master_t *master, fbm_metadata_t *meta, unsigned index) {
    return fbm_get_subtype(_FBTP(master), _FBTP(meta), index);
}

static inline unsigned fbm_get_length_typed(
        fbc_master_t *master, fbm_metadata_t *meta, unsigned index) {
    return fbm_get_length(_FBTP(master), _FBTP(meta), index);
}

static inline int fbm_get_scale_typed(
        fbc_master_t *master, fbm_metadata_t *meta, unsigned index) {
    return fbm_get_scale(_FBTP(master), _FBTP(meta), index);
}

static inline unsigned fbm_get_charset_typed(
        fbc_master_t *master, fbm_metadata_t *meta, unsigned index) {
    return fbm_get_charset(_FBTP(master), _FBTP(meta), index);
}

static inline const char *fbm_get_field_typed(
        fbc_master_t *master, fbm_metadata_t *meta, unsigned index) {
    return fbm_get_field(_FBTP(master), _FBTP(meta), index);
}

static inline const char *fbm_get_alias_typed(
        fbc_master_t *master, fbm_metadata_t *meta, unsigned index) {
    return fbm_get_alias(_FBTP(master), _FBTP(meta), index);
}

static inline const char *fbm_get_relation_typed(
        fbc_master_t *master, fbm_metadata_t *meta, unsigned index) {
    return fbm_get_relation(_FBTP(master), _FBTP(meta), index);
}

static inline void fbm_release_typed(fbm_metadata_t *meta) {
    fbm_release(_FBTP(meta));
}

/* =========================================================================
 * Type-safe inline wrappers - Array functions (fba_*)
 * ========================================================================= */

static inline int fba_get_slice_typed(
        fbc_master_t *master, fbc_attachment_t *att, fbt_handle_t *trans_handle,
        ISC_QUAD *array_id, const ISC_ARRAY_DESC *desc,
        void *buffer, ISC_LONG *buffer_length,
        ISC_STATUS *sv) {
    return fba_get_slice(_FBTP(master), _FBTP(att), _FBTP(trans_handle),
                         array_id, desc, buffer, buffer_length, sv);
}

static inline int fba_put_slice_typed(
        fbc_master_t *master, fbc_attachment_t *att, fbt_handle_t *trans_handle,
        ISC_QUAD *array_id, const ISC_ARRAY_DESC *desc,
        const void *buffer, ISC_LONG buffer_length,
        ISC_STATUS *sv) {
    return fba_put_slice(_FBTP(master), _FBTP(att), _FBTP(trans_handle),
                         array_id, desc, buffer, buffer_length, sv);
}

static inline int fba_lookup_bounds_typed(
        fbc_master_t *master, fbc_attachment_t *att, fbt_handle_t *trans_handle,
        const char *relation_name, const char *field_name,
        ISC_ARRAY_DESC *desc, ISC_STATUS *sv) {
    return fba_lookup_bounds(_FBTP(master), _FBTP(att), _FBTP(trans_handle),
                             relation_name, field_name, desc, sv);
}

/* =========================================================================
 * Type-safe inline wrappers - TPB builder (fbxpb_*)
 * ========================================================================= */

static inline unsigned char *fbxpb_build_tpb_typed(
        fbc_master_t *master,
        zend_long trans_flags, zend_long lock_timeout,
        unsigned *buffer_length, ISC_STATUS *sv) {
    return fbxpb_build_tpb(_FBTP(master), trans_flags, lock_timeout,
                            buffer_length, sv);
}

/* =========================================================================
 * Type-safe inline wrappers - Batch functions (fbbatch_*), FB 4.0+ only
 * ========================================================================= */

#if FB_API_VER >= 40

static inline fbbatch_batch_t fbbatch_create_typed(
        fbc_master_t *master, void *statement_ptr,
        unsigned buffer_size, ISC_STATUS *sv) {
    fbbatch_batch_t b;
    b.p = fbbatch_create(_FBTP(master), statement_ptr, buffer_size, sv);
    return b;
}

static inline int fbbatch_add_typed(
        fbc_master_t *master, fbbatch_batch_t *batch,
        unsigned count, const void *in_buffer, ISC_STATUS *sv) {
    return fbbatch_add(_FBTP(master), _FBTP(batch), count, in_buffer, sv);
}

static inline int fbbatch_execute_typed(
        fbc_master_t *master, fbbatch_batch_t *batch,
        fbt_handle_t *trans_handle,
        unsigned *total_processed, unsigned *error_count,
        ISC_STATUS *sv) {
    return fbbatch_execute(_FBTP(master), _FBTP(batch), _FBTP(trans_handle),
                           total_processed, error_count, sv);
}

static inline int fbbatch_cancel_typed(
        fbc_master_t *master, fbbatch_batch_t *batch, ISC_STATUS *sv) {
    return fbbatch_cancel(_FBTP(master), _FBTP(batch), sv);
}

static inline int fbbatch_close_typed(
        fbc_master_t *master, fbbatch_batch_t *batch, ISC_STATUS *sv) {
    return fbbatch_close(_FBTP(master), _FBTP(batch), sv);
}

static inline fbm_metadata_t fbbatch_get_metadata_typed(
        fbc_master_t *master, fbbatch_batch_t *batch, ISC_STATUS *sv) {
    fbm_metadata_t m;
    m.p = fbbatch_get_metadata(_FBTP(master), _FBTP(batch), sv);
    return m;
}

static inline unsigned fbbatch_get_blob_alignment_typed(
        fbc_master_t *master, fbbatch_batch_t *batch, ISC_STATUS *sv) {
    return fbbatch_get_blob_alignment(_FBTP(master), _FBTP(batch), sv);
}

static inline int fbbatch_add_blob_typed(
        fbc_master_t *master, fbbatch_batch_t *batch,
        unsigned length, const void *data,
        ISC_QUAD *blob_id_out,
        unsigned bpb_length, const unsigned char *bpb,
        ISC_STATUS *sv) {
    return fbbatch_add_blob(_FBTP(master), _FBTP(batch), length, data,
                            blob_id_out, bpb_length, bpb, sv);
}

static inline int fbbatch_append_blob_data_typed(
        fbc_master_t *master, fbbatch_batch_t *batch,
        unsigned length, const void *data, ISC_STATUS *sv) {
    return fbbatch_append_blob_data(_FBTP(master), _FBTP(batch), length, data, sv);
}

static inline int fbbatch_add_blob_stream_typed(
        fbc_master_t *master, fbbatch_batch_t *batch,
        unsigned length, const void *data, ISC_STATUS *sv) {
    return fbbatch_add_blob_stream(_FBTP(master), _FBTP(batch), length, data, sv);
}

static inline int fbbatch_register_blob_typed(
        fbc_master_t *master, fbbatch_batch_t *batch,
        const ISC_QUAD *existing_blob, ISC_QUAD *batch_blob_id,
        ISC_STATUS *sv) {
    return fbbatch_register_blob(_FBTP(master), _FBTP(batch),
                                 existing_blob, batch_blob_id, sv);
}

static inline int fbbatch_set_default_bpb_typed(
        fbc_master_t *master, fbbatch_batch_t *batch,
        unsigned bpb_length, const unsigned char *bpb,
        ISC_STATUS *sv) {
    return fbbatch_set_default_bpb(_FBTP(master), _FBTP(batch),
                                   bpb_length, bpb, sv);
}

static inline int fbbatch_execute_detailed_typed(
        fbc_master_t *master, fbbatch_batch_t *batch,
        fbt_handle_t *trans_handle,
        fbbatch_completion_result *result, ISC_STATUS *sv) {
    return fbbatch_execute_detailed(_FBTP(master), _FBTP(batch),
                                    _FBTP(trans_handle), result, sv);
}

#endif /* FB_API_VER >= 40 */

/* =========================================================================
 * Type-safe inline wrappers - FB 4.0+ Timezone-aware datetime (fbu_*_tz)
 * ========================================================================= */

#if FB_API_VER >= 40

static inline void fbu_decode_time_tz_typed(
        fbc_master_t *master, const ISC_TIME_TZ *time_tz,
        unsigned *hours, unsigned *minutes, unsigned *seconds, unsigned *fractions,
        unsigned tz_buf_len, char *tz_buf) {
    fbu_decode_time_tz(_FBTP(master), time_tz,
                       hours, minutes, seconds, fractions, tz_buf_len, tz_buf);
}

static inline void fbu_decode_timestamp_tz_typed(
        fbc_master_t *master, const ISC_TIMESTAMP_TZ *ts_tz,
        unsigned *year, unsigned *month, unsigned *day,
        unsigned *hours, unsigned *minutes, unsigned *seconds, unsigned *fractions,
        unsigned tz_buf_len, char *tz_buf) {
    fbu_decode_timestamp_tz(_FBTP(master), ts_tz,
                            year, month, day,
                            hours, minutes, seconds, fractions,
                            tz_buf_len, tz_buf);
}

static inline int fbu_encode_time_tz_typed(
        fbc_master_t *master, ISC_TIME_TZ *time_tz,
        unsigned hours, unsigned minutes, unsigned seconds, unsigned fractions,
        const char *time_zone) {
    return fbu_encode_time_tz(_FBTP(master), time_tz,
                              hours, minutes, seconds, fractions, time_zone);
}

static inline int fbu_encode_timestamp_tz_typed(
        fbc_master_t *master, ISC_TIMESTAMP_TZ *ts_tz,
        unsigned year, unsigned month, unsigned day,
        unsigned hours, unsigned minutes, unsigned seconds, unsigned fractions,
        const char *time_zone) {
    return fbu_encode_timestamp_tz(_FBTP(master), ts_tz,
                                   year, month, day,
                                   hours, minutes, seconds, fractions, time_zone);
}

static inline int fbu_int128_to_string_typed(
        fbc_master_t *master, const void *value, int scale,
        char *buffer, unsigned buffer_length) {
    return fbu_int128_to_string(_FBTP(master), value, scale, buffer, buffer_length);
}

static inline int fbu_decfloat16_to_string_typed(
        fbc_master_t *master, const void *value,
        char *buffer, unsigned buffer_length) {
    return fbu_decfloat16_to_string(_FBTP(master), value, buffer, buffer_length);
}

static inline int fbu_decfloat34_to_string_typed(
        fbc_master_t *master, const void *value,
        char *buffer, unsigned buffer_length) {
    return fbu_decfloat34_to_string(_FBTP(master), value, buffer, buffer_length);
}

#endif /* FB_API_VER >= 40 */

#undef _FBTP

#ifdef __cplusplus
}
#endif

#endif /* FIREBIRD_UTILS_TYPED_H */
