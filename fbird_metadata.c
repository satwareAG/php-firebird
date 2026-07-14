/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

/* Enable GNU features (includes X/Open for strptime, BSD for strlcpy/tm_zone) */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <ctype.h>
#include <string.h>

#include "php.h"
#include "php_ini.h"

#if HAVE_FIREBIRD

#include "ext/standard/php_standard.h"
#include "php_firebird.h"
#include "php_fbird_includes.h"
#include "php_fbird_query_internal.h"
#include "firebird_utils.h"
#include "fbird_classes.h"

#define ISC_LONG_MIN    INT_MIN
#define ISC_LONG_MAX    INT_MAX

/* Forward declarations */
static bool _php_fbird_infer_returning_prefix(const char *sql, size_t index, char *out, size_t out_len);
static bool _php_fbird_infer_returning_full_alias(const char *sql, size_t index, char *out, size_t out_len);
static bool _php_fbird_sql_has_returning(const char *sql);

void _php_fbird_insert_alias(HashTable *ht, const char *alias)
{
	/* Buffer size increased to handle aliases of maximum length plus suffix */
	char buf[METADATALENGTH + 32];
	zval t2;
	int i = 1;
	char const *base = alias;
	size_t alias_len = strlen(alias);

	if (*alias == '\0') {
		base = "FIELD";
		/* For empty name, immediately enter deduplication logic starting with 0 */
		i = 0;
		snprintf(buf, sizeof(buf), "%s_%02d", base, i++);
		alias = buf;
		alias_len = strlen(alias);
	}

	while (zend_symtable_str_find_ptr(ht, alias, alias_len) != NULL) {
		snprintf(buf, sizeof(buf), "%s_%02d", base, i++);
		alias = buf;
		alias_len = strlen(alias);
	}

	ZVAL_NULL(&t2);
	zend_symtable_str_update(ht, alias, alias_len, &t2);
}

void _php_fbird_field_info(zval *return_value, fbird_query *fb_query, int is_outvar, int num)
{
	unsigned short len;
	char buf[16], *s = buf;
	XSQLDA *sqlda;
	XSQLVAR *var;

 if(is_outvar){
        sqlda = fb_query->out_sqlda;
        if (sqlda == NULL) {
            _php_fbird_module_error("Trying to get field info from a non-select query");
            RETURN_FALSE;
        }
    } else {
        sqlda = fb_query->in_sqlda;
        /* For parameter metadata, return false quietly when not available */
        if (sqlda == NULL) {
            RETURN_FALSE;
        }
    }

	var = sqlda->sqlvar;

 if (!var || num < 0 || num >= sqlda->sqld) {
        /* For parameters, do not emit a warning on out-of-range; return false quietly */
        if (is_outvar) {
            _php_fbird_module_error("Field %d does not exist (valid range: 0-%d)", num, sqlda ? sqlda->sqld - 1 : -1);
        }
        RETURN_FALSE;
    }

	var += num;

	array_init(return_value);

	/* Enhanced parameter metadata building with fallbacks for missing bind description data
	 *
	 * Note: With OO API migration, we always use the XSQLDA-based fallback path.
	 * The out_sqlda and in_sqlda are now populated from OO API metadata during prepare,
	 * so the XSQLDA data path provides complete field information.
	 *
	 * The legacy fbu_insert_field_info() path using get_statement_interface is disabled
	 * because it requires fb_query->stmt.stmt (legacy isc_stmt_handle) which is no longer
	 * available in OO API mode.
	 */
	{
		// Old API with enhanced parameter support
		/* Handle name - provide fallback for parameters where sqlname might be empty */
		const char *field_name = (var->sqlname[0] != '\0') ? var->sqlname : "";
		if (!is_outvar && strlen(field_name) == 0) {
			/* For parameters, generate a meaningful fallback name */
			snprintf(buf, sizeof(buf), "PARAM_%d", num);
			field_name = buf;
		}
		add_index_stringl(return_value, 0, field_name, strlen(field_name));
		add_assoc_stringl(return_value, "name", field_name, strlen(field_name));

		/* Handle alias - for parameters, alias typically same as name */
		const char *alias_name = (var->aliasname[0] != '\0') ? var->aliasname : field_name;
		add_index_stringl(return_value, 1, alias_name, strlen(alias_name));
		add_assoc_stringl(return_value, "alias", alias_name, strlen(alias_name));

		/* Handle relation - typically empty for parameters */
		const char *relation_name = (var->relname[0] != '\0') ? var->relname : "";
		add_index_stringl(return_value, 2, relation_name, strlen(relation_name));
		add_assoc_stringl(return_value, "relation", relation_name, strlen(relation_name));
	}

	len = slprintf(buf, 16, "%d", var->sqllen);
	add_index_stringl(return_value, 3, buf, len);
	add_assoc_stringl(return_value, "length", buf, len);

	/*
	* SQL_ consts are part of Firebird-API.
	*/

	if (var->sqlscale < 0) {
		unsigned short precision = 0;

		switch (var->sqltype & ~1) {
#ifdef SQL_BOOLEAN
			case SQL_BOOLEAN:
				precision = 1;
				break;
#endif
			case SQL_SHORT:
				precision = 4;
				break;
			case SQL_LONG:
				precision = 9;
				break;
			case SQL_INT64:
				precision = 18;
				break;
#if FB_API_VER >= 40
			case SQL_INT128:
				precision = 38;
				break;
#endif
			default:
				break;
		}
		len = slprintf(buf, 16, "NUMERIC(%d,%d)", precision, -var->sqlscale);
		add_index_stringl(return_value, 4, s, len);
		add_assoc_stringl(return_value, "type", s, len);
	} else {
		switch (var->sqltype & ~1) {
			case SQL_TEXT:
				s = "CHAR";
				break;
			case SQL_VARYING:
				s = "VARCHAR";
				break;
			case SQL_SHORT:
				s = "SMALLINT";
				break;
#ifdef SQL_BOOLEAN
			case SQL_BOOLEAN:
				s = "BOOLEAN";
				break;
#endif
			case SQL_LONG:
				s = "INTEGER";
				break;
			case SQL_FLOAT:
				s = "FLOAT"; break;
			case SQL_DOUBLE:
			case SQL_D_FLOAT:
				s = "DOUBLE PRECISION"; break;
			case SQL_INT64:
				s = "BIGINT";
				break;
#if FB_API_VER >= 40
			case SQL_INT128:
				s = "INT128";
				break;
			case SQL_DEC16:
				s = "DECFLOAT(16)";
				break;
			case SQL_DEC34:
				s = "DECFLOAT(34)";
				break;
#endif
			case SQL_TIMESTAMP:
				s = "TIMESTAMP";
				break;
			case SQL_TYPE_DATE:
				s = "DATE";
				break;
			case SQL_TYPE_TIME:
				s = "TIME";
				break;
			case SQL_BLOB:
				s = "BLOB";
				break;
			case SQL_ARRAY:
				s = "ARRAY";
				break;
				/*
				 * FUTURE ENHANCEMENT: For ARRAY fields, this function could return
				 * extended metadata including element type, size, and dimensions
				 * (e.g., "ARRAY[1:10] OF INTEGER"). This would require calling
				 * isc_array_lookup_bounds() for each array field. Current behavior
				 * returns "ARRAY" which is sufficient for most use cases.
				 */
			case SQL_QUAD:
				s = "QUAD";
				break;
			default:
				s = "UNKNOWN";
				break;
#if FB_API_VER >= 40
			// These are converted to VARCHAR via isc_dpb_set_bind tag at
			// connect and will appear to clients as VARCHAR
			// case SQL_DEC16:
			// case SQL_DEC34:
			// case SQL_INT128:
			case SQL_TIMESTAMP_TZ:
				s = "TIMESTAMP WITH TIME ZONE";
				break;
			case SQL_TIME_TZ:
				s = "TIME WITH TIME ZONE";
				break;
#endif
		}
		add_index_string(return_value, 4, s);
		add_assoc_string(return_value, "type", s);
	}
}

PHP_FUNCTION(fbird_field_info)
{
	zval *result_arg;
	zend_long field_arg;
	fbird_query *fb_query;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "zl", &result_arg, &field_arg) == FAILURE) {
		return;
	}

	/* Validate first argument is a query resource with proper error messages */
	FBIRD_VALIDATE_QUERY_EX(result_arg, 1, fb_query);
	if (!fb_query) {
		RETURN_FALSE;
	}

	_php_fbird_field_info(return_value, fb_query, 1, (ISC_SHORT)field_arg);
}

PHP_FUNCTION(fbird_num_params)
{
	zval *result;
	fbird_query *fb_query;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &result) == FAILURE) {
		return;
	}

	/* Validate first argument is a query resource with proper error messages */
	FBIRD_VALIDATE_QUERY_EX(result, 1, fb_query);
	if (!fb_query) {
		RETURN_FALSE;
	}

	/*
	 * Firebird 3.0+ OO API - use cached parameter count from IMessageMetadata
	 * Set during query preparation via fbs_get_input_count()
	 */
	RETURN_LONG(fb_query->in_fields_count);
}

PHP_FUNCTION(fbird_param_info)
{
	zval *result_arg;
	zend_long field_arg;
	fbird_query *fb_query;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "zl", &result_arg, &field_arg) == FAILURE) {
		return;
	}

	/* Validate first argument is a query resource with proper error messages */
	FBIRD_VALIDATE_QUERY_EX(result_arg, 1, fb_query);
	if (!fb_query) {
		RETURN_FALSE;
	}

	_php_fbird_field_info(return_value, fb_query, 0, field_arg);
}

PHP_FUNCTION(fbird_num_fields)
{
	zval *result;
	fbird_query *fb_query;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &result) == FAILURE) {
		return;
	}

	/* Validate first argument is a query resource with proper error messages */
	FBIRD_VALIDATE_QUERY_EX(result, 1, fb_query);
	if (!fb_query) {
		RETURN_FALSE;
	}

	/*
	 * Firebird 3.0+ OO API - use cached field count from IMessageMetadata
	 * Set during query preparation via fbs_get_output_count()
	 */
	RETURN_LONG(fb_query->out_fields_count);
}

// We can't rely on aliasname coming from XSQLVAR if we want long field names
// (>31). We also can't rely on parsing buffer from isc_dsql_sql_info() because
// it's 32KB limit can be easily overflown with combination of long field names
// and large amounts of fields. So I added wrapper to use newer API but that
// also require runtime fbclient > 40 hence the runtime checks. Ideally rewrite
// everything using newer API but that's a bit of work.

/* Helper function to trim trailing whitespace from alias
 * Returns a newly allocated string that must be freed by the caller.
 * Issue #23: Firebird 3.0+ may return CHAR-type aliases with trailing space padding. */
static char *_php_fbird_rtrim_alias(const char *alias)
{
	if (!alias || !alias[0]) {
		return estrdup("");
	}

	size_t len = strlen(alias);

	/* Find the last non-whitespace character */
	while (len > 0 && (alias[len - 1] == ' ' || alias[len - 1] == '\t')) {
		len--;
	}

	char *result = emalloc(len + 1);
	memcpy(result, alias, len);
	result[len] = '\0';

	return result;
}

int _php_fbird_alloc_ht_aliases(fbird_query *fb_query)
{
	ALLOC_HASHTABLE(fb_query->ht_aliases);
	zend_hash_init(fb_query->ht_aliases, fb_query->out_fields_count, NULL, ZVAL_PTR_DTOR, 0);

	/* OO API alias extraction - supports long field names (63 chars in FB 4.0+)
	 *
	 * Prior approach used XSQLVAR.aliasname which is limited to 31 chars.
	 * The OO API's fbm_get_alias() returns the full name, so we read directly
	 * from IMessageMetadata to support Firebird 4.0+ long identifiers.
	 */
	for(size_t i = 0; i < fb_query->out_fields_count; i++){
		const char *base_alias = "";
		const char *base_field = "";

		/* Prefer OO API metadata for full-length names (supports 63+ chars) */
		if (fb_query->out_metadata) {
			const char *alias_str = fbm_get_alias(FBG(master_instance), fb_query->out_metadata, (unsigned)i);
			const char *field_str = fbm_get_field(FBG(master_instance), fb_query->out_metadata, (unsigned)i);

			if (alias_str && alias_str[0]) {
				base_alias = alias_str;
			}
			if (field_str && field_str[0]) {
				base_field = field_str;
			}
		} else if (fb_query->out_sqlda) {
			/* Fallback to XSQLDA (limited to 31 chars) */
			XSQLVAR *var = &fb_query->out_sqlda->sqlvar[i];
			if (var->aliasname[0]) {
				base_alias = var->aliasname;
			}
			if (var->sqlname[0]) {
				base_field = var->sqlname;
			}
		}

		/* Use alias if available, otherwise fall back to field name
		 * Trim trailing whitespace (Issue #23: Firebird 3.0+ pads CHAR-type aliases) */
		const char *raw_alias = (base_alias[0]) ? base_alias : base_field;
		char *effective_alias = _php_fbird_rtrim_alias(raw_alias);

		/* For DML ... RETURNING (or when SQL text contains RETURNING), preserve prefixes when present */
		if ((fb_query->statement_type == isc_info_sql_stmt_insert ||
			 fb_query->statement_type == isc_info_sql_stmt_update ||
			 fb_query->statement_type == isc_info_sql_stmt_delete) ||
			_php_fbird_sql_has_returning(fb_query->query)) {
			char full[METADATALENGTH + 6 + 1] = {0};
			if (_php_fbird_infer_returning_full_alias(fb_query->query, i, full, sizeof(full))) {
				_php_fbird_insert_alias(fb_query->ht_aliases, full);
				efree(effective_alias);
				continue;
			} else {
				char pref[5] = {0};
				if (_php_fbird_infer_returning_prefix(fb_query->query, i, pref, sizeof(pref)) && pref[0] != '\0') {
					char buf[METADATALENGTH + 5 + 1];
					snprintf(buf, sizeof(buf), "%s%s", pref, effective_alias);
					_php_fbird_insert_alias(fb_query->ht_aliases, buf);
					efree(effective_alias);
					continue;
				}
			}
		}

		_php_fbird_insert_alias(fb_query->ht_aliases, effective_alias);
		efree(effective_alias);
	}

	return SUCCESS;
}

void _php_fbird_alloc_ht_ind(fbird_query *fb_query)
{
	ALLOC_HASHTABLE(fb_query->ht_ind);
	zend_hash_init(fb_query->ht_ind, fb_query->out_fields_count, NULL, ZVAL_PTR_DTOR, 0);

	zval t2;
	ZVAL_NULL(&t2);

	for(size_t i = 0; i < fb_query->out_fields_count; i++) {
		zend_hash_index_add(fb_query->ht_ind, i, &t2);
	}
}

/* Parse the RETURNING list and extract a qualifier prefix (OLD./NEW.) for the
 * k-th expression, if present. Returns 1 when detected and writes uppercased
 * qualifier including trailing dot into out; otherwise returns 0. */
static bool _php_fbird_infer_returning_prefix(const char *sql, size_t index, char *out, size_t out_len)
{
    if (!sql || !out || out_len < 5) { /* needs space for "OLD."/"NEW." */
        return 0;
    }

    /* Case-insensitive search for "returning" */
    const char *p = sql;
    const char *ret = NULL;
    while (*p) {
        if (strncasecmp(p, "returning", 9) == 0) { ret = p + 9; break; }
        p++;
    }
    if (!ret) return 0;

    /* Tokenize returning list by commas, trim simple whitespace. */
    size_t i = 0;
    const char *s = ret;
    while (*s && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')) s++;

    const char *tok_start = s;
    for (;;) {
        if (*s == ',' || *s == '\0' || *s == ';') {
            /* token = [tok_start, s) */
            if (i == index) {
                /* Trim trailing whitespace */
                const char *tok_end = s;
                while (tok_end > tok_start && (tok_end[-1] == ' ' || tok_end[-1] == '\t' || tok_end[-1] == '\n' || tok_end[-1] == '\r')) {
                    tok_end--;
                }
                /* Trim leading whitespace */
                while (tok_start < tok_end && (*tok_start == ' ' || *tok_start == '\t' || *tok_start == '\n' || *tok_start == '\r')) tok_start++;

                /* Check for OLD./NEW. prefix (case-insensitive) */
                if ((size_t)(tok_end - tok_start) >= 4) {
                    if (strncasecmp(tok_start, "old.", 4) == 0) {
                        memcpy(out, "OLD.", 4);
                        out[4] = '\0';
                        return 1;
                    } else if (strncasecmp(tok_start, "new.", 4) == 0) {
                        memcpy(out, "NEW.", 4);
                        out[4] = '\0';
                        return 1;
                    }
                }
                return 0;
            }
            i++;
            if (*s == '\0' || *s == ';') break;
            s++; /* skip comma */
            while (*s && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')) s++;
            tok_start = s;
        } else {
            s++;
        }
    }
    return 0;
}

/* Parse the full alias for the k-th expression in RETURNING list when the
 * expression is qualified with OLD./NEW. Returns 1 and writes the alias
 * (e.g., "OLD.I") into out when detected; otherwise returns 0. This is a
 * simple tokenizer aimed at test cases with unquoted identifiers. */
static bool _php_fbird_infer_returning_full_alias(const char *sql, size_t index, char *out, size_t out_len)
{
    if (!sql || !out || out_len < 6) {
        return 0;
    }

    /* Find RETURNING */
    const char *p = sql;
    const char *ret = NULL;
    while (*p) {
        if (strncasecmp(p, "returning", 9) == 0) { ret = p + 9; break; }
        p++;
    }
    if (!ret) return 0;

    size_t i = 0;
    const char *s = ret;
    while (*s && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')) s++;

    const char *tok_start = s;
    for (;;) {
        if (*s == ',' || *s == '\0' || *s == ';') {
            if (i == index) {
                const char *tok_end = s;
                while (tok_end > tok_start && (tok_end[-1] == ' ' || tok_end[-1] == '\t' || tok_end[-1] == '\n' || tok_end[-1] == '\r')) {
                    tok_end--;
                }
                while (tok_start < tok_end && (*tok_start == ' ' || *tok_start == '\t' || *tok_start == '\n' || *tok_start == '\r')) tok_start++;

                /* Expect pattern QUAL.COL where QUAL in {OLD,NEW} */
                const char *dot = memchr(tok_start, '.', tok_end - tok_start);
                if (dot && (dot - tok_start) >= 3) {
                    if (strncasecmp(tok_start, "old", 3) == 0) {
                        size_t rem = (size_t)(tok_end - (dot + 1));
                        if (rem + 4 < out_len) {
                            memcpy(out, "OLD.", 4);
                            memcpy(out + 4, dot + 1, rem);
                            out[4 + rem] = '\0';
                            return 1;
                        }
                    } else if (strncasecmp(tok_start, "new", 3) == 0) {
                        size_t rem = (size_t)(tok_end - (dot + 1));
                        if (rem + 4 < out_len) {
                            memcpy(out, "NEW.", 4);
                            memcpy(out + 4, dot + 1, rem);
                            out[4 + rem] = '\0';
                            return 1;
                        }
                    }
                }
                return 0;
            }
            i++;
            if (*s == '\0' || *s == ';') break;
            s++;
            while (*s && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')) s++;
            tok_start = s;
        } else {
            s++;
        }
    }
    return 0;
}

/* Case-insensitive probe for the word RETURNING in the SQL text. */
static bool _php_fbird_sql_has_returning(const char *sql)
{
    if (!sql) return 0;
    const char *p = sql;
    while (*p) {
        if (strncasecmp(p, "returning", 9) == 0) return 1;
        p++;
    }
    return 0;
}

/* ==========================================================================
 * Procedural Parity: fbird_list_tables, fbird_list_fields, fbird_meta_data (#440)
 *
 * These functions query Firebird system tables (RDB$*) to provide schema
 * introspection. Used by Doctrine DBAL SchemaManager.
 * ========================================================================== */

static void _fbird_meta_exec_and_collect(
	zval *return_value,
	zval *link_zv,
	const char *sql,
	const char *key_field,
	bool use_field_as_key
) {
	zval fn_name, sql_zv, query_ret, row;
	zval args[2];

	/* Call fbird_query($link, $sql) */
	ZVAL_STRING(&fn_name, "fbird_query");
	ZVAL_STRING(&sql_zv, sql);
	args[0] = *link_zv;
	args[1] = sql_zv;
	call_user_function(EG(function_table), NULL, &fn_name, &query_ret, 2, args);
	zval_ptr_dtor(&fn_name);
	zval_ptr_dtor(&sql_zv);

	if (Z_TYPE(query_ret) == IS_FALSE) {
		RETVAL_FALSE;
		return;
	}

	array_init(return_value);

	/* Loop: fbird_fetch_assoc($query) */
	ZVAL_STRING(&fn_name, "fbird_fetch_assoc");
	while (1) {
		call_user_function(EG(function_table), NULL, &fn_name, &row, 1, &query_ret);
		if (Z_TYPE(row) == IS_FALSE || Z_TYPE(row) == IS_NULL) {
			zval_ptr_dtor(&row);
			break;
		}

		zval *field_val = zend_hash_str_find(Z_ARRVAL(row), key_field, strlen(key_field));
		if (field_val) {
			if (use_field_as_key) {
				/* meta_data mode: build sub-array with field properties */
				zval col_info;
				array_init(&col_info);

				zval *ftype = zend_hash_str_find(Z_ARRVAL(row), "FIELD_TYPE", 10);
				zval *flen = zend_hash_str_find(Z_ARRVAL(row), "LENGTH", 6);
				zval *fscale = zend_hash_str_find(Z_ARRVAL(row), "SCALE", 5);
				zval *fnull = zend_hash_str_find(Z_ARRVAL(row), "NULL_FLAG", 9);

				if (ftype) { Z_TRY_ADDREF_P(ftype); add_assoc_zval(&col_info, "type", ftype); }
				if (flen) { Z_TRY_ADDREF_P(flen); add_assoc_zval(&col_info, "length", flen); }
				if (fscale) { Z_TRY_ADDREF_P(fscale); add_assoc_zval(&col_info, "scale", fscale); }
				if (fnull) { Z_TRY_ADDREF_P(fnull); add_assoc_zval(&col_info, "nullable", fnull); }

				add_assoc_zval(return_value, Z_STRVAL_P(field_val), &col_info);
			} else {
				/* list mode: just add value to indexed array */
				Z_TRY_ADDREF_P(field_val);
				add_next_index_zval(return_value, field_val);
			}
		}
		zval_ptr_dtor(&row);
	}

	zval_ptr_dtor(&fn_name);

	/* Free the query resource */
	ZVAL_STRING(&fn_name, "fbird_free_query");
	call_user_function(EG(function_table), NULL, &fn_name, &row, 1, &query_ret);
	zval_ptr_dtor(&fn_name);
	zval_ptr_dtor(&query_ret);
	zval_ptr_dtor(&row);
}

PHP_FUNCTION(fbird_list_tables)
{
	zval *link_arg = NULL;
	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z!", &link_arg) == FAILURE) {
		RETURN_THROWS();
	}

	/* Resolve default link */
	zval default_zv;
	if (link_arg == NULL || Z_TYPE_P(link_arg) == IS_NULL) {
		if (!FBG(default_link)) {
			_php_fbird_module_error("No default connection");
			RETURN_FALSE;
		}
		ZVAL_RES(&default_zv, FBG(default_link));
		link_arg = &default_zv;
	}

	_fbird_meta_exec_and_collect(return_value, link_arg,
		"SELECT TRIM(RDB$RELATION_NAME) AS TBL "
		"FROM RDB$RELATIONS "
		"WHERE RDB$VIEW_BLR IS NULL "
		"AND (RDB$SYSTEM_FLAG = 0 OR RDB$SYSTEM_FLAG IS NULL) "
		"ORDER BY RDB$RELATION_NAME",
		"TBL", false);
}

PHP_FUNCTION(fbird_list_fields)
{
	zval *link_arg;
	char *table_name;
	size_t table_len;
	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "zs", &link_arg, &table_name, &table_len) == FAILURE) {
		RETURN_THROWS();
	}

	/* Firebird stores unquoted identifiers in uppercase */
	char upper_name[256] = {0};
	for (size_t i = 0; i < table_len && i < sizeof(upper_name) - 1; i++) {
		upper_name[i] = toupper((unsigned char)table_name[i]);
	}

	char sql[512];
	snprintf(sql, sizeof(sql),
		"SELECT TRIM(RDB$FIELD_NAME) AS COL "
		"FROM RDB$RELATION_FIELDS "
		"WHERE RDB$RELATION_NAME = '%s' "
		"ORDER BY RDB$FIELD_POSITION",
		upper_name);

	_fbird_meta_exec_and_collect(return_value, link_arg, sql, "COL", false);
}

PHP_FUNCTION(fbird_meta_data)
{
	zval *link_arg;
	char *table_name;
	size_t table_len;
	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "zs", &link_arg, &table_name, &table_len) == FAILURE) {
		RETURN_THROWS();
	}

	/* Firebird stores unquoted identifiers in uppercase */
	char upper_name[256] = {0};
	for (size_t i = 0; i < table_len && i < sizeof(upper_name) - 1; i++) {
		upper_name[i] = toupper((unsigned char)table_name[i]);
	}

	char sql[1024];
	snprintf(sql, sizeof(sql),
		"SELECT "
		"  TRIM(rf.RDB$FIELD_NAME) AS FIELD_NAME, "
		"  TRIM(f.RDB$FIELD_TYPE) AS FIELD_TYPE, "
		"  f.RDB$FIELD_LENGTH AS LENGTH, "
		"  f.RDB$FIELD_SCALE AS SCALE, "
		"  rf.RDB$NULL_FLAG AS NULL_FLAG "
		"FROM RDB$RELATION_FIELDS rf "
		"JOIN RDB$FIELDS f ON rf.RDB$FIELD_SOURCE = f.RDB$FIELD_NAME "
		"WHERE rf.RDB$RELATION_NAME = '%s' "
		"ORDER BY rf.RDB$FIELD_POSITION",
		upper_name);

	_fbird_meta_exec_and_collect(return_value, link_arg, sql, "FIELD_NAME", true);
}

#endif /* HAVE_FIREBIRD */
