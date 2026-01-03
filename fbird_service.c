/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"

#if HAVE_FIREBIRD

#include "php_firebird.h"
#include "php_fbird_includes.h"
#include "firebird_utils.h"

typedef struct {
	void *handle; /* void* to support 64-bit handles */
	char *hostname;
	char *username;
	zend_resource *res;
	void *fbsvc_service; /* OO API ServiceWrapper* (Phase 8) */
	pid_t created_pid;   /* PID when service was created (fork detection, Issue #56) */
} fbird_service;

static int le_service;

static void _php_fbird_free_service(zend_resource *rsrc)
{
	fbird_service *sv = (fbird_service *) rsrc->ptr;

	/* Guard 1: NULL pointer check (Issue #56)
	 * In forked PHP workers, rsrc->ptr may be NULL when inherited resource
	 * descriptors are destroyed during child process shutdown. */
	if (sv == NULL) {
		return;
	}

#ifndef PHP_WIN32
	/* Guard 2: Fork-safety - Global level (Issue #22, #36, #56 pattern)
	 * After pcntl_fork(), child inherits global state including master_instance.
	 * Attempting to detach handles in child that were created in parent causes
	 * segfault. Only the original process should perform cleanup. */
	pid_t current_pid = getpid();
	if (IBG(init_pid) != 0 && current_pid != IBG(init_pid)) {
		/* In forked child - just free the struct, don't call Firebird API */
		if (sv->hostname) {
			efree(sv->hostname);
		}
		if (sv->username) {
			efree(sv->username);
		}
		efree(sv);
		return;
	}

	/* Guard 3: Fork-safety - Service level
	 * Even if global init_pid matches, this specific service may have been
	 * created in a different process (e.g., worker spawned after module init). */
	if (sv->created_pid != 0 && current_pid != sv->created_pid) {
		if (sv->hostname) {
			efree(sv->hostname);
		}
		if (sv->username) {
			efree(sv->username);
		}
		efree(sv);
		return;
	}
#endif

	/* Guard 4: MSHUTDOWN safety
	 * During module shutdown, master_instance may be in undefined state.
	 * Skip API calls but still free PHP-allocated memory. */
	if (IBG(in_mshutdown)) {
		if (sv->hostname) {
			efree(sv->hostname);
		}
		if (sv->username) {
			efree(sv->username);
		}
		efree(sv);
		return;
	}

	/* Guard 5: master_instance validation before OO API calls */
	if (sv->fbsvc_service != NULL && IBG(master_instance) != NULL) {
		fbsvc_detach(IBG(master_instance), sv->fbsvc_service, IB_STATUS);
		fbsvc_free(sv->fbsvc_service);
		sv->fbsvc_service = NULL;
	}

	/* Legacy API cleanup - only if handle is valid */
	if (sv->handle != 0) {
		if (isc_service_detach(IB_STATUS, (isc_svc_handle *)&sv->handle)) {
			_php_fbird_error();
		}
	}

	/* Free PHP-allocated memory */
	if (sv->hostname) {
		efree(sv->hostname);
	}
	if (sv->username) {
		efree(sv->username);
	}

	efree(sv);
}

/* the svc api seems to get confused after an error has occurred,
   so invalidate the handle on errors */
#define FBIRD_SVC_ERROR(svm) \
	do { zend_list_delete(svm->res); _php_fbird_error(); } while (0)


void php_fbird_service_minit(INIT_FUNC_ARGS)
{
	le_service = zend_register_list_destructors_ex(_php_fbird_free_service, NULL,
		LE_SCVH, module_number);

	/* backup options */
	REGISTER_LONG_CONSTANT("FBIRD_BKP_IGNORE_CHECKSUMS", isc_spb_bkp_ignore_checksums, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_BKP_IGNORE_LIMBO", isc_spb_bkp_ignore_limbo, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_BKP_METADATA_ONLY", isc_spb_bkp_metadata_only, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_BKP_NO_GARBAGE_COLLECT", isc_spb_bkp_no_garbage_collect, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_BKP_OLD_DESCRIPTIONS", isc_spb_bkp_old_descriptions, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_BKP_NON_TRANSPORTABLE", isc_spb_bkp_non_transportable, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_BKP_CONVERT", isc_spb_bkp_convert, CONST_PERSISTENT);

	/* restore options */
	REGISTER_LONG_CONSTANT("FBIRD_RES_DEACTIVATE_IDX", isc_spb_res_deactivate_idx, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_RES_NO_SHADOW", isc_spb_res_no_shadow, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_RES_NO_VALIDITY", isc_spb_res_no_validity, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_RES_ONE_AT_A_TIME", isc_spb_res_one_at_a_time, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_RES_REPLACE", isc_spb_res_replace, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_RES_CREATE", isc_spb_res_create, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_RES_USE_ALL_SPACE", isc_spb_res_use_all_space, CONST_PERSISTENT);

	/* manage options */
	REGISTER_LONG_CONSTANT("FBIRD_PRP_PAGE_BUFFERS", isc_spb_prp_page_buffers, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_PRP_SWEEP_INTERVAL", isc_spb_prp_sweep_interval, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_PRP_SHUTDOWN_DB", isc_spb_prp_shutdown_db, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_PRP_DENY_NEW_TRANSACTIONS", isc_spb_prp_deny_new_transactions, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_PRP_DENY_NEW_ATTACHMENTS", isc_spb_prp_deny_new_attachments, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_PRP_RESERVE_SPACE", isc_spb_prp_reserve_space, CONST_PERSISTENT);
	  REGISTER_LONG_CONSTANT("FBIRD_PRP_RES_USE_FULL", isc_spb_prp_res_use_full, CONST_PERSISTENT);
	  REGISTER_LONG_CONSTANT("FBIRD_PRP_RES", isc_spb_prp_res, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_PRP_WRITE_MODE", isc_spb_prp_write_mode, CONST_PERSISTENT);
	  REGISTER_LONG_CONSTANT("FBIRD_PRP_WM_ASYNC", isc_spb_prp_wm_async, CONST_PERSISTENT);
	  REGISTER_LONG_CONSTANT("FBIRD_PRP_WM_SYNC", isc_spb_prp_wm_sync, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_PRP_ACCESS_MODE", isc_spb_prp_access_mode, CONST_PERSISTENT);
	  REGISTER_LONG_CONSTANT("FBIRD_PRP_AM_READONLY", isc_spb_prp_am_readonly, CONST_PERSISTENT);
	  REGISTER_LONG_CONSTANT("FBIRD_PRP_AM_READWRITE", isc_spb_prp_am_readwrite, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_PRP_SET_SQL_DIALECT", isc_spb_prp_set_sql_dialect, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_PRP_ACTIVATE", isc_spb_prp_activate, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_PRP_DB_ONLINE", isc_spb_prp_db_online, CONST_PERSISTENT);

	/* repair options */
	REGISTER_LONG_CONSTANT("FBIRD_RPR_CHECK_DB", isc_spb_rpr_check_db, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_RPR_IGNORE_CHECKSUM", isc_spb_rpr_ignore_checksum, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_RPR_KILL_SHADOWS", isc_spb_rpr_kill_shadows, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_RPR_MEND_DB", isc_spb_rpr_mend_db, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_RPR_VALIDATE_DB", isc_spb_rpr_validate_db, CONST_PERSISTENT);
	  REGISTER_LONG_CONSTANT("FBIRD_RPR_FULL", isc_spb_rpr_full, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_RPR_SWEEP_DB", isc_spb_rpr_sweep_db, CONST_PERSISTENT);

	/* db info arguments */
	REGISTER_LONG_CONSTANT("FBIRD_STS_DATA_PAGES", isc_spb_sts_data_pages, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_STS_DB_LOG", isc_spb_sts_db_log, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_STS_HDR_PAGES", isc_spb_sts_hdr_pages, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_STS_IDX_PAGES", isc_spb_sts_idx_pages, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_STS_SYS_RELATIONS", isc_spb_sts_sys_relations, CONST_PERSISTENT);

	/* server info arguments */
	REGISTER_LONG_CONSTANT("FBIRD_SVC_SERVER_VERSION", isc_info_svc_server_version, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_SVC_IMPLEMENTATION", isc_info_svc_implementation, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_SVC_GET_ENV", isc_info_svc_get_env, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_SVC_GET_ENV_LOCK", isc_info_svc_get_env_lock, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_SVC_GET_ENV_MSG", isc_info_svc_get_env_msg, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_SVC_USER_DBPATH", isc_info_svc_user_dbpath, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_SVC_SVR_DB_INFO", isc_info_svc_svr_db_info, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_SVC_GET_USERS", isc_info_svc_get_users, CONST_PERSISTENT);
}

static void _php_fbird_user(INTERNAL_FUNCTION_PARAMETERS, char operation)
{
	/* user = 0, password = 1, first_name = 2, middle_name = 3, last_name = 4 */
	static char const user_flags[] = { isc_spb_sec_username, isc_spb_sec_password,
	    isc_spb_sec_firstname, isc_spb_sec_middlename, isc_spb_sec_lastname };
	char buf[128], *args[] = { NULL, NULL, NULL, NULL, NULL };
	int i, args_len[] = { 0, 0, 0, 0, 0 };
	unsigned short spb_len = 1;
	zval *res;
	fbird_service *svm;

	RESET_ERRMSG;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS(),
			(operation == isc_action_svc_delete_user) ? "rs" : "rss|sss",
			&res, &args[0], &args_len[0], &args[1], &args_len[1], &args[2], &args_len[2],
			&args[3], &args_len[3], &args[4], &args_len[4])) {
		RETURN_FALSE;
	}

	svm = (fbird_service *)zend_fetch_resource_ex(res, "Firebird service manager handle",
		le_service);

	buf[0] = operation;

	for (i = 0; i < sizeof(user_flags); ++i) {
		if (args[i] != NULL) {
			int chunk = slprintf(&buf[spb_len], sizeof(buf) - spb_len, "%c%c%c%s",
				user_flags[i], (char)args_len[i], (char)(args_len[i] >> 8), args[i]);

			if ((spb_len + chunk) > sizeof(buf) || chunk <= 0) {
				_php_fbird_module_error("Internal error: insufficient buffer space for SPB (%d)", spb_len);
				RETURN_FALSE;
			}
			spb_len += chunk;
		}
	}

	/* now start the job */
	if (isc_service_start(IB_STATUS, (isc_svc_handle *)&svm->handle, NULL, spb_len, buf)) {
		FBIRD_SVC_ERROR(svm);
		RETURN_FALSE;
	}

	RETURN_TRUE;
}

PHP_FUNCTION(fbird_add_user)
{
	_php_fbird_user(INTERNAL_FUNCTION_PARAM_PASSTHRU, isc_action_svc_add_user);
}

PHP_FUNCTION(fbird_modify_user)
{
	_php_fbird_user(INTERNAL_FUNCTION_PARAM_PASSTHRU, isc_action_svc_modify_user);
}

PHP_FUNCTION(fbird_delete_user)
{
	_php_fbird_user(INTERNAL_FUNCTION_PARAM_PASSTHRU, isc_action_svc_delete_user);
}

PHP_FUNCTION(fbird_service_attach)
{
	size_t hlen = 0, ulen = 0, plen = 0;
	fbird_service *svm;
	char *host = NULL, *user = NULL, *pass = NULL;
	char buf[350];
	char loc[128] = "service_mgr";
	void *handle = 0;
	unsigned short p = 0;

	RESET_ERRMSG;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS(), "|s!s!s!",
			&host, &hlen, &user, &ulen, &pass, &plen)) {

		RETURN_FALSE;
	}

	/* Fall back to INI defaults if user/password not provided (Issue #71) */
	if (ulen == 0) {
		char *ini_user = INI_STR("fbird.default_user");
		if (ini_user && *ini_user) {
			user = ini_user;
			ulen = strlen(ini_user);
		}
	}

	if (plen == 0) {
		char *ini_pass = INI_STR("fbird.default_password");
		if (ini_pass && *ini_pass) {
			pass = ini_pass;
			plen = strlen(ini_pass);
		}
	}

	if (ulen > 63) {
		_php_fbird_module_error("Internal error: dba_username too long");
		RETURN_FALSE;
	}

	if (plen > 255) {
		_php_fbird_module_error("Internal error: dba_password too long");
		RETURN_FALSE;
	}

	// 13 = strlen(":service_mgr") + \0;
	if (hlen + 13 > sizeof(loc)) {
		_php_fbird_module_error("Internal error: insufficient buffer space for name of the service (%zd)", hlen + 13);
		RETURN_FALSE;
	}

	buf[p++] = isc_spb_version;
	buf[p++] = isc_spb_current_version;

	if(ulen > 0){
		buf[p++] = isc_spb_user_name;
		buf[p++] = (char)ulen;
		memcpy(&buf[p], user, ulen);
		p += ulen;
	}

	if(plen > 0){
		buf[p++] = isc_spb_password;
		buf[p++] = (char)plen;
		memcpy(&buf[p], pass, plen);
		p += plen;
	}

	if(hlen > 0){
		slprintf(loc, sizeof(loc), "%s:service_mgr", host);
	}

	/* attach to the service manager */
	if (isc_service_attach(IB_STATUS, 0, loc, (isc_svc_handle *)&handle, p, buf)) {
		_php_fbird_error();
		RETURN_FALSE;
	}

	svm = (fbird_service*)emalloc(sizeof(fbird_service));
	svm->handle = handle;
	svm->hostname = hlen > 0 ? estrdup(host) : NULL;
	svm->username = ulen > 0 ? estrdup(user) : NULL;
	svm->fbsvc_service = NULL;  /* Phase 8: OO API wrapper, initialized on demand */
#ifndef PHP_WIN32
	svm->created_pid = getpid();  /* Issue #56: Track creation PID for fork detection */
#else
	svm->created_pid = 0;
#endif

	RETVAL_RES(zend_register_resource(svm, le_service));
	Z_TRY_ADDREF_P(return_value);
	svm->res = Z_RES_P(return_value);
}

PHP_FUNCTION(fbird_service_detach)
{
	zval *res;

	RESET_ERRMSG;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS(), "r", &res)) {
		RETURN_FALSE;
	}

	zend_list_delete(Z_RES_P(res));

	RETURN_TRUE;
}

static void _php_fbird_service_query(INTERNAL_FUNCTION_PARAMETERS,
	fbird_service *svm, char info_action)
{
	static char spb[] = { isc_info_svc_timeout, 10, 0, 0, 0 };

	char res_buf[400], *result, *heap_buf = NULL, *heap_p;
	zend_long heap_buf_size = 200, line_len;

	/* info about users requires an action first */
	if (info_action == isc_info_svc_get_users) {
		static char action[] = { isc_action_svc_display_user };

		if (isc_service_start(IB_STATUS, (isc_svc_handle *)&svm->handle, NULL, sizeof(action), action)) {
			FBIRD_SVC_ERROR(svm);
			RETURN_FALSE;
		}
	}

query_loop:
	result = res_buf;

	if (isc_service_query(IB_STATUS, (isc_svc_handle *)&svm->handle, NULL, sizeof(spb), spb,
			1, &info_action, sizeof(res_buf), res_buf)) {

		FBIRD_SVC_ERROR(svm);
		RETURN_FALSE;
	}
	while (*result != isc_info_end) {
		switch (*result++) {
			default:
				RETURN_FALSE;

			case isc_info_svc_line:
				if (! (line_len = isc_vax_integer(result, 2))) {
					/* done */
					if (heap_buf) {
						RETVAL_STRING(heap_buf);
						efree(heap_buf);
						return;
					}
					RETURN_TRUE;
				}
				if (!heap_buf || (heap_p - heap_buf + line_len +2) > heap_buf_size) {
					zend_long res_size = heap_buf ? heap_p - heap_buf : 0;

					while (heap_buf_size < (res_size + line_len +2)) {
						heap_buf_size *= 2;
					}
					heap_buf = (char*) erealloc(heap_buf, heap_buf_size);
					heap_p = heap_buf + res_size;
				}
				result += 2;
				*(result+line_len) = 0;
				snprintf(heap_p, heap_buf_size - (heap_p - heap_buf), "%s\n", result);
				heap_p += line_len +1;
				goto query_loop; /* repeat until result is exhausted */

			case isc_info_svc_server_version:
			case isc_info_svc_implementation:
			case isc_info_svc_get_env:
			case isc_info_svc_get_env_lock:
			case isc_info_svc_get_env_msg:
			case isc_info_svc_user_dbpath:
				RETURN_STRINGL(result + 2, isc_vax_integer(result, 2));

			case isc_info_svc_svr_db_info:
				array_init(return_value);

				do {
					switch (*result++) {
						int len;

						case isc_spb_num_att:
							add_assoc_long(return_value, "attachments", isc_vax_integer(result,4));
							result += 4;
							break;

						case isc_spb_num_db:
							add_assoc_long(return_value, "databases", isc_vax_integer(result,4));
							result += 4;
							break;

						case isc_spb_dbname:
							len = isc_vax_integer(result,2);
							add_next_index_stringl(return_value, result +2, len);
							result += len+2;
							break;
						default:
							break;
					}
				} while (*result != isc_info_flag_end);
				return;

			case isc_info_svc_get_users: {
				zval user;
				array_init(return_value);

				while (*result != isc_info_end) {

					switch (*result++) {
						int len;

						case isc_spb_sec_username:
							/* it appears that the username is always first */
							array_init(&user);
							add_next_index_zval(return_value, &user);

							len = isc_vax_integer(result,2);
							add_assoc_stringl(&user, "user_name", result +2, len);
							result += len+2;
							break;

						case isc_spb_sec_firstname:
							len = isc_vax_integer(result,2);
							add_assoc_stringl(&user, "first_name", result +2, len);
							result += len+2;
							break;

						case isc_spb_sec_middlename:
							len = isc_vax_integer(result,2);
							add_assoc_stringl(&user, "middle_name", result +2, len);
							result += len+2;
							break;

						case isc_spb_sec_lastname:
							len = isc_vax_integer(result,2);
							add_assoc_stringl(&user, "last_name", result +2, len);
							result += len+2;
							break;

						case isc_spb_sec_userid:
							add_assoc_long(&user, "user_id", isc_vax_integer(result, 4));
							result += 4;
							break;

						case isc_spb_sec_groupid:
							add_assoc_long(&user, "group_id", isc_vax_integer(result, 4));
							result += 4;
							break;
						default:
							break;
					}
				}
				return;
			}
		}
	}
}

static void _php_fbird_backup_restore(INTERNAL_FUNCTION_PARAMETERS, char operation)
{
	/**
	 * It appears that the service API is a little bit confused about which flag
	 * to use for the source and destination in the case of a restore operation.
	 * When passing the backup file as isc_spb_dbname and the destination db as
	 * bpk_file, things work well.
	 */
	zval *res;
	char *db, *bk, buf[200];
	size_t dblen, bklen, spb_len;
	zend_long opts = 0;
	zend_bool verbose = 0;
	fbird_service *svm;

	RESET_ERRMSG;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS(), "rss|lb",
			&res, &db, &dblen, &bk, &bklen, &opts, &verbose)) {
		RETURN_FALSE;
	}

	svm = (fbird_service *)zend_fetch_resource_ex(res,
		"Firebird service manager handle", le_service);

	/* fill the param buffer */
	spb_len = slprintf(buf, sizeof(buf), "%c%c%c%c%s%c%c%c%s%c%c%c%c%c",
		operation, isc_spb_dbname, (char)dblen, (char)(dblen >> 8), db,
		isc_spb_bkp_file, (char)bklen, (char)(bklen >> 8), bk, isc_spb_options,
		(char)opts,(char)(opts >> 8), (char)(opts >> 16), (char)(opts >> 24));

	if (verbose) {
		buf[spb_len++] = isc_spb_verbose;
	}

	if (spb_len > sizeof(buf) || spb_len <= 0) {
		_php_fbird_module_error("Internal error: insufficient buffer space for SPB (%zd)", spb_len);
		RETURN_FALSE;
	}

	/* now start the backup/restore job */
	if (isc_service_start(IB_STATUS, (isc_svc_handle *)&svm->handle, NULL, (unsigned short)spb_len, buf)) {
		FBIRD_SVC_ERROR(svm);
		RETURN_FALSE;
	}

	if (!verbose) {
		RETURN_TRUE;
	} else {
		_php_fbird_service_query(INTERNAL_FUNCTION_PARAM_PASSTHRU, svm, isc_info_svc_line);
	}
}

PHP_FUNCTION(fbird_backup)
{
	_php_fbird_backup_restore(INTERNAL_FUNCTION_PARAM_PASSTHRU, isc_action_svc_backup);
}

PHP_FUNCTION(fbird_restore)
{
	_php_fbird_backup_restore(INTERNAL_FUNCTION_PARAM_PASSTHRU, isc_action_svc_restore);
}

static void _php_fbird_service_action(INTERNAL_FUNCTION_PARAMETERS, char svc_action)
{
	zval *res;
	char buf[128], *db;
	size_t dblen;
	int spb_len;
	zend_long action, argument = 0;
	fbird_service *svm;

	RESET_ERRMSG;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS(), "rsl|l",
			&res, &db, &dblen, &action, &argument)) {
		RETURN_FALSE;
	}

	svm = (fbird_service *)zend_fetch_resource_ex(res,
		"Firebird service manager handle", le_service);

	if (svc_action == isc_action_svc_db_stats) {
		switch (action) {
			default:
				goto unknown_option;

			case isc_spb_sts_data_pages:
			case isc_spb_sts_db_log:
			case isc_spb_sts_hdr_pages:
			case isc_spb_sts_idx_pages:
			case isc_spb_sts_sys_relations:
				goto options_argument;
		}
	} else {
		/* these actions all expect different types of arguments */
		switch (action) {
			default:
unknown_option:
				_php_fbird_module_error("Unrecognised option (" ZEND_LONG_FMT ")", action);
				RETURN_FALSE;

			case isc_spb_rpr_check_db:
			case isc_spb_rpr_ignore_checksum:
			case isc_spb_rpr_kill_shadows:
			case isc_spb_rpr_mend_db:
			case isc_spb_rpr_validate_db:
			case isc_spb_rpr_sweep_db:
				svc_action = isc_action_svc_repair;

			case isc_spb_prp_activate:
			case isc_spb_prp_db_online:
options_argument:
				argument |= action;
				action = isc_spb_options;

			case isc_spb_prp_page_buffers:
			case isc_spb_prp_sweep_interval:
			case isc_spb_prp_shutdown_db:
			case isc_spb_prp_deny_new_transactions:
			case isc_spb_prp_deny_new_attachments:
			case isc_spb_prp_set_sql_dialect:
				spb_len = slprintf(buf, sizeof(buf), "%c%c%c%c%s%c%c%c%c%c",
					svc_action, isc_spb_dbname, (char)dblen, (char)(dblen >> 8), db,
					(char)action, (char)argument, (char)(argument >> 8), (char)(argument >> 16),
					(char)(argument >> 24));
				break;

			case isc_spb_prp_reserve_space:
			case isc_spb_prp_write_mode:
			case isc_spb_prp_access_mode:
				spb_len = slprintf(buf, sizeof(buf), "%c%c%c%c%s%c%c",
					isc_action_svc_properties, isc_spb_dbname, (char)dblen, (char)(dblen >> 8),
					db, (char)action, (char)argument);
		}
	}

	if (spb_len > sizeof(buf) || spb_len == -1) {
		_php_fbird_module_error("Internal error: insufficient buffer space for SPB (%d)", spb_len);
		RETURN_FALSE;
	}

	if (isc_service_start(IB_STATUS, (isc_svc_handle *)&svm->handle, NULL, (unsigned short)spb_len, buf)) {
		FBIRD_SVC_ERROR(svm);
		RETURN_FALSE;
	}

	if (svc_action == isc_action_svc_db_stats) {
		_php_fbird_service_query(INTERNAL_FUNCTION_PARAM_PASSTHRU, svm, isc_info_svc_line);
	} else {
		RETURN_TRUE;
	}
}

PHP_FUNCTION(fbird_maintain_db)
{
	_php_fbird_service_action(INTERNAL_FUNCTION_PARAM_PASSTHRU, isc_action_svc_properties);
}

PHP_FUNCTION(fbird_db_info)
{
	_php_fbird_service_action(INTERNAL_FUNCTION_PARAM_PASSTHRU, isc_action_svc_db_stats);
}

PHP_FUNCTION(fbird_server_info)
{
	zval *res;
	zend_long action;
	fbird_service *svm;

	RESET_ERRMSG;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS(), "rl", &res, &action)) {
		RETURN_FALSE;
	}

	svm = (fbird_service *)zend_fetch_resource_ex(res,
		"Firebird service manager handle", le_service);

	_php_fbird_service_query(INTERNAL_FUNCTION_PARAM_PASSTHRU, svm, (char)action);
}

#else

void php_fbird_register_service_constants(INIT_FUNC_ARGS) { /* nop */ }

#endif /* HAVE_FIREBIRD */
