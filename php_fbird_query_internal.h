/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

#ifndef PHP_FBIRD_QUERY_INTERNAL_H
#define PHP_FBIRD_QUERY_INTERNAL_H

#include "php.h"
#include "php_fbird_includes.h"

/* Resource type for queries */
extern int le_query;

/* Internal function declarations for cross-file usage */

/* From fbird_query_exec.c */
int _php_fbird_fetch_query_res(zval *from, fbird_query **ib_query);
void _php_fbird_alloc_xsqlda_vars(XSQLDA *sqlda, ISC_SHORT *nullinds);
void _php_fbird_free_query_impl(INTERNAL_FUNCTION_PARAMETERS, int as_result);
int _php_fbird_safe_copy_sqlvar_data(XSQLVAR *dest_var, const XSQLVAR *src_var, int field_index, const char *query_context);

/* From fbird_metadata.c */
int _php_fbird_alloc_ht_aliases(fbird_query *ib_query);
void _php_fbird_alloc_ht_ind(fbird_query *ib_query);
void _php_fbird_field_info(zval *return_value, fbird_query *ib_query, int is_outvar, int num);

/* From fbird_result.c */
/* Fetch type flags for _php_fbird_fetch_hash_query() — mirror FETCH_ROW/FETCH_ARRAY in fbird_result.c */
#ifndef FBIRD_FETCH_ROW
#  define FBIRD_FETCH_ROW   1
#  define FBIRD_FETCH_ASSOC 2
#endif

/* Core fetch logic — callable from OOP layer without PHP string dispatch */
void _php_fbird_fetch_hash_query(
	fbird_query *ib_query,
	int fetch_type,
	zend_long flag,
	zval *return_value);

/* Helper for time conversion */
time_t fbird_timegm_portable(struct tm *tm);
time_t fbird_mktime_with_tz(struct tm *tm, const char *tz);

#endif /* PHP_FBIRD_QUERY_INTERNAL_H */
