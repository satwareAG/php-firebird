/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

#ifndef PHP_FBIRD_CONNECTION_H
#define PHP_FBIRD_CONNECTION_H

#include "php_fbird_includes.h"

/* Resource destructor callbacks — de-staticised so PHP_MINIT_FUNCTION in
 * firebird.c can pass them to zend_register_list_destructors_ex(). */
void _php_fbird_commit_link(fbird_db_link *link);
void php_fbird_commit_link_rsrc(zend_resource *rsrc);
void _php_fbird_close_link(zend_resource *rsrc);
void _php_fbird_close_plink(zend_resource *rsrc);

#endif /* PHP_FBIRD_CONNECTION_H */
