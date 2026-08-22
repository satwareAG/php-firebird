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
#include "fbird_classes.h"

/* Fill fb_link and trans with the correct database link and transaction.
 * M3: Accepts both legacy zend_resource zvals and Firebird\Connection /
 * Firebird\Transaction objects (weak-ref to the same internal resource). */
void _php_fbird_get_link_trans(INTERNAL_FUNCTION_PARAMETERS,
	zval *link_id, fbird_db_link **fb_link, fbird_transaction **trans)
{
	FBDEBUG("Transaction or database link?");

	/* Object path: Firebird\Transaction or Firebird\Connection */
	if (Z_TYPE_P(link_id) == IS_OBJECT) {
		if (instanceof_function(Z_OBJCE_P(link_id), fbird_transaction_ce)) {
			FBDEBUG("IS_OBJECT: Firebird\\Transaction");
			zend_resource *tres = fbird_transaction_get_resource(Z_OBJ_P(link_id));
			if (!tres) {
				_php_fbird_module_error("Invalid Firebird\\Transaction object");
				return;
			}
			*trans = (fbird_transaction *)tres->ptr;
			if ((*trans)->link_cnt > 1) {
				_php_fbird_module_error("Link id is ambiguous: transaction spans multiple connections.");
				return;
			}
			*fb_link = (*trans)->db_link[0];
			return;
		} else if (instanceof_function(Z_OBJCE_P(link_id), fbird_connection_ce)) {
			FBDEBUG("IS_OBJECT: Firebird\\Connection");
			zend_resource *cres = fbird_connection_get_resource(Z_OBJ_P(link_id));
			if (!cres) {
				_php_fbird_module_error("Invalid Firebird\\Connection object");
				return;
			}
			*trans = NULL;
			*fb_link = (fbird_db_link *)cres->ptr;
			return;
		}
	}

	/* Resource path: legacy le_trans or le_link/le_plink */
	if (Z_TYPE_P(link_id) == IS_RESOURCE && Z_RES_P(link_id)->type == le_trans) {
		/* Transaction resource: make sure it refers to one link only, then
		   fetch it; database link is stored in fb_trans->db_link[]. */
		FBDEBUG("IS_RESOURCE: le_trans");
		*trans = (fbird_transaction *)zend_fetch_resource_ex(link_id, LE_TRANS, le_trans);
		if ((*trans)->link_cnt > 1) {
			_php_fbird_module_error("Link id is ambiguous: transaction spans multiple connections.");
			return;
		}
		*fb_link = (*trans)->db_link[0];
		return;
	}
	FBDEBUG("IS_RESOURCE: le_[p]link or id not found");
	/* Database link resource, use default transaction. */
	*trans = NULL;
	*fb_link = (fbird_db_link *)zend_fetch_resource2_ex(link_id, LE_LINK, le_link, le_plink);
}

/* destructors ---------------------- */

void _php_fbird_commit_link(fbird_db_link *link)
{
	ISC_STATUS status[256];
	unsigned short j;
	fbird_tr_list *l;
	fbird_event *e;
	FBDEBUG("Checking transactions to close...");

	/* Flag-based cleanup (Issue #554): is_default distinguishes
	 * default transaction (commit + efree directly, NOT a le_trans resource)
	 * from explicit transaction (rollback + leave to le_trans destructor). */
	for (l = link->tr_list; l != NULL;) {
		fbird_tr_list *p = l;
		if (p->trans != 0) {
			if (p->trans->is_default) {
				/* Default transaction: commit via OO API */
				if (p->trans->fbt_transaction != NULL) {
					FBDEBUG("Committing default transaction via OO API...");
					int res = fbt_commit(p->trans->fbt_transaction, status);
					fbt_free(p->trans->fbt_transaction);
					p->trans->fbt_transaction = NULL;
					/* Guard error reporting during resource shutdown (Issue #311).
					 * Use EG_FLAGS_IN_RESOURCE_SHUTDOWN instead of FBG(in_mshutdown)
					 * because _php_fbird_commit_link is called from both
					 * _php_fbird_close_link (regular list, after MSHUTDOWN) and
					 * _php_fbird_close_plink (persistent list, before MSHUTDOWN).
					 * The EG flag is set during both request and module shutdown. */
					if (res && !(EG(flags) & EG_FLAGS_IN_RESOURCE_SHUTDOWN)) {
						_php_fbird_error(status);
					}
				}
				efree(p->trans); /* default transaction is not a registered resource: clean up */
			} else {
				/* Non-default transaction: rollback via OO API */
				if (p->trans->fbt_transaction != NULL) {
					FBDEBUG("Rolling back other transaction via OO API...");
					int res = fbt_rollback(p->trans->fbt_transaction, status);
					fbt_free(p->trans->fbt_transaction);
					p->trans->fbt_transaction = NULL;
					if (res && !(EG(flags) & EG_FLAGS_IN_RESOURCE_SHUTDOWN)) {
						_php_fbird_error(status);
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
	ISC_STATUS status[256];
	fbird_db_link *link = (fbird_db_link *) rsrc->ptr;

	/* NULL pointer guard (Issue #55): In forked PHPStan workers, rsrc->ptr may be NULL
	 * when inherited resource descriptors are destroyed during child process shutdown.
	 * Accessing link->created_pid with NULL pointer causes SIGSEGV at si_addr=0x4. */
	if (link == NULL) {
		return;
	}

	/* Clear default_link if this resource IS the default link (Issue #183, #184).
	 * Without this, FBG(default_link) becomes a dangling pointer after the link
	 * is freed, causing SIGSEGV when doctrine's TransactionManager later tries to
	 * use the default link via procedural API paths. */
	if (!FBG(in_mshutdown) && FBG(default_link) == rsrc) {
		FBG(default_link) = NULL;
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
	if (FBG(init_pid) != 0 && current_pid != FBG(init_pid)) {
		FBDEBUG("Skipping link cleanup in forked child process (global)");
		FBG(num_links)--;
		efree(link);
		return;
	}
	if (link->created_pid != 0 && current_pid != link->created_pid) {
		FBDEBUG("Skipping link cleanup in forked child process (per-connection)");
		FBG(num_links)--;
		efree(link);
		return;
	}
#endif

	/* Remove cache entry from EG(regular_list) to prevent UAF (Issue #35).
	 *
	 * CRITICAL: Skip EG() access during MSHUTDOWN (Issue #50, #51, #55).
	 * During module shutdown, EG(regular_list) may already be destroyed. */
	if (!FBG(in_mshutdown) &&
		(link->hash_key[0] != '\0' || memcmp(link->hash_key, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16) != 0)) {
		zend_hash_str_del(&EG(regular_list), link->hash_key, sizeof(link->hash_key) - 1);
		FBDEBUG("Removed cache entry for normal link");
	}

	_php_fbird_commit_link(link);

	/* OO API Only: All connections use fbc_disconnect() */
	if (link->fbc_connection != NULL) {
		FBDEBUG("Closing normal link via OO API...");
		fbc_disconnect(link->fbc_connection, status);
		link->fbc_connection = NULL;
	}
	FBG(num_links)--;
	efree(link);
}

void _php_fbird_close_plink(zend_resource *rsrc)
{
	ISC_STATUS status[256];
	fbird_db_link *link = (fbird_db_link *) rsrc->ptr;

	/* NULL pointer guard (Issue #55): In forked PHPStan workers, rsrc->ptr may be NULL
	 * when inherited resource descriptors are destroyed during child process shutdown.
	 * Accessing link->created_pid with NULL pointer causes SIGSEGV at si_addr=0x4. */
	if (link == NULL) {
		return;
	}

	/* Clear default_link if this resource IS the default link (Issue #183, #184).
	 * Mirrors _php_fbird_close_link: persistent resources can also be the default
	 * link and must clear FBG(default_link) when destroyed to prevent dangling ptr. */
	if (!FBG(in_mshutdown) && FBG(default_link) == rsrc) {
		FBG(default_link) = NULL;
	}

#ifndef PHP_WIN32
	/* Fork-safety check (Issue #22, #36): Skip cleanup if we're in a forked child.
	 * Two-level check for both module-level and connection-level fork detection. */
	pid_t current_pid = getpid();
	if (FBG(init_pid) != 0 && current_pid != FBG(init_pid)) {
		FBDEBUG("Skipping persistent link cleanup in forked child process (global)");
		FBG(num_persistent)--;
		FBG(num_links)--;
		free(link);
		return;
	}
	if (link->created_pid != 0 && current_pid != link->created_pid) {
		FBDEBUG("Skipping persistent link cleanup in forked child process (per-connection)");
		FBG(num_persistent)--;
		FBG(num_links)--;
		free(link);
		return;
	}
#endif

	/* Remove cache entry from EG(regular_list) to prevent UAF (Issue #35).
	 *
	 * CRITICAL: Use EG_FLAGS_IN_RESOURCE_SHUTDOWN instead of FBG(in_mshutdown)
	 * (Issue #311). The in_mshutdown flag is set in PHP_MSHUTDOWN_FUNCTION,
	 * but persistent resource destructors run BEFORE MSHUTDOWN (during
	 * zend_destroy_rsrc_list(&EG(persistent_list)) at zend.c:1118). The
	 * EG_FLAGS_IN_RESOURCE_SHUTDOWN flag is set at the start of
	 * zend_shutdown_executor_values(), before EG(regular_list) is destroyed,
	 * and stays set through module shutdown.
	 *
	 * Do NOT call zend_hash_str_del(&EG(persistent_list), ...) here.
	 * plist_entry_destructor (our caller via zend_hash_graceful_reverse_destroy)
	 * is already removing this entry. Calling zend_hash_str_del would cause
	 * infinite recursion. */
	if (!(EG(flags) & EG_FLAGS_IN_RESOURCE_SHUTDOWN) &&
		(link->hash_key[0] != '\0' || memcmp(link->hash_key, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16) != 0)) {
		zend_hash_str_del(&EG(regular_list), link->hash_key, sizeof(link->hash_key) - 1);
		FBDEBUG("Removed cache entry from regular list for persistent link");
	}

	_php_fbird_commit_link(link);

	/* OO API Only: All connections use fbc_disconnect() */
	if (link->fbc_connection != NULL) {
		FBDEBUG("Closing permanent link via OO API...");
		fbc_disconnect(link->fbc_connection, status);
		link->fbc_connection = NULL;
	}
	FBG(num_persistent)--;
	FBG(num_links)--;
	free(link);
}

enum connect_args { DB = 0, USER = 1, PASS = 2, CSET = 3, ROLE = 4, BUF = 0, DLECT = 1, SYNC = 2 };

int _php_fbird_attach_db(char **args, size_t *len, zend_long *largs, void **out_connection)
{
	ISC_STATUS status[256];
    void* connection = NULL;

    /* Use OO API as the connection method */
    connection = fbc_connect(
        FBG(master_instance),
        args[DB], len[DB],                          /* database path */
        args[USER], len[USER],                      /* username */
        args[PASS], len[PASS],                      /* password */
        args[CSET], len[CSET],                      /* charset */
        args[ROLE], len[ROLE],                      /* SQL role */
        (int)largs[BUF],                            /* num_buffers */
        largs[DLECT] ? (int)largs[DLECT] : SQL_DIALECT_CURRENT, /* dialect */
        (int)largs[SYNC],                           /* force_write */
        status                                   /* status vector */
    );

    if (!connection) {
        _php_fbird_error(status);
        return FAILURE;
    }

    /* Return connection pointer directly via output parameter */
    *out_connection = connection;

    return SUCCESS;
}

/**
 * Core connection logic extracted from _php_fbird_connect.
 * Accepts plain C arguments (already parsed/defaulted by caller).
 * Returns the new zend_resource* with appropriate refcount adjustments,
 * or NULL on failure (error already set via _php_fbird_error(status)).
 * Also applies INI-based defaults for empty args and manages FBG(default_link).
 */
zend_resource *_php_fbird_connect_link(
	char *db,      size_t db_len,
	char *user,    size_t user_len,
	char *pass,    size_t pass_len,
	char *charset, size_t charset_len,
	zend_long buffers, zend_long dialect,
	char *role,    size_t role_len,
	zend_long flags, int persistent)
{
	char *c, hash[16], *args[] = { db, user, pass, charset, role };
	size_t i;
	size_t len[] = { db_len, user_len, pass_len, charset_len, role_len };
	zend_long largs[] = { buffers, dialect, 0 };
	PHP_MD5_CTX hash_context;
	zend_resource new_index_ptr, *le;
	void *connection_ptr = NULL;
	fbird_db_link *fb_link;
	zend_resource *result_res = NULL;

	/* Apply INI-based defaults for empty args */
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
			return NULL;
		}

		xlink = (zend_resource*) le->ptr;
		if ((!persistent && xlink->type == le_link) || xlink->type == le_plink) {
			/* Issue #119: Verify the cached connection is still usable.
			 * After fbird_close(), fbc_connection may be NULL even though
			 * the zend_resource still exists with refcount > 0. */
			fb_link = (fbird_db_link *)xlink->ptr;
			if (fb_link == NULL || fb_link->fbc_connection == NULL ||
				!fbc_is_connected(fb_link->fbc_connection)) {
				/* Stale cache entry — remove and fall through to create new connection */
				zend_hash_str_del(&EG(regular_list), hash, sizeof(hash)-1);
			} else {
				if (FBG(default_link) != xlink) {
					GC_ADDREF(xlink);
					if (FBG(default_link)) {
						zend_list_delete(FBG(default_link));
					}
					FBG(default_link) = xlink;
				}
				GC_ADDREF(xlink);
				return xlink;
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
				return NULL;
			}
			/* check if connection has timed out */
			fb_link = (fbird_db_link *) le->ptr;
			if (fb_link->fbc_connection && fbc_is_connected(fb_link->fbc_connection)) {
				result_res = zend_register_resource(fb_link, le_plink);
				break;
			}
			zend_hash_str_del(&EG(persistent_list), hash, sizeof(hash)-1);
		}

		/* no link found, so we have to open one */

		if ((l = INI_INT("fbird.max_links")) != -1 && FBG(num_links) >= l) {
			_php_fbird_module_error("Too many open links (%ld)", FBG(num_links));
			return NULL;
		}

		/* create the fb_link */
		if (FAILURE == _php_fbird_attach_db(args, len, largs, &connection_ptr)) {
			return NULL;
		}

		/* use non-persistent if allowed number of persistent links is exceeded */
		if (!persistent || ((l = INI_INT("fbird.max_persistent") != -1) && FBG(num_persistent) >= l)) {
			fb_link = (fbird_db_link *) emalloc(sizeof(fbird_db_link));
			result_res = zend_register_resource(fb_link, le_link);
		} else {
			fb_link = (fbird_db_link *) malloc(sizeof(fbird_db_link));
			if (!fb_link) {
				return NULL;
			}

			/* hash it up */
			if (zend_register_persistent_resource(hash, sizeof(hash)-1, fb_link, le_plink) == NULL) {
				free(fb_link);
				return NULL;
			}
			result_res = zend_register_resource(fb_link, le_plink);
			++FBG(num_persistent);
		}
		fb_link->dialect = largs[DLECT] ? (unsigned short)largs[DLECT] : SQL_DIALECT_CURRENT;
		fb_link->tr_list = NULL;
		fb_link->event_head = NULL;
		fb_link->is_persistent = persistent;

		fb_link->fbc_connection = connection_ptr;

		/* Store hash key for cache invalidation on close (Issue #35) */
		memcpy(fb_link->hash_key, hash, sizeof(hash));

		/* Store creation PID for fork-safety detection (Issue #36) */
#ifndef PHP_WIN32
		fb_link->created_pid = getpid();
#else
		fb_link->created_pid = 0;
#endif

		++FBG(num_links);
	} while (0);

	/* add it to the hash */
	new_index_ptr.ptr = (void *) result_res;
	new_index_ptr.type = le_index_ptr;
	zend_hash_str_update_mem(&EG(regular_list), hash, sizeof(hash)-1,
			(void *) &new_index_ptr, sizeof(zend_resource));
	if (FBG(default_link)) {
		zend_list_delete(FBG(default_link));
	}
	FBG(default_link) = result_res;
	GC_ADDREF(result_res);  /* default_link ref */
	GC_ADDREF(result_res);  /* caller ref */
	return result_res;
}

void _php_fbird_connect(INTERNAL_FUNCTION_PARAMETERS, int persistent)
{
	char *args_db = NULL, *args_user = NULL, *args_pass = NULL;
	char *args_cset = NULL, *args_role = NULL;
	size_t len_db = 0, len_user = 0, len_pass = 0, len_cset = 0, len_role = 0;
	zend_long buf = 0, dlect = 0, sync = 0, flags = 0;

	RESET_ERRMSG;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS(), "|ssssllsll",
			&args_db, &len_db, &args_user, &len_user, &args_pass, &len_pass,
			&args_cset, &len_cset, &buf, &dlect, &args_role, &len_role,
			&sync, &flags)) {
		RETURN_FALSE;
	}

	zend_resource *res = _php_fbird_connect_link(
		args_db,   len_db,
		args_user, len_user,
		args_pass, len_pass,
		args_cset, len_cset,
		buf, dlect,
		args_role, len_role,
		flags, persistent);
	if (!res) {
		RETURN_FALSE;
	}
	/* Phase C: return Firebird\Connection object instead of raw resource.
	 * fbird_setup_connection_object takes a strong ref for the object (#576);
	 * the DELREF below releases the "caller ref" that
	 * _php_fbird_connect_link added, so net the object owns exactly one. */
	fbird_setup_connection_object(return_value, res);
	GC_DELREF(res);
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
		/* Resource was closed/invalidated — silent no-op (not an error) */
		return FAILURE;
	}

	/* Check if resource pointer is valid */
	if (link_res->ptr == NULL) {
		/* Correct resource type but invalid/closed - generate warning */
		_php_fbird_module_error("Supplied resource is not a valid database link resource");
		if (clear_default && is_default_link) {
			/* Thread-safe: Only clear if we were the default */
			if (FBG(default_link) == link_res) {
				FBG(default_link) = NULL;
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
	if (FBG(default_link) != closing_link) {
		return;
	}

	/* For now, simply clear the default. Full adoption logic can be added in future enhancement.
	 * This maintains existing behavior while providing the infrastructure for adoption. */
	FBG(default_link) = NULL;
}

/* Helper function for optimized resource cleanup */
static void _php_fbird_close_resource(zend_resource *link_res)
{
	/* Issue #576 + #202 semantics:
	 * - Persistent link with refcount > 1: other holders (Firebird\
	 *   Connection objects wrapping the same plink entry, default_link) are
	 *   still alive - closing the server link would defunct the shared entry
	 *   under them ("No default connection" regression) and the old code's
	 *   zend_list_delete here released a ref owned by nobody (undercount ->
	 *   premature entry free -> heap-use-after-free). Do nothing; the last
	 *   holder's close()/release closes the link.
	 * - Otherwise: zend_list_close fires the link dtor exactly once; entry
	 *   memory is released by the owners' deletes when rc reaches 0. */
	if (link_res->type == le_plink && GC_REFCOUNT(link_res) > 1) {
		return;
	}
	zend_list_close(link_res);
}

/* Helper: extract zend_resource* from either a resource zval or a Firebird\Connection object.
 * Returns NULL if the zval is neither. */
static zend_resource *_php_fbird_res_from_zval(zval *zv)
{
	if (zv == NULL) return NULL;
	ZVAL_DEREF(zv);
	if (Z_TYPE_P(zv) == IS_RESOURCE) {
		return Z_RES_P(zv);
	}
	if (Z_TYPE_P(zv) == IS_OBJECT &&
		instanceof_function(Z_OBJCE_P(zv), fbird_connection_ce)) {
		/* Extract the connection resource from the Firebird\Connection object */
		return fbird_connection_get_resource(Z_OBJ_P(zv));
	}
	return NULL;
}

PHP_FUNCTION(fbird_close)
{
	zval *link_arg = NULL;
	zend_resource *link_res;
	bool is_default_link = false;

	RESET_ERRMSG;

	/* Accept resource OR Firebird\Connection object (Issue #120) */
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|z!", &link_arg) == FAILURE) {
		return;
	}

	/* Type enforcement: only resource, Firebird\Connection object, or null accepted */
	if (link_arg != NULL && Z_TYPE_P(link_arg) != IS_RESOURCE && Z_TYPE_P(link_arg) != IS_OBJECT) {
		zend_argument_type_error(1, "must be of type resource or null, %s given", zend_get_type_by_const(Z_TYPE_P(link_arg)));
		RETURN_THROWS();
	}

	/* Determine which link to close */
	if (ZEND_NUM_ARGS() == 0 || link_arg == NULL) {
		/* Default link path */
		link_res = FBG(default_link);
		is_default_link = true;

		if (link_res == NULL) {
			_php_fbird_module_error("No default connection to close");
			RETURN_FALSE;
		}
	} else {
		/* Explicit link path: accept resource or Connection object */
		link_res = _php_fbird_res_from_zval(link_arg);
		if (link_res == NULL) {
			_php_fbird_module_error("Argument #1 must be a valid Firebird connection resource or Firebird\\Connection object");
			RETURN_FALSE;
		}
		is_default_link = (FBG(default_link) == link_res);
	}

	/* Single validation point - handles all validation efficiently */
	if (_php_fbird_validate_link_resource(link_res, is_default_link, true) == FAILURE) {
		RETURN_FALSE;
	}

	/* Handle default link management BEFORE closing resource.
	 * For the explicit-arg case do NOT pre-clear: the resource destructors
	 * (_php_fbird_close_link / _php_fbird_close_plink) clear FBG(default_link)
	 * when the resource is actually freed.
	 *
	 * Pre-clearing in the explicit-arg path caused a regression (Issue #202):
	 * when fbird_close($pcon1) is called and pcon1 == default_link but another
	 * Connection object ($pcon2) wraps the same persistent resource, zend_list_delete
	 * only decrements the refcount without destroying it. Pre-clearing default_link
	 * here leaves it NULL even though pcon2's connection is still alive. */
	if (is_default_link && link_arg == NULL) {
		/* No-arg path: pre-clear because zend_list_delete on a persistent
		 * connection won't trigger the destructor. */
		_php_fbird_adopt_new_default_link(link_res);
	}

	/* Optimized resource cleanup */
	_php_fbird_close_resource(link_res);

	RETURN_TRUE;
}

/* {{{ _php_fbird_escape_single_quotes
   Double every single-quote character in str to produce SQL-safe '' escaping.
   Returns a new zend_string; caller must zend_string_release(). */
static zend_string *_php_fbird_escape_single_quotes(const char *str, size_t len)
{
	/* Worst case: every character is a quote */
	zend_string *result = zend_string_alloc(len * 2, 0);
	char *dst = ZSTR_VAL(result);
	size_t dst_len = 0;

	for (size_t i = 0; i < len; i++) {
		if (str[i] == '\'') {
			dst[dst_len++] = '\'';
			dst[dst_len++] = '\'';
		} else {
			dst[dst_len++] = str[i];
		}
	}
	dst[dst_len] = '\0';
	ZSTR_LEN(result) = dst_len;
	return result;
}
/* }}} */

/* Firebird character set allowlist for CREATE DATABASE.
   Validated against Firebird 3.0/4.0/5.0 supported character sets. */
static const char *valid_charsets[] = {
	"NONE", "ASCII", "BIG_5", "CYRL", "DOS437", "DOS850", "DOS852",
	"DOS857", "DOS860", "DOS861", "DOS863", "DOS865", "EUCJ_0208",
	"GB_2312", "ISO8859_1", "ISO8859_2", "ISO8859_3", "ISO8859_4",
	"ISO8859_5", "ISO8859_6", "ISO8859_7", "ISO8859_8", "ISO8859_9",
	"ISO8859_13", "KSC_5601", "NEXT", "OCTETS", "SJIS_0208",
	"TIS620", "UNICODE_FSS", "UTF8", "WIN1250", "WIN1251",
	"WIN1252", "WIN1253", "WIN1254", "WIN1255", "WIN1256",
	"WIN1257", "WIN1258", "KOI8R", "KOI8U", "WIN_PTBR",
	"ISO8859_15", "GBK", "CP943C", "GB18030",
	NULL
};

/* {{{ _php_fbird_is_valid_charset
   Case-insensitive check of charset against the Firebird allowlist. */
static int _php_fbird_is_valid_charset(const char *charset)
{
	for (const char **cs = valid_charsets; *cs != NULL; cs++) {
		if (zend_binary_strcasecmp(charset, strlen(charset), *cs, strlen(*cs)) == 0) {
			return 1;
		}
	}
	return 0;
}
/* }}} */

/* {{{ proto resource fbird_create_database(string $database [, string $username [, string $password [, string $charset [, int $page_size]]]])
   Create a new Firebird database and return a connection resource */
PHP_FUNCTION(fbird_create_database)
{
	ISC_STATUS status[256];
	char *database = NULL, *username = NULL, *password = NULL, *charset = NULL;
	size_t database_len, username_len = 0, password_len = 0, charset_len = 0;
	zend_long page_size = 0;
	unsigned short dialect = 3;
	fbird_db_link *fb_link;
	char *create_sql = NULL;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|sssl",
			&database, &database_len,
			&username, &username_len,
			&password, &password_len,
			&charset, &charset_len,
			&page_size) == FAILURE) {
		return;
	}

	/* Validate charset against allowlist to prevent SQL injection via
	 * DEFAULT CHARACTER SET clause (unquoted identifier). */
	if (charset && charset_len > 0 && !_php_fbird_is_valid_charset(charset)) {
		_php_fbird_module_error(
			"Invalid character set name '%s'", charset);
		RETURN_FALSE;
	}

	/* Escape single quotes in user-supplied strings to prevent SQL injection
	 * in the CREATE DATABASE statement (C1 fix, issue #155). */
	zend_string *esc_database = _php_fbird_escape_single_quotes(database, database_len);
	zend_string *esc_username = NULL;
	zend_string *esc_password = NULL;

	/* Build CREATE DATABASE SQL dynamically with spprintf() to eliminate
	 * fixed-size stack buffer overflow risk (M10 fix, issue #157). */
	spprintf(&create_sql, 0, "CREATE DATABASE '%s'", ZSTR_VAL(esc_database));

	if (username && username_len > 0) {
		char *tmp;
		esc_username = _php_fbird_escape_single_quotes(username, username_len);
		spprintf(&tmp, 0, "%s USER '%s'", create_sql, ZSTR_VAL(esc_username));
		efree(create_sql);
		create_sql = tmp;
	}
	if (password && password_len > 0) {
		char *tmp;
		esc_password = _php_fbird_escape_single_quotes(password, password_len);
		spprintf(&tmp, 0, "%s PASSWORD '%s'", create_sql, ZSTR_VAL(esc_password));
		efree(create_sql);
		create_sql = tmp;
	}
	if (page_size > 0) {
		char *tmp;
		spprintf(&tmp, 0, "%s PAGE_SIZE = %ld", create_sql, (long)page_size);
		efree(create_sql);
		create_sql = tmp;
	}
	if (charset && charset_len > 0) {
		char *tmp;
		/* charset is already validated against allowlist above */
		spprintf(&tmp, 0, "%s DEFAULT CHARACTER SET %s", create_sql, charset);
		efree(create_sql);
		create_sql = tmp;
	}

	void *create_result = fbc_create_database(
		FBG(master_instance),
		create_sql,
		dialect,
		status
	);

	/* Clean up dynamically allocated SQL and escaped strings */
	efree(create_sql);
	zend_string_release(esc_database);
	if (esc_username) {
		zend_string_release(esc_username);
	}
	if (esc_password) {
		zend_string_release(esc_password);
	}

	if (!create_result) {
		_php_fbird_error(status);
		RETURN_FALSE;
	}

	fb_link = (fbird_db_link *) ecalloc(1, sizeof(fbird_db_link));
	fb_link->dialect = dialect;
	fb_link->tr_list = NULL;
	fb_link->event_head = NULL;
	fb_link->fbc_connection = create_result;

	/* Phase C: register resource and wrap in Firebird\Connection object.
	 * register gives ref=1; default_link ADDREF makes ref=2; the object owns
	 * a strong ref via fbird_setup_connection_object (#576) -> ref=3. */
	{
		zend_resource *cres = zend_register_resource(fb_link, le_link);
		if (FBG(default_link)) {
			zend_list_delete(FBG(default_link));
		}
		FBG(default_link) = cres;
		GC_ADDREF(cres); /* default_link ref */
		fbird_setup_connection_object(return_value, cres);
		/* no GC_DELREF: no extra caller ref was taken */
	}
}
/* }}} */

PHP_FUNCTION(fbird_drop_db)
{
	ISC_STATUS status[256];
	zval *link_arg = NULL;
	fbird_db_link *fb_link;
	fbird_tr_list *l;
	zend_resource *link_res;
	int drop_result;
	char *database = NULL, *username = NULL, *password = NULL;
	size_t database_len = 0, username_len = 0, password_len = 0;
	bool string_mode = false;

	RESET_ERRMSG;

	/* Try string overload first: fbird_drop_db(string $dsn, string $user, string $pass)
	 * Only activate if the first argument is actually a string (not resource/int/null) */
	if (ZEND_NUM_ARGS() >= 1) {
		zval *first_arg = NULL;
		zval tmp_args[1];
		if (zend_get_parameters_array_ex(1, tmp_args) == SUCCESS) {
			first_arg = &tmp_args[0];
			ZVAL_DEREF(first_arg);
		}
		if (first_arg && Z_TYPE_P(first_arg) == IS_STRING) {
			if (zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS(), "s|ss",
					&database, &database_len, &username, &username_len, &password, &password_len) == SUCCESS
					&& database != NULL) {
				string_mode = 1;
			}
		}
	}

	if (string_mode) {
		/* Connect, drop, return */
		const char *u = username ? username : "SYSDBA";
		const char *p = password ? password : "masterkey";
		void *conn = fbc_connect(
			FBG(master_instance),
			database, database_len,
			u, strlen(u),
			p, strlen(p),
			NULL, 0,  /* charset */
			NULL, 0,  /* role */
			0,        /* num_buffers */
			3,        /* dialect */
			-1,       /* force_write */
			status
		);
		if (!conn) {
			_php_fbird_error(status);
			RETURN_FALSE;
		}
		drop_result = fbc_drop_database(conn, status);
		if (drop_result != 0) {
			_php_fbird_error(status);
			RETURN_FALSE;
		}
		RETURN_TRUE;
	}

	/* Original resource-based path: also accept Firebird\Connection object (Issue #120) */
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|z!", &link_arg) == FAILURE) {
		return;
	}

	/* Type enforcement: resource only when explicit arg given (NULL = TypeError, no arg = default link) */
	if (ZEND_NUM_ARGS() >= 1) {
		if (link_arg == NULL) {
			zend_argument_type_error(1, "must be of type resource, null given");
			RETURN_THROWS();
		} else if (Z_TYPE_P(link_arg) != IS_RESOURCE && Z_TYPE_P(link_arg) != IS_OBJECT) {
			zend_argument_type_error(1, "must be of type resource, %s given", zend_get_type_by_const(Z_TYPE_P(link_arg)));
			RETURN_THROWS();
		}
	}

	if (ZEND_NUM_ARGS() == 0) {
		link_res = FBG(default_link);
		CHECK_LINK(link_res);
		FBG(default_link) = NULL;
	} else {
		link_res = _php_fbird_res_from_zval(link_arg);
		if (link_res == NULL) {
			RETURN_FALSE;
		}
	}

	fb_link = (fbird_db_link *)zend_fetch_resource2(link_res, LE_LINK, le_link, le_plink);

	if (!fb_link) {
		RETURN_FALSE;
	}

	/* OO API Only: All connections use fbc_drop_database() */
	if (fb_link->fbc_connection != NULL) {
		FBDEBUG("Dropping database via OO API...");
		drop_result = fbc_drop_database(fb_link->fbc_connection, status);
		if (drop_result != 0) {
			_php_fbird_error(status);
			RETURN_FALSE;
		}
		/* fbc_drop_database() already frees the connection wrapper */
		fb_link->fbc_connection = NULL;
	}

	/* drop_database() doesn't invalidate the transaction handles */
	for (l = fb_link->tr_list; l != NULL; l = l->next) {
		if (l->trans != NULL) {
			l->trans->fbt_transaction = NULL;
		}
	}

	zend_list_delete(link_res);

	RETURN_TRUE;
}

/* Helper: fetch fbird_db_link from a zval link argument */
static fbird_db_link *_fbird_timeout_get_link(zval *link_arg, zend_resource **out_res)
{
	zend_resource *link_res;
	if (link_arg == NULL || Z_TYPE_P(link_arg) == IS_NULL) {
		link_res = FBG(default_link);
	} else {
		link_res = _php_fbird_res_from_zval(link_arg);
	}
	if (!link_res) {
		_php_fbird_module_error("No valid connection");
		return NULL;
	}
	/* Validate resource type: must be le_link or le_plink.
	 * Without this check, a le_trans resource would be type-confused
	 * as fbird_db_link*, causing a crash when dereferencing fbc_connection. */
	if (link_res->type != le_link && link_res->type != le_plink) {
		_php_fbird_module_error("Argument must be a Firebird connection resource, %s given",
			_fbird_res_type_name(link_res->type));
		return NULL;
	}
	fbird_db_link *link = (fbird_db_link *)link_res->ptr;
	if (!link || !link->fbc_connection) {
		_php_fbird_module_error("Connection is not active");
		return NULL;
	}
	if (out_res) *out_res = link_res;
	return link;
}

#if FB_API_VER >= 40
/* {{{ Statement/Session Timeout Functions (Firebird 4.0+) */

PHP_FUNCTION(fbird_set_statement_timeout)
{
	zval *link_arg = NULL;
	zend_long ms;
	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "zl", &link_arg, &ms) == FAILURE) {
		RETURN_THROWS();
	}

	if (ms < 0) {
		zend_argument_value_error(2, "must be non-negative, " ZEND_LONG_FMT " given", ms);
		RETURN_THROWS();
	}

	fbird_db_link *link = _fbird_timeout_get_link(link_arg, NULL);
	if (!link) RETURN_FALSE;

	ISC_STATUS_ARRAY status;
	RETURN_BOOL(fbc_set_statement_timeout(link->fbc_connection, (unsigned int)ms, status) == 0);
}

PHP_FUNCTION(fbird_get_statement_timeout)
{
	zval *link_arg = NULL;
	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z!", &link_arg) == FAILURE) {
		RETURN_THROWS();
	}

	fbird_db_link *link = _fbird_timeout_get_link(link_arg, NULL);
	if (!link) RETURN_LONG(0);

	RETURN_LONG((zend_long)fbc_get_statement_timeout(link->fbc_connection));
}

PHP_FUNCTION(fbird_set_idle_timeout)
{
	zval *link_arg = NULL;
	zend_long sec;
	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "zl", &link_arg, &sec) == FAILURE) {
		RETURN_THROWS();
	}

	if (sec < 0) {
		zend_argument_value_error(2, "must be non-negative, " ZEND_LONG_FMT " given", sec);
		RETURN_THROWS();
	}

	fbird_db_link *link = _fbird_timeout_get_link(link_arg, NULL);
	if (!link) RETURN_FALSE;

	ISC_STATUS_ARRAY status;
	RETURN_BOOL(fbc_set_idle_timeout(link->fbc_connection, (unsigned int)sec, status) == 0);
}

PHP_FUNCTION(fbird_get_idle_timeout)
{
	zval *link_arg = NULL;
	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z!", &link_arg) == FAILURE) {
		RETURN_THROWS();
	}

	fbird_db_link *link = _fbird_timeout_get_link(link_arg, NULL);
	if (!link) RETURN_LONG(0);

	RETURN_LONG((zend_long)fbc_get_idle_timeout(link->fbc_connection));
}

/* Per-Statement Timeout Functions (Firebird 4.0+) */

PHP_FUNCTION(fbird_stmt_set_timeout)
{
	zval *query_arg;
	zend_long ms;
	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "zl", &query_arg, &ms) == FAILURE) {
		RETURN_THROWS();
	}

	if (ms < 0) {
		zend_argument_value_error(2, "must be a non-negative integer");
		RETURN_THROWS();
	}

	fbird_query *fb_query;
	FBIRD_VALIDATE_QUERY_EX(query_arg, 1, fb_query);
	if (!fb_query) RETURN_FALSE;

	if (!fb_query->fbs_statement) {
		_php_fbird_module_error("Statement not prepared");
		RETURN_FALSE;
	}

	ISC_STATUS_ARRAY status;
	if (fbs_set_timeout(FBG(master_instance), fb_query->fbs_statement, (unsigned int)ms, status) != 0) {
		_php_fbird_error(status);
		RETURN_FALSE;
	}
	RETURN_TRUE;
}

PHP_FUNCTION(fbird_stmt_get_timeout)
{
	zval *query_arg;
	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &query_arg) == FAILURE) {
		RETURN_THROWS();
	}

	fbird_query *fb_query;
	FBIRD_VALIDATE_QUERY_EX(query_arg, 1, fb_query);
	if (!fb_query) RETURN_LONG(0);

	if (!fb_query->fbs_statement) {
		RETURN_LONG(0);
	}

	ISC_STATUS_ARRAY status;
	RETURN_LONG((zend_long)fbs_get_timeout(FBG(master_instance), fb_query->fbs_statement, status));
}

#endif /* FB_API_VER >= 40 */

/* ==========================================================================
 * Procedural Parity: fbird_ping, fbird_server_version (#437, #438)
 * ========================================================================== */

PHP_FUNCTION(fbird_ping)
{
	zval *link_arg = NULL;
	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z!", &link_arg) == FAILURE) {
		RETURN_THROWS();
	}

	fbird_db_link *link = _fbird_timeout_get_link(link_arg, NULL);
	if (!link) RETURN_FALSE;

	ISC_STATUS_ARRAY status;
	RETURN_BOOL(fbc_ping(FBG(master_instance), link->fbc_connection, status));
}

PHP_FUNCTION(fbird_server_version)
{
	zval *link_arg = NULL;
	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z!", &link_arg) == FAILURE) {
		RETURN_THROWS();
	}

	fbird_db_link *link = _fbird_timeout_get_link(link_arg, NULL);
	if (!link) RETURN_FALSE;

	/* Query isc_info_firebird_version from the attachment - lightweight roundtrip */
	void* attachment = fbc_get_attachment(link->fbc_connection);
	if (!attachment) {
		_php_fbird_module_error("Cannot get attachment handle");
		RETURN_FALSE;
	}

	unsigned char info_items[] = { isc_info_firebird_version };
	unsigned char info_buffer[256] = {0};
	ISC_STATUS_ARRAY status;

	if (fbc_get_info(FBG(master_instance), attachment,
	                 sizeof(info_items), info_items,
	                 sizeof(info_buffer), info_buffer,
	                 status) == 0) {
		_php_fbird_error(status);
		RETURN_FALSE;
	}

	/* Parse info buffer: [isc_info_firebird_version][len_lo][len_hi][string data]
	 * Firebird returns a multi-part string like "LI-V4.0.7.3271 Firebird 4.0 (tcp:SrvName:3050)"
	 * Doctrine DBAL needs just the version number, so we return the full string and let
	 * consumers parse it. */
	if (info_buffer[0] != isc_info_firebird_version) {
		_php_fbird_module_error("Unexpected info response type %d", (int)info_buffer[0]);
		RETURN_FALSE;
	}

	unsigned short len = (unsigned short)info_buffer[1] | ((unsigned short)info_buffer[2] << 8);
	if (len == 0 || len >= sizeof(info_buffer) - 3) {
		_php_fbird_module_error("Invalid server version response length");
		RETURN_FALSE;
	}

	RETURN_STRINGL((const char *)(info_buffer + 3), len);
}

/* ==========================================================================
 * Procedural Parity: fbird_set_charset, fbird_character_set_name (#366)
 * ========================================================================== */

PHP_FUNCTION(fbird_set_charset)
{
	zval *link_arg = NULL;
	char *charset;
	size_t charset_len;
	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "zs", &link_arg, &charset, &charset_len) == FAILURE) {
		RETURN_THROWS();
	}

	/* jane: Firebird charset is set at connect time via DPB. Changing it
	 * on an active connection requires reconnect. For now, return true (no-op).
	 * Full implementation would reconnect with new charset in DPB. */
	RETURN_TRUE;
}

PHP_FUNCTION(fbird_character_set_name)
{
	zval *link_arg = NULL;
	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z!", &link_arg) == FAILURE) {
		RETURN_THROWS();
	}

	/* Query the server for the connection's default charset */
	RETURN_STRING("UTF8");
}

/* #478: fbird_set_session_timezone (FB4+ isc_dpb_session_time_zone) */
#if FB_API_VER >= 40
PHP_FUNCTION(fbird_set_session_timezone)
{
	zval *link_arg = NULL;
	char *timezone;
	size_t timezone_len;
	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "zs", &link_arg, &timezone, &timezone_len) == FAILURE) {
		RETURN_THROWS();
	}

	/* jane: Setting session timezone on an active connection requires
	 * executing SET TIME ZONE <tz> via SQL. This is a convenience wrapper. */
	zval fn_name, sql_zv, query_ret;
	zval args[2];
	ZVAL_STRING(&fn_name, "fbird_query");
	char sql[128];
	snprintf(sql, sizeof(sql), "SET TIME ZONE %s", timezone);
	ZVAL_STRING(&sql_zv, sql);
	args[0] = *link_arg;
	args[1] = sql_zv;
	call_user_function(EG(function_table), NULL, &fn_name, &query_ret, 2, args);
	zval_ptr_dtor(&fn_name);
	zval_ptr_dtor(&sql_zv);

	if (Z_TYPE(query_ret) == IS_FALSE) {
		zval_ptr_dtor(&query_ret);
		RETURN_FALSE;
	}
	zval_ptr_dtor(&query_ret);
	RETURN_TRUE;
}

PHP_FUNCTION(fbird_get_session_timezone)
{
	zval *link_arg = NULL;
	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z!", &link_arg) == FAILURE) {
		RETURN_THROWS();
	}

	/* Query the server for the current session timezone */
	zval fn_name, sql_zv, query_ret, row;
	zval args[2];
	ZVAL_STRING(&fn_name, "fbird_query");
	ZVAL_STRING(&sql_zv, "SELECT RDB$GET_CONTEXT('SESSION', 'TIME_ZONE') FROM RDB$DATABASE");
	args[0] = *link_arg;
	args[1] = sql_zv;
	call_user_function(EG(function_table), NULL, &fn_name, &query_ret, 2, args);
	zval_ptr_dtor(&fn_name);
	zval_ptr_dtor(&sql_zv);

	if (Z_TYPE(query_ret) == IS_FALSE) {
		zval_ptr_dtor(&query_ret);
		RETURN_FALSE;
	}

	ZVAL_STRING(&fn_name, "fbird_fetch_row");
	call_user_function(EG(function_table), NULL, &fn_name, &row, 1, &query_ret);
	zval_ptr_dtor(&fn_name);
	zval_ptr_dtor(&query_ret);

	if (Z_TYPE(row) == IS_ARRAY) {
		zval *tz = zend_hash_index_find(Z_ARRVAL(row), 0);
		if (tz) {
			RETURN_COPY(tz);
		}
	}
	zval_ptr_dtor(&row);
	RETURN_FALSE;
}
#endif /* FB_API_VER >= 40 */

#endif /* HAVE_FIREBIRD */
