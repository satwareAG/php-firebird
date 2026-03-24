/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

#ifndef FBIRD_CLASSES_H
#define FBIRD_CLASSES_H

#include "php.h"

extern zend_class_entry *fbird_connection_ce;

void fbird_register_classes(void);
void fbird_setup_connection_object(zval *return_value, zend_resource *res);

#endif /* FBIRD_CLASSES_H */
