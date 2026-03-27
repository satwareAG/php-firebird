/* SPDX-License-Identifier: PHP-3.01
 * Legacy wrappers - bridge old C call sites to Firebird C API.
 * DO NOT add new calls here; migrate call sites to v2 instead.
 *
 * Blob wrappers call isc_* directly (fbb_* bridge never implemented).
 * Metadata/TZ wrappers are stubs for future migration (not yet called).
 *
 * NOTE: db_handle and tr_handle parameters are raw isc_db_handle*
 * and isc_tr_handle* cast to void*. Callers pass link->handle
 * and trans->handle directly. */

#ifndef FIREBIRD_LEGACY_WRAPPERS_H
#define FIREBIRD_LEGACY_WRAPPERS_H

#include <ibase.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Blob wrappers: call Firebird C API directly.
 * db_handle = raw isc_db_handle* (caller passes link->handle)
 * tr_handle = raw isc_tr_handle* (caller passes trans->handle) */
void*  fbb_create(void *master, void *db_handle, void *tr_handle,
	ISC_QUAD *qd, unsigned short bpb_len, const char *bpb,
	ISC_STATUS *status);
void*  fbb_open(void *master, void *db_handle, void *tr_handle,
	ISC_QUAD *qd, unsigned short bpb_len, const char *bpb,
	ISC_STATUS *status);
int    fbb_put_segment(void *master, void *blob, unsigned len,
	const void *buf, ISC_STATUS *status);
int    fbb_get_segment(void *master, void *blob, unsigned len,
	void *buf, unsigned *actual_len, ISC_STATUS *status);
int    fbb_close(void *master, void *blob, ISC_STATUS *status);
void   fbb_free(void *blob);
int    fbb_cancel(void *master, void *blob, ISC_STATUS *status);
int    fbb_get_info(void *master, void *blob, unsigned items_len,
	const unsigned char *items, unsigned buf_len, unsigned char *buf,
	ISC_STATUS *status);

/* Metadata wrappers: stubs (not yet called from .c files) */
unsigned fbm_get_message_length(void *master, void *metadata);
unsigned fbm_get_count(void *master, void *metadata);
unsigned fbm_get_type(void *master, void *metadata, unsigned index);
unsigned fbm_get_offset(void *master, void *metadata, unsigned index);
unsigned fbm_get_null_offset(void *master, void *metadata, unsigned index);
unsigned fbm_get_length(void *master, void *metadata, unsigned index);
int      fbm_get_scale(void *master, void *metadata, unsigned index);

/* TZ datetime wrappers (Firebird 4.0+) - stubs (not yet called) */
#if FB_API_VER >= 40
ISC_TIME_TZ      fbu_encode_time_tz(void *master, ISC_TIME_TZ *tz,
	unsigned hours, unsigned minutes, unsigned seconds, unsigned fractions);
ISC_TIMESTAMP_TZ fbu_encode_timestamp_tz(void *master, ISC_TIMESTAMP_TZ *tz,
	unsigned year, unsigned month, unsigned day,
	unsigned hours, unsigned minutes, unsigned seconds, unsigned fractions);
#endif

#ifdef __cplusplus
}
#endif

#endif /* FIREBIRD_LEGACY_WRAPPERS_H */