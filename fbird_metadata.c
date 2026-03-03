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

#define ISC_LONG_MIN    INT_MIN
#define ISC_LONG_MAX    INT_MAX

/* Forward declarations */
static zend_bool _php_fbird_infer_returning_prefix(const char *sql, size_t index, char *out, size_t out_len);
static zend_bool _php_fbird_infer_returning_full_alias(const char *sql, size_t index, char *out, size_t out_len);
static zend_bool _php_fbird_returning_token_alias(const char *sql, size_t index, char *out, size_t out_len);
static zend_bool _php_fbird_sql_has_returning(const char *sql);

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
	zend_hash_str_add_new(ht, alias, alias_len, &t2);
}

void _php_fbird_field_info(zval *return_value, fbird_query *ib_query, int is_outvar, int num)
{
	unsigned short len;
	char buf[16], *s = buf;
	XSQLDA *sqlda;
	XSQLVAR *var;

 if(is_outvar){
        sqlda = ib_query->out_sqlda;
        if (sqlda == NULL) {
            _php_fbird_module_error("Trying to get field info from a non-select query"); /* LCOV_EXCL_LINE */
            RETURN_FALSE; /* LCOV_EXCL_LINE */
        }
    } else {
        sqlda = ib_query->in_sqlda;
        /* For parameter metadata, return false quietly when not available */
        if (sqlda == NULL) {
            RETURN_FALSE; /* LCOV_EXCL_LINE */
        }
    }

	var = sqlda->sqlvar;

 if (!var || num < 0 || num >= sqlda->sqld) {
        /* For parameters, do not emit a warning on out-of-range; return false quietly */
        if (is_outvar) {
            _php_fbird_module_error("Field %d does not exist (valid range: 0-%d)", num, sqlda ? sqlda->sqld - 1 : -1); /* LCOV_EXCL_LINE */
        }
        RETURN_FALSE; /* LCOV_EXCL_LINE */
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
	 * because it requires ib_query->stmt.stmt (legacy isc_stmt_handle) which is no longer
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
	fbird_query *ib_query;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "rl", &result_arg, &field_arg) == FAILURE) {
		return;
	}

	/* Validate first argument is a query resource with proper error messages */
	FBIRD_VALIDATE_QUERY_EX(result_arg, 1, ib_query);
	if (!ib_query) {
		RETURN_FALSE; /* LCOV_EXCL_LINE */
	}

	_php_fbird_field_info(return_value, ib_query, 1, (ISC_SHORT)field_arg);
}

PHP_FUNCTION(fbird_num_params)
{
	zval *result;
	fbird_query *ib_query;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &result) == FAILURE) {
		return;
	}

	/* Validate first argument is a query resource with proper error messages */
	FBIRD_VALIDATE_QUERY_EX(result, 1, ib_query);
	if (!ib_query) {
		RETURN_FALSE; /* LCOV_EXCL_LINE */
	}

	/*
	 * Firebird 3.0+ OO API - use cached parameter count from IMessageMetadata
	 * Set during query preparation via fbs_get_input_count()
	 */
	RETURN_LONG(ib_query->in_fields_count);
}

PHP_FUNCTION(fbird_param_info)
{
	zval *result_arg;
	zend_long field_arg;
	fbird_query *ib_query;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "rl", &result_arg, &field_arg) == FAILURE) {
		return;
	}

	/* Validate first argument is a query resource with proper error messages */
	FBIRD_VALIDATE_QUERY_EX(result_arg, 1, ib_query);
	if (!ib_query) {
		RETURN_FALSE; /* LCOV_EXCL_LINE */
	}

	_php_fbird_field_info(return_value, ib_query, 0, field_arg);
}

PHP_FUNCTION(fbird_num_fields)
{
	zval *result;
	fbird_query *ib_query;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &result) == FAILURE) {
		return;
	}

	/* Validate first argument is a query resource with proper error messages */
	FBIRD_VALIDATE_QUERY_EX(result, 1, ib_query);
	if (!ib_query) {
		RETURN_FALSE; /* LCOV_EXCL_LINE */
	}

	/*
	 * Firebird 3.0+ OO API - use cached field count from IMessageMetadata
	 * Set during query preparation via fbs_get_output_count()
	 */
	RETURN_LONG(ib_query->out_fields_count);
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

int _php_fbird_alloc_ht_aliases(fbird_query *ib_query)
{
	ALLOC_HASHTABLE(ib_query->ht_aliases);
	zend_hash_init(ib_query->ht_aliases, ib_query->out_fields_count, NULL, ZVAL_PTR_DTOR, 0);

	/* OO API alias extraction - supports long field names (63 chars in FB 4.0+)
	 *
	 * Prior approach used XSQLVAR.aliasname which is limited to 31 chars.
	 * The OO API's fbm_get_alias() returns the full name, so we read directly
	 * from IMessageMetadata to support Firebird 4.0+ long identifiers.
	 */
	for(size_t i = 0; i < ib_query->out_fields_count; i++){
		const char *base_alias = "";
		const char *base_field = "";

		/* Prefer OO API metadata for full-length names (supports 63+ chars) */
		if (ib_query->out_metadata) {
			const char *alias_str = fbm_get_alias(IBG(master_instance), ib_query->out_metadata, (unsigned)i);
			const char *field_str = fbm_get_field(IBG(master_instance), ib_query->out_metadata, (unsigned)i);

			if (alias_str && alias_str[0]) {
				base_alias = alias_str;
			}
			if (field_str && field_str[0]) {
				base_field = field_str;
			}
		} else if (ib_query->out_sqlda) {
			/* Fallback to XSQLDA (limited to 31 chars) */
			XSQLVAR *var = &ib_query->out_sqlda->sqlvar[i];
			if (var->aliasname && var->aliasname[0]) {
				base_alias = var->aliasname;
			}
			if (var->sqlname && var->sqlname[0]) {
				base_field = var->sqlname;
			}
		}

		/* Use alias if available, otherwise fall back to field name
		 * Trim trailing whitespace (Issue #23: Firebird 3.0+ pads CHAR-type aliases) */
		const char *raw_alias = (base_alias[0]) ? base_alias : base_field;
		char *effective_alias = _php_fbird_rtrim_alias(raw_alias);

		/* For DML ... RETURNING (or when SQL text contains RETURNING), preserve prefixes when present */
		if ((ib_query->statement_type == isc_info_sql_stmt_insert ||
			 ib_query->statement_type == isc_info_sql_stmt_update ||
			 ib_query->statement_type == isc_info_sql_stmt_delete) ||
			_php_fbird_sql_has_returning(ib_query->query)) {
			char full[METADATALENGTH + 6 + 1] = {0};
			if (_php_fbird_infer_returning_full_alias(ib_query->query, i, full, sizeof(full))) {
				_php_fbird_insert_alias(ib_query->ht_aliases, full);
				efree(effective_alias);
				continue;
			} else {
				char pref[5] = {0};
				if (_php_fbird_infer_returning_prefix(ib_query->query, i, pref, sizeof(pref)) && pref[0] != '\0') {
					char buf[METADATALENGTH + 5 + 1];
					snprintf(buf, sizeof(buf), "%s%s", pref, effective_alias);
					_php_fbird_insert_alias(ib_query->ht_aliases, buf);
					efree(effective_alias);
					continue;
				}
			}
		}

		_php_fbird_insert_alias(ib_query->ht_aliases, effective_alias);
		efree(effective_alias);
	}

	return SUCCESS;
}

void _php_fbird_alloc_ht_ind(fbird_query *ib_query)
{
	ALLOC_HASHTABLE(ib_query->ht_ind);
	zend_hash_init(ib_query->ht_ind, ib_query->out_fields_count, NULL, ZVAL_PTR_DTOR, 0);

	zval t2;
	ZVAL_NULL(&t2);

	for(size_t i = 0; i < ib_query->out_fields_count; i++) {
		zend_hash_index_add(ib_query->ht_ind, i, &t2);
	}
}

/* Parse the RETURNING list and extract a qualifier prefix (OLD./NEW.) for the
 * k-th expression, if present. Returns 1 when detected and writes uppercased
 * qualifier including trailing dot into out; otherwise returns 0. */
static zend_bool _php_fbird_infer_returning_prefix(const char *sql, size_t index, char *out, size_t out_len)
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
static zend_bool _php_fbird_infer_returning_full_alias(const char *sql, size_t index, char *out, size_t out_len)
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

/* Extract the raw token for the k-th RETURNING expression. If the token
 * contains a qualifier (e.g., OLD.I or NEW.C), write it as-is (uppercased
 * qualifier plus original column part) into out and return 1. If token has
 * no qualifier, return 0 so caller can fallback to base alias. */
static zend_bool _php_fbird_returning_token_alias(const char *sql, size_t index, char *out, size_t out_len)
{
    if (!sql || !out || out_len < 6) return 0;

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
                while (tok_end > tok_start && (tok_end[-1] == ' ' || tok_end[-1] == '\t' || tok_end[-1] == '\n' || tok_end[-1] == '\r')) tok_end--;
                while (tok_start < tok_end && (*tok_start == ' ' || *tok_start == '\t' || *tok_start == '\n' || *tok_start == '\r')) tok_start++;

                const char *dot = memchr(tok_start, '.', tok_end - tok_start);
                if (!dot) return 0;

                /* Copy qualifier uppercased + '.' + rest as-is */
                size_t qual_len = (size_t)(dot - tok_start);
                size_t rest_len = (size_t)(tok_end - (dot + 1));
                if (qual_len < 3 || (qual_len + 1 + rest_len + 1) > out_len) return 0;

                /* Qualifier */
                for (size_t k = 0; k < qual_len; k++) {
                    out[k] = (char) toupper((unsigned char) tok_start[k]);
                }
                out[qual_len] = '.';
                /* Column part */
                memcpy(out + qual_len + 1, dot + 1, rest_len);
                out[qual_len + 1 + rest_len] = '\0';
                return 1;
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
static zend_bool _php_fbird_sql_has_returning(const char *sql)
{
    if (!sql) return 0;
    const char *p = sql;
    while (*p) {
        if (strncasecmp(p, "returning", 9) == 0) return 1;
        p++;
    }
    return 0;
}

#endif /* HAVE_FIREBIRD */
