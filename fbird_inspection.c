/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_firebird.h"
#include "php_fbird_includes.h"

#include "firebird_utils.h"

/* =============================================================================
 * OO API Only Helper Functions for Inspection
 *
 * These helpers use fbs_* functions for SQL execution via the modern OO API.
 * All connections MUST have fbc_connection (OO API is the only supported path).
 * ============================================================================= */

/**
 * Execute a DELETE statement with one BIGINT parameter using OO API.
 * Used for killing attachments.
 *
 * @param link Database connection
 * @param trans Transaction
 * @param attachment_id The attachment ID to delete
 * @return SUCCESS or FAILURE
 */
static int _fbird_exec_kill(fbird_db_link *link, fbird_transaction *trans, ISC_INT64 attachment_id)
{
	ISC_STATUS status[256];
	static const char *sql = "DELETE FROM MON$ATTACHMENTS WHERE MON$ATTACHMENT_ID = ?";
	void *stmt = NULL;
	void *attachment = NULL;
	void *kill_trans = NULL;
	void *transaction = NULL;
	int result = FAILURE;

	/* OO API Only: Require fbc_connection */
	if (!link->fbc_connection) {
		_php_fbird_module_error("fbird_kill_attachment requires OO API connection (fbc_connection required)");
		return FAILURE;
	}

	/* Get OO API handles */
	attachment = fbc_get_attachment(link->fbc_connection);
	if (!attachment) {
		_php_fbird_module_error("fbird_kill_attachment: Failed to get attachment from OO API connection");
		return FAILURE;
	}

	/* Issue #583: run the DELETE on a dedicated transaction and COMMIT it
	 * before returning. The engine applies the kill only when the deleting
	 * transaction commits; committing the caller's transaction as a side
	 * effect would be wrong, and leaving it pending made the function
	 * report success while the kill never took effect. */
	kill_trans = fbt_start(FBG(master_instance), attachment, 0, NULL, status);
	if (!kill_trans) {
		_php_fbird_error(status);
		return FAILURE;
	}

	/* fbs_prepare()/fbs_execute() take the raw ITransaction*, not the
	 * fb::Transaction wrapper fbt_start() returns. */
	transaction = fbt_get_handle(kill_trans);
	if (!transaction) {
		_php_fbird_module_error("fbird_kill_attachment: failed to get transaction handle");
		fbt_rollback(kill_trans, status);
		fbt_free(kill_trans);
		return FAILURE;
	}

	/* Prepare statement */
	stmt = fbs_prepare(
		FBG(master_instance),
		attachment,
		transaction,
		NULL,  /* #593: statement freed synchronously below - no registry enrollment needed */
		sql,
		0,  /* null-terminated */
		SQL_DIALECT_V6,
		status
	);

	if (!stmt) {
		_php_fbird_error(status);
		fbt_rollback(kill_trans, status);
		fbt_free(kill_trans);
		return FAILURE;
	}

	/* Get input metadata for parameter binding */
	void *in_metadata = fbs_get_input_metadata(FBG(master_instance), stmt, status);
	if (!in_metadata) {
		_php_fbird_error(status);
		fbs_free(stmt, status);
		fbt_rollback(kill_trans, status);
		fbt_free(kill_trans);
		return FAILURE;
	}
	/* Build input message buffer - Issue #583.
	 * The offsets are ENGINE-DEFINED (IMessageMetadata), not the legacy
	 * XSQLDA layout: for this statement FB3 reports null_off=8, data_off=0,
	 * msglen=10. The old code hardcoded null@0 + value@8, so the engine read
	 * its null flag from the middle of the value (nonzero = NULL) and the
	 * DELETE matched zero rows - reporting success while the attachment
	 * survived. Derive everything from the metadata instead. */
	{
		unsigned msg_len = fbm_get_message_length(FBG(master_instance), in_metadata);
		unsigned null_off = fbm_get_null_offset(FBG(master_instance), in_metadata, 0);
		unsigned data_off = fbm_get_offset(FBG(master_instance), in_metadata, 0);
		unsigned char *in_msg;

		if (msg_len < sizeof(short) || data_off + sizeof(ISC_INT64) > msg_len) {
			_php_fbird_module_error("fbird_kill_attachment: unexpected input metadata layout");
			fbs_free(stmt, status);
			fbm_release(in_metadata);
			fbt_rollback(kill_trans, status);
			fbt_free(kill_trans);
			return FAILURE;
		}

		in_msg = ecalloc(1, msg_len);
		*(short *)(in_msg + null_off) = 0;  /* 0 = not NULL */
		*(ISC_INT64 *)(in_msg + data_off) = attachment_id;

		/* Execute the statement */
		if (!fbs_execute(
			FBG(master_instance),
			stmt,
			transaction,
			in_msg,
			in_metadata,
			NULL,  /* no output */
			NULL,
			status
		)) {
			_php_fbird_error(status);
			efree(in_msg);
			fbs_free(stmt, status);
			fbm_release(in_metadata);
			fbt_rollback(kill_trans, status);
			fbt_free(kill_trans);
			return FAILURE;
		}
		efree(in_msg);
	}

	/* Issue #583: release the input metadata - the old path leaked it. */
	fbm_release(in_metadata);
	fbs_free(stmt, status);

	/* The engine applies the kill when the deleting transaction commits. */
	if (fbt_commit(kill_trans, status) != 0) {
		_php_fbird_error(status);
		fbt_free(kill_trans);
		return FAILURE;
	}
	fbt_free(kill_trans);

	result = SUCCESS;
	return result;
}

/**
 * Execute DROP TABLE statement using OO API.
 *
 * @param link Database connection
 * @param trans Transaction
 * @param table_name Name of table to drop
 * @return SUCCESS or FAILURE
 */
static int _fbird_drop_table(fbird_db_link *link, fbird_transaction *trans, const char *table_name)
{
	ISC_STATUS status[256];
	char *drop_sql = NULL;
	void *stmt = NULL;
	void *attachment = NULL;
	void *transaction = NULL;

	/* OO API Only: Require fbc_connection */
	if (!link->fbc_connection) {
		_php_fbird_module_error("fbird_drop_table_force requires OO API connection (fbc_connection required)");
		return FAILURE;
	}

	if (!trans->fbt_transaction) {
		_php_fbird_module_error("fbird_drop_table_force requires OO API transaction (fbt_transaction required)");
		return FAILURE;
	}

	/* Get OO API handles */
	attachment = fbc_get_attachment(link->fbc_connection);
	if (!attachment) {
		return FAILURE;
	}

	transaction = fbt_get_handle(trans->fbt_transaction);
	if (!transaction) {
		return FAILURE;
	}

	/* Build DROP statement */
	spprintf(&drop_sql, 0, "DROP TABLE %s", table_name);

	/* Prepare statement */
	stmt = fbs_prepare(
		FBG(master_instance),
		attachment,
		transaction,
		NULL,  /* #593: statement freed synchronously below - no registry enrollment needed */
		drop_sql,
		0,  /* null-terminated */
		SQL_DIALECT_V6,
		status
	);

	if (!stmt) {
		_php_fbird_error(status);
		efree(drop_sql);
		return FAILURE;
	}

	/* Execute the statement (no parameters) */
	if (!fbs_execute(
		FBG(master_instance),
		stmt,
		transaction,
		NULL,  /* no input */
		NULL,
		NULL,  /* no output */
		NULL,
		status
	)) {
		_php_fbird_error(status);
		fbs_free(stmt, status);
		efree(drop_sql);
		return FAILURE;
	}

	/* Free statement before commit */
	fbs_free(stmt, status);
	efree(drop_sql);

	/* Commit for DDL visibility
	 * fbt_commit returns 0 on success, -1 on error. */
	if (fbt_commit(trans->fbt_transaction, status) != 0) {
		_php_fbird_error(status);
		return FAILURE;
	}

	/* CRITICAL: Clean up the wrapper after commit.
	 * fbt_commit() does NOT free the wrapper (verified in firebird_utils.cpp).
	 * The PHP resource destructor (_php_fbird_free_trans) only calls fbt_rollback()
	 * for uncommitted transactions (when fbt_transaction != NULL).
	 * For committed transactions, the destructor skips rollback, leaving the wrapper
	 * orphaned. We MUST call fbt_free() here to prevent memory leak.
	 *
	 * Setting trans->fbt_transaction = NULL tells the destructor the transaction
	 * is already handled, preventing double-free attempts. */
	fbt_free(trans->fbt_transaction);
	trans->fbt_transaction = NULL;

	/* Remove this transaction from all connection tr_lists to prevent
	 * use-after-free during PHP shutdown. The destructor tries to traverse
	 * db_link[i]->tr_list, but if cleanup order is unexpected, db_link[i]
	 * could be stale. Setting db_link[i] = NULL tells the destructor to skip.
	 * This mirrors what _php_fbird_commit_link() does when closing connections.
	 *
	 * IMPORTANT (Issue #554): If this is the default transaction
	 * (is_default flag), keep the node and just clear its trans pointer so
	 * the slot can be reused. Explicit transaction nodes are unlinked and freed. */
	for (unsigned short i = 0; i < trans->link_cnt; ++i) {
		if (trans->db_link[i] != NULL) {
			fbird_tr_list **l;
			for (l = &trans->db_link[i]->tr_list; *l != NULL; l = &(*l)->next) {
				if ((*l)->trans == trans) {
					if (trans->is_default) {
						/* Default tx node: don't free - just clear the trans
						 * pointer so it can be reused. */
						(*l)->trans = NULL;
					} else {
						/* Explicit tx node: unlink and free */
						fbird_tr_list *p = *l;
						*l = p->next;
						efree(p);
					}
					break;
				}
			}
			trans->db_link[i] = NULL;
		}
	}

	/* Issue #575: the default-tx struct is NOT a le_trans resource (no
	 * destructor will free it) and this path just detached it from every
	 * tr_list, so the link-close commit path can no longer reach it.
	 * Detach query/batch back-refs (#594/#599 registries) and efree here,
	 * mirroring what _php_fbird_commit_link() does for the default tx.
	 * Explicit transactions keep resource ownership: their dtor frees. */
	if (trans->is_default) {
		_php_fbird_trans_detach_queries(trans);
		efree(trans);
	}

	return SUCCESS;
}


PHP_FUNCTION(fbird_kill_attachment)
{
	zval *link_arg;
	zend_long attachment_id;
	fbird_db_link *link;
	fbird_transaction *trans;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "zl", &link_arg, &attachment_id) == FAILURE) {
		return;
	}

	PHP_FBIRD_LINK_TRANS(link_arg, link, trans);

	if (_fbird_exec_kill(link, trans, (ISC_INT64) attachment_id) == FAILURE) {
		RETURN_FALSE;
	}

	RETURN_TRUE;
}

PHP_FUNCTION(fbird_list_table_blockers)
{
	ISC_STATUS status[256];
	zval *link_arg;
	char *table_name;
	size_t table_name_len;
	fbird_db_link *link;
	fbird_transaction *trans;
	void *stmt = NULL;
	void *attachment = NULL;
	void *transaction = NULL;
	void *in_metadata = NULL;
	void *out_metadata = NULL;

	RESET_ERRMSG;

	/* SQL to find attachments using the table in statements.
	 * CONTAINING is Firebird's BLOB-aware, case-insensitive substring search. */
	static const char *sql =
		"SELECT DISTINCT A.MON$ATTACHMENT_ID, A.MON$USER "
		"FROM MON$ATTACHMENTS A "
		"JOIN MON$STATEMENTS S ON S.MON$ATTACHMENT_ID = A.MON$ATTACHMENT_ID "
		"WHERE A.MON$ATTACHMENT_ID <> CURRENT_CONNECTION "
		"AND S.MON$SQL_TEXT CONTAINING ?";

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "zs", &link_arg, &table_name, &table_name_len) == FAILURE) {
		return;
	}

	PHP_FBIRD_LINK_TRANS(link_arg, link, trans);

	/* OO API Only: Require fbc_connection */
	if (!link->fbc_connection) {
		_php_fbird_module_error("fbird_list_table_blockers requires OO API connection (fbc_connection required)");
		RETURN_FALSE;
	}

	if (!trans->fbt_transaction) {
		_php_fbird_module_error("fbird_list_table_blockers requires OO API transaction (fbt_transaction required)");
		RETURN_FALSE;
	}

	/* Get OO API handles */
	attachment = fbc_get_attachment(link->fbc_connection);
	if (!attachment) {
		_php_fbird_module_error("Failed to get attachment from OO API connection");
		RETURN_FALSE;
	}

	transaction = fbt_get_handle(trans->fbt_transaction);
	if (!transaction) {
		_php_fbird_module_error("Failed to get transaction handle from OO API transaction");
		RETURN_FALSE;
	}

	/* Prepare statement */
	stmt = fbs_prepare(
		FBG(master_instance),
		attachment,
		transaction,
		NULL,  /* #593: statement freed synchronously below - no registry enrollment needed */
		sql,
		0,  /* null-terminated */
		SQL_DIALECT_V6,
		status
	);

	if (!stmt) {
		_php_fbird_error(status);
		RETURN_FALSE;
	}

	/* Get input metadata for parameter binding */
	in_metadata = fbs_get_input_metadata(FBG(master_instance), stmt, status);
	if (!in_metadata) {
		_php_fbird_error(status);
		fbs_free(stmt, status);
		RETURN_FALSE;
	}

	/* Get output metadata for result fetching */
	out_metadata = fbs_get_output_metadata(FBG(master_instance), stmt, status);
	if (!out_metadata) {
		_php_fbird_error(status);
		fbs_free(stmt, status);
		RETURN_FALSE;
	}

	/* Build input message buffer for VARCHAR parameter
	 * VARCHAR format: 2-byte length + data
	 * With null indicator at offset determined by metadata
	 *
	 * Simple layout for single VARCHAR parameter:
	 * - bytes 0-1: null indicator (short)
	 * - bytes 2-3: varchar length (short)
	 * - bytes 4+:  varchar data
	 */
	size_t in_msg_size = 4 + table_name_len + 4; /* null ind + len + data + padding */
	unsigned char *in_msg = emalloc(in_msg_size);
	memset(in_msg, 0, in_msg_size);

	/* Set null indicator (0 = not null) at offset 0 */
	*(short *)&in_msg[0] = 0;

	/* Set VARCHAR length at offset 2 */
	*(short *)&in_msg[2] = (short)table_name_len;

	/* Copy string data at offset 4 */
	memcpy(&in_msg[4], table_name, table_name_len);

	/* Open cursor for fetching results */
	if (!fbs_open_cursor(
		FBG(master_instance),
		stmt,
		transaction,
		in_msg,
		in_metadata,
		0,  /* cursor_flags */
		status
	)) {
		_php_fbird_error(status);
		efree(in_msg);
		fbs_free(stmt, status);
		RETURN_FALSE;
	}

	efree(in_msg);

	/* Prepare output buffer
	 * Output columns: MON$ATTACHMENT_ID (BIGINT), MON$USER (VARCHAR/CHAR)
	 *
	 * Layout:
	 * - bytes 0-1: null indicator for attachment_id
	 * - bytes 8-15: attachment_id (BIGINT, aligned to 8)
	 * - bytes 16-17: null indicator for user
	 * - bytes 18-19: varchar length for user
	 * - bytes 20+: user data (up to 255 chars)
	 */
	unsigned char out_msg[512];
	memset(out_msg, 0, sizeof(out_msg));

	array_init(return_value);

	/* Fetch loop */
	while (1) {
		memset(out_msg, 0, sizeof(out_msg));

		int fetch_result = fbs_fetch(
			FBG(master_instance),
			stmt,
			out_msg,
			status
		);

		if (fetch_result == 0) {
			/* EOF - no more rows */
			break;
		} else if (fetch_result < 0) {
			/* Error */
			_php_fbird_error(status);
			fbs_close_cursor(stmt, status);
			fbs_free(stmt, status);
			/* Return partial result */
			return;
		}

		/* Extract attachment_id (BIGINT at offset 8, null indicator at offset 0) */
		short null_ind_id = *(short *)&out_msg[0];
		ISC_INT64 attachment_id = *(ISC_INT64 *)&out_msg[8];

		/* Extract user (VARCHAR at offset 16+, null indicator at offset 16) */
		short null_ind_user = *(short *)&out_msg[16];
		short user_len = *(short *)&out_msg[18];
		char *user_data = (char *)&out_msg[20];

		/* Build result row */
		zval row;
		array_init(&row);

		if (null_ind_id == 0) {
			add_assoc_long(&row, "attachment_id", (zend_long)attachment_id);
		} else {
			add_assoc_null(&row, "attachment_id");
		}

		if (null_ind_user == 0 && user_len > 0) {
			/* Trim trailing spaces */
			while (user_len > 0 && user_data[user_len - 1] == ' ') {
				user_len--;
			}
			add_assoc_stringl(&row, "user", user_data, user_len);
		} else {
			add_assoc_null(&row, "user");
		}

		add_next_index_zval(return_value, &row);
	}

	fbs_close_cursor(stmt, status);
	fbs_free(stmt, status);
}

PHP_FUNCTION(fbird_drop_table_force)
{
	zval *link_arg;
	char *table_name;
	size_t table_name_len;
	fbird_db_link *link;
	fbird_transaction *trans;
	int result;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "zs", &link_arg, &table_name, &table_name_len) == FAILURE) {
		return;
	}

	PHP_FBIRD_LINK_TRANS(link_arg, link, trans);

	/* Note: Blocker detection via MON$SQL_TEXT requires complex BLOB handling.
	 * For now, we skip the blocker-killing step and just do the DROP.
	 * In most cases, DDL will fail cleanly if there are active locks. */

	result = _fbird_drop_table(link, trans, table_name);

	if (result == SUCCESS) {
		RETURN_TRUE;
	} else {
		RETURN_FALSE;
	}
}
