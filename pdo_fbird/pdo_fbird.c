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

#ifdef COMPILE_DL_PDO_FBIRD
ZEND_GET_MODULE(pdo_fbird)
#endif
