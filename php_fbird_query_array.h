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
