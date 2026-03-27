/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

#ifndef FBIRD_CLASSES_H
#define FBIRD_CLASSES_H

#include "php.h"

extern zend_class_entry *fbird_connection_ce;

void fbird_register_classes(void);
void fbird_setup_connection_object(zval *return_value, zend_resource *res);

/**
 * Extract the zend_resource* from a Firebird\Connection object.
 * Returns NULL if obj is not a Firebird\Connection or has no resource.
 */
zend_resource *fbird_connection_get_resource(zend_object *obj);

#endif /* FBIRD_CLASSES_H */
