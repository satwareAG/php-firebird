/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

#ifndef PHP_FBIRD_TRANSACTION_H
#define PHP_FBIRD_TRANSACTION_H

/* Resource destructor callback — de-staticised so PHP_MINIT_FUNCTION in
 * firebird.c can pass it to zend_register_list_destructors_ex(). */
void _php_fbird_free_trans(zend_resource *rsrc);

/* Identifier validation — used by firebird.c (fbird_gen_id) */
int is_valid_identifier(const char *s, size_t len);

#endif /* PHP_FBIRD_TRANSACTION_H */
