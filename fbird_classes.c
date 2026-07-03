/*
 * fbird_classes.c - Layer 2 OOP class registry
 *
 * This file registers all Firebird\* OOP classes. Per-class implementation
 * is in fbird_class_*.c files. Struct types and accessors are in
 * fbird_class_internal.h.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "zend_exceptions.h"
#include "php_firebird.h"
#include "php_fbird_includes.h"
#include "firebird_utils.h"
#include "fbird_class_internal.h"

zend_class_entry *fbird_connection_exception_ce;
zend_class_entry *fbird_query_exception_ce;
zend_class_entry *fbird_service_exception_ce;

zend_class_entry *fbird_connection_ce;
zend_class_entry *fbird_transaction_ce;
zend_class_entry *fbird_statement_ce;
zend_class_entry *fbird_resultset_ce;
zend_class_entry *fbird_blob_ce;
zend_class_entry *fbird_service_ce;
zend_class_entry *fbird_event_ce;
zend_class_entry *fbird_batch_ce;

void fbird_register_classes(void)
{
	zend_class_entry ce;

	INIT_CLASS_ENTRY(ce, "Firebird\\ConnectionException", NULL);
	fbird_connection_exception_ce = zend_register_internal_class_ex(&ce, firebird_exception_ce);

	INIT_CLASS_ENTRY(ce, "Firebird\\QueryException", NULL);
	fbird_query_exception_ce = zend_register_internal_class_ex(&ce, firebird_exception_ce);

	INIT_CLASS_ENTRY(ce, "Firebird\\ServiceException", NULL);
	fbird_service_exception_ce = zend_register_internal_class_ex(&ce, firebird_exception_ce);

	INIT_CLASS_ENTRY(ce, "Firebird\\Connection", fbird_connection_methods);
	fbird_connection_ce = zend_register_internal_class(&ce);
	fbird_connection_ce->create_object = fbird_connection_create;
	memcpy(&fbird_connection_handlers, zend_get_std_object_handlers(),
		sizeof(zend_object_handlers));
	fbird_connection_handlers.offset    = XtOffsetOf(fbird_connection_obj, std);
	fbird_connection_handlers.free_obj  = fbird_connection_free;

	INIT_CLASS_ENTRY(ce, "Firebird\\Transaction", fbird_transaction_methods);
	fbird_transaction_ce = zend_register_internal_class(&ce);
	fbird_transaction_ce->create_object = fbird_transaction_create;
	memcpy(&fbird_transaction_handlers, zend_get_std_object_handlers(),
		sizeof(zend_object_handlers));
	fbird_transaction_handlers.offset    = XtOffsetOf(fbird_transaction_obj, std);
	fbird_transaction_handlers.free_obj  = fbird_transaction_free;

	INIT_CLASS_ENTRY(ce, "Firebird\\Statement", fbird_statement_methods);
	fbird_statement_ce = zend_register_internal_class(&ce);
	fbird_statement_ce->create_object = fbird_statement_create;
	memcpy(&fbird_statement_handlers, zend_get_std_object_handlers(),
		sizeof(zend_object_handlers));
	fbird_statement_handlers.offset    = XtOffsetOf(fbird_statement_obj, std);
	fbird_statement_handlers.free_obj  = fbird_statement_free;

	INIT_CLASS_ENTRY(ce, "Firebird\\ResultSet", fbird_resultset_methods);
	fbird_resultset_ce = zend_register_internal_class(&ce);
	fbird_resultset_ce->create_object = fbird_resultset_create;
	memcpy(&fbird_resultset_handlers, zend_get_std_object_handlers(),
		sizeof(zend_object_handlers));
	fbird_resultset_handlers.offset    = XtOffsetOf(fbird_resultset_obj, std);
	fbird_resultset_handlers.free_obj  = fbird_resultset_free;

	INIT_CLASS_ENTRY(ce, "Firebird\\Blob", fbird_blob_methods);
	fbird_blob_ce = zend_register_internal_class(&ce);
	fbird_blob_ce->create_object = fbird_blob_create_obj;
	memcpy(&fbird_blob_handlers, zend_get_std_object_handlers(),
		sizeof(zend_object_handlers));
	fbird_blob_handlers.offset    = XtOffsetOf(fbird_blob_obj, std);
	fbird_blob_handlers.free_obj  = fbird_blob_free_obj;

	INIT_CLASS_ENTRY(ce, "Firebird\\Service", fbird_service_methods);
	fbird_service_ce = zend_register_internal_class(&ce);
	fbird_service_ce->create_object = fbird_service_create_obj;
	memcpy(&fbird_service_handlers, zend_get_std_object_handlers(),
		sizeof(zend_object_handlers));
	fbird_service_handlers.offset    = XtOffsetOf(fbird_service, std);
	fbird_service_handlers.free_obj  = fbird_service_free_obj;

	INIT_CLASS_ENTRY(ce, "Firebird\\Event", fbird_event_methods);
	fbird_event_ce = zend_register_internal_class(&ce);
	fbird_event_ce->create_object = fbird_event_create_obj;
	memcpy(&fbird_event_handlers, zend_get_std_object_handlers(),
		sizeof(zend_object_handlers));
	fbird_event_handlers.offset   = XtOffsetOf(fbird_event_obj, std);
	fbird_event_handlers.free_obj = fbird_event_free_obj;

#if FB_API_VER >= 40
	INIT_CLASS_ENTRY(ce, "Firebird\\BatchHandle", NULL);
	fbird_batch_ce = zend_register_internal_class(&ce);
	fbird_batch_ce->create_object = fbird_batch_create_obj;
	memcpy(&fbird_batch_handlers, zend_get_std_object_handlers(),
		sizeof(zend_object_handlers));
	fbird_batch_handlers.offset   = XtOffsetOf(fbird_batch_obj, std);
	fbird_batch_handlers.free_obj = fbird_batch_free_obj;
#else
	fbird_batch_ce = NULL;
#endif
}
