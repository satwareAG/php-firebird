/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"

#if HAVE_FIREBIRD

#include "php_ini.h"
#include "ext/standard/md5.h"
#include "php_firebird.h"
#include "php_fbird_includes.h"
#include "php_fbird_connection.h"
#include "firebird_utils.h"

/* ISC_TEB is defined in php_fbird_includes.h */

/* Fill ib_link and trans with the correct database link and transaction. */
void _php_fbird_get_link_trans(INTERNAL_FUNCTION_PARAMETERS,
	zval *link_id, fbird_db_link **ib_link, fbird_transaction **trans)
{
	FBDEBUG("Transaction or database link?");
	if (Z_RES_P(link_id)->type == le_trans) {
		/* Transaction resource: make sure it refers to one link only, then
		   fetch it; database link is stored in ib_trans->db_link[]. */
		FBDEBUG("Type is le_trans");
		*trans = (fbird_transaction *)zend_fetch_resource_ex(link_id, LE_TRANS, le_trans);
		if ((*trans)->link_cnt > 1) {
			_php_fbird_module_error("Link id is ambiguous: transaction spans multiple connections."
				);
			return;
		}
		*ib_link = (*trans)->db_link[0];
		return;
	}
	FBDEBUG("Type is le_[p]link or id not found");
	/* Database link resource, use default transaction. */
	*trans = NULL;
	*ib_link = (fbird_db_link *)zend_fetch_resource2_ex(link_id, LE_LINK, le_link, le_plink);
}

/* destructors ---------------------- */

void _php_fbird_commit_link(fbird_db_link *link)
{
	unsigned short i = 0, j;
	fbird_tr_list *l;
	fbird_event *e;
	FBDEBUG("Checking transactions to close...");

	for (l = link->tr_list; l != NULL; ++i) {
		fbird_tr_list *p = l;
		if (p->trans != 0) {
			if (i == 0) {
				/* Default transaction: commit via OO API */
				if (p->trans->fbt_transaction != NULL) {
					FBDEBUG("Committing default transaction via OO API...");
					int res = fbt_commit(p->trans->fbt_transaction, IB_STATUS);
					fbt_free(p->trans->fbt_transaction);
					p->trans->fbt_transaction = NULL;
					if (res) {
						_php_fbird_error();
					}
				}
				efree(p->trans); /* default transaction is not a registered resource: clean up */
			} else {
				/* Non-default transaction: rollback via OO API */
				if (p->trans->fbt_transaction != NULL) {
					FBDEBUG("Rolling back other transaction via OO API...");
					int res = fbt_rollback(p->trans->fbt_transaction, IB_STATUS);
					fbt_free(p->trans->fbt_transaction);
					p->trans->fbt_transaction = NULL;
					if (res) {
						_php_fbird_error();
					}
				}
				/* set this link pointer to NULL in the transaction */
				for (j = 0; j < p->trans->link_cnt; ++j) {
					if (p->trans->db_link[j] == link) {
						p->trans->db_link[j] = NULL;
						break;
					}
				}
			}
		}
		l = l->next;
		efree(p);
	}
	link->tr_list = NULL;

	/*
	 * IMPORTANT: _php_fbird_free_event() unlinks and efree()s the current node.
	 * Never follow e->event_next after freeing e.
	 */
	for (e = link->event_head; e; ) {
		fbird_event *next = e->event_next;
		_php_fbird_free_event(e);
		/* e is freed inside _php_fbird_free_event(); only use cached next */
		e = next;
	}
}


void php_fbird_commit_link_rsrc(zend_resource *rsrc)
{
	fbird_db_link *link = (fbird_db_link *) rsrc->ptr;

	_php_fbird_commit_link(link);
}

void _php_fbird_close_link(zend_resource *rsrc)
{
	fbird_db_link *link = (fbird_db_link *) rsrc->ptr;

	/* NULL pointer guard (Issue #55): In forked PHPStan workers, rsrc->ptr may be NULL
	 * when inherited resource descriptors are destroyed during child process shutdown.
	 * Accessing link->created_pid with NULL pointer causes SIGSEGV at si_addr=0x4. */
	if (link == NULL) {
		return;
	}

#ifndef PHP_WIN32
	/* Fork-safety check (Issue #22, #36): Skip cleanup if we're in a forked child.
	 * After pcntl_fork(), child inherits global state including master_instance
	 * and connection handles. Attempting to close handles in child that were
	 * created in parent causes segfault. Only the original process should
	 * perform cleanup operations.
	 *
	 * Two-level check:
	 * 1. Global init_pid - module-level fork detection
	 * 2. Per-connection created_pid - connection-level fork detection */
	pid_t current_pid = getpid();
	if (IBG(init_pid) != 0 && current_pid != IBG(init_pid)) {
		FBDEBUG("Skipping link cleanup in forked child process (global)");
		IBG(num_links)--;
		efree(link);
		return;
	}
	if (link->created_pid != 0 && current_pid != link->created_pid) {
		FBDEBUG("Skipping link cleanup in forked child process (per-connection)");
		IBG(num_links)--;
		efree(link);
		return;
	}
#endif

	/* Remove cache entry from EG(regular_list) to prevent UAF (Issue #35).
	 *
	 * CRITICAL: Skip EG() access during MSHUTDOWN (Issue #50, #51, #55).
	 * During module shutdown, EG(regular_list) may already be destroyed. */
	if (!IBG(in_mshutdown) &&
		(link->hash_key[0] != '\0' || memcmp(link->hash_key, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16) != 0)) {
		zend_hash_str_del(&EG(regular_list), link->hash_key, sizeof(link->hash_key) - 1);
		FBDEBUG("Removed cache entry for normal link");
	}

	_php_fbird_commit_link(link);

	/* OO API Only: All connections use fbc_disconnect() */
	if (link->fbc_connection != NULL) {
		FBDEBUG("Closing normal link via OO API...");
		fbc_disconnect(link->fbc_connection, IB_STATUS);
		link->fbc_connection = NULL;
	}
	IBG(num_links)--;
	efree(link);
}

void _php_fbird_close_plink(zend_resource *rsrc)
{
	fbird_db_link *link = (fbird_db_link *) rsrc->ptr;

	/* NULL pointer guard (Issue #55): In forked PHPStan workers, rsrc->ptr may be NULL
	 * when inherited resource descriptors are destroyed during child process shutdown.
	 * Accessing link->created_pid with NULL pointer causes SIGSEGV at si_addr=0x4. */
	if (link == NULL) {
		return;
	}

#ifndef PHP_WIN32
	/* Fork-safety check (Issue #22, #36): Skip cleanup if we're in a forked child.
	 * Two-level check for both module-level and connection-level fork detection. */
	pid_t current_pid = getpid();
	if (IBG(init_pid) != 0 && current_pid != IBG(init_pid)) {
		FBDEBUG("Skipping persistent link cleanup in forked child process (global)");
		IBG(num_persistent)--;
		IBG(num_links)--;
		free(link);
		return;
	}
	if (link->created_pid != 0 && current_pid != link->created_pid) {
		FBDEBUG("Skipping persistent link cleanup in forked child process (per-connection)");
		IBG(num_persistent)--;
		IBG(num_links)--;
		free(link);
		return;
	}
#endif

	/* Remove cache entries from both regular and persistent lists (Issue #35).
	 * Persistent connections are cached in EG(persistent_list) with hash key.
	 *
	 * CRITICAL: Skip EG() access during MSHUTDOWN (Issue #50, #51).
	 * During module shutdown, EG(regular_list) and EG(persistent_list) may already
	 * be destroyed, causing SIGSEGV (exit code 139) if accessed. */
	if (!IBG(in_mshutdown) &&
		(link->hash_key[0] != '\0' || memcmp(link->hash_key, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16) != 0)) {
		zend_hash_str_del(&EG(regular_list), link->hash_key, sizeof(link->hash_key) - 1);
		zend_hash_str_del(&EG(persistent_list), link->hash_key, sizeof(link->hash_key) - 1);
		FBDEBUG("Removed cache entries for persistent link");
	}

	_php_fbird_commit_link(link);

	/* OO API Only: All connections use fbc_disconnect() */
	if (link->fbc_connection != NULL) {
		FBDEBUG("Closing permanent link via OO API...");
		fbc_disconnect(link->fbc_connection, IB_STATUS);
		link->fbc_connection = NULL;
	}
	IBG(num_persistent)--;
	IBG(num_links)--;
	free(link);
}

enum connect_args { DB = 0, USER = 1, PASS = 2, CSET = 3, ROLE = 4, BUF = 0, DLECT = 1, SYNC = 2 };

static char const dpb_args[] = {
	0, isc_dpb_user_name, isc_dpb_password, isc_dpb_lc_ctype, isc_dpb_sql_role_name, 0
};

int _php_fbird_attach_db(char **args, size_t *len, zend_long *largs, void **db)
{
    void* connection = NULL;

    /* Use OO API as the connection method */
    connection = fbc_connect(
        IBG(master_instance),
        args[DB], len[DB],                          /* database path */
        args[USER], len[USER],                      /* username */
        args[PASS], len[PASS],                      /* password */
        args[CSET], len[CSET],                      /* charset */
        args[ROLE], len[ROLE],                      /* SQL role */
        (int)largs[BUF],                            /* num_buffers */
        largs[DLECT] ? (int)largs[DLECT] : SQL_DIALECT_CURRENT, /* dialect */
        (int)largs[SYNC],                           /* force_write */
        IB_STATUS                                   /* status vector */
    );

    if (!connection) {
        _php_fbird_error();
        return FAILURE;
    }

    /* Store the OO API connection pointer in the status vector's last slot
     * for retrieval by _php_fbird_connect() */
    IBG(status[ISC_STATUS_LENGTH - 1]) = (ISC_STATUS)(uintptr_t)connection;

    *db = NULL;

    return SUCCESS;
}

void _php_fbird_connect(INTERNAL_FUNCTION_PARAMETERS, int persistent)
{
	char *c, hash[16], *args[] = { NULL, NULL, NULL, NULL, NULL };
	int i;
	size_t len[] = { 0, 0, 0, 0, 0 };
	zend_long largs[] = { 0, 0, 0 };
	zend_long flags = 0;
	PHP_MD5_CTX hash_context;
	zend_resource new_index_ptr, *le;
	void *db_handle = 0;
	fbird_db_link *ib_link;

	RESET_ERRMSG;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS(), "|ssssllsll",
			&args[DB], &len[DB], &args[USER], &len[USER], &args[PASS], &len[PASS],
			&args[CSET], &len[CSET], &largs[BUF], &largs[DLECT], &args[ROLE], &len[ROLE],
			&largs[SYNC], &flags)) {
		RETURN_FALSE;
	}

	/* restrict to the server/db in the .ini if in safe mode */
	if (!len[DB] && (c = INI_STR("fbird.default_db"))) {
		args[DB] = c;
		len[DB] = strlen(c);
	}
	if (!len[USER] && (c = INI_STR("fbird.default_user"))) {
		args[USER] = c;
		len[USER] = strlen(c);
	}
	if (!len[PASS] && (c = INI_STR("fbird.default_password"))) {
		args[PASS] = c;
		len[PASS] = strlen(c);
	}
	if (!len[CSET] && (c = INI_STR("fbird.default_charset"))) {
		args[CSET] = c;
		len[CSET] = strlen(c);
	}

	/* don't want usernames and passwords floating around */
	PHP_MD5Init(&hash_context);
	for (i = 0; i < sizeof(args)/sizeof(char*); ++i) {
		PHP_MD5Update(&hash_context,args[i],len[i]);
	}
	for (i = 0; i < sizeof(largs)/sizeof(zend_long); ++i) {
		PHP_MD5Update(&hash_context,(char*)&largs[i],sizeof(zend_long));
	}
	PHP_MD5Final((unsigned char*)hash, &hash_context);

	/* try to reuse a connection (skip if FBIRD_CONNECT_FORCE_NEW is set) */
	if (!(flags & PHP_FBIRD_CONNECT_FORCE_NEW) &&
			(le = zend_hash_str_find_ptr(&EG(regular_list), hash, sizeof(hash)-1)) != NULL) {
		zend_resource *xlink;

		if (le->type != le_index_ptr) {
			RETURN_FALSE;
		}

		xlink = (zend_resource*) le->ptr;
		if ((!persistent && xlink->type == le_link) || xlink->type == le_plink) {
			/* Issue #119: Verify the cached connection is still usable.
			 * After fbird_close(), fbc_connection may be NULL even though
			 * the zend_resource still exists with refcount > 0. */
			ib_link = (fbird_db_link *)xlink->ptr;
			if (ib_link == NULL || ib_link->fbc_connection == NULL ||
				!fbc_is_connected(ib_link->fbc_connection)) {
				/* Stale cache entry — remove and fall through to create new connection */
				zend_hash_str_del(&EG(regular_list), hash, sizeof(hash)-1);
			} else {
				if (IBG(default_link) != xlink) {
					GC_ADDREF(xlink);
					if (IBG(default_link)) {
						zend_list_delete(IBG(default_link));
					}
					IBG(default_link) = xlink;
				}
				GC_ADDREF(xlink);
				RETURN_RES(xlink);
			}
		} else {
			zend_hash_str_del(&EG(regular_list), hash, sizeof(hash)-1);
		}
	}

	/* ... or a persistent one */
	do {
		zend_long l;

		if ((le = zend_hash_str_find_ptr(&EG(persistent_list), hash, sizeof(hash)-1)) != NULL) {
			if (le->type != le_plink) {
				RETURN_FALSE;
			}
			/* check if connection has timed out */
			ib_link = (fbird_db_link *) le->ptr;
			if (ib_link->fbc_connection && fbc_is_connected(ib_link->fbc_connection)) {
				RETVAL_RES(zend_register_resource(ib_link, le_plink));
				break;
			}
			zend_hash_str_del(&EG(persistent_list), hash, sizeof(hash)-1);
		}

		/* no link found, so we have to open one */

		if ((l = INI_INT("fbird.max_links")) != -1 && IBG(num_links) >= l) {
			_php_fbird_module_error("Too many open links (%ld)", IBG(num_links));
			RETURN_FALSE;
		}

		/* create the ib_link */
		if (FAILURE == _php_fbird_attach_db(args, len, largs, &db_handle)) {
			RETURN_FALSE;
		}

		/* use non-persistent if allowed number of persistent links is exceeded */
		if (!persistent || ((l = INI_INT("fbird.max_persistent") != -1) && IBG(num_persistent) >= l)) {
			ib_link = (fbird_db_link *) emalloc(sizeof(fbird_db_link));
			RETVAL_RES(zend_register_resource(ib_link, le_link));
		} else {
			ib_link = (fbird_db_link *) malloc(sizeof(fbird_db_link));
			if (!ib_link) {
				RETURN_FALSE;
			}

			/* hash it up */
			if (zend_register_persistent_resource(hash, sizeof(hash)-1, ib_link, le_plink) == NULL) {
				free(ib_link);
				RETURN_FALSE;
			}
			RETVAL_RES(zend_register_resource(ib_link, le_plink));
			++IBG(num_persistent);
		}
		ib_link->dialect = largs[DLECT] ? (unsigned short)largs[DLECT] : SQL_DIALECT_CURRENT;
		ib_link->tr_list = NULL;
		ib_link->event_head = NULL;

		ib_link->fbc_connection = (void *)(uintptr_t)IBG(status[ISC_STATUS_LENGTH - 1]);
		IBG(status[ISC_STATUS_LENGTH - 1]) = 0;  /* Clear the temporary storage */

		/* Store hash key for cache invalidation on close (Issue #35) */
		memcpy(ib_link->hash_key, hash, sizeof(hash));

		/* Store creation PID for fork-safety detection (Issue #36) */
#ifndef PHP_WIN32
		ib_link->created_pid = getpid();
#else
		ib_link->created_pid = 0;
#endif

		++IBG(num_links);
	} while (0);

	/* add it to the hash */
	new_index_ptr.ptr = (void *) Z_RES_P(return_value);
	new_index_ptr.type = le_index_ptr;
	zend_hash_str_update_mem(&EG(regular_list), hash, sizeof(hash)-1,
			(void *) &new_index_ptr, sizeof(zend_resource));
	if (IBG(default_link)) {
		zend_list_delete(IBG(default_link));
	}
	IBG(default_link) = Z_RES_P(return_value);
	Z_TRY_ADDREF_P(return_value);
	Z_TRY_ADDREF_P(return_value);
}

PHP_FUNCTION(fbird_connect)
{
	_php_fbird_connect(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}

PHP_FUNCTION(fbird_pconnect)
{
	_php_fbird_connect(INTERNAL_FUNCTION_PARAM_PASSTHRU, INI_INT("fbird.allow_persistent"));
}

/* Helper function for consolidated resource validation with proper error differentiation */
static int _php_fbird_validate_link_resource(zend_resource *link_res, bool is_default_link, bool clear_default)
{
	if (link_res == NULL) {
		return FAILURE;
	}

	/* Check resource type and validity directly */
	if (link_res->type != le_link && link_res->type != le_plink) {
		/* Wrong type - will be caught by zend_parse_parameters */
		return FAILURE;
	}

	/* Check if resource pointer is valid */
	if (link_res->ptr == NULL) {
		/* Correct resource type but invalid/closed - generate warning */
		php_error_docref(NULL, E_WARNING, "Supplied resource is not a valid database link resource");
		if (clear_default && is_default_link) {
			/* Thread-safe: Only clear if we were the default */
			if (IBG(default_link) == link_res) {
				IBG(default_link) = NULL;
			}
		}
		return FAILURE;
	}

	return SUCCESS;
}

/* Helper function for thread-safe default link adoption */
static void _php_fbird_adopt_new_default_link(zend_resource *closing_link)
{
	/* Only search if we're actually clearing the current default */
	if (IBG(default_link) != closing_link) {
		return;
	}

	/* For now, simply clear the default. Full adoption logic can be added in future enhancement.
	 * This maintains existing behavior while providing the infrastructure for adoption. */
	IBG(default_link) = NULL;
}

/* Helper function for optimized resource cleanup */
static void _php_fbird_close_resource(zend_resource *link_res)
{
	/* For persistent connections, check reference count more carefully */
	if (link_res->type == le_plink && GC_REFCOUNT(link_res) > 1) {
		/* Multiple references exist - just decrease our refcount */
		zend_list_delete(link_res);
	} else {
		/* Safe to close: either non-persistent or no other references */
		zend_list_close(link_res);
	}
}

PHP_FUNCTION(fbird_close)
{
	zval *link_arg = NULL;
	zend_resource *link_res;
	bool is_default_link = false;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|r!", &link_arg) == FAILURE) {
		return;
	}

	/* Determine which link to close */
	if (ZEND_NUM_ARGS() == 0 || link_arg == NULL) {
		/* Default link path */
		link_res = IBG(default_link);
		is_default_link = true;

		if (link_res == NULL) {
			RETURN_FALSE;
		}
	} else {
		/* Explicit link path */
		link_res = Z_RES_P(link_arg);
		is_default_link = (IBG(default_link) == link_res);
	}

	/* Single validation point - handles all validation efficiently */
	if (_php_fbird_validate_link_resource(link_res, is_default_link, true) == FAILURE) {
		RETURN_FALSE;
	}

	/* Handle default link management BEFORE closing resource
	 * Special handling for explicit links that are also the default */
	if (is_default_link) {
		if (link_arg != NULL) {
			/* When closing explicit link that's also default, only clear if
			 * resource's reference count will drop to zero */
			if (GC_REFCOUNT(link_res) <= 2) {
				_php_fbird_adopt_new_default_link(link_res);
			}
		} else {
			/* Default link path - always clear default */
			_php_fbird_adopt_new_default_link(link_res);
		}
	}

	/* Optimized resource cleanup */
	_php_fbird_close_resource(link_res);

	RETURN_TRUE;
}

PHP_FUNCTION(fbird_drop_db)
{
	zval *link_arg = NULL;
	fbird_db_link *ib_link;
	fbird_tr_list *l;
	zend_resource *link_res;
	int drop_result;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|r", &link_arg) == FAILURE) {
		return;
	}

	if (ZEND_NUM_ARGS() == 0) {
		link_res = IBG(default_link);
		CHECK_LINK(link_res);
		IBG(default_link) = NULL;
	} else {
		link_res = Z_RES_P(link_arg);
	}

	ib_link = (fbird_db_link *)zend_fetch_resource2(link_res, LE_LINK, le_link, le_plink);

	if (!ib_link) {
		RETURN_FALSE;
	}

	/* OO API Only: All connections use fbc_drop_database() */
	if (ib_link->fbc_connection != NULL) {
		FBDEBUG("Dropping database via OO API...");
		drop_result = fbc_drop_database(ib_link->fbc_connection, IB_STATUS);
		if (drop_result != 0) {
			_php_fbird_error();
			RETURN_FALSE;
		}
		/* fbc_drop_database() already frees the connection wrapper */
		ib_link->fbc_connection = NULL;
	}

	/* drop_database() doesn't invalidate the transaction handles */
	for (l = ib_link->tr_list; l != NULL; l = l->next) {
		if (l->trans != NULL) {
			l->trans->fbt_transaction = NULL;
		}
	}

	zend_list_delete(link_res);

	RETURN_TRUE;
}

#endif /* HAVE_FIREBIRD */
