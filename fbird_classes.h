#ifndef FBIRD_CLASSES_H
#define FBIRD_CLASSES_H

#include "php.h"

extern zend_class_entry *fbird_connection_ce;
extern zend_class_entry *fbird_transaction_ce;
extern zend_class_entry *fbird_statement_ce;
extern zend_class_entry *fbird_resultset_ce;
extern zend_class_entry *fbird_connection_exception_ce;
extern zend_class_entry *fbird_query_exception_ce;
extern zend_class_entry *fbird_service_exception_ce;

void fbird_register_classes(void);

#endif /* FBIRD_CLASSES_H */
