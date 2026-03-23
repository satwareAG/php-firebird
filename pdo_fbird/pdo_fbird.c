/*
 * pdo_fbird — PDO driver for Firebird databases
 * pdo_fbird.c — module init/shutdown, driver registration
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "ext/pdo/php_pdo.h"
#include "ext/pdo/php_pdo_driver.h"
#include "php_pdo_fbird.h"
#include "php_pdo_fbird_int.h"

/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(pdo_fbird)
{
	if (php_pdo_register_driver(&pdo_fbird_driver) == FAILURE) {
		return FAILURE;
	}

	/* Register custom attribute constants into the PDO class */
	zend_class_entry *pdo_ce = php_pdo_get_dbh_ce();
	if (pdo_ce) {
		zend_declare_class_constant_long(pdo_ce, "FBIRD_ATTR_DIALECT",
			sizeof("FBIRD_ATTR_DIALECT") - 1, PDO_FBIRD_ATTR_DIALECT);
		zend_declare_class_constant_long(pdo_ce, "FBIRD_ATTR_CHARSET",
			sizeof("FBIRD_ATTR_CHARSET") - 1, PDO_FBIRD_ATTR_CHARSET);
		zend_declare_class_constant_long(pdo_ce, "FBIRD_ATTR_ROLE",
			sizeof("FBIRD_ATTR_ROLE") - 1, PDO_FBIRD_ATTR_ROLE);
		zend_declare_class_constant_long(pdo_ce, "FBIRD_ATTR_PAGE_BUFFERS",
			sizeof("FBIRD_ATTR_PAGE_BUFFERS") - 1, PDO_FBIRD_ATTR_PAGE_BUFFERS);
		zend_declare_class_constant_long(pdo_ce, "FBIRD_ATTR_TRANSACTION_ISOLATION_LEVEL",
			sizeof("FBIRD_ATTR_TRANSACTION_ISOLATION_LEVEL") - 1, PDO_FBIRD_ATTR_TRANSACTION_ISOLATION_LEVEL);
		zend_declare_class_constant_long(pdo_ce, "FBIRD_ATTR_WRITABLE_TRANSACTION",
			sizeof("FBIRD_ATTR_WRITABLE_TRANSACTION") - 1, PDO_FBIRD_ATTR_WRITABLE_TRANSACTION);

		/* Transaction isolation level values */
		zend_declare_class_constant_long(pdo_ce, "FBIRD_TXN_READ_COMMITTED",
			sizeof("FBIRD_TXN_READ_COMMITTED") - 1, PDO_FBIRD_TXN_READ_COMMITTED);
		zend_declare_class_constant_long(pdo_ce, "FBIRD_TXN_REPEATABLE_READ",
			sizeof("FBIRD_TXN_REPEATABLE_READ") - 1, PDO_FBIRD_TXN_REPEATABLE_READ);
		zend_declare_class_constant_long(pdo_ce, "FBIRD_TXN_SERIALIZABLE",
			sizeof("FBIRD_TXN_SERIALIZABLE") - 1, PDO_FBIRD_TXN_SERIALIZABLE);

		/* Date/time format attributes */
		zend_declare_class_constant_long(pdo_ce, "FBIRD_ATTR_DATE_FORMAT",
			sizeof("FBIRD_ATTR_DATE_FORMAT") - 1, PDO_FBIRD_ATTR_DATE_FORMAT);
		zend_declare_class_constant_long(pdo_ce, "FBIRD_ATTR_TIME_FORMAT",
			sizeof("FBIRD_ATTR_TIME_FORMAT") - 1, PDO_FBIRD_ATTR_TIME_FORMAT);
		zend_declare_class_constant_long(pdo_ce, "FBIRD_ATTR_TIMESTAMP_FORMAT",
			sizeof("FBIRD_ATTR_TIMESTAMP_FORMAT") - 1, PDO_FBIRD_ATTR_TIMESTAMP_FORMAT);

		/* Fetch table names attribute */
		zend_declare_class_constant_long(pdo_ce, "FBIRD_ATTR_FETCH_TABLE_NAMES",
			sizeof("FBIRD_ATTR_FETCH_TABLE_NAMES") - 1, PDO_FBIRD_ATTR_FETCH_TABLE_NAMES);

		/* Bind config attribute (FB 4+ SET BIND) */
		zend_declare_class_constant_long(pdo_ce, "FBIRD_ATTR_SET_BIND",
			sizeof("FBIRD_ATTR_SET_BIND") - 1, PDO_FBIRD_ATTR_SET_BIND);
	}

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION */
PHP_MSHUTDOWN_FUNCTION(pdo_fbird)
{
	php_pdo_unregister_driver(&pdo_fbird_driver);
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION */
PHP_MINFO_FUNCTION(pdo_fbird)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "PDO Driver for Firebird", "enabled");
	php_info_print_table_row(2, "PDO Firebird (fbird:) version", PHP_PDO_FBIRD_VERSION);
	php_info_print_table_end();
}
/* }}} */

/* {{{ pdo_fbird_module_entry */
zend_module_entry pdo_fbird_module_entry = {
	STANDARD_MODULE_HEADER,
	PHP_PDO_FBIRD_EXTNAME,
	NULL,                   /* functions */
	PHP_MINIT(pdo_fbird),
	PHP_MSHUTDOWN(pdo_fbird),
	NULL,                   /* RINIT */
	NULL,                   /* RSHUTDOWN */
	PHP_MINFO(pdo_fbird),
	PHP_PDO_FBIRD_VERSION,
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

/* When built as standalone extension */
#ifdef COMPILE_DL_PDO_FBIRD
ZEND_GET_MODULE(pdo_fbird)
#endif
