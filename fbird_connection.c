/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

#include "config.h"
#include "php.h"
#include "php_firebird.h"
#include "php_fbird_includes.h"
#include "fbird_classes.h"
#include "src/cpp/fb_bridge.h"
#include "ext/standard/md5.h"

static int le_link, le_plink, le_index_ptr;

int _php_fbird_attach_db(char **args, size_t *len, zend_long *largs, void **db)
{
    void* connection = fbc_connect(
        IBG(master_instance),
        args[0], len[0], args[1], len[1], args[2], len[2],
        args[3], len[3], args[4], len[4], (int)largs[0], (int)largs[1], (int)largs[2], IBG(status)
    );
    if (!connection) { _php_fbird_error(); return FAILURE; }
    *db = connection;
    return SUCCESS;
}

void _php_fbird_connect(INTERNAL_FUNCTION_PARAMETERS, int persistent)
{
	char *args[] = { NULL, NULL, NULL, NULL, NULL };
	size_t len[] = { 0, 0, 0, 0, 0 };
	zend_long largs[] = { 0, 0, 0 };
	char hash[16];
	int i;
	PHP_MD5_CTX hash_context;
	void *db_handle = NULL;
	fbird_db_link *ib_link;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS(), "|ssssllls",
			&args[0], &len[0], &args[1], &len[1], &args[2], &len[2],
			&args[3], &len[3], &largs[0], &largs[1], &largs[2], &args[4], &len[4])) {
		return;
	}

	PHP_MD5Init(&hash_context);
	for (i=0; i<5; i++) if (args[i]) PHP_MD5Update(&hash_context, args[i], len[i]);
	PHP_MD5Final((unsigned char*)hash, &hash_context);

	if (FAILURE == _php_fbird_attach_db(args, len, largs, &db_handle)) RETURN_FALSE;

	ib_link = emalloc(sizeof(fbird_db_link));
	ib_link->fbc_connection = (fbc_connection_t*)db_handle;
	memcpy(ib_link->hash_key, hash, 16);
	ib_link->dialect = (unsigned short)largs[1];
	ib_link->tr_list = NULL;
	ib_link->event_head = NULL;

	zend_resource *res = zend_register_resource(ib_link, persistent ? le_plink : le_link);
	fbird_setup_connection_object(return_value, res);
}

PHP_FUNCTION(fbird_connect) { _php_fbird_connect(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0); }
PHP_FUNCTION(fbird_pconnect) { _php_fbird_connect(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1); }

void php_fbird_connection_minit(int link, int plink, int index_ptr) {
	le_link = link;
	le_plink = plink;
	le_index_ptr = index_ptr;
}
