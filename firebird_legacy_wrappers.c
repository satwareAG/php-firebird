/* SPDX-License-Identifier: PHP-3.01
 * Legacy wrappers - bridge old C call sites to Firebird C API.
 * DO NOT add new calls here; migrate call sites to v2 instead.
 *
 * Blob wrappers call isc_* directly (fbb_* bridge never implemented).
 * Metadata/TZ wrappers are stubs for future migration (not yet called). */

#include "php.h"
#include "firebird_legacy_wrappers.h"

#ifndef FBDEBUG
#define FBDEBUG(fmt, ...) do { } while (0)
#endif

/* ------------------------------------------------------------------ */
/* Internal blob wrapper                                               */
/* ------------------------------------------------------------------ */

typedef struct {
	isc_blob_handle handle;
} legacy_blob_wrap_t;

/* ------------------------------------------------------------------ */
/* Blob wrappers - direct isc_* calls                                  */
/* ------------------------------------------------------------------ */

static inline int fb_isc_error(const ISC_STATUS *status)
{
	return status[0] == 1 && status[1] > 0;
}

void* fbb_create(void *master, void *db_handle, void *tr_handle,
	ISC_QUAD *qd, unsigned short bpb_len, const char *bpb,
	ISC_STATUS *status)
{
	legacy_blob_wrap_t *wrap;

	(void) master;

	wrap = (legacy_blob_wrap_t *) emalloc(sizeof(*wrap));
	wrap->handle = 0;

	isc_create_blob2(status,
		(isc_db_handle *) db_handle,
		(isc_tr_handle *) tr_handle,
		&wrap->handle,
		qd,
		bpb_len,
		(const unsigned char *) bpb);

	if (fb_isc_error(status)) {
		efree(wrap);
		return NULL;
	}

	return (void *) wrap;
}

void* fbb_open(void *master, void *db_handle, void *tr_handle,
	ISC_QUAD *qd, unsigned short bpb_len, const char *bpb,
	ISC_STATUS *status)
{
	legacy_blob_wrap_t *wrap;

	(void) master;

	wrap = (legacy_blob_wrap_t *) emalloc(sizeof(*wrap));
	wrap->handle = 0;

	isc_open_blob2(status,
		(isc_db_handle *) db_handle,
		(isc_tr_handle *) tr_handle,
		&wrap->handle,
		qd,
		bpb_len,
		(const unsigned char *) bpb);

	if (fb_isc_error(status)) {
		efree(wrap);
		return NULL;
	}

	return (void *) wrap;
}

int fbb_put_segment(void *master, void *blob, unsigned len,
	const void *buf, ISC_STATUS *status)
{
	legacy_blob_wrap_t *wrap = (legacy_blob_wrap_t *) blob;

	(void) master;

	if (!wrap) {
		return 0;
	}

	isc_put_segment(status, &wrap->handle,
		(unsigned short) len, (const char *) buf);

	return fb_isc_error(status) ? 0 : 1;
}

int fbb_get_segment(void *master, void *blob, unsigned len,
	void *buf, unsigned *actual_len, ISC_STATUS *status)
{
	ISC_LONG ret;
	unsigned short seg_len = 0;
	legacy_blob_wrap_t *wrap = (legacy_blob_wrap_t *) blob;

	(void) master;

	if (!wrap) {
		return -1;
	}

	ret = isc_get_segment(status, &wrap->handle, &seg_len,
		(unsigned short) len, (char *) buf);

	if (fb_isc_error(status)) {
		return -1;
	}

	if (actual_len) {
		*actual_len = (unsigned) seg_len;
	}

	return (int) ret;
}

int fbb_close(void *master, void *blob, ISC_STATUS *status)
{
	legacy_blob_wrap_t *wrap = (legacy_blob_wrap_t *) blob;

	(void) master;

	if (!wrap) {
		return 0;
	}

	isc_close_blob(status, &wrap->handle);

	if (fb_isc_error(status)) {
		return 0;
	}

	efree(wrap);
	return 1;
}

void fbb_free(void *blob)
{
	if (blob) {
		efree(blob);
	}
}

int fbb_cancel(void *master, void *blob, ISC_STATUS *status)
{
	legacy_blob_wrap_t *wrap = (legacy_blob_wrap_t *) blob;

	(void) master;

	if (!wrap) {
		return 0;
	}

	isc_cancel_blob(status, &wrap->handle);

	if (fb_isc_error(status)) {
		return 0;
	}

	efree(wrap);
	return 1;
}

int fbb_get_info(void *master, void *blob, unsigned items_len,
	const unsigned char *items, unsigned buf_len, unsigned char *buf,
	ISC_STATUS *status)
{
	legacy_blob_wrap_t *wrap = (legacy_blob_wrap_t *) blob;

	(void) master;

	if (!wrap) {
		return 0;
	}

	isc_blob_info(status, &wrap->handle,
		(short) items_len, (const char *) items,
		(short) buf_len, (char *) buf);

	return fb_isc_error(status) ? 0 : 1;
}

/* ------------------------------------------------------------------ */
/* Metadata wrappers - stubs (not yet called from .c files)            */
/* ------------------------------------------------------------------ */

unsigned fbm_get_message_length(void *master, void *metadata)
{
	(void) master;
	(void) metadata;
	FBDEBUG("fbm_get_message_length: stub called, returning 0");
	return 0;
}

unsigned fbm_get_count(void *master, void *metadata)
{
	(void) master;
	(void) metadata;
	FBDEBUG("fbm_get_count: stub called, returning 0");
	return 0;
}

unsigned fbm_get_type(void *master, void *metadata, unsigned index)
{
	(void) master;
	(void) metadata;
	(void) index;
	FBDEBUG("fbm_get_type: stub called, returning 0");
	return 0;
}

unsigned fbm_get_offset(void *master, void *metadata, unsigned index)
{
	(void) master;
	(void) metadata;
	(void) index;
	FBDEBUG("fbm_get_offset: stub called, returning 0");
	return 0;
}

unsigned fbm_get_null_offset(void *master, void *metadata, unsigned index)
{
	(void) master;
	(void) metadata;
	(void) index;
	FBDEBUG("fbm_get_null_offset: stub called, returning 0");
	return 0;
}

unsigned fbm_get_length(void *master, void *metadata, unsigned index)
{
	(void) master;
	(void) metadata;
	(void) index;
	FBDEBUG("fbm_get_length: stub called, returning 0");
	return 0;
}

int fbm_get_scale(void *master, void *metadata, unsigned index)
{
	(void) master;
	(void) metadata;
	(void) index;
	FBDEBUG("fbm_get_scale: stub called, returning 0");
	return 0;
}

/* ------------------------------------------------------------------ */
/* TZ datetime wrappers (Firebird 4.0+) - stubs (not yet called)       */
/* ------------------------------------------------------------------ */

#if FB_API_VER >= 40

ISC_TIME_TZ fbu_encode_time_tz(void *master, ISC_TIME_TZ *tz,
	unsigned hours, unsigned minutes, unsigned seconds, unsigned fractions)
{
	ISC_TIME_TZ result;

	(void) master;
	(void) tz;

	memset(&result, 0, sizeof(result));
	result.utc_time = ((ISC_TIME) hours * 3600 +
		(ISC_TIME) minutes * 60 + (ISC_TIME) seconds)
		* ISC_TIME_SECONDS_PRECISION + (ISC_TIME) fractions;

	return result;
}

ISC_TIMESTAMP_TZ fbu_encode_timestamp_tz(void *master, ISC_TIMESTAMP_TZ *tz,
	unsigned year, unsigned month, unsigned day,
	unsigned hours, unsigned minutes, unsigned seconds, unsigned fractions)
{
	ISC_TIMESTAMP_TZ result;
	struct tm t;

	(void) master;
	(void) tz;

	memset(&result, 0, sizeof(result));
	memset(&t, 0, sizeof(t));
	t.tm_year = (int) year - 1900;
	t.tm_mon = (int) month - 1;
	t.tm_mday = (int) day;
	t.tm_hour = (int) hours;
	t.tm_min = (int) minutes;
	t.tm_sec = (int) seconds;
	t.tm_isdst = -1;

	isc_encode_timestamp(&t, &result.utc_timestamp);
	result.utc_timestamp.timestamp_time += (ISC_TIME) fractions;

	return result;
}

#endif /* FB_API_VER >= 40 */