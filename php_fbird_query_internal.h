/*
   +----------------------------------------------------------------------+
   | PHP Version 8                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) The PHP Group                                          |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
 */

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
/* (helpers if needed by others, currently mostly consumers) */

/* Helper for time conversion */
time_t fbird_timegm_portable(struct tm *tm);
time_t fbird_mktime_with_tz(struct tm *tm, const char *tz);

#endif /* PHP_FBIRD_QUERY_INTERNAL_H */
