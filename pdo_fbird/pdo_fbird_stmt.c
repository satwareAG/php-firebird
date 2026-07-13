/*
 * pdo_fbird — statement handler (T12, T13)
 * prepare, execute, fetch, describe, param binding
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_PDO_FBIRD

#include "php.h"
#include "ext/pdo/php_pdo.h"
#include "ext/pdo/php_pdo_driver.h"
#include "php_pdo_fbird.h"
#include "php_pdo_fbird_int.h"
#include <ibase.h>
#include <ctype.h>
#include <time.h>
#include <math.h>
#include "zend_smart_str.h"
#include "../firebird_utils.h"
#include "../php_firebird.h"
#include "../php_fbird_includes.h"
#include "../fbird_classes.h"

/* {{{ php_firebird_preprocess — replace :name with ? and build name→position map */
zend_string *php_firebird_preprocess(const char *sql, size_t sql_len,
	pdo_fbird_named_param **out_params, unsigned int *out_count)
{
	/* Pass 1: count named params and compute output length */
	unsigned int count = 0;
	int in_string = 0;
	int in_comment = 0;
	int in_block = 0; /* inside EXECUTE BLOCK's BEGIN...END */

	for (size_t i = 0; i < sql_len; i++) {
		char c = sql[i];
		if (in_comment) {
			if (c == '*' && i + 1 < sql_len && sql[i + 1] == '/') {
				in_comment = 0; i++;
			}
			continue;
		}
		if (c == '/' && i + 1 < sql_len && sql[i + 1] == '*') {
			in_comment = 1; i++;
			continue;
		}
		if (c == '-' && i + 1 < sql_len && sql[i + 1] == '-') {
			while (i < sql_len && sql[i] != '\n') i++;
			continue;
		}
		if (c == '\'') { in_string = !in_string; continue; }
		if (in_string) continue;

		/* Detect BEGIN keyword (for EXECUTE BLOCK) */
		if (!in_block && (c == 'B' || c == 'b') && i + 5 <= sql_len) {
			if (strncasecmp(sql + i, "BEGIN", 5) == 0 &&
			    (i + 5 >= sql_len || !isalnum((unsigned char)sql[i + 5]))) {
				in_block = 1;
				i += 4;
				continue;
			}
		}

		/* Skip :name inside BEGIN...END block (those are PSQL variables) */
		if (in_block) {
			if ((c == 'E' || c == 'e') && i + 3 <= sql_len) {
				if (strncasecmp(sql + i, "END", 3) == 0 &&
				    (i + 3 >= sql_len || !isalnum((unsigned char)sql[i + 3]))) {
					in_block = 0;
					i += 2;
				}
			}
			continue;
		}

		if (c == ':' && i + 1 < sql_len && (isalpha((unsigned char)sql[i + 1]) || sql[i + 1] == '_')) {
			size_t start = i + 1;
			size_t end = start;
			while (end < sql_len && (isalnum((unsigned char)sql[end]) || sql[end] == '_')) end++;
			count++;
			i = end - 1;
		}
	}

	if (count == 0) {
		*out_params = NULL;
		*out_count = 0;
		return NULL; /* no named params, use original SQL */
	}

	/* Pass 2: build output SQL and param map */
	pdo_fbird_named_param *params = ecalloc(count, sizeof(pdo_fbird_named_param));
	zend_string *out = zend_string_alloc(sql_len + 16, 0); /* generous */
	char *dst = ZSTR_VAL(out);
	unsigned int pidx = 0;
	in_string = 0;
	in_comment = 0;
	in_block = 0;

	for (size_t i = 0; i < sql_len; i++) {
		char c = sql[i];
		if (in_comment) {
			*dst++ = c;
			if (c == '*' && i + 1 < sql_len && sql[i + 1] == '/') {
				in_comment = 0; *dst++ = sql[++i];
			}
			continue;
		}
		if (c == '/' && i + 1 < sql_len && sql[i + 1] == '*') {
			in_comment = 1; *dst++ = c; *dst++ = sql[++i];
			continue;
		}
		if (c == '-' && i + 1 < sql_len && sql[i + 1] == '-') {
			while (i < sql_len && sql[i] != '\n') { *dst++ = sql[i]; i++; }
			if (i < sql_len) *dst++ = sql[i];
			continue;
		}
		if (c == '\'') { in_string = !in_string; *dst++ = c; continue; }
		if (in_string) { *dst++ = c; continue; }

		if (!in_block && (c == 'B' || c == 'b') && i + 5 <= sql_len) {
			if (strncasecmp(sql + i, "BEGIN", 5) == 0 &&
			    (i + 5 >= sql_len || !isalnum((unsigned char)sql[i + 5]))) {
				in_block = 1;
				memcpy(dst, sql + i, 5); dst += 5;
				i += 4;
				continue;
			}
		}

		if (in_block) {
			if ((c == 'E' || c == 'e') && i + 3 <= sql_len) {
				if (strncasecmp(sql + i, "END", 3) == 0 &&
				    (i + 3 >= sql_len || !isalnum((unsigned char)sql[i + 3]))) {
					in_block = 0;
				}
			}
			*dst++ = c;
			continue;
		}

		if (c == ':' && i + 1 < sql_len && (isalpha((unsigned char)sql[i + 1]) || sql[i + 1] == '_')) {
			size_t start = i + 1;
			size_t end = start;
			while (end < sql_len && (isalnum((unsigned char)sql[end]) || sql[end] == '_')) end++;
			params[pidx].name = estrndup(sql + start, end - start);
			params[pidx].position = pidx;
			pidx++;
			*dst++ = '?';
			i = end - 1;
			continue;
		}

		*dst++ = c;
	}
	*dst = '\0';

	size_t final_len = dst - ZSTR_VAL(out);
	out = zend_string_truncate(out, final_len, 0);

	*out_params = params;
	*out_count = pidx;
	return out;
}
/* }}} */

/* Helper: read a null indicator from message buffer */
static int _pdo_fbird_is_null(pdo_fbird_stmt *S, unsigned idx)
{
	if (!S->out_meta) return 0;
	unsigned null_off = fbm_get_null_offset(FBG(master_instance), S->out_meta, idx);
	short null_flag = 0;
	memcpy(&null_flag, S->out_buf + null_off, sizeof(short));
	return (null_flag != 0);
}

/* {{{ pdo_fbird_stmt_dtor */
static int pdo_fbird_stmt_dtor(pdo_stmt_t *stmt)
{
	pdo_fbird_stmt *S = (pdo_fbird_stmt *)stmt->driver_data;
	if (!S) return 1;

	if (S->named_params) {
		for (unsigned int i = 0; i < S->named_param_count; i++) {
			if (S->named_params[i].name) efree(S->named_params[i].name);
		}
		efree(S->named_params);
		S->named_params = NULL;
	}

	if (S->out_buf)  { efree(S->out_buf);  S->out_buf  = NULL; }
	if (S->in_buf)   { efree(S->in_buf);   S->in_buf   = NULL; }

	/* Release metadata first (ref-counted, independent of statement lifetime) */
	if (S->out_meta) { fbm_release(S->out_meta); S->out_meta = NULL; }
	if (S->in_meta)  { fbm_release(S->in_meta);  S->in_meta  = NULL; }

	if (S->fbs_stmt) {
		/* Only close cursor if connection still alive and cursor open */
		if (S->has_rows && S->H && S->H->fbc_conn &&
		    fbc_is_connected(S->H->fbc_conn) &&
		    fbs_is_cursor_open(S->fbs_stmt)) {
			fbs_close_cursor(S->fbs_stmt, S->status);
		}
		fbs_free(S->fbs_stmt, S->status);
		S->fbs_stmt = NULL;
	}

	efree(S);
	stmt->driver_data = NULL;
	return 1;
}
/* }}} */

/* {{{ pdo_fbird_stmt_execute */
static int pdo_fbird_stmt_execute(pdo_stmt_t *stmt)
{
	pdo_fbird_stmt    *S = (pdo_fbird_stmt *)stmt->driver_data;
	pdo_fbird_db_handle *H = S->H;

	/* Close previously open cursor before re-executing this statement.
	 * This only affects the current statement — other statements' cursors
	 * remain open (MARS). The C++ openCursor() also handles this, but
	 * we close here too for DML re-execution (where openCursor is not called). */
	if (S->cursor_executed && fbs_is_cursor_open(S->fbs_stmt)) {
		fbs_close_cursor(S->fbs_stmt, S->status);
	}
	S->has_rows = 0;

	void *tr = fbt_get_handle(H->fbt_trans);
	unsigned stmt_type = fbs_get_type(FBG(master_instance), S->fbs_stmt, S->status);

	/* SELECT / stored proc with output → open cursor */
	if (stmt_type == isc_info_sql_stmt_select ||
	    stmt_type == isc_info_sql_stmt_select_for_upd ||
	    stmt_type == isc_info_sql_stmt_exec_procedure) {

		unsigned cursor_flags = S->scrollable ? 0x1 : 0; /* CURSOR_TYPE_SCROLLABLE */
		int rc = fbs_open_cursor(
			FBG(master_instance), S->fbs_stmt, tr,
			S->in_buf, S->in_meta,
			cursor_flags, S->status
		);
		if (!rc) {
			pdo_fbird_stmt_error(stmt);
			return 0;
		}
		S->has_rows = 1;
		S->cursor_executed = 1;
		stmt->row_count = -1; /* unknown for SELECT */
	} else {
		/* DML / DDL */
		int rc = fbs_execute(
			FBG(master_instance), S->fbs_stmt, tr,
			S->in_buf, S->in_meta,
			S->out_buf, S->out_meta,
			S->status
		);
		if (!rc) {
			pdo_fbird_stmt_error(stmt);
			return 0;
		}
		S->cursor_executed = 1;  /* DML/DDL executed — mark for re-execution guard */
		ISC_UINT64 aff = fbs_get_affected_records(
			FBG(master_instance), S->fbs_stmt, S->status);
		stmt->row_count = (zend_long)aff;

		/* Only autocommit for DML/DDL — never for SELECT (cursor still open) */
 	if (H->autocommit && !H->in_manually_transaction) {
 		fbt_commit_retaining(H->fbt_trans, H->status);
 	}
	}
	return 1;
}
/* }}} */

/* {{{ pdo_fbird_stmt_fetch */
static int pdo_fbird_stmt_fetch(pdo_stmt_t *stmt,
	enum pdo_fetch_orientation ori, zend_long offset)
{
	pdo_fbird_stmt *S = (pdo_fbird_stmt *)stmt->driver_data;

	if (!S->has_rows || !S->fbs_stmt) return 0;

	int rc;
	if (S->scrollable) {
		switch (ori) {
			case PDO_FETCH_ORI_FIRST:
				rc = fbs_fetch_first(FBG(master_instance), S->fbs_stmt, S->out_buf, S->status);
				break;
			case PDO_FETCH_ORI_LAST:
				rc = fbs_fetch_last(FBG(master_instance), S->fbs_stmt, S->out_buf, S->status);
				break;
			case PDO_FETCH_ORI_ABS:
				rc = fbs_fetch_absolute(FBG(master_instance), S->fbs_stmt, (int)offset, S->out_buf, S->status);
				break;
			case PDO_FETCH_ORI_REL:
				rc = fbs_fetch_relative(FBG(master_instance), S->fbs_stmt, (int)offset, S->out_buf, S->status);
				break;
			case PDO_FETCH_ORI_PRIOR:
				rc = fbs_fetch_prior(FBG(master_instance), S->fbs_stmt, S->out_buf, S->status);
				break;
			case PDO_FETCH_ORI_NEXT:
			default:
				rc = fbs_fetch(FBG(master_instance), S->fbs_stmt, S->out_buf, S->status);
				break;
		}
	} else {
		rc = fbs_fetch(FBG(master_instance), S->fbs_stmt, S->out_buf, S->status);
	}

	if (rc == 1) return 1;   /* row fetched */
	if (rc == 0) {           /* end of data (BOF/EOF) */
		if (!S->scrollable) {
			/* Forward-only: close cursor on EOF */
			S->has_rows = 0;
			fbs_close_cursor(S->fbs_stmt, S->status);
		}
		/* Scrollable: keep cursor open for repositioning */
		return 0;
	}
	/* error */
	pdo_fbird_stmt_error(stmt);
	S->has_rows = 0;
	return 0;
}
/* }}} */

/* {{{ pdo_fbird_stmt_describe */
static int pdo_fbird_stmt_describe(pdo_stmt_t *stmt, int colno)
{
	pdo_fbird_stmt *S = (pdo_fbird_stmt *)stmt->driver_data;
	if (!S->out_meta || (unsigned)colno >= S->out_count) return 0;

	struct pdo_column_data *col = &stmt->columns[colno];

	const char *alias = fbm_get_alias(FBG(master_instance), S->out_meta, (unsigned)colno);
	const char *field = fbm_get_field(FBG(master_instance), S->out_meta, (unsigned)colno);
	const char *name  = (alias && alias[0]) ? alias : (field ? field : "");

	/* §3.2 FETCH_TABLE_NAMES: prepend "TABLE." to column name */
	if (S->H->fetch_table_names) {
		const char *relation = fbm_get_relation(FBG(master_instance), S->out_meta, (unsigned)colno);
		if (relation && relation[0]) {
			size_t rlen = strlen(relation);
			size_t nlen = strlen(name);
			char *combined = emalloc(rlen + 1 + nlen + 1);
			memcpy(combined, relation, rlen);
			combined[rlen] = '.';
			memcpy(combined + rlen + 1, name, nlen);
			combined[rlen + 1 + nlen] = '\0';
			col->name = zend_string_init(combined, rlen + 1 + nlen, 0);
			efree(combined);
		} else {
			col->name = zend_string_init(name, strlen(name), 0);
		}
	} else {
		col->name = zend_string_init(name, strlen(name), 0);
	}
	col->maxlen      = fbm_get_length(FBG(master_instance), S->out_meta, (unsigned)colno);
	col->precision   = (zend_long)fbm_get_scale(FBG(master_instance), S->out_meta, (unsigned)colno);

	(void)fbm_get_type(FBG(master_instance), S->out_meta, (unsigned)colno); /* type info available if needed */
	return 1;
}
/* }}} */

/* {{{ _pdo_fbird_arr_zval — convert Firebird array slice to PHP array */
static int _pdo_fbird_arr_zval(zval *ar_zval, char *data, zend_ulong data_size,
	ISC_ARRAY_DESC *desc, unsigned short el_type, unsigned short el_size,
	int dim)
{
	int u_bound = desc->array_desc_bounds[dim].array_bound_upper;
	int l_bound = desc->array_desc_bounds[dim].array_bound_lower;
	int dim_len = 1 + u_bound - l_bound;

	if (dim < desc->array_desc_dimensions) {
		zend_ulong slice_size = data_size / dim_len;
		array_init(ar_zval);
		for (int i = 0; i < dim_len; i++) {
			zval slice_zval;
			if (_pdo_fbird_arr_zval(&slice_zval, data, slice_size, desc,
					el_type, el_size, dim + 1) != SUCCESS) {
				return FAILURE;
			}
			data += slice_size;
			add_index_zval(ar_zval, l_bound + i, &slice_zval);
		}
	} else {
		/* leaf element */
		switch (el_type) {
			case SQL_SHORT: {
				short v; memcpy(&v, data, sizeof(short));
				if (desc->array_desc_scale < 0) {
					char buf[64]; snprintf(buf, sizeof(buf), "%.*f", -desc->array_desc_scale, v / pow(10, -desc->array_desc_scale));
					ZVAL_STRING(ar_zval, buf);
				} else { ZVAL_LONG(ar_zval, v); }
				break;
			}
			case SQL_LONG: {
				ISC_LONG v; memcpy(&v, data, sizeof(ISC_LONG));
				if (desc->array_desc_scale < 0) {
					char buf[64]; snprintf(buf, sizeof(buf), "%.*f", -desc->array_desc_scale, v / pow(10, -desc->array_desc_scale));
					ZVAL_STRING(ar_zval, buf);
				} else { ZVAL_LONG(ar_zval, v); }
				break;
			}
			case SQL_INT64: {
				ISC_INT64 v; memcpy(&v, data, sizeof(ISC_INT64));
				if (desc->array_desc_scale < 0) {
					char buf[64]; snprintf(buf, sizeof(buf), "%.*f", -desc->array_desc_scale, (double)v / pow(10, -desc->array_desc_scale));
					ZVAL_STRING(ar_zval, buf);
				} else { ZVAL_LONG(ar_zval, (zend_long)v); }
				break;
			}
			case SQL_FLOAT: {
				float v; memcpy(&v, data, sizeof(float));
				ZVAL_DOUBLE(ar_zval, (double)v);
				break;
			}
			case SQL_DOUBLE: {
				double v; memcpy(&v, data, sizeof(double));
				ZVAL_DOUBLE(ar_zval, v);
				break;
			}
			case SQL_VARYING: {
				/* cstring format: null-terminated */
				size_t slen = strnlen(data, el_size - 1);
				ZVAL_STRINGL(ar_zval, data, slen);
				break;
			}
			case SQL_TEXT: {
				/* Fixed CHAR: trim trailing spaces */
				int len = desc->array_desc_length;
				while (len > 0 && data[len - 1] == ' ') len--;
				ZVAL_STRINGL(ar_zval, data, len);
				break;
			}
 		case SQL_TIMESTAMP: {
				ISC_TIMESTAMP ts; memcpy(&ts, data, sizeof(ISC_TIMESTAMP));
				struct tm t;
				isc_decode_timestamp(&ts, &t);
				char buf[64]; snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
					t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
				ZVAL_STRING(ar_zval, buf);
				break;
			}
			case SQL_TYPE_DATE: {
				ISC_DATE dt; memcpy(&dt, data, sizeof(ISC_DATE));
				struct tm t;
				isc_decode_sql_date(&dt, &t);
				char buf[32]; snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
					t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
				ZVAL_STRING(ar_zval, buf);
				break;
			}
			case SQL_TYPE_TIME: {
				ISC_TIME tm_val; memcpy(&tm_val, data, sizeof(ISC_TIME));
				struct tm t;
				isc_decode_sql_time(&tm_val, &t);
				char buf[32]; snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
					t.tm_hour, t.tm_min, t.tm_sec);
				ZVAL_STRING(ar_zval, buf);
				break;
			}
			default:
				ZVAL_STRINGL(ar_zval, data, el_size);
				break;
		}
	}
	return SUCCESS;
}
/* }}} */

/* {{{ pdo_fbird_stmt_get_col */
static int pdo_fbird_stmt_get_col(pdo_stmt_t *stmt, int colno,
	zval *result, enum pdo_param_type *type)
{
	pdo_fbird_stmt *S = (pdo_fbird_stmt *)stmt->driver_data;
	if (!S->out_meta || !S->out_buf || (unsigned)colno >= S->out_count) {
		ZVAL_NULL(result);
		return 1;
	}

	if (_pdo_fbird_is_null(S, (unsigned)colno)) {
		ZVAL_NULL(result);
		return 1;
	}

	unsigned sql_type = fbm_get_type(FBG(master_instance), S->out_meta, (unsigned)colno);
	unsigned offset   = fbm_get_offset(FBG(master_instance), S->out_meta, (unsigned)colno);
	unsigned length   = fbm_get_length(FBG(master_instance), S->out_meta, (unsigned)colno);
	int      scale    = fbm_get_scale(FBG(master_instance), S->out_meta, (unsigned)colno);
	unsigned char *data = S->out_buf + offset;

	switch (sql_type & ~1) {
		case SQL_SHORT: {
			short v; memcpy(&v, data, sizeof(short));
			if (scale < 0) {
				char buf[32]; snprintf(buf, sizeof(buf), "%.*f", -scale, v * pow(10.0, scale));
				ZVAL_STRING(result, buf);
			} else ZVAL_LONG(result, v);
			break;
		}
		case SQL_LONG: {
			ISC_LONG v; memcpy(&v, data, sizeof(ISC_LONG));
			if (scale < 0) {
				char buf[32]; snprintf(buf, sizeof(buf), "%.*f", -scale, v * pow(10.0, scale));
				ZVAL_STRING(result, buf);
			} else ZVAL_LONG(result, v);
			break;
		}
		case SQL_INT64: {
			ISC_INT64 v; memcpy(&v, data, sizeof(ISC_INT64));
			if (scale < 0) {
				char buf[48]; snprintf(buf, sizeof(buf), "%.*f", -scale, v * pow(10.0, scale));
				ZVAL_STRING(result, buf);
			} else ZVAL_LONG(result, (zend_long)v);
			break;
		}
		case SQL_FLOAT: {
			float v; memcpy(&v, data, sizeof(float));
			ZVAL_DOUBLE(result, (double)v);
			break;
		}
		case SQL_DOUBLE:
		case SQL_D_FLOAT: {
			double v; memcpy(&v, data, sizeof(double));
			ZVAL_DOUBLE(result, v);
			break;
		}
		case SQL_BOOLEAN: {
			unsigned char v = *data;
			ZVAL_BOOL(result, v != 0);
			break;
		}
		case SQL_VARYING: {
			/* VARY: first 2 bytes = length, then data */
			unsigned short vlen; memcpy(&vlen, data, sizeof(unsigned short));
			ZVAL_STRINGL(result, (char *)(data + sizeof(unsigned short)), vlen);
			break;
		}
		case SQL_TEXT: {
			/* Fixed CHAR: trim trailing spaces */
			char *s = (char *)data;
			size_t len = length;
			while (len > 0 && s[len - 1] == ' ') len--;
			ZVAL_STRINGL(result, s, len);
			break;
		}
		case SQL_TYPE_DATE: {
			ISC_DATE dt; memcpy(&dt, data, sizeof(ISC_DATE));
			unsigned year, month, day;
			fbu_decode_date(FBG(master_instance), dt, &year, &month, &day);
			char buf[64];
			if (S->H->date_format) {
				struct tm tm = {0};
				tm.tm_year = year - 1900; tm.tm_mon = month - 1; tm.tm_mday = day;
				strftime(buf, sizeof(buf), S->H->date_format, &tm);
			} else {
				snprintf(buf, sizeof(buf), "%04u-%02u-%02u", year, month, day);
			}
			ZVAL_STRING(result, buf);
			break;
		}
		case SQL_TYPE_TIME: {
			ISC_TIME tm_val; memcpy(&tm_val, data, sizeof(ISC_TIME));
			unsigned hours, minutes, seconds, fractions;
			fbu_decode_time(FBG(master_instance), tm_val, &hours, &minutes, &seconds, &fractions);
			char buf[64];
			if (S->H->time_format) {
				struct tm tm = {0};
				tm.tm_hour = hours; tm.tm_min = minutes; tm.tm_sec = seconds;
				strftime(buf, sizeof(buf), S->H->time_format, &tm);
			} else {
				if (fractions > 0) {
					snprintf(buf, sizeof(buf), "%02u:%02u:%02u.%04u", hours, minutes, seconds, fractions);
				} else {
					snprintf(buf, sizeof(buf), "%02u:%02u:%02u", hours, minutes, seconds);
				}
			}
			ZVAL_STRING(result, buf);
			break;
		}
		case SQL_TIMESTAMP: {
			ISC_TIMESTAMP ts; memcpy(&ts, data, sizeof(ISC_TIMESTAMP));
			unsigned year, month, day, hours, minutes, seconds, fractions;
			fbu_decode_timestamp(FBG(master_instance), &ts, &year, &month, &day, &hours, &minutes, &seconds, &fractions);
			char buf[80];
			if (S->H->timestamp_format) {
				struct tm tm = {0};
				tm.tm_year = year - 1900; tm.tm_mon = month - 1; tm.tm_mday = day;
				tm.tm_hour = hours; tm.tm_min = minutes; tm.tm_sec = seconds;
				strftime(buf, sizeof(buf), S->H->timestamp_format, &tm);
			} else {
				if (fractions > 0) {
					snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u.%04u", year, month, day, hours, minutes, seconds, fractions);
				} else {
					snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u", year, month, day, hours, minutes, seconds);
				}
			}
			ZVAL_STRING(result, buf);
			break;
		}
#ifdef SQL_TIMESTAMP_TZ
		case SQL_TIMESTAMP_TZ: {
			ISC_TIMESTAMP_TZ ts_tz; memcpy(&ts_tz, data, sizeof(ISC_TIMESTAMP_TZ));
			unsigned year, month, day, hours, minutes, seconds, fractions;
			char tz_buf[64] = "";
			fbu_decode_timestamp_tz(FBG(master_instance), &ts_tz, &year, &month, &day, &hours, &minutes, &seconds, &fractions, sizeof(tz_buf), tz_buf);
			char buf[128];
			snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u.%04u %s", year, month, day, hours, minutes, seconds, fractions, tz_buf);
			ZVAL_STRING(result, buf);
			break;
		}
#endif
#ifdef SQL_TIME_TZ
		case SQL_TIME_TZ: {
			ISC_TIME_TZ tm_tz; memcpy(&tm_tz, data, sizeof(ISC_TIME_TZ));
			unsigned hours, minutes, seconds, fractions;
			char tz_buf[64] = "";
			fbu_decode_time_tz(FBG(master_instance), &tm_tz, &hours, &minutes, &seconds, &fractions, sizeof(tz_buf), tz_buf);
			char buf[128];
			snprintf(buf, sizeof(buf), "%02u:%02u:%02u.%04u %s", hours, minutes, seconds, fractions, tz_buf);
			ZVAL_STRING(result, buf);
			break;
		}
#endif
#ifdef SQL_INT128
		case SQL_INT128: {
			char buf[64];
			if (fbu_int128_to_string(FBG(master_instance), data, scale, buf, sizeof(buf)) == 0) {
				ZVAL_STRING(result, buf);
			} else {
				ZVAL_STRINGL(result, (char *)data, length);
			}
			break;
		}
#endif
#if defined(SQL_DEC16) || defined(SQL_DEC34)
		case SQL_DEC16:
		case SQL_DEC34: {
			/* jane: SQL_DEC16=576, SQL_DEC34=577. The switch uses `sql_type & ~1`
			 * to strip the nullable bit, which collapses DEC34 into DEC16
			 * (577 & ~1 = 576). Must check the ORIGINAL sql_type to pick the
			 * correct converter - DEC16 reads 8 bytes, DEC34 reads 16 bytes. */
			char buf[48];
			int rc;
			if (sql_type == SQL_DEC34) {
				rc = fbu_decfloat34_to_string(FBG(master_instance), data, buf, sizeof(buf));
			} else {
				rc = fbu_decfloat16_to_string(FBG(master_instance), data, buf, sizeof(buf));
			}
			if (rc == 0) {
				ZVAL_STRING(result, buf);
			} else {
				ZVAL_STRINGL(result, (char *)data, length);
			}
			break;
		}
#endif
		case SQL_ARRAY: {
			/* Read array field */
			ISC_QUAD ar_qd; memcpy(&ar_qd, data, sizeof(ISC_QUAD));
			void *attachment = fbc_get_attachment(S->H->fbc_conn);
			void *transaction = fbt_get_handle(S->H->fbt_trans);
			if (!attachment || !transaction) {
				ZVAL_NULL(result);
				break;
			}
			/* Get table/column name for descriptor lookup */
			const char *rel = fbm_get_relation(FBG(master_instance), S->out_meta, (unsigned)colno);
			const char *fld = fbm_get_field(FBG(master_instance), S->out_meta, (unsigned)colno);
			if (!rel || !fld) { ZVAL_NULL(result); break; }
			ISC_ARRAY_DESC ar_desc;
			if (fba_lookup_bounds(FBG(master_instance), attachment, transaction,
					rel, fld, &ar_desc, S->status) != 0) {
				ZVAL_NULL(result); break;
			}
			/* Determine element type and size */
			unsigned short el_type, el_size;
			switch (ar_desc.array_desc_dtype) {
				case blr_text: case blr_text2:
					el_type = SQL_TEXT; el_size = ar_desc.array_desc_length; break;
				case blr_short:
					el_type = SQL_SHORT; el_size = sizeof(short); break;
				case blr_long:
					el_type = SQL_LONG; el_size = sizeof(ISC_LONG); break;
				case blr_int64:
					el_type = SQL_INT64; el_size = sizeof(ISC_INT64); break;
				case blr_float:
					el_type = SQL_FLOAT; el_size = sizeof(float); break;
				case blr_double:
					el_type = SQL_DOUBLE; el_size = sizeof(double); break;
				case blr_timestamp:
					el_type = SQL_TIMESTAMP; el_size = sizeof(ISC_TIMESTAMP); break;
				case blr_sql_date:
					el_type = SQL_TYPE_DATE; el_size = sizeof(ISC_DATE); break;
				case blr_sql_time:
					el_type = SQL_TYPE_TIME; el_size = sizeof(ISC_TIME); break;
				case blr_varying: case blr_varying2:
					el_type = SQL_VARYING; el_size = ar_desc.array_desc_length + 1; break;
				default:
					ZVAL_NULL(result); break;
			}
			/* Calculate total array size */
			zend_ulong total_elems = 1;
			for (unsigned short d = 0; d < ar_desc.array_desc_dimensions; d++) {
				total_elems *= 1 + ar_desc.array_desc_bounds[d].array_bound_upper
					- ar_desc.array_desc_bounds[d].array_bound_lower;
			}
			ISC_LONG fetch_size = (ISC_LONG)(el_size * total_elems);
			void *ar_data = ecalloc(1, (size_t)fetch_size);
			if (fba_get_slice(FBG(master_instance), attachment, transaction,
					&ar_qd, &ar_desc, ar_data, &fetch_size, S->status) != 0) {
				efree(ar_data);
				ZVAL_NULL(result); break;
			}
			if (_pdo_fbird_arr_zval(result, (char *)ar_data, fetch_size,
					&ar_desc, el_type, el_size, 0) != SUCCESS) {
				efree(ar_data);
				ZVAL_NULL(result); break;
			}
			efree(ar_data);
			break;
		}
		case SQL_BLOB: {
			/* Read blob content */
			ISC_QUAD bid; memcpy(&bid, data, sizeof(ISC_QUAD));
			void *attachment = fbc_get_attachment(S->H->fbc_conn);
			void *transaction = fbt_get_handle(S->H->fbt_trans);
			void *blob = (attachment && transaction) ? fbb_open(FBG(master_instance), attachment, transaction, &bid, 0, NULL, S->H->status) : NULL;
			if (!blob) {
				/* Fallback: return blob ID */
				char buf[48];
				snprintf(buf, sizeof(buf), "%08x:%08x", bid.gds_quad_high, bid.gds_quad_low);
				ZVAL_STRING(result, buf);
				break;
			}
			smart_str blob_str = {0};
			char seg_buf[4096];
			unsigned actual_len = 0;
			int rc;
			while ((rc = fbb_get_segment(FBG(master_instance), blob, sizeof(seg_buf), seg_buf, &actual_len, S->H->status)) == 0 || rc == 2) {
				smart_str_appendl(&blob_str, seg_buf, actual_len);
				if (rc == 0) continue; /* more data */
			}
			fbb_close(FBG(master_instance), blob, S->H->status);

			if (type && *type == PDO_PARAM_LOB) {
				/* Return as PHP stream for LOB binding */
				php_stream *stream = php_stream_memory_create(TEMP_STREAM_DEFAULT);
				if (stream && blob_str.s) {
					php_stream_write(stream, ZSTR_VAL(blob_str.s), ZSTR_LEN(blob_str.s));
					php_stream_seek(stream, 0, SEEK_SET);
				}
				if (blob_str.s) {
					smart_str_free(&blob_str);
				}
				if (stream) {
					php_stream_to_zval(stream, result);
				} else {
					ZVAL_NULL(result);
				}
			} else {
				/* Return as string */
				if (blob_str.s) {
					smart_str_0(&blob_str);
					ZVAL_STR(result, blob_str.s);
				} else {
					ZVAL_EMPTY_STRING(result);
				}
			}
			break;
		}
		default:
			ZVAL_STRINGL(result, (char *)data, length);
			break;
	}
	return 1;
}
/* }}} */

/* {{{ pdo_fbird_stmt_param_hook — bind positional ? parameters */
static int pdo_fbird_stmt_param_hook(pdo_stmt_t *stmt,
	struct pdo_bound_param_data *param, enum pdo_param_event event_type)
{
	pdo_fbird_stmt *S = (pdo_fbird_stmt *)stmt->driver_data;

	/* Resolve named parameter to positional index.
	   PDO passes names with leading ':', our map stores without. */
	if (event_type == PDO_PARAM_EVT_NORMALIZE && param->name && S->named_params) {
		const char *pname = ZSTR_VAL(param->name);
		if (pname[0] == ':') pname++;
		for (unsigned int i = 0; i < S->named_param_count; i++) {
			if (S->named_params[i].name && strcasecmp(pname, S->named_params[i].name) == 0) {
				param->paramno = S->named_params[i].position;
				return 1;
			}
		}
		return 1;
	}

	if (event_type != PDO_PARAM_EVT_EXEC_PRE) return 1;
	if (!S->in_meta || !S->in_buf) return 1;
	if (param->paramno < 0 || (unsigned)param->paramno >= S->in_count) return 1;

	unsigned idx    = (unsigned)param->paramno;
	unsigned offset = fbm_get_offset(FBG(master_instance), S->in_meta, idx);
	unsigned length = fbm_get_length(FBG(master_instance), S->in_meta, idx);
	unsigned null_off = fbm_get_null_offset(FBG(master_instance), S->in_meta, idx);
	unsigned sql_type = fbm_get_type(FBG(master_instance), S->in_meta, idx);
	int      scale   = fbm_get_scale(FBG(master_instance), S->in_meta, idx);
	unsigned char *dest = S->in_buf + offset;
	short *null_flag    = (short *)(S->in_buf + null_off);

	/* Allow NULL binding: ensure nullable bit is set (sql_type |= 1) */
	sql_type |= 1;

	zval *val = &param->parameter;
	if (!val || Z_TYPE_P(val) == IS_NULL) {
		*null_flag = -1;
		memset(dest, 0, length);
		return 1;
	}
	*null_flag = 0;

	switch (sql_type & ~1) {
		case SQL_SHORT: {
			short v = (short)zval_get_long(val);
			memcpy(dest, &v, sizeof(short));
			break;
		}
		case SQL_LONG: {
			ISC_LONG v = (ISC_LONG)zval_get_long(val);
			memcpy(dest, &v, sizeof(ISC_LONG));
			break;
		}
		case SQL_INT64: {
			ISC_INT64 v = (ISC_INT64)zval_get_long(val);
			memcpy(dest, &v, sizeof(ISC_INT64));
			break;
		}
		case SQL_FLOAT: {
			float v = (float)zval_get_double(val);
			memcpy(dest, &v, sizeof(float));
			break;
		}
		case SQL_DOUBLE:
		case SQL_D_FLOAT: {
			double v = zval_get_double(val);
			memcpy(dest, &v, sizeof(double));
			break;
		}
		case SQL_BOOLEAN: {
			unsigned char v = zval_is_true(val) ? 1 : 0;
			*dest = v;
			break;
		}
#ifdef SQL_INT128
		case SQL_INT128: {
			zend_string *s = zval_get_string(val);
			FB_I128 i128;
			if (fbu_string_to_int128(FBG(master_instance), ZSTR_VAL(s), (int)scale, &i128) == 0) {
				memcpy(dest, &i128, sizeof(FB_I128));
			} else {
				memset(dest, 0, sizeof(FB_I128));
			}
			zend_string_release(s);
			break;
		}
#endif
#ifdef SQL_DEC16
		case SQL_DEC16: {
			zend_string *s = zval_get_string(val);
			FB_DEC16 dec16;
			if (fbu_string_to_decfloat16(FBG(master_instance), ZSTR_VAL(s), &dec16) == 0) {
				memcpy(dest, &dec16, sizeof(FB_DEC16));
			} else {
				memset(dest, 0, sizeof(FB_DEC16));
			}
			zend_string_release(s);
			break;
		}
#endif
#ifdef SQL_DEC34
		case SQL_DEC34: {
			zend_string *s = zval_get_string(val);
			FB_DEC34 dec34;
			if (fbu_string_to_decfloat34(FBG(master_instance), ZSTR_VAL(s), &dec34) == 0) {
				memcpy(dest, &dec34, sizeof(FB_DEC34));
			} else {
				memset(dest, 0, sizeof(FB_DEC34));
			}
			zend_string_release(s);
			break;
		}
#endif
		case SQL_VARYING: {
			zend_string *s = zval_get_string(val);
			unsigned short slen = (unsigned short)MIN(ZSTR_LEN(s), length);
			memcpy(dest, &slen, sizeof(unsigned short));
			memcpy(dest + sizeof(unsigned short), ZSTR_VAL(s), slen);
			zend_string_release(s);
			break;
		}
		case SQL_TEXT: {
			zend_string *s = zval_get_string(val);
			size_t slen = MIN(ZSTR_LEN(s), length);
			memcpy(dest, ZSTR_VAL(s), slen);
			if (slen < length) memset(dest + slen, ' ', length - slen);
			zend_string_release(s);
			break;
		}
		case SQL_ARRAY: {
			/* Array parameter binding: serialize PHP array into Firebird layout */
			if (Z_TYPE_P(val) != IS_ARRAY) {
				/* Non-array value: treat as string for the ISC_QUAD */
				memset(dest, 0, sizeof(ISC_QUAD));
				break;
			}
			void *attachment = fbc_get_attachment(S->H->fbc_conn);
			void *transaction = fbt_get_handle(S->H->fbt_trans);
			if (!attachment || !transaction) { memset(dest, 0, sizeof(ISC_QUAD)); break; }
			/* Get relation/field name from input metadata */
			const char *rel = fbm_get_relation(FBG(master_instance), S->in_meta, idx);
			const char *fld = fbm_get_field(FBG(master_instance), S->in_meta, idx);
			if (!rel || !fld || !*rel || !*fld) { memset(dest, 0, sizeof(ISC_QUAD)); break; }
			ISC_ARRAY_DESC ar_desc;
			if (fba_lookup_bounds(FBG(master_instance), attachment, transaction,
					rel, fld, &ar_desc, S->status) != 0) {
				memset(dest, 0, sizeof(ISC_QUAD)); break;
			}
			/* Determine element size */
			unsigned short el_size;
			switch (ar_desc.array_desc_dtype) {
				case blr_short: el_size = sizeof(short); break;
				case blr_long: el_size = sizeof(ISC_LONG); break;
				case blr_int64: el_size = sizeof(ISC_INT64); break;
				case blr_float: el_size = sizeof(float); break;
				case blr_double: el_size = sizeof(double); break;
				case blr_text: case blr_text2: el_size = ar_desc.array_desc_length; break;
				case blr_varying: case blr_varying2: el_size = ar_desc.array_desc_length + 1; break;
				case blr_timestamp: el_size = sizeof(ISC_TIMESTAMP); break;
				case blr_sql_date: el_size = sizeof(ISC_DATE); break;
				case blr_sql_time: el_size = sizeof(ISC_TIME); break;
				default: el_size = ar_desc.array_desc_length; break;
			}
			/* Calculate total elements */
			zend_ulong total_elems = 1;
			for (unsigned short d = 0; d < ar_desc.array_desc_dimensions; d++) {
				total_elems *= 1 + ar_desc.array_desc_bounds[d].array_bound_upper
					- ar_desc.array_desc_bounds[d].array_bound_lower;
			}
			ISC_LONG buf_size = (ISC_LONG)(el_size * total_elems);
			char *ar_buf = ecalloc(1, (size_t)buf_size);
			/* Flatten PHP array into buffer */
			char *ptr = ar_buf;
			zval *elem;
			zend_ulong elem_idx = 0;
			ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(val), elem) {
				if (elem_idx >= total_elems) break;
				switch (ar_desc.array_desc_dtype) {
					case blr_short: { short v = (short)zval_get_long(elem); memcpy(ptr, &v, sizeof(short)); break; }
					case blr_long: { ISC_LONG v = (ISC_LONG)zval_get_long(elem); memcpy(ptr, &v, sizeof(ISC_LONG)); break; }
					case blr_int64: { ISC_INT64 v = (ISC_INT64)zval_get_long(elem); memcpy(ptr, &v, sizeof(ISC_INT64)); break; }
					case blr_float: { float v = (float)zval_get_double(elem); memcpy(ptr, &v, sizeof(float)); break; }
					case blr_double: { double v = zval_get_double(elem); memcpy(ptr, &v, sizeof(double)); break; }
					case blr_text: case blr_text2: {
						zend_string *s = zval_get_string(elem);
						size_t slen = MIN(ZSTR_LEN(s), el_size);
						memcpy(ptr, ZSTR_VAL(s), slen);
						if (slen < el_size) memset(ptr + slen, ' ', el_size - slen);
						zend_string_release(s);
						break;
					}
					case blr_varying: case blr_varying2: {
						zend_string *s = zval_get_string(elem);
						size_t slen = MIN(ZSTR_LEN(s), (size_t)(el_size - 1));
						memcpy(ptr, ZSTR_VAL(s), slen);
						ptr[slen] = '\0';
						zend_string_release(s);
						break;
					}
					default: {
						zend_string *s = zval_get_string(elem);
						size_t slen = MIN(ZSTR_LEN(s), el_size);
						memcpy(ptr, ZSTR_VAL(s), slen);
						zend_string_release(s);
						break;
					}
				}
				ptr += el_size;
				elem_idx++;
			} ZEND_HASH_FOREACH_END();
			ISC_QUAD array_id = {0, 0};
			if (fba_put_slice(FBG(master_instance), attachment, transaction,
					&array_id, &ar_desc, ar_buf, buf_size, S->status) != 0) {
				efree(ar_buf);
				memset(dest, 0, sizeof(ISC_QUAD)); break;
			}
			efree(ar_buf);
			memcpy(dest, &array_id, sizeof(ISC_QUAD));
			break;
		}
		default: {
			zend_string *s = zval_get_string(val);
			size_t slen = MIN(ZSTR_LEN(s), length);
			memcpy(dest, ZSTR_VAL(s), slen);
			zend_string_release(s);
			break;
		}
	}
	return 1;
}
/* }}} */

/* {{{ pdo_fbird_stmt_next_rowset — Firebird does not support multi-rowset */
static int pdo_fbird_stmt_next_rowset(pdo_stmt_t *stmt)
{
	(void)stmt;
	return 0; /* not supported */
}
/* }}} */

/* {{{ pdo_fbird_stmt_set_attribute */
static int pdo_fbird_stmt_set_attribute(pdo_stmt_t *stmt, zend_long attr, zval *val)
{
	pdo_fbird_stmt *S = (pdo_fbird_stmt *)stmt->driver_data;

	switch (attr) {
		case PDO_ATTR_CURSOR_NAME: {
			if (Z_TYPE_P(val) != IS_STRING) return 0;
			fbs_set_cursor_name(FBG(master_instance), S->fbs_stmt,
				Z_STRVAL_P(val), S->status);
			return 1;
		}
		default:
			return 0;
	}
}
/* }}} */

/* {{{ pdo_fbird_stmt_get_attribute */
static int pdo_fbird_stmt_get_attribute(pdo_stmt_t *stmt, zend_long attr, zval *val)
{
	pdo_fbird_stmt *S = (pdo_fbird_stmt *)stmt->driver_data;

	switch (attr) {
		case PDO_ATTR_CURSOR:
			ZVAL_LONG(val, S->scrollable ? PDO_CURSOR_SCROLL : PDO_CURSOR_FWDONLY);
			return 1;
		default:
			return 0;
	}
}
/* }}} */

/* {{{ pdo_fbird_stmt_cursor_closer */
static int pdo_fbird_stmt_cursor_closer(pdo_stmt_t *stmt)
{
	pdo_fbird_stmt *S = (pdo_fbird_stmt *)stmt->driver_data;
	if (S->has_rows && S->fbs_stmt && fbs_is_cursor_open(S->fbs_stmt)) {
		fbs_close_cursor(S->fbs_stmt, S->status);
	}
	S->has_rows = 0;
	S->cursor_executed = 0;
	return 1;
}
/* }}} */

/* {{{ pdo_fbird_stmt_methods */
const struct pdo_stmt_methods pdo_fbird_stmt_methods = {
	pdo_fbird_stmt_dtor,
	pdo_fbird_stmt_execute,
	pdo_fbird_stmt_fetch,
	pdo_fbird_stmt_describe,
	pdo_fbird_stmt_get_col,
	pdo_fbird_stmt_param_hook,
	pdo_fbird_stmt_set_attribute,
	pdo_fbird_stmt_get_attribute,
	NULL, /* get_column_meta */
	pdo_fbird_stmt_next_rowset,
	pdo_fbird_stmt_cursor_closer,
};
/* }}} */

#endif /* HAVE_PDO_FBIRD */
