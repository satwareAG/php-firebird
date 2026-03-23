/*
 * pdo_fbird — PDO driver for Firebird databases
 * Uses fbird: DSN prefix to avoid collision with bundled pdo_firebird.
 *
 * php_pdo_fbird.h — module entry and version defines
 */

#ifndef PHP_PDO_FBIRD_H
#define PHP_PDO_FBIRD_H

#include "php.h"

#define PHP_PDO_FBIRD_VERSION "1.0.0"
#define PHP_PDO_FBIRD_EXTNAME "pdo_fbird"

extern zend_module_entry pdo_fbird_module_entry;
#define phpext_pdo_fbird_ptr &pdo_fbird_module_entry

PHP_MINIT_FUNCTION(pdo_fbird);
PHP_MSHUTDOWN_FUNCTION(pdo_fbird);

/* Custom PDO attributes */
#define PDO_FBIRD_ATTR_DIALECT      1001
#define PDO_FBIRD_ATTR_CHARSET      1002
#define PDO_FBIRD_ATTR_ROLE         1003
#define PDO_FBIRD_ATTR_PAGE_BUFFERS 1004
#define PDO_FBIRD_ATTR_TRANSACTION_ISOLATION_LEVEL 1005
#define PDO_FBIRD_ATTR_WRITABLE_TRANSACTION        1006
#define PDO_FBIRD_ATTR_DATE_FORMAT                 1007
#define PDO_FBIRD_ATTR_TIME_FORMAT                 1008
#define PDO_FBIRD_ATTR_TIMESTAMP_FORMAT            1009
#define PDO_FBIRD_ATTR_FETCH_TABLE_NAMES           1010
#define PDO_FBIRD_ATTR_SET_BIND                    1011

/* Transaction isolation level values for PDO_FBIRD_ATTR_TRANSACTION_ISOLATION_LEVEL */
#define PDO_FBIRD_TXN_READ_COMMITTED   1
#define PDO_FBIRD_TXN_REPEATABLE_READ  2
#define PDO_FBIRD_TXN_SERIALIZABLE     3

#endif /* PHP_PDO_FBIRD_H */
