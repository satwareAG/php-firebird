/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

#ifndef PHP_FBIRD_BATCH_H
#define PHP_FBIRD_BATCH_H

#include "php_fbird_includes.h"

#define LE_BATCH "Firebird batch"

#if FB_API_VER >= 40
/**
 * Batch operation wrapper for Firebird 4.0+ IBatch interface.
 * Provides high-performance bulk INSERT operations.
 */
typedef struct {
    void *fbbatch_wrapper;    /* OO API batch wrapper (from fbbatch_create()) */
    fbird_transaction *trans; /* Associated transaction */
    fbird_query *query;       /* Parent prepared statement */
    void *in_metadata;        /* IMessageMetadata for input parameters */
    void *in_msg_buffer;      /* Message buffer for row data */
    unsigned in_msg_length;   /* Message buffer size */
} fbird_batch;

/* Resource destructor callback — de-staticised so PHP_MINIT_FUNCTION in
 * firebird.c can pass it to zend_register_list_destructors_ex(). */
void _php_fbird_free_batch(zend_resource *rsrc);
#endif /* FB_API_VER >= 40 */

#endif /* PHP_FBIRD_BATCH_H */
