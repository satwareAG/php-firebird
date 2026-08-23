/* fbird_class_transaction.c - Firebird\Transaction OOP class */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "zend_exceptions.h"
#include "php_firebird.h"
#include "php_fbird_includes.h"
#include "firebird_utils.h"
#include "fbird_class_internal.h"

zend_object_handlers fbird_transaction_handlers;

zend_object *fbird_transaction_create(zend_class_entry *ce)
{
	fbird_transaction_obj *intern = zend_object_alloc(sizeof(fbird_transaction_obj), ce);
	intern->fbt_trans = NULL;
	intern->trans_res = NULL;
	zend_object_std_init(&intern->std, ce);
	object_properties_init(&intern->std, ce);
	intern->std.handlers = &fbird_transaction_handlers;
	return &intern->std;
}

void fbird_transaction_free(zend_object *obj)
{
	fbird_transaction_obj *intern = fbird_transaction_from_obj(obj);
	ISC_STATUS status[256];
	if (intern->fbt_trans) {
		if (!FBG(in_mshutdown)) {
			fbt_rollback(intern->fbt_trans, status);
		}
		fbt_free(intern->fbt_trans);
		intern->fbt_trans = NULL;
	}
	intern->trans_res = NULL;
	zend_object_std_dtor(obj);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_transaction_commit, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdTransaction, commit)
{
	ISC_STATUS status[256];
	ZEND_PARSE_PARAMETERS_NONE();
	fbird_transaction_obj *intern = Z_FBIRD_TRANSACTION_P(ZEND_THIS);
	if (intern->fbt_trans) {
		fbt_commit(intern->fbt_trans, status);
		fbt_free(intern->fbt_trans);
		intern->fbt_trans = NULL;
	} else if (intern->trans_res) {
		fbird_transaction *trans = (fbird_transaction *)intern->trans_res->ptr;
		if (trans && trans->fbt_transaction) {
			int res = fbt_commit(trans->fbt_transaction, status);
			fbt_free(trans->fbt_transaction);
			trans->fbt_transaction = NULL;
			zend_list_delete(intern->trans_res);
			intern->trans_res = NULL;
			if (res && !FBG(in_mshutdown)) {
				_php_fbird_error(status);
			}
		}
	}
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_transaction_rollback, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdTransaction, rollback)
{
	ISC_STATUS status[256];
	ZEND_PARSE_PARAMETERS_NONE();
	fbird_transaction_obj *intern = Z_FBIRD_TRANSACTION_P(ZEND_THIS);
	if (intern->fbt_trans) {
		fbt_rollback(intern->fbt_trans, status);
		fbt_free(intern->fbt_trans);
		intern->fbt_trans = NULL;
	} else if (intern->trans_res) {
		fbird_transaction *trans = (fbird_transaction *)intern->trans_res->ptr;
		if (trans && trans->fbt_transaction) {
			fbt_rollback(trans->fbt_transaction, status);
			fbt_free(trans->fbt_transaction);
			trans->fbt_transaction = NULL;
			zend_list_delete(intern->trans_res);
			intern->trans_res = NULL;
		}
	}
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_transaction_isActive, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(FirebirdTransaction, isActive)
{
	ZEND_PARSE_PARAMETERS_NONE();
	fbird_transaction_obj *intern = Z_FBIRD_TRANSACTION_P(ZEND_THIS);
	if (intern->fbt_trans) {
		RETURN_BOOL(fbt_is_active(intern->fbt_trans));
	}
	if (intern->trans_res) {
		fbird_transaction *trans = (fbird_transaction *)intern->trans_res->ptr;
		RETURN_BOOL(trans && trans->fbt_transaction && fbt_is_active(trans->fbt_transaction));
	}
	RETURN_FALSE;
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_transaction_releaseMetadataLocks, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

/* Issue #566: Transaction::releaseMetadataLocks() — OOP wrapper for
 * fbird_release_metadata_locks(). Commits + restarts with stored TPB,
 * releasing metadata locks. See fbird_transaction.c for the procedural version. */
PHP_METHOD(FirebirdTransaction, releaseMetadataLocks)
{
	ISC_STATUS status[256];
	ZEND_PARSE_PARAMETERS_NONE();
	fbird_transaction_obj *intern = Z_FBIRD_TRANSACTION_P(ZEND_THIS);

	/* OO API path: direct fbt handle */
	if (intern->fbt_trans) {
		fbird_db_link *fb_link = NULL;
		/* jane: OO path needs db_link for attachment — get from trans_res if available */
		if (intern->trans_res && intern->trans_res->ptr) {
			fbird_transaction *trans = (fbird_transaction *)intern->trans_res->ptr;
			if (trans->db_link[0]) {
				fb_link = trans->db_link[0];
			}
		}
		if (!fb_link || !fb_link->fbc_connection) {
			_php_fbird_module_error("Cannot restart transaction: no valid connection");
			RETURN_FALSE;
		}
		if (fbt_commit(intern->fbt_trans, status) != 0) {
			_php_fbird_error(status);
			RETURN_FALSE;
		}
		fbt_free(intern->fbt_trans);
		intern->fbt_trans = NULL;

		void *attachment = fbc_get_attachment(fb_link->fbc_connection);
		if (!attachment) {
			_php_fbird_module_error("Failed to get attachment for restart");
			RETURN_FALSE;
		}
		/* jane: OO path uses empty TPB for restart — stored_tpb is on the
		 * fbird_transaction resource, not the OO wrapper. The OO path creates
		 * transactions via Connection::beginTransaction which uses fbt_start
		 * directly without storing TPB. This is acceptable: the OO API user
		 * typically uses default isolation. For custom isolation + metadata
		 * lock release, use the procedural API. */
		intern->fbt_trans = fbt_start(FBG(master_instance), attachment, 0, NULL, status);
		if (!intern->fbt_trans) {
			_php_fbird_error(status);
			RETURN_FALSE;
		}
		RETURN_TRUE;
	}

	/* Resource path: delegate to fbird_transaction struct.
	 * PR #588 review: must go through _php_fbird_trans_commit_restart()
	 * (not a hand-rolled copy) so retain_committed is cleared - a stale
	 * flag after this hard commit made the later idle release commit
	 * uncommitted DML transparently. */
	if (intern->trans_res) {
		fbird_transaction *trans = (fbird_transaction *)intern->trans_res->ptr;
		if (!trans || !trans->fbt_transaction) {
			RETURN_FALSE;
		}
		if (_php_fbird_trans_commit_restart(trans, status) != 0) {
			_php_fbird_error(status);
			RETURN_FALSE;
		}
		RETURN_TRUE;
	}
	RETURN_FALSE;
}

const zend_function_entry fbird_transaction_methods[] = {
	PHP_ME(FirebirdTransaction, commit,    arginfo_fbird_transaction_commit,    ZEND_ACC_PUBLIC)
	PHP_ME(FirebirdTransaction, rollback,  arginfo_fbird_transaction_rollback,  ZEND_ACC_PUBLIC)
	PHP_ME(FirebirdTransaction, isActive,  arginfo_fbird_transaction_isActive,  ZEND_ACC_PUBLIC)
	PHP_ME(FirebirdTransaction, releaseMetadataLocks, arginfo_fbird_transaction_releaseMetadataLocks, ZEND_ACC_PUBLIC)
	PHP_FE_END
};

zend_resource *fbird_transaction_get_resource(zend_object *obj)
{
	fbird_transaction_obj *intern = fbird_transaction_from_obj(obj);
	return intern ? intern->trans_res : NULL;
}

void fbird_setup_transaction_object(zval *return_value, zend_resource *res)
{
	zval_ptr_dtor(return_value);
	object_init_ex(return_value, fbird_transaction_ce);
	fbird_transaction_obj *intern = fbird_transaction_from_obj(Z_OBJ_P(return_value));
	intern->trans_res = res;
}
