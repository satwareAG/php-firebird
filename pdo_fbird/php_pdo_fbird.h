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

/* Service API attributes */
#define PDO_FBIRD_ATTR_SERVICE_ATTACH               1012
#define PDO_FBIRD_ATTR_SERVICE_DETACH               1013
#define PDO_FBIRD_ATTR_SERVICE_BACKUP               1014
#define PDO_FBIRD_ATTR_SERVICE_RESTORE              1015
#define PDO_FBIRD_ATTR_SERVICE_SERVER_VERSION       1016
#define PDO_FBIRD_ATTR_SERVICE_SERVER_INFO          1017
#define PDO_FBIRD_ATTR_SERVICE_DB_STATS             1018
#define PDO_FBIRD_ATTR_SERVICE_ADD_USER             1019
#define PDO_FBIRD_ATTR_SERVICE_MODIFY_USER          1020
#define PDO_FBIRD_ATTR_SERVICE_DELETE_USER          1021

/* Event API attributes */
#define PDO_FBIRD_ATTR_EVENT_NAMES                 1022
#define PDO_FBIRD_ATTR_EVENT_WAIT                  1023
#define PDO_FBIRD_ATTR_EVENT_CANCEL                1024
#define PDO_FBIRD_ATTR_EVENT_COUNT                 1025

/* FB 4.0+ Timeout attributes */
#define PDO_FBIRD_ATTR_STATEMENT_TIMEOUT           1026
#define PDO_FBIRD_ATTR_IDLE_TIMEOUT                1027

/* Transaction isolation level values for PDO_FBIRD_ATTR_TRANSACTION_ISOLATION_LEVEL */
#define PDO_FBIRD_TXN_READ_COMMITTED   1
#define PDO_FBIRD_TXN_REPEATABLE_READ  2
#define PDO_FBIRD_TXN_SERIALIZABLE     3

#endif /* PHP_PDO_FBIRD_H */
