/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

#ifndef FBIRD_CLASSES_H
#define FBIRD_CLASSES_H

#include "php.h"
#include "php_fbird_includes.h"

extern zend_class_entry *fbird_connection_ce;
extern zend_class_entry *fbird_transaction_ce;
extern zend_class_entry *fbird_statement_ce;
extern zend_class_entry *fbird_resultset_ce;
extern zend_class_entry *fbird_blob_ce;
extern zend_class_entry *fbird_service_ce;
extern zend_class_entry *fbird_event_ce;
extern zend_class_entry *fbird_batch_ce;

void fbird_register_classes(void);
void fbird_setup_connection_object(zval *return_value, zend_resource *res);
void fbird_setup_event_object(zval *rv, fbird_event *ev);
#if FB_API_VER >= 40
void fbird_setup_batch_object(zval *rv, fbird_batch *batch);
#endif

/**
 * Extract the zend_resource* from a Firebird\Connection object.
 * Returns NULL if obj is not a Firebird\Connection or has no resource.
 */
zend_resource *fbird_connection_get_resource(zend_object *obj);

/**
 * Extract the zend_resource* from a Firebird\Transaction object.
 * Returns NULL if obj is not a Firebird\Transaction or has no resource.
 */
zend_resource *fbird_transaction_get_resource(zend_object *obj);

/**
 * Wrap a le_trans zend_resource* in a Firebird\Transaction object.
 * Stores the resource as a weak reference (EG(regular_list) owns it).
 */
void fbird_setup_transaction_object(zval *return_value, zend_resource *res);

/**
 * Extract the zend_resource* from a Firebird\ResultSet object.
 * Returns NULL if obj is not a Firebird\ResultSet or has no resource.
 */
zend_resource *fbird_resultset_get_resource(zend_object *obj);

/**
 * Wrap a le_query zend_resource* in a Firebird\ResultSet object.
 * Stores the resource as a weak reference (EG(regular_list) owns it).
 */
void fbird_setup_resultset_object(zval *return_value, zend_resource *res);

/**
 * Extract the zend_resource* from a Firebird\Blob object (M3 Phase G bridge).
 * Returns NULL if no resource is set (OOP-native path).
 */
zend_resource *fbird_blob_get_resource(zend_object *obj);

/**
 * Wrap a le_blob zend_resource* in a Firebird\Blob object.
 * Stores the resource as a weak reference (EG(regular_list) owns it).
 */
void fbird_setup_blob_object(zval *return_value, zend_resource *res);

/**
 * Extract the fbird_event* from a Firebird\Event object.
 * Returns NULL if obj is not a Firebird\Event.
 */
fbird_event *fbird_event_get_ptr(zend_object *obj);

#if FB_API_VER >= 40
/**
 * Extract the fbird_batch* from a Firebird\Batch object.
 * Returns NULL if obj is not a Firebird\Batch.
 */
fbird_batch *fbird_batch_get_ptr(zend_object *obj);
#endif

#endif /* FBIRD_CLASSES_H */
