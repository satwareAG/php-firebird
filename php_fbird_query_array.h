/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

#ifndef PHP_FBIRD_QUERY_ARRAY_H
#define PHP_FBIRD_QUERY_ARRAY_H

#include "php.h"
#include "php_fbird_includes.h"

/* Array handling functions */

/**
 * Allocate array descriptors for an SQLDA.
 *
 * @param ib_arrayp Output pointer to array of fbird_array structures.
 * @param sqlda The SQLDA structure to scan for array fields.
 * @param link Database connection handle.
 * @param trans Transaction handle.
 * @param array_cnt Output: number of arrays allocated.
 * @return SUCCESS on success, FAILURE on error.
 */
int _php_fbird_alloc_array(fbird_array **ib_arrayp, XSQLDA *sqlda,
    fb_safe_handle link, fb_safe_handle trans, unsigned short *array_cnt);

/**
 * Bind PHP array values to Firebird array format recursively.
 *
 * @param val PHP zval containing array data.
 * @param buf Buffer to write array data to.
 * @param buf_size Size of the buffer.
 * @param array Array descriptor.
 * @param dim Current dimension (0 for top level).
 * @return SUCCESS on success, FAILURE on error.
 */
int _php_fbird_bind_array(zval *val, char *buf, zend_ulong buf_size,
    fbird_array *array, int dim);

#endif /* PHP_FBIRD_QUERY_ARRAY_H */
