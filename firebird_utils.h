/*
  +----------------------------------------------------------------------+
  | Copyright (c) The PHP Group                                          |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | https://www.php.net/license/3_01.txt                                 |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Simonov Denis <sim-mail@list.ru>                             |
  +----------------------------------------------------------------------+
*/

#ifndef PDO_FIREBIRD_UTILS_H
#define PDO_FIREBIRD_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif


#if FB_API_VER >= 30

#include <ibase.h>
#include "php_fbird_includes.h"

unsigned fbu_get_client_version(void *master_ptr);
ISC_TIME fbu_encode_time(void *master_ptr, unsigned hours, unsigned minutes,
  unsigned seconds, unsigned fractions);
ISC_DATE fbu_encode_date(void *master_ptr, unsigned year, unsigned month, unsigned day);

/* =============================================================================
 * Phase 2: Firebird OO API Connection Functions (FB 3.0+)
 *
 * These functions provide a C interface to the modern Firebird C++ OO API.
 * They use RAII-managed connections internally for safety and proper cleanup.
 * ============================================================================= */

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
 * Get the server version from a connection.
 *
 * @param connection Pointer returned by fbc_connect()
 * @return Version code (FB30=30, FB40=40, FB50=50), or 0 if invalid
 */
unsigned fbc_get_server_version(void* connection);

/* =============================================================================
 * Phase 4: Firebird OO API Transaction Functions (FB 3.0+)
 *
 * These functions provide a C interface to the modern Firebird C++ OO API
 * for transaction management. They replace the legacy isc_start_transaction(),
 * isc_commit_transaction(), and isc_rollback_transaction() functions.
 * ============================================================================= */

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

/* =============================================================================
 * Phase 5: Firebird OO API Statement Functions (FB 3.0+)
 *
 * These functions provide a C interface to the modern Firebird C++ OO API
 * for statement preparation, execution, and result fetching.
 * They replace the legacy isc_dsql_* functions.
 * ============================================================================= */

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

/* =============================================================================
 * Phase 6: Firebird OO API Blob Functions (FB 3.0+)
 *
 * These functions provide a C interface to the modern Firebird C++ OO API
 * for blob operations. They replace the legacy isc_create_blob, isc_open_blob,
 * isc_put_segment, isc_get_segment, isc_close_blob, and isc_cancel_blob.
 * ============================================================================= */

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

/* =============================================================================
 * Phase 7: Firebird OO API Event Functions (FB 3.0+)
 *
 * These functions provide a C interface to the modern Firebird C++ OO API
 * for event operations. They provide async event queuing via IAttachment::queEvents()
 * and event cancellation via IEvents::cancel().
 *
 * Note: The OO API uses callback-based event handling (IEventCallback),
 * which differs from the legacy synchronous isc_wait_for_event() approach.
 * The current implementation continues to use isc_wait_for_event() for the
 * synchronous polling model, with OO API available for future async support.
 * ============================================================================= */

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

/* =============================================================================
 * Phase 8: Firebird OO API Service Functions (FB 3.0+)
 *
 * These functions provide a C interface to the modern Firebird C++ OO API
 * for service manager operations. They replace the legacy isc_service_attach,
 * isc_service_detach, isc_service_start, and isc_service_query functions.
 * ============================================================================= */

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

#endif // FB_API_VER >= 30


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
int fbu_insert_field_info(void *master_ptr, ISC_STATUS* status, int is_outvar, int num,
  zval *into_array, void *statement_ptr);
int fbu_insert_aliases(void *master_ptr, ISC_STATUS* status, fbird_query *ib_query,
  void *statement_ptr);

#endif // FB_API_VER >= 30


#ifdef __cplusplus
}
#endif

#endif	/* PDO_FIREBIRD_UTILS_H */
