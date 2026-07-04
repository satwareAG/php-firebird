/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

#ifndef PHP_FBIRD_QUERY_BIND_H
#define PHP_FBIRD_QUERY_BIND_H

#include "php.h"
#include "php_fbird_includes.h"

/* Parameter binding functions */

/**
 * Bind PHP values to query input SQLDA.
 *
 * @param fb_query The query structure containing SQLDA and bind buffers.
 * @param b_vars Array of zvals to bind.
 * @return SUCCESS on success, FAILURE on error.
 */
int _php_fbird_bind(fbird_query *fb_query, zval *b_vars);
int _php_fbird_xsqlda_to_msg_buffer(fbird_query *fb_query);

/**
 * Safely copy data from source SQLVAR to destination SQLVAR with bounds checking.
 *
 * This function validates input, checks type consistency, and allocates
 * memory for the copied data with comprehensive error handling.
 *
 * @param dest_var Destination SQLVAR.
 * @param src_var Source SQLVAR.
 * @param field_index Field index for error messages.
 * @param query_context Query string for error messages (may be NULL).
 * @return SUCCESS on success, FAILURE on error.
 */
int _php_fbird_safe_copy_sqlvar_data(XSQLVAR *dest_var, const XSQLVAR *src_var,
    int field_index, const char *query_context);

#endif /* PHP_FBIRD_QUERY_BIND_H */
