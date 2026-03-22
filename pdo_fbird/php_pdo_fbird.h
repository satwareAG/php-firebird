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

/* Custom PDO attributes */
#define PDO_FBIRD_ATTR_DIALECT      1001
#define PDO_FBIRD_ATTR_CHARSET      1002
#define PDO_FBIRD_ATTR_ROLE         1003
#define PDO_FBIRD_ATTR_PAGE_BUFFERS 1004

#endif /* PHP_PDO_FBIRD_H */
