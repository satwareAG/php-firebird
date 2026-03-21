/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

#ifndef PHP_FBIRD_QUERY_ARRAY_H
#define PHP_FBIRD_QUERY_ARRAY_H

#include "php.h"
#include "php_fbird_includes.h"

/* Array handling functions */

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
