/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

#ifndef PHP_FBIRD_QUERY_PREPARE_H
#define PHP_FBIRD_QUERY_PREPARE_H

#include "php.h"
#include "php_fbird_includes.h"

/* Max identifier size for Firebird 3+ (63 chars) but we alloc more for safety */
#define MAX_IDENTIFIER_LEN 255

/* Query preparation and cleanup functions */

/**
 * Get statement type and field counts for a prepared query.
 *
 * @param fb_query The query structure to populate with metadata.
 * @return SUCCESS on success, FAILURE on error.
 */
int _php_fbird_set_query_info(fbird_query *fb_query);

/**
 * Allocate and prepare a query.
 *
 * @param new_query Output pointer to the allocated query structure.
 * @param link Database link.
 * @param trans Transaction.
 * @param trans_res Transaction resource (may be NULL).
 * @param query SQL query string.
 * @return SUCCESS on success, FAILURE on error.
 */
int _php_fbird_prepare(fbird_query **new_query, fbird_db_link *link,
    fbird_transaction *trans, zend_resource *trans_res, char *query);

/**
 * Allocate XSQLDA variable buffers.
 *
 * @param sqlda The XSQLDA structure.
 * @param nullinds Null indicator array.
 */
void _php_fbird_alloc_xsqlda_vars(XSQLDA *sqlda, ISC_SHORT *nullinds);

/**
 * Free XSQLDA structure and its variable buffers.
 *
 * @param sqlda The XSQLDA structure to free.
 */
void _php_fbird_free_xsqlda(XSQLDA *sqlda);

/**
 * Free query structure and all associated resources.
 *
 * @param fb_query The query structure to free.
 */
void _php_fbird_free_query(fbird_query *fb_query);

/**
 * Resource destructor for query resources.
 *
 * @param rsrc The resource to destroy.
 */
void php_fbird_free_query_rsrc(zend_resource *rsrc);

/**
 * Module initialization for query resource type.
 *
 * @param type Module type.
 * @param module_number Module number.
 */
void php_fbird_query_minit(INIT_FUNC_ARGS);

#endif /* PHP_FBIRD_QUERY_PREPARE_H */
