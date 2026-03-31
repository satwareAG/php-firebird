/*
 * pdo_fbird — PDO driver for Firebird databases
 * pdo_fbird.c — module init/shutdown, driver registration
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_PDO_FBIRD

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

		/* Service API attributes */
		zend_declare_class_constant_long(pdo_ce, "FBIRD_ATTR_SERVICE_ATTACH",
			sizeof("FBIRD_ATTR_SERVICE_ATTACH") - 1, PDO_FBIRD_ATTR_SERVICE_ATTACH);
		zend_declare_class_constant_long(pdo_ce, "FBIRD_ATTR_SERVICE_DETACH",
			sizeof("FBIRD_ATTR_SERVICE_DETACH") - 1, PDO_FBIRD_ATTR_SERVICE_DETACH);
		zend_declare_class_constant_long(pdo_ce, "FBIRD_ATTR_SERVICE_BACKUP",
			sizeof("FBIRD_ATTR_SERVICE_BACKUP") - 1, PDO_FBIRD_ATTR_SERVICE_BACKUP);
		zend_declare_class_constant_long(pdo_ce, "FBIRD_ATTR_SERVICE_RESTORE",
			sizeof("FBIRD_ATTR_SERVICE_RESTORE") - 1, PDO_FBIRD_ATTR_SERVICE_RESTORE);
		zend_declare_class_constant_long(pdo_ce, "FBIRD_ATTR_SERVICE_SERVER_VERSION",
			sizeof("FBIRD_ATTR_SERVICE_SERVER_VERSION") - 1, PDO_FBIRD_ATTR_SERVICE_SERVER_VERSION);
		zend_declare_class_constant_long(pdo_ce, "FBIRD_ATTR_SERVICE_SERVER_INFO",
			sizeof("FBIRD_ATTR_SERVICE_SERVER_INFO") - 1, PDO_FBIRD_ATTR_SERVICE_SERVER_INFO);
		zend_declare_class_constant_long(pdo_ce, "FBIRD_ATTR_SERVICE_DB_STATS",
			sizeof("FBIRD_ATTR_SERVICE_DB_STATS") - 1, PDO_FBIRD_ATTR_SERVICE_DB_STATS);
		zend_declare_class_constant_long(pdo_ce, "FBIRD_ATTR_SERVICE_ADD_USER",
			sizeof("FBIRD_ATTR_SERVICE_ADD_USER") - 1, PDO_FBIRD_ATTR_SERVICE_ADD_USER);
		zend_declare_class_constant_long(pdo_ce, "FBIRD_ATTR_SERVICE_MODIFY_USER",
			sizeof("FBIRD_ATTR_SERVICE_MODIFY_USER") - 1, PDO_FBIRD_ATTR_SERVICE_MODIFY_USER);
		zend_declare_class_constant_long(pdo_ce, "FBIRD_ATTR_SERVICE_DELETE_USER",
			sizeof("FBIRD_ATTR_SERVICE_DELETE_USER") - 1, PDO_FBIRD_ATTR_SERVICE_DELETE_USER);

		/* Event API attributes */
		zend_declare_class_constant_long(pdo_ce, "FBIRD_ATTR_EVENT_NAMES",
			sizeof("FBIRD_ATTR_EVENT_NAMES") - 1, PDO_FBIRD_ATTR_EVENT_NAMES);
		zend_declare_class_constant_long(pdo_ce, "FBIRD_ATTR_EVENT_WAIT",
			sizeof("FBIRD_ATTR_EVENT_WAIT") - 1, PDO_FBIRD_ATTR_EVENT_WAIT);
		zend_declare_class_constant_long(pdo_ce, "FBIRD_ATTR_EVENT_CANCEL",
			sizeof("FBIRD_ATTR_EVENT_CANCEL") - 1, PDO_FBIRD_ATTR_EVENT_CANCEL);
		zend_declare_class_constant_long(pdo_ce, "FBIRD_ATTR_EVENT_COUNT",
			sizeof("FBIRD_ATTR_EVENT_COUNT") - 1, PDO_FBIRD_ATTR_EVENT_COUNT);
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

#ifdef HAVE_FIREBIRD
	php_info_print_table_row(2, "Firebird Extension Integration", "integrated");
#else
	php_info_print_table_row(2, "Firebird Extension Integration", "standalone");
#endif

#ifdef FB_API_VER
	php_info_print_table_row(2, "Client Library Version",
#if FB_API_VER >= 50
		"Firebird 5.0 or later"
#elif FB_API_VER >= 40
		"Firebird 4.0"
#else
		"Firebird 3.0"
#endif
	);
#endif

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

#endif /* HAVE_PDO_FBIRD */
