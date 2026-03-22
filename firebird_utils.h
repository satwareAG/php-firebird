/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

#ifndef FIREBIRD_UTILS_H
#define FIREBIRD_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Firebird 3.0+ OO API Required
 *
 * This extension requires Firebird 3.0 or later. The modern OO API introduced
 * in Firebird 3.0 (FB_API_VER >= 30) is mandatory.
 */
#if FB_API_VER < 30
#error "This extension requires Firebird 3.0 or later (FB_API_VER >= 30). Legacy API is not supported."
#endif

#include <ibase.h>
#include "php_fbird_includes.h"

unsigned fbu_get_client_version(void *master_ptr);
ISC_TIME fbu_encode_time(void *master_ptr, unsigned hours, unsigned minutes,
  unsigned seconds, unsigned fractions);
ISC_DATE fbu_encode_date(void *master_ptr, unsigned year, unsigned month, unsigned day);

/* Type Encoding/Decoding Functions (OO API via IUtil interface) */

/**
 * Encode timestamp from components using OO API.
 *
 * @param master_ptr IMaster interface pointer
 * @param year Year (1-9999)
 * @param month Month (1-12)
 * @param day Day (1-31)
 * @param hours Hours (0-23)
 * @param minutes Minutes (0-59)
 * @param seconds Seconds (0-59)
 * @param fractions Fractions of second (0-9999, tenths of milliseconds)
 * @return Encoded ISC_TIMESTAMP, or {0,0} on error
 */
ISC_TIMESTAMP fbu_encode_timestamp(void *master_ptr, unsigned year, unsigned month, unsigned day,
    unsigned hours, unsigned minutes, unsigned seconds, unsigned fractions);

/**
 * Decode ISC_TIME to time components using OO API.
 * Replaces legacy isc_decode_sql_time().
 *
 * @param master_ptr IMaster interface pointer
 * @param time ISC_TIME value to decode
 * @param hours Output: hours (0-23)
 * @param minutes Output: minutes (0-59)
 * @param seconds Output: seconds (0-59)
 * @param fractions Output: fractions of second (0-9999)
 */
void fbu_decode_time(void *master_ptr, ISC_TIME time,
    unsigned* hours, unsigned* minutes, unsigned* seconds, unsigned* fractions);

/**
 * Decode ISC_DATE to date components using OO API.
 * Replaces legacy isc_decode_sql_date().
 *
 * @param master_ptr IMaster interface pointer
 * @param date ISC_DATE value to decode
 * @param year Output: year (1-9999)
 * @param month Output: month (1-12)
 * @param day Output: day (1-31)
 */
void fbu_decode_date(void *master_ptr, ISC_DATE date,
    unsigned* year, unsigned* month, unsigned* day);

/**
 * Decode ISC_TIMESTAMP to date and time components using OO API.
 * Replaces legacy isc_decode_timestamp().
 *
 * @param master_ptr IMaster interface pointer
 * @param timestamp ISC_TIMESTAMP value to decode
 * @param year Output: year (1-9999)
 * @param month Output: month (1-12)
 * @param day Output: day (1-31)
 * @param hours Output: hours (0-23)
 * @param minutes Output: minutes (0-59)
 * @param seconds Output: seconds (0-59)
 * @param fractions Output: fractions of second (0-9999)
 */
void fbu_decode_timestamp(void *master_ptr, const ISC_TIMESTAMP* timestamp,
    unsigned* year, unsigned* month, unsigned* day,
    unsigned* hours, unsigned* minutes, unsigned* seconds, unsigned* fractions);

/* Firebird OO API Connection Functions */

/**
 * Create a database connection using the Firebird OO API.
 *
 * @param master_ptr Pointer to IMaster interface (from IBG(master_instance))
 * @param database Database path (null-terminated)
 * @param db_len Length of database path
 * @param user Username (null-terminated, may be NULL)
 * @param user_len Length of username
 * @param password Password (null-terminated, may be NULL)
 * @param password_len Length of password
 * @param charset Character set (null-terminated, may be NULL)
 * @param charset_len Length of charset
 * @param role SQL role (null-terminated, may be NULL)
 * @param role_len Length of role
 * @param num_buffers Number of page buffers (0 for default)
 * @param dialect SQL dialect (1, 2, or 3)
 * @param force_write Force write flag (-1 = not set, 0 = async, 1 = sync)
 * @param status_vector Output status vector for errors
 * @return Pointer to connection object, or NULL on failure
 */
void* fbc_connect(
    void* master_ptr,
    const char* database, size_t db_len,
    const char* user, size_t user_len,
    const char* password, size_t password_len,
    const char* charset, size_t charset_len,
    const char* role, size_t role_len,
    int num_buffers,
    int dialect,
    int force_write,
    ISC_STATUS* status_vector
);

/**
 * Detach a connection created with fbc_connect().
 *
 * @param connection Pointer returned by fbc_connect()
 * @param status_vector Output status vector for errors
 * @return 0 on success, non-zero on failure
 */
int fbc_disconnect(void* connection, ISC_STATUS* status_vector);

/**
 * Drop a database (destructive operation).
 *
 * @param connection Pointer returned by fbc_connect()
 * @param status_vector Output status vector for errors
 * @return 0 on success, non-zero on failure
 */
int fbc_drop_database(void* connection, ISC_STATUS* status_vector);

/**
 * Create a new database using CREATE DATABASE SQL statement.
 * Uses OO API IUtil::executeCreateDatabase() internally.
 *
 * @param master_ptr Pointer to IMaster interface
 * @param create_sql Complete CREATE DATABASE SQL statement
 * @param dialect SQL dialect (typically 3)
 * @param status_vector Output status vector for errors
 * @return Pointer to fb::Connection object for the new database, or NULL on failure
 */
void* fbc_create_database(
    void* master_ptr,
    const char* create_sql,
    unsigned dialect,
    ISC_STATUS* status_vector
);

/**
 * Check if a connection is valid.
 *
 * @param connection Pointer returned by fbc_connect()
 * @return 1 if connected, 0 if not
 */
int fbc_is_connected(void* connection);

/**
 * Get the IAttachment pointer from a connection.
 *
 * @param connection Pointer returned by fbc_connect()
 * @return Raw IAttachment pointer, or NULL
 */
void* fbc_get_attachment(void* connection);

/**
 * Get a pointer to the legacy isc_db_handle stored inside the connection.
 * Required for isc_wait_for_event() and similar legacy APIs that need
 * a stable pointer to the handle (not the handle value itself).
 *
 * @param connection Pointer returned by fbc_connect()
 * @return Pointer to the internal isc_db_handle, or NULL
 */
void* fbc_get_legacy_handle_ptr(void* connection);

/**
 * Get the server version from a connection.
 *
 * @param connection Pointer returned by fbc_connect()
 * @return Version code (FB30=30, FB40=40, FB50=50), or 0 if invalid
 */
unsigned fbc_get_server_version(void* connection);

/**
 * Retrieve database/connection information via the OO API.
 * Wraps Firebird::IAttachment::getInfo().
 *
 * @param master_ptr IMaster interface pointer
 * @param attachment_ptr IAttachment pointer (from fbc_get_attachment())
 * @param items_length Info items length
 * @param items Info items to request (isc_info_* constants)
 * @param buffer_length Output buffer length
 * @param buffer Output buffer
 * @param status_vector Output status vector
 * @return 1 on success, 0 on error
 */
int fbc_get_info(
    void* master_ptr,
    void* attachment_ptr,
    unsigned items_length,
    const unsigned char* items,
    unsigned buffer_length,
    unsigned char* buffer,
    ISC_STATUS* status_vector
);

/* Firebird OO API Transaction Functions */

/**
 * Start a transaction using the OO API.
 *
 * @param master_ptr Pointer to IMaster interface
 * @param attachment_ptr Pointer to IAttachment interface (from fbc_get_attachment())
 * @param tpb_len Length of TPB buffer
 * @param tpb Transaction parameter buffer (may be NULL for defaults)
 * @param status_vector Output status vector for errors
 * @return Pointer to transaction object, or NULL on failure
 */
void* fbt_start(
    void* master_ptr,
    void* attachment_ptr,
    unsigned tpb_len,
    const unsigned char* tpb,
    ISC_STATUS* status_vector
);

/**
 * Commit a transaction created with fbt_start().
 *
 * @param transaction Pointer returned by fbt_start()
 * @param status_vector Output status vector for errors
 * @return 0 on success, non-zero on failure
 */
int fbt_commit(void* transaction, ISC_STATUS* status_vector);

/**
 * Rollback a transaction created with fbt_start().
 *
 * @param transaction Pointer returned by fbt_start()
 * @param status_vector Output status vector for errors
 * @return 0 on success, non-zero on failure
 */
int fbt_rollback(void* transaction, ISC_STATUS* status_vector);

/**
 * Commit with retaining (keeps transaction context).
 *
 * @param transaction Pointer returned by fbt_start()
 * @param status_vector Output status vector for errors
 * @return 0 on success, non-zero on failure
 */
int fbt_commit_retaining(void* transaction, ISC_STATUS* status_vector);

/**
 * Rollback with retaining (keeps transaction context).
 *
 * @param transaction Pointer returned by fbt_start()
 * @param status_vector Output status vector for errors
 * @return 0 on success, non-zero on failure
 */
int fbt_rollback_retaining(void* transaction, ISC_STATUS* status_vector);

/**
 * Check if a transaction is active.
 *
 * @param transaction Pointer returned by fbt_start()
 * @return 1 if active, 0 if not
 */
int fbt_is_active(void* transaction);

/**
 * Get the ITransaction pointer from a transaction wrapper.
 *
 * @param transaction Pointer returned by fbt_start()
 * @return Raw ITransaction pointer, or NULL
 */
void* fbt_get_handle(void* transaction);

/**
 * Free a transaction wrapper without commit/rollback.
 * Use only when the transaction was already ended via other means.
 *
 * @param transaction Pointer returned by fbt_start()
 */
void fbt_free(void* transaction);

/**
 * Retrieve transaction information via the OO API.
 * Wraps Firebird::ITransaction::getInfo().
 *
 * @param master_ptr IMaster interface pointer
 * @param transaction_ptr ITransaction pointer (from fbt_get_handle())
 * @param items_length Info items length
 * @param items Info items to request
 * @param buffer_length Output buffer length
 * @param buffer Output buffer
 * @param status_vector Output status vector
 * @return 1 on success, 0 on error
 */
int fbt_get_info(
    void* master_ptr,
    void* transaction_ptr,
    unsigned items_length,
    const unsigned char* items,
    unsigned buffer_length,
    unsigned char* buffer,
    ISC_STATUS* status_vector
);

/* Firebird OO API Statement Functions */

/**
 * Prepare a statement using OO API.
 *
 * @param master_ptr IMaster interface pointer (from IBG(master_instance))
 * @param attachment_ptr IAttachment pointer (from fbc_get_attachment())
 * @param transaction_ptr ITransaction pointer (from fbt_get_handle())
 * @param sql SQL statement text
 * @param sql_length Length of SQL text (0 = null-terminated)
 * @param dialect SQL dialect (1, 2, or 3)
 * @param status_vector Output ISC_STATUS array
 * @return Opaque StatementWrapper pointer or NULL on error
 */
void* fbs_prepare(
    void* master_ptr,
    void* attachment_ptr,
    void* transaction_ptr,
    const char* sql,
    unsigned sql_length,
    unsigned dialect,
    ISC_STATUS* status_vector
);

/**
 * Execute a non-SELECT statement.
 *
 * @param master_ptr IMaster interface pointer
 * @param statement_ptr StatementWrapper pointer (from fbs_prepare())
 * @param transaction_ptr ITransaction pointer
 * @param in_msg Input message buffer
 * @param in_metadata IMessageMetadata for input
 * @param out_msg Output message buffer
 * @param out_metadata IMessageMetadata for output
 * @param status_vector Output ISC_STATUS array
 * @return 1 on success, 0 on error
 */
int fbs_execute(
    void* master_ptr,
    void* statement_ptr,
    void* transaction_ptr,
    void* in_msg,
    void* in_metadata,
    void* out_msg,
    void* out_metadata,
    ISC_STATUS* status_vector
);

/**
 * Open cursor for SELECT statement.
 *
 * @param master_ptr IMaster interface pointer
 * @param statement_ptr StatementWrapper pointer
 * @param transaction_ptr ITransaction pointer
 * @param in_msg Input message buffer
 * @param in_metadata IMessageMetadata for input
 * @param cursor_flags Cursor flags
 * @param status_vector Output ISC_STATUS array
 * @return 1 on success, 0 on error
 */
int fbs_open_cursor(
    void* master_ptr,
    void* statement_ptr,
    void* transaction_ptr,
    void* in_msg,
    void* in_metadata,
    unsigned cursor_flags,
    ISC_STATUS* status_vector
);

/**
 * Fetch next row from cursor.
 *
 * @param master_ptr IMaster interface pointer
 * @param statement_ptr StatementWrapper pointer
 * @param out_msg Output message buffer
 * @param status_vector Output ISC_STATUS array
 * @return 1 = row fetched, 0 = end of data, -1 = error
 */
int fbs_fetch(
    void* master_ptr,
    void* statement_ptr,
    void* out_msg,
    ISC_STATUS* status_vector
);

/**
 * Close cursor.
 *
 * @param statement_ptr StatementWrapper pointer
 * @param status_vector Output ISC_STATUS array
 * @return 1 on success, 0 on error
 */
int fbs_close_cursor(void* statement_ptr, ISC_STATUS* status_vector);

/**
 * Free/unprepare statement.
 *
 * @param statement_ptr StatementWrapper pointer
 * @param status_vector Output ISC_STATUS array
 * @return 1 on success, 0 on error
 */
int fbs_free(void* statement_ptr, ISC_STATUS* status_vector);

/**
 * Get statement type.
 *
 * @param master_ptr IMaster interface pointer
 * @param statement_ptr StatementWrapper pointer
 * @param status_vector Output ISC_STATUS array
 * @return Statement type constant
 */
unsigned fbs_get_type(void* master_ptr, void* statement_ptr, ISC_STATUS* status_vector);

/**
 * Get affected rows count.
 *
 * @param master_ptr IMaster interface pointer
 * @param statement_ptr StatementWrapper pointer
 * @param status_vector Output ISC_STATUS array
 * @return Affected rows count
 */
ISC_UINT64 fbs_get_affected_records(void* master_ptr, void* statement_ptr, ISC_STATUS* status_vector);

/**
 * Get input metadata.
 *
 * @param master_ptr IMaster interface pointer
 * @param statement_ptr StatementWrapper pointer
 * @param status_vector Output ISC_STATUS array
 * @return IMessageMetadata pointer or NULL
 */
void* fbs_get_input_metadata(void* master_ptr, void* statement_ptr, ISC_STATUS* status_vector);

/**
 * Get output metadata.
 *
 * @param master_ptr IMaster interface pointer
 * @param statement_ptr StatementWrapper pointer
 * @param status_vector Output ISC_STATUS array
 * @return IMessageMetadata pointer or NULL
 */
void* fbs_get_output_metadata(void* master_ptr, void* statement_ptr, ISC_STATUS* status_vector);

/**
 * Get input parameter count from statement.
 *
 * @param master_ptr IMaster interface pointer
 * @param statement_ptr StatementWrapper pointer
 * @param status_vector Output ISC_STATUS array
 * @return Number of input parameters (0 if none or error)
 */
unsigned fbs_get_input_count(void* master_ptr, void* statement_ptr, ISC_STATUS* status_vector);

/**
 * Get output field count from statement.
 *
 * @param master_ptr IMaster interface pointer
 * @param statement_ptr StatementWrapper pointer
 * @param status_vector Output ISC_STATUS array
 * @return Number of output fields (0 if none or error)
 */
unsigned fbs_get_output_count(void* master_ptr, void* statement_ptr, ISC_STATUS* status_vector);

/**
 * Get raw IStatement pointer.
 *
 * @param statement_ptr StatementWrapper pointer
 * @return Raw IStatement pointer or NULL
 */
void* fbs_get_statement(void* statement_ptr);

/**
 * Check if statement is prepared.
 *
 * @param statement_ptr StatementWrapper pointer
 * @return 1 if prepared, 0 otherwise
 */
int fbs_is_prepared(void* statement_ptr);

/**
 * Check if cursor is open.
 *
 * @param statement_ptr StatementWrapper pointer
 * @return 1 if cursor open, 0 otherwise
 */
int fbs_is_cursor_open(void* statement_ptr);

/**
 * Set cursor name for positioned updates (WHERE CURRENT OF).
 *
 * @param master_ptr IMaster interface pointer
 * @param statement_ptr StatementWrapper pointer
 * @param cursor_name Cursor name string
 * @param status_vector Output ISC_STATUS array
 * @return 1 on success, 0 on error
 */
int fbs_set_cursor_name(void* master_ptr, void* statement_ptr, const char* cursor_name, ISC_STATUS* status_vector);

/**
 * Execute a statement that returns a single INT64 value (e.g., GEN_ID()).
 * This is a convenience function that opens a cursor, fetches one row,
 * extracts the first INT64 column, and closes the cursor.
 *
 * @param master_ptr IMaster interface pointer
 * @param statement_ptr StatementWrapper pointer (from fbs_prepare())
 * @param transaction_ptr ITransaction pointer
 * @param status_vector Output ISC_STATUS array
 * @return The INT64 value from the first column, or 0 on error
 */
ISC_INT64 fbs_execute_singleton_int64(
    void* master_ptr,
    void* statement_ptr,
    void* transaction_ptr,
    ISC_STATUS* status_vector
);

/* Firebird OO API Blob Functions */

/**
 * Create a new blob for writing.
 *
 * @param master_ptr IMaster interface pointer
 * @param attachment_ptr IAttachment pointer (from fbc_get_attachment())
 * @param transaction_ptr ITransaction pointer (from fbt_get_handle())
 * @param blob_id Output: blob ID after creation
 * @param bpb_length BPB (Blob Parameter Block) length
 * @param bpb BPB data
 * @param status_vector Output status vector
 * @return Opaque blob wrapper pointer, or NULL on error
 */
void* fbb_create(void* master_ptr,
                 void* attachment_ptr,
                 void* transaction_ptr,
                 ISC_QUAD* blob_id,
                 unsigned bpb_length,
                 const unsigned char* bpb,
                 ISC_STATUS* status_vector);

/**
 * Open an existing blob for reading.
 *
 * @param master_ptr IMaster interface pointer
 * @param attachment_ptr IAttachment pointer
 * @param transaction_ptr ITransaction pointer
 * @param blob_id Blob ID to open
 * @param bpb_length BPB length
 * @param bpb BPB data
 * @param status_vector Output status vector
 * @return Opaque blob wrapper pointer, or NULL on error
 */
void* fbb_open(void* master_ptr,
               void* attachment_ptr,
               void* transaction_ptr,
               const ISC_QUAD* blob_id,
               unsigned bpb_length,
               const unsigned char* bpb,
               ISC_STATUS* status_vector);

/**
 * Write a segment to the blob.
 *
 * @param master_ptr IMaster interface pointer
 * @param blob_wrapper Blob wrapper pointer
 * @param length Segment length
 * @param buffer Data to write
 * @param status_vector Output status vector
 * @return 1 on success, 0 on error
 */
int fbb_put_segment(void* master_ptr,
                    void* blob_wrapper,
                    unsigned length,
                    const void* buffer,
                    ISC_STATUS* status_vector);

/**
 * Read a segment from the blob.
 *
 * @param master_ptr IMaster interface pointer
 * @param blob_wrapper Blob wrapper pointer
 * @param buffer_length Buffer size
 * @param buffer Output buffer
 * @param actual_length Output: actual bytes read
 * @param status_vector Output status vector
 * @return 0 on success with more data, 1 on EOF, 2 on segment, -1 on error
 */
int fbb_get_segment(void* master_ptr,
                    void* blob_wrapper,
                    unsigned buffer_length,
                    void* buffer,
                    unsigned* actual_length,
                    ISC_STATUS* status_vector);

/**
 * Close the blob (commit writes).
 *
 * @param master_ptr IMaster interface pointer
 * @param blob_wrapper Blob wrapper pointer
 * @param status_vector Output status vector
 * @return 1 on success, 0 on error
 */
int fbb_close(void* master_ptr, void* blob_wrapper, ISC_STATUS* status_vector);

/**
 * Seek within an open stream BLOB.
 *
 * @param master_ptr       IMaster interface (from fb_get_master_interface)
 * @param blob_wrapper     BlobWrapper pointer from fbb_open or fbb_create
 * @param whence           Seek mode: 0=SEEK_SET, 1=SEEK_CUR, 2=SEEK_END
 * @param offset           Byte offset relative to whence
 * @param result_position  Output: New absolute position after seek
 * @param status_vector    ISC status vector for error reporting
 *
 * @return 1 on success, 0 on failure
 */
int fbb_seek(void* master_ptr,
             void* blob_wrapper,
             int whence,
             int offset,
             int* result_position,
             ISC_STATUS* status_vector);

/**
 * Cancel the blob (discard writes).
 *
 * @param master_ptr IMaster interface pointer
 * @param blob_wrapper Blob wrapper pointer
 * @param status_vector Output status vector
 * @return 1 on success, 0 on error
 */
int fbb_cancel(void* master_ptr, void* blob_wrapper, ISC_STATUS* status_vector);

/**
 * Get blob info.
 *
 * @param master_ptr IMaster interface pointer
 * @param blob_wrapper Blob wrapper pointer
 * @param items_length Info items length
 * @param items Info items to request
 * @param buffer_length Output buffer length
 * @param buffer Output buffer
 * @param status_vector Output status vector
 * @return 1 on success, 0 on error
 */
int fbb_get_info(void* master_ptr,
                 void* blob_wrapper,
                 unsigned items_length,
                 const unsigned char* items,
                 unsigned buffer_length,
                 unsigned char* buffer,
                 ISC_STATUS* status_vector);

/**
 * Get blob ID from wrapper.
 *
 * @param blob_wrapper Blob wrapper pointer
 * @param blob_id Output: blob ID
 */
void fbb_get_blob_id(void* blob_wrapper, ISC_QUAD* blob_id);

/**
 * Check if blob is open.
 *
 * @param blob_wrapper Blob wrapper pointer
 * @return 1 if open, 0 if closed or invalid
 */
int fbb_is_open(void* blob_wrapper);

/**
 * Get the raw IBlob handle from wrapper.
 *
 * @param blob_wrapper Blob wrapper pointer
 * @return Raw IBlob pointer, or NULL if invalid
 */
void* fbb_get_handle(void* blob_wrapper);

/**
 * Free blob wrapper (without closing - blob must be closed first).
 *
 * @param blob_wrapper Blob wrapper pointer
 */
void fbb_free(void* blob_wrapper);

/* Firebird OO API Event Functions */

/**
 * Queue events for notification using OO API.
 *
 * @param master_ptr IMaster interface pointer
 * @param attachment_ptr IAttachment pointer (from fbc_get_attachment())
 * @param length Event buffer length
 * @param events Event buffer (from isc_event_block())
 * @param status_vector Output status vector
 * @return Opaque events wrapper pointer, or NULL on error
 */
void* fbe_queue(void* master_ptr,
                void* attachment_ptr,
                unsigned length,
                const unsigned char* events,
                ISC_STATUS* status_vector);

/**
 * Cancel queued events.
 *
 * @param master_ptr IMaster interface pointer
 * @param events_wrapper Events wrapper pointer (from fbe_queue())
 * @param status_vector Output status vector
 * @return 1 on success, 0 on error
 */
int fbe_cancel(void* master_ptr, void* events_wrapper, ISC_STATUS* status_vector);

/**
 * Check if an event has fired (non-blocking).
 *
 * @param events_wrapper Events wrapper pointer
 * @return 1 if event fired, 0 otherwise
 */
int fbe_has_event_fired(void* events_wrapper);

/**
 * Reset the event fired flag.
 *
 * @param events_wrapper Events wrapper pointer
 */
void fbe_reset_event_fired(void* events_wrapper);

/**
 * Get event data from the last fired event.
 *
 * @param events_wrapper Events wrapper pointer
 * @param length Output: length of event data
 * @return Pointer to event data, or NULL if no event
 */
const unsigned char* fbe_get_event_data(void* events_wrapper, unsigned* length);

/**
 * Check if events are queued.
 *
 * @param events_wrapper Events wrapper pointer
 * @return 1 if queued, 0 otherwise
 */
int fbe_is_queued(void* events_wrapper);

/**
 * Free events wrapper.
 *
 * @param events_wrapper Events wrapper pointer
 */
void fbe_free(void* events_wrapper);

/**
 * Free an event buffer allocated by fbe_event_block() / isc_event_block().
 *
 * @param buf Buffer to free (may be NULL)
 */
void fbe_event_free(unsigned char* buf);

/**
 * Build event parameter block (EPB) for up to 15 events.
 * Replacement for isc_event_block().
 *
 * @param event_buf  Output: allocated event buffer
 * @param result_buf Output: allocated result buffer
 * @param count      Number of event names
 * @return Length of the event buffer
 */
unsigned short fbe_event_block(unsigned char** event_buf, unsigned char** result_buf,
                               unsigned short count, ...);

/**
 * Wait synchronously for any of the registered events.
 * Replacement for isc_wait_for_event().
 *
 * @param status_vector  Output status vector
 * @param db_handle_ptr  Pointer to isc_db_handle (from fbc_get_legacy_handle_ptr())
 * @param buffer_length  Length of event buffer
 * @param event_buffer   Event buffer (from fbe_event_block())
 * @param result_buffer  Result buffer (from fbe_event_block())
 * @return 0 on success, non-zero on error
 */
ISC_STATUS fbe_wait_for_event(ISC_STATUS* status_vector, void* db_handle_ptr,
                              unsigned short buffer_length,
                              unsigned char* event_buffer,
                              unsigned char* result_buffer);

/**
 * Decode event counts from result buffer.
 * Replacement for isc_event_counts().
 *
 * @param result_counts  Output: array of event counts
 * @param buffer_length  Length of event buffer
 * @param event_buffer   Event buffer (from fbe_event_block())
 * @param result_buffer  Result buffer (filled by fbe_wait_for_event())
 */
void fbe_event_counts(ISC_ULONG* result_counts, unsigned short buffer_length,
                      unsigned char* event_buffer, unsigned char* result_buffer);

/* Firebird OO API Service Functions */

/**
 * Attach to the service manager using OO API.
 *
 * @param master_ptr IMaster interface pointer
 * @param service_name Service name (e.g., "localhost:service_mgr")
 * @param spb_length Service parameter buffer length
 * @param spb Service parameter buffer
 * @param status_vector Output status vector
 * @return Opaque service wrapper pointer, or NULL on error
 */
void* fbsvc_attach(void* master_ptr,
                   const char* service_name,
                   unsigned spb_length,
                   const unsigned char* spb,
                   ISC_STATUS* status_vector);

/**
 * Detach from the service manager.
 *
 * @param master_ptr IMaster interface pointer (unused, for API consistency)
 * @param service_wrapper Service wrapper pointer (from fbsvc_attach())
 * @param status_vector Output status vector
 * @return 1 on success, 0 on error
 */
int fbsvc_detach(void* master_ptr, void* service_wrapper, ISC_STATUS* status_vector);

/**
 * Start a service task.
 *
 * @param master_ptr IMaster interface pointer (unused, for API consistency)
 * @param service_wrapper Service wrapper pointer
 * @param spb_length Service parameter buffer length
 * @param spb Service parameter buffer
 * @param status_vector Output status vector
 * @return 1 on success, 0 on error
 */
int fbsvc_start(void* master_ptr,
                void* service_wrapper,
                unsigned spb_length,
                const unsigned char* spb,
                ISC_STATUS* status_vector);

/**
 * Query service status/results.
 *
 * @param master_ptr IMaster interface pointer (unused, for API consistency)
 * @param service_wrapper Service wrapper pointer
 * @param send_length Send buffer length
 * @param send_items Send buffer
 * @param recv_length Receive items length
 * @param recv_items Receive items
 * @param buffer_length Output buffer length
 * @param buffer Output buffer
 * @param status_vector Output status vector
 * @return 1 on success, 0 on error
 */
int fbsvc_query(void* master_ptr,
                void* service_wrapper,
                unsigned send_length,
                const unsigned char* send_items,
                unsigned recv_length,
                const unsigned char* recv_items,
                unsigned buffer_length,
                unsigned char* buffer,
                ISC_STATUS* status_vector);

/**
 * Check if attached to service manager.
 *
 * @param service_wrapper Service wrapper pointer
 * @return 1 if attached, 0 otherwise
 */
int fbsvc_is_attached(void* service_wrapper);

/**
 * Free service wrapper (without detach - service must be detached first).
 *
 * @param service_wrapper Service wrapper pointer
 */
void fbsvc_free(void* service_wrapper);

/* Firebird OO API Array Functions */

/**
 * Get an array slice from the database using OO API.
 *
 * @param master_ptr IMaster interface pointer
 * @param attachment_ptr IAttachment pointer (from fbc_get_attachment())
 * @param transaction_ptr ITransaction pointer (from fbt_get_handle())
 * @param array_id The ISC_QUAD array identifier
 * @param desc Array descriptor (ISC_ARRAY_DESC)
 * @param buffer Output buffer to receive array data
 * @param buffer_length Buffer length (updated with actual bytes read)
 * @param status_vector Output status vector
 * @return 0 on success, non-zero on failure
 */
int fba_get_slice(void* master_ptr,
                  void* attachment_ptr,
                  void* transaction_ptr,
                  ISC_QUAD* array_id,
                  const ISC_ARRAY_DESC* desc,
                  void* buffer,
                  ISC_LONG* buffer_length,
                  ISC_STATUS* status_vector);

/**
 * Lookup array descriptor/bounds using OO API.
 *
 * This replaces isc_array_lookup_bounds() by querying system tables
 * via IStatement/IResultSet.
 *
 * @param master_ptr IMaster interface pointer
 * @param attachment_ptr IAttachment pointer (from fbc_get_attachment())
 * @param transaction_ptr ITransaction pointer (from fbt_get_handle())
 * @param relation_name Table name (case-insensitive)
 * @param field_name Column name (case-insensitive)
 * @param desc Output descriptor (ISC_ARRAY_DESC)
 * @param status_vector Output status vector
 * @return 0 on success, non-zero on failure
 */
int fba_lookup_bounds(void* master_ptr,
                      void* attachment_ptr,
                      void* transaction_ptr,
                      const char* relation_name,
                      const char* field_name,
                      ISC_ARRAY_DESC* desc,
                      ISC_STATUS* status_vector);

/**
 * Put an array slice to the database using OO API.
 *
 * @param master_ptr IMaster interface pointer
 * @param attachment_ptr IAttachment pointer (from fbc_get_attachment())
 * @param transaction_ptr ITransaction pointer (from fbt_get_handle())
 * @param array_id The ISC_QUAD array identifier (output for new arrays)
 * @param desc Array descriptor (ISC_ARRAY_DESC)
 * @param buffer Input buffer containing array data
 * @param buffer_length Buffer length
 * @param status_vector Output status vector
 * @return 0 on success, non-zero on failure
 */
int fba_put_slice(void* master_ptr,
                  void* attachment_ptr,
                  void* transaction_ptr,
                  ISC_QUAD* array_id,
                  const ISC_ARRAY_DESC* desc,
                  const void* buffer,
                  ISC_LONG buffer_length,
                  ISC_STATUS* status_vector);

/* FB 4.0+ Extended Features (Timezone support, enhanced metadata) */
#if FB_API_VER >= 40
void fbu_decode_time_tz(void *master_ptr, const ISC_TIME_TZ* time_tz, unsigned* hours, unsigned* minutes, unsigned* seconds, unsigned* fractions,
	unsigned time_zone_buffer_length, char* time_zone_buffer);
void fbu_decode_timestamp_tz(void *master_ptr, const ISC_TIMESTAMP_TZ* timestamp_tz,
	unsigned* year, unsigned* month, unsigned* day,
	unsigned* hours, unsigned* minutes, unsigned* seconds, unsigned* fractions,
	unsigned time_zone_buffer_length, char* time_zone_buffer);

/* Encode time with timezone - pass timezone as string like "+02:00" or "Europe/Berlin" */
int fbu_encode_time_tz(void *master_ptr, ISC_TIME_TZ* time_tz,
	unsigned hours, unsigned minutes, unsigned seconds, unsigned fractions,
	const char* time_zone);

/* Encode timestamp with timezone - pass timezone as string like "+02:00" or "Europe/Berlin" */
int fbu_encode_timestamp_tz(void *master_ptr, ISC_TIMESTAMP_TZ* timestamp_tz,
	unsigned year, unsigned month, unsigned day,
	unsigned hours, unsigned minutes, unsigned seconds, unsigned fractions,
	const char* time_zone);
#endif // FB_API_VER >= 40

int fbu_insert_field_info(void *master_ptr, ISC_STATUS* status, int is_outvar, int num,
  zval *into_array, void *statement_ptr);
int fbu_insert_aliases(void *master_ptr, ISC_STATUS* status, fbird_query *ib_query,
  void *statement_ptr);

/**
 * Metadata C interop functions for OO API message buffer operations.
 * These functions provide access to IMessageMetadata interface for
 * allocating message buffers and extracting field values during fetch.
 */

/**
 * Get message buffer size from metadata.
 * @param master_ptr IMaster pointer
 * @param metadata_ptr IMessageMetadata pointer (from fbs_get_output_metadata)
 * @return Buffer size in bytes, or 0 on error
 */
unsigned fbm_get_message_length(void* master_ptr, void* metadata_ptr);

/**
 * Get field count from metadata.
 * @param master_ptr IMaster pointer
 * @param metadata_ptr IMessageMetadata pointer
 * @return Number of fields
 */
unsigned fbm_get_count(void* master_ptr, void* metadata_ptr);

/**
 * Get field data offset in message buffer.
 * @param master_ptr IMaster pointer
 * @param metadata_ptr IMessageMetadata pointer
 * @param index Field index (0-based)
 * @return Byte offset of field data in buffer
 */
unsigned fbm_get_offset(void* master_ptr, void* metadata_ptr, unsigned index);

/**
 * Get null indicator offset in message buffer.
 * @param master_ptr IMaster pointer
 * @param metadata_ptr IMessageMetadata pointer
 * @param index Field index (0-based)
 * @return Byte offset of null indicator in buffer
 */
unsigned fbm_get_null_offset(void* master_ptr, void* metadata_ptr, unsigned index);

/**
 * Get field SQL type.
 * @param master_ptr IMaster pointer
 * @param metadata_ptr IMessageMetadata pointer
 * @param index Field index (0-based)
 * @return SQL type code (SQL_TEXT, SQL_VARYING, SQL_LONG, etc.)
 */
unsigned fbm_get_type(void* master_ptr, void* metadata_ptr, unsigned index);

/**
 * Get field subtype (for blobs, char sets).
 * @param master_ptr IMaster pointer
 * @param metadata_ptr IMessageMetadata pointer
 * @param index Field index (0-based)
 * @return Subtype value
 */
unsigned fbm_get_subtype(void* master_ptr, void* metadata_ptr, unsigned index);

/**
 * Get field data length.
 * @param master_ptr IMaster pointer
 * @param metadata_ptr IMessageMetadata pointer
 * @param index Field index (0-based)
 * @return Data length in bytes
 */
unsigned fbm_get_length(void* master_ptr, void* metadata_ptr, unsigned index);

/**
 * Get field scale (for numeric types).
 * @param master_ptr IMaster pointer
 * @param metadata_ptr IMessageMetadata pointer
 * @param index Field index (0-based)
 * @return Scale value (negative for decimal places)
 */
int fbm_get_scale(void* master_ptr, void* metadata_ptr, unsigned index);

/**
 * Get field charset ID.
 * @param master_ptr IMaster pointer
 * @param metadata_ptr IMessageMetadata pointer
 * @param index Field index (0-based)
 * @return Charset ID
 */
unsigned fbm_get_charset(void* master_ptr, void* metadata_ptr, unsigned index);

/**
 * Get field name.
 * @param master_ptr IMaster pointer
 * @param metadata_ptr IMessageMetadata pointer
 * @param index Field index (0-based)
 * @return Field name (internal pointer - do not free)
 */
const char* fbm_get_field(void* master_ptr, void* metadata_ptr, unsigned index);

/**
 * Get field alias.
 * @param master_ptr IMaster pointer
 * @param metadata_ptr IMessageMetadata pointer
 * @param index Field index (0-based)
 * @return Field alias (internal pointer - do not free)
 */
const char* fbm_get_alias(void* master_ptr, void* metadata_ptr, unsigned index);

/**
 * Get field relation (table) name.
 * @param master_ptr IMaster pointer
 * @param metadata_ptr IMessageMetadata pointer
 * @param index Field index (0-based)
 * @return Relation name (internal pointer - do not free)
 */
const char* fbm_get_relation(void* master_ptr, void* metadata_ptr, unsigned index);

/**
 * Release metadata reference.
 * @param metadata_ptr IMessageMetadata pointer
 */
void fbm_release(void* metadata_ptr);

/* IXpbBuilder-based Parameter Block Construction */

/**
 * Build TPB (Transaction Parameter Buffer) using OO API IXpbBuilder.
 * Replaces manual byte array construction with cleaner builder pattern.
 *
 * Supported flags (PHP_FBIRD_* constants from php_fbird_includes.h):
 * - Access mode: PHP_FBIRD_READ, PHP_FBIRD_WRITE
 * - Isolation: PHP_FBIRD_CONSISTENCY, PHP_FBIRD_CONCURRENCY, PHP_FBIRD_COMMITTED
 * - Record versioning: PHP_FBIRD_REC_VERSION, PHP_FBIRD_REC_NO_VERSION
 * - Lock resolution: PHP_FBIRD_WAIT, PHP_FBIRD_NOWAIT, PHP_FBIRD_LOCK_TIMEOUT
 * - FB 4.0+ READ CONSISTENCY: PHP_FBIRD_READ_CONSISTENCY
 *
 * @param master_ptr IMaster interface pointer
 * @param trans_flags PHP transaction flags bitmask (PHP_FBIRD_* constants)
 * @param lock_timeout Lock timeout in seconds (only used when flags include PHP_FBIRD_LOCK_TIMEOUT)
 * @param buffer_length Output: length of TPB buffer
 * @param status_vector Output status vector for errors
 * @return Allocated TPB buffer (caller must free with fbxpb_free_tpb), or NULL on error
 */
unsigned char* fbxpb_build_tpb(
    void* master_ptr,
    zend_long trans_flags,
    zend_long lock_timeout,
    unsigned* buffer_length,
    ISC_STATUS* status_vector
);

/**
 * Free TPB buffer allocated by fbxpb_build_tpb().
 *
 * @param buffer Buffer to free (NULL-safe)
 */
void fbxpb_free_tpb(unsigned char* buffer);

/* Limbo Transaction Functions (Two-Phase Commit Recovery)
 *
 * These functions support recovery of transactions that were prepared but
 * not committed in a two-phase commit scenario (limbo transactions).
 */

/**
 * Get list of limbo transaction IDs from a database.
 *
 * Limbo transactions are transactions that were prepared (first phase of 2PC)
 * but never committed or rolled back. This function retrieves their IDs using
 * isc_info_limbo database info request.
 *
 * @param master_ptr IMaster interface pointer
 * @param attachment_ptr IAttachment pointer (from fbc_get_attachment())
 * @param trans_ids Output array to receive transaction IDs
 * @param max_ids Maximum number of IDs to retrieve (size of trans_ids array)
 * @param status_vector Output status vector
 * @return Number of limbo transaction IDs found (0 if none), -1 on error
 */
int fbt_get_limbo_transactions(
    void* master_ptr,
    void* attachment_ptr,
    ISC_INT64* trans_ids,
    unsigned max_ids,
    ISC_STATUS* status_vector
);

/**
 * Reconnect to a limbo transaction for recovery.
 *
 * This function reconnects to a prepared (limbo) transaction using its ID,
 * allowing the caller to commit or rollback the transaction.
 *
 * @param master_ptr IMaster interface pointer
 * @param attachment_ptr IAttachment pointer
 * @param trans_id The limbo transaction ID (from fbt_get_limbo_transactions)
 * @param status_vector Output status vector
 * @return Transaction wrapper pointer that can be committed/rolled back, or NULL on error
 */
void* fbt_reconnect(
    void* master_ptr,
    void* attachment_ptr,
    ISC_INT64 trans_id,
    ISC_STATUS* status_vector
);

/* IBatch API Functions (Firebird 4.0+ Bulk Operations)
 *
 * The IBatch interface provides high-performance bulk INSERT operations.
 * Using batch operations can provide 10-12x speedup for large data loads.
 * These functions are only available when compiled with FB_API_VER >= 40.
 */

#if FB_API_VER >= 40

/**
 * Create a batch operation from a prepared statement.
 *
 * @param master_ptr IMaster interface pointer
 * @param statement_ptr IStatement pointer (from fbs_get_statement())
 * @param buffer_size Buffer size hint (0 for default, typically 16MB)
 * @param status_vector Output status vector
 * @return Opaque batch wrapper pointer, or NULL on error
 */
void* fbbatch_create(
    void* master_ptr,
    void* statement_ptr,
    unsigned buffer_size,
    ISC_STATUS* status_vector
);

/**
 * Add parameter data to the batch.
 *
 * The input buffer should contain the parameter values in the format
 * expected by the prepared statement's input metadata.
 *
 * @param master_ptr IMaster interface pointer
 * @param batch_wrapper Batch wrapper pointer (from fbbatch_create())
 * @param count Number of messages (rows) to add
 * @param in_buffer Input message buffer containing parameter data
 * @param status_vector Output status vector
 * @return 1 on success, 0 on error
 */
int fbbatch_add(
    void* master_ptr,
    void* batch_wrapper,
    unsigned count,
    const void* in_buffer,
    ISC_STATUS* status_vector
);

/**
 * Execute the batch operation.
 *
 * @param master_ptr IMaster interface pointer
 * @param batch_wrapper Batch wrapper pointer
 * @param transaction_ptr ITransaction pointer for the batch execution
 * @param total_processed Output: total number of messages processed
 * @param error_count Output: number of messages that failed
 * @param status_vector Output status vector
 * @return 1 on success (even with partial errors), 0 on complete failure
 */
int fbbatch_execute(
    void* master_ptr,
    void* batch_wrapper,
    void* transaction_ptr,
    unsigned* total_processed,
    unsigned* error_count,
    ISC_STATUS* status_vector
);

/**
 * Cancel the batch without executing.
 *
 * @param master_ptr IMaster interface pointer
 * @param batch_wrapper Batch wrapper pointer
 * @param status_vector Output status vector
 * @return 1 on success, 0 on error
 */
int fbbatch_cancel(
    void* master_ptr,
    void* batch_wrapper,
    ISC_STATUS* status_vector
);

/**
 * Close and free the batch.
 *
 * @param master_ptr IMaster interface pointer
 * @param batch_wrapper Batch wrapper pointer
 * @param status_vector Output status vector
 * @return 1 on success, 0 on error
 */
int fbbatch_close(
    void* master_ptr,
    void* batch_wrapper,
    ISC_STATUS* status_vector
);

/**
 * Get the input metadata for the batch.
 *
 * This can be used to determine the message buffer format for fbbatch_add().
 *
 * @param master_ptr IMaster interface pointer
 * @param batch_wrapper Batch wrapper pointer
 * @param status_vector Output status vector
 * @return IMessageMetadata pointer (caller should release), or NULL on error
 */
void* fbbatch_get_metadata(
    void* master_ptr,
    void* batch_wrapper,
    ISC_STATUS* status_vector
);

/**
 * Get BLOB alignment requirement for the batch.
 *
 * @param master_ptr IMaster interface pointer
 * @param batch_wrapper Batch wrapper pointer
 * @param status_vector Output status vector
 * @return Alignment in bytes, or 0 on error
 */
unsigned fbbatch_get_blob_alignment(
    void* master_ptr,
    void* batch_wrapper,
    ISC_STATUS* status_vector
);

/* IBatch BLOB Handling Functions
 *
 * These functions provide advanced BLOB handling within batch operations,
 * allowing inline BLOB creation without pre-creating BLOBs separately.
 */

/**
 * Add inline BLOB data to the batch.
 *
 * Creates a new BLOB within the batch context and returns a BLOB ID
 * that can be used when adding rows via fbbatch_add().
 *
 * @param master_ptr IMaster interface pointer
 * @param batch_wrapper Batch wrapper pointer (from fbbatch_create())
 * @param length Length of BLOB data in bytes
 * @param data BLOB data buffer
 * @param blob_id_out Output: BLOB ID for use in batch parameters
 * @param bpb_length BPB (Blob Parameter Block) length (0 for default)
 * @param bpb BPB data (NULL for default)
 * @param status_vector Output status vector
 * @return 1 on success, 0 on error
 */
int fbbatch_add_blob(
    void* master_ptr,
    void* batch_wrapper,
    unsigned length,
    const void* data,
    ISC_QUAD* blob_id_out,
    unsigned bpb_length,
    const unsigned char* bpb,
    ISC_STATUS* status_vector
);

/**
 * Append data to the current BLOB being constructed.
 *
 * Used for streaming large BLOBs in chunks. Must be called after
 * fbbatch_add_blob() or a previous fbbatch_append_blob_data() call.
 *
 * @param master_ptr IMaster interface pointer
 * @param batch_wrapper Batch wrapper pointer
 * @param length Length of data chunk
 * @param data Data chunk buffer
 * @param status_vector Output status vector
 * @return 1 on success, 0 on error
 */
int fbbatch_append_blob_data(
    void* master_ptr,
    void* batch_wrapper,
    unsigned length,
    const void* data,
    ISC_STATUS* status_vector
);

/**
 * Add BLOB data using stream mode.
 *
 * Alternative to addBlob() that uses different internal handling.
 * Data is streamed directly without intermediate buffering.
 *
 * @param master_ptr IMaster interface pointer
 * @param batch_wrapper Batch wrapper pointer
 * @param length Length of BLOB data
 * @param data BLOB data buffer
 * @param status_vector Output status vector
 * @return 1 on success, 0 on error
 */
int fbbatch_add_blob_stream(
    void* master_ptr,
    void* batch_wrapper,
    unsigned length,
    const void* data,
    ISC_STATUS* status_vector
);

/**
 * Register an existing BLOB for use in batch operations.
 *
 * Takes a BLOB ID that was created outside the batch context and
 * registers it for use within the batch.
 *
 * @param master_ptr IMaster interface pointer
 * @param batch_wrapper Batch wrapper pointer
 * @param existing_blob Existing BLOB ID to register
 * @param batch_blob_id Output: BLOB ID for use in batch parameters
 * @param status_vector Output status vector
 * @return 1 on success, 0 on error
 */
int fbbatch_register_blob(
    void* master_ptr,
    void* batch_wrapper,
    const ISC_QUAD* existing_blob,
    ISC_QUAD* batch_blob_id,
    ISC_STATUS* status_vector
);

/**
 * Set default BPB (Blob Parameter Block) for batch BLOB operations.
 *
 * Sets the default parameters used for all subsequent BLOB operations
 * in this batch that don't specify their own BPB.
 *
 * @param master_ptr IMaster interface pointer
 * @param batch_wrapper Batch wrapper pointer
 * @param bpb_length BPB length
 * @param bpb BPB data
 * @param status_vector Output status vector
 * @return 1 on success, 0 on error
 */
int fbbatch_set_default_bpb(
    void* master_ptr,
    void* batch_wrapper,
    unsigned bpb_length,
    const unsigned char* bpb,
    ISC_STATUS* status_vector
);

/* IBatch Detailed Error Reporting
 *
 * These structures and functions provide detailed per-row error information
 * from batch execution, including SQLSTATE codes and error messages.
 */

/**
 * Per-row error entry from batch execution.
 *
 * Contains detailed information about a specific row that failed
 * during batch execution.
 */
typedef struct {
    unsigned position;       /**< Row position (0-based index in batch) */
    int state;              /**< Completion state (see IBatchCompletionState constants) */
    char sqlstate[6];       /**< SQLSTATE code (5 chars + null terminator) */
    char* message;          /**< Error message (allocated, caller must free) */
} fbbatch_error_entry;

/**
 * Batch completion result with detailed error information.
 *
 * Contains summary counts and an array of detailed error entries
 * for all failed rows.
 */
typedef struct {
    unsigned total_count;           /**< Total rows processed */
    unsigned success_count;         /**< Number of successful rows */
    unsigned error_count;           /**< Number of failed rows */
    fbbatch_error_entry* errors;    /**< Array of error_count entries (caller must free) */
} fbbatch_completion_result;

/** IBatchCompletionState constants */
#define FBBATCH_EXECUTE_FAILED  (-1)  /**< Row execution failed */
#define FBBATCH_SUCCESS_NO_INFO  0    /**< Success, no additional info */
#define FBBATCH_NO_MORE_ERRORS  (-2)  /**< No more errors to report */

/**
 * Execute batch with detailed completion state.
 *
 * Similar to fbbatch_execute() but returns detailed per-row error
 * information via the fbbatch_completion_result structure.
 *
 * @param master_ptr IMaster interface pointer
 * @param batch_wrapper Batch wrapper pointer
 * @param transaction_ptr ITransaction pointer
 * @param result Output: Completion result with error details (caller must free via fbbatch_free_result())
 * @param status_vector Output status vector
 * @return 1 on success (even with partial errors), 0 on complete failure
 */
int fbbatch_execute_detailed(
    void* master_ptr,
    void* batch_wrapper,
    void* transaction_ptr,
    fbbatch_completion_result* result,
    ISC_STATUS* status_vector
);

/**
 * Free error entries array from fbbatch_execute_detailed().
 *
 * Frees all allocated memory in the error entries array, including
 * individual error messages.
 *
 * @param errors Error entries array to free
 * @param count Number of entries in the array
 */
void fbbatch_free_errors(fbbatch_error_entry* errors, unsigned count);

/**
 * Free completion result from fbbatch_execute_detailed().
 *
 * Convenience function that frees the errors array and resets the result.
 *
 * @param result Completion result to free
 */
void fbbatch_free_result(fbbatch_completion_result* result);

#endif /* FB_API_VER >= 40 */


#ifdef __cplusplus
}
#endif

#endif	/* FIREBIRD_UTILS_H */
