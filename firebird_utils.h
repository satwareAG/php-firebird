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
