/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

#ifndef PHP_FBIRD_INCLUDES_H
#define PHP_FBIRD_INCLUDES_H

#include <ibase.h>
#ifndef PHP_WIN32
#include <sys/types.h>
#include <unistd.h>
#endif

/* Firebird 3.0+ required; FB_API_VER undefined means a missing or incompatible client library. */
#ifndef FB_API_VER
#error "FB_API_VER is not defined. Firebird 3.0+ client libraries are required."
#endif

/* Compatibility for older Firebird headers (pre-4.0) */
#ifndef isc_tpb_read_consistency
#define isc_tpb_read_consistency 70
#endif

#ifndef isc_dpb_set_bind
#define isc_dpb_set_bind 88
#endif

#ifndef SQLDA_CURRENT_VERSION
#define SQLDA_CURRENT_VERSION SQLDA_VERSION1
#endif

/* Metadata identifier length (bytes). FB 4.0+ supports 63 chars (UTF8 = 4 bytes/char) */
#ifndef METADATALENGTH
#	if FB_API_VER >= 40
#		define METADATALENGTH 252 /* 63 characters * 4 bytes (UTF8) */
#	else
#		define METADATALENGTH 31  /* Legacy 31 byte limit */
#	endif
#endif

/* CHECK_LINK macro — shared by firebird.c and fbird_connection.c */
#define CHECK_LINK(link) { if ((link)==NULL) { \
	php_error_docref(NULL, E_WARNING, "A link to the server could not be established"); \
	RETURN_FALSE; } }

#define RESET_ERRMSG do { IBG(errmsg)[0] = '\0'; IBG(sql_code) = 0; } while (0)

#define IB_STATUS (IBG(status))

#ifdef FBIRD_DEBUG
#define FBDEBUG(a) php_printf("::: %s (%s:%d)\n", a, __FILE__, __LINE__);
#endif

#ifndef FBDEBUG
#define FBDEBUG(a)
#endif

extern int le_link, le_plink, le_trans, le_query, le_blob, le_event;
#if FB_API_VER >= 40
extern int le_batch;
#endif

#define LE_LINK  "Firebird link"
#define LE_PLINK "Firebird persistent link"
#define LE_TRANS "Firebird transaction"
#define LE_EVENT "Firebird event"
#define LE_BLOB  "Firebird blob"
#define LE_QUERY "Firebird query"
#define LE_SCVH  "Firebird service manager handle"
#define LE_BATCH "Firebird batch"

#define FBIRD_MSGSIZE 512
#define MAX_ERRMSG (FBIRD_MSGSIZE*2)

#define IB_DEF_DATE_FMT "%Y-%m-%d"
#define IB_DEF_TIME_FMT "%H:%M:%S"

/* this value should never be > USHRT_MAX */
#define FBIRD_BLOB_SEG 4096

ZEND_BEGIN_MODULE_GLOBALS(fbird)
	ISC_STATUS status[256];
	zend_resource *default_link;
	zend_long num_links, num_persistent;
	char errmsg[MAX_ERRMSG];
	zend_long sql_code;
	zend_long default_trans_params;
	zend_long default_lock_timeout; /* only used together with trans_param FBIRD_LOCK_TIMEOUT */
	zend_long blob_segment_size;    /* configurable BLOB segment size (default: 4096) */
	void *get_master_interface;
	void *master_instance;
	int client_version;
	int client_major_version;
	int client_minor_version;
	pid_t init_pid;                 /* PID at initialization for fork-safety detection */
	int exception_mode;             /* Exception mode: 0=SILENT (default), 1=THROW */
	bool in_mshutdown;         /* Flag: true during MSHUTDOWN to prevent EG() access */
ZEND_END_MODULE_GLOBALS(fbird)

ZEND_EXTERN_MODULE_GLOBALS(fbird)

typedef struct {
	struct tr_list *tr_list;
	unsigned short dialect;
	struct event *event_head;
	/* OO API connection wrapper (fb::Connection* from fbc_connect()) */
	void *fbc_connection;
	/* Hash key for connection cache lookup (16-byte MD5).
	 * Used by fbird_close to remove stale cache entries from EG(regular_list).
	 * Fixes: Issue #35 - Heap Use-After-Free in fbird_pconnect */
	char hash_key[16];
	/* PID at connection creation for fork-safety validation.
	 * Fixes: Issue #36 - UAF with pcntl_fork/PHPStan parallel mode */
	pid_t created_pid;
	/* Whether this is a persistent connection (le_plink).
	 * Used by autocommit logic to skip default-tx commit for pconnect
	 * (Issue #294 — cleanup_db() may drop DB before MSHUTDOWN). */
	bool is_persistent;
} fbird_db_link;

typedef struct {
	unsigned short link_cnt;
	unsigned long affected_rows;
	/* OO API transaction wrapper (fb::Transaction* from fbt_start()) */
	void *fbt_transaction;
	fbird_db_link *db_link[1]; /* last member */
} fbird_transaction;

typedef struct tr_list {
	fbird_transaction *trans;
	struct tr_list *next;
} fbird_tr_list;

typedef struct {
	unsigned short type;
	ISC_QUAD bl_qd;
	/* OO API blob wrapper (fb::BlobWrapper* from fbb_create()/fbb_open()) */
	void *fbb_blob;
} fbird_blob;

typedef struct event {
	fbird_db_link *link;
	zend_resource* link_res;
	ISC_LONG event_id;
	unsigned short event_count;
	char **events;
    unsigned char *event_buffer;  /* Buffer for event registration (isc_que_events) */
    unsigned char *result_buffer; /* Buffer for event results (counts) */
	zval callback;
	void *thread_ctx;
	struct event *event_next;
	enum event_state { NEW, ACTIVE, PENDING_REREGISTER, DEAD } state;
	int needs_reregistration;
	unsigned short buffer_size;
	int callback_count;
	int max_callbacks;
	/* Phase 7: OO API event wrapper (fb::EventsWrapper* from fbe_queue())
	 * When non-NULL, events are queued via the modern OO API.
	 * Note: The current polling model continues to use isc_wait_for_event()
	 * for synchronous operation; this field is for future async support. */
	void *fbe_events;
} fbird_event;

typedef struct {
	char *hostname;
	char *username;
	zend_resource *res;
	void *fbsvc_service; /* OO API ServiceWrapper* */
} fbird_service;

/* sql variables union
 * used for convert and binding input variables
 */
typedef struct {
	union {
#ifdef SQL_BOOLEAN
		FB_BOOLEAN bval;
#endif
		short sval;
		float fval;
		double dval;        /* Added for SQL_DOUBLE binding */
		ISC_LONG lval;
		ISC_INT64 i64val;   /* Added for SQL_INT64 binding */
		ISC_QUAD qval;
		ISC_TIMESTAMP tsval;
		ISC_DATE dtval;
		ISC_TIME tmval;
#if FB_API_VER >= 40
		ISC_TIMESTAMP_TZ tstzval;
		ISC_TIME_TZ tmtzval;
#endif
	} val;
	short nullind;
} BIND_BUF;

typedef struct {
	ISC_ARRAY_DESC ar_desc;
	ISC_LONG ar_size; /* size of entire array in bytes */
	unsigned short el_type, el_size;
} fbird_array;

typedef struct _ib_query {
    fbird_db_link *link;
    fbird_transaction *trans;
    zend_resource *trans_res;
    zend_resource *res;
    XSQLDA *in_sqlda, *out_sqlda;
    fbird_array *in_array, *out_array;
    unsigned short type, has_more_rows, is_open;
    unsigned short in_array_cnt, out_array_cnt;
    unsigned short dialect;
    char *query;
    ISC_UCHAR statement_type;
    BIND_BUF *bind_buf;
    ISC_SHORT *in_nullind, *out_nullind;
    ISC_USHORT in_fields_count, out_fields_count;
    HashTable *ht_aliases, *ht_ind; // Precomputed for fbird_fetch_*()
    int was_result_once;
    /* Whether this query instance owns the statement handle and must DSQL_drop it
     * in the destructor. Result resources created for SELECT reuse the parent's
     * statement handle and must NOT drop it to avoid invalidating the prepared
     * statement (fixes: tests/006.phpt, tests/bug45373.phpt, etc.). */
    bool owns_stmt_handle;
    /* Parent/children linkage to allow invalidating dependent results when the
     * prepared statement is freed (ensures TypeError on use-after-free, as
     * expected by tests/use_after_free-002.phpt). */
    struct _ib_query *parent;
    struct _ib_query *child_head;
    struct _ib_query *child_next;
    /* OO API statement wrapper (fb::Statement* from fbs_prepare()) */
    void *fbs_statement;
    void *fbs_resultset;  /* OO API IResultSet* for cursor operations */
    /* OO API message buffer for fetch operations (Phase 12+)
     * These replace XSQLDA-based data transfer when using OO API. */
    void *out_metadata;     /* IMessageMetadata* from fbs_get_output_metadata() */
    void *out_msg_buffer;   /* Message buffer for fetch (allocated based on metadata) */
    unsigned out_msg_length; /* Message buffer size */
    void *in_metadata;      /* IMessageMetadata* for input parameters */
    void *in_msg_buffer;    /* Message buffer for input parameters */
    unsigned in_msg_length; /* Input message buffer size */
} fbird_query;

#if FB_API_VER >= 40
/**
 * Batch operation wrapper for Firebird 4.0+ IBatch interface.
 * Provides high-performance bulk INSERT operations.
 */
typedef struct {
    void *fbbatch_wrapper;    /* OO API batch wrapper (from fbbatch_create()) */
    fbird_transaction *trans; /* Associated transaction */
    fbird_query *query;       /* Parent prepared statement */
    zend_resource *query_res; /* Strong reference to query resource (Issue #185).
                               * Prevents premature destruction of the IStatement*
                               * when the PHP query variable goes out of scope
                               * before fbird_batch_execute() is called. */
    void *in_metadata;        /* IMessageMetadata for input parameters */
    void *in_msg_buffer;      /* Message buffer for row data */
    unsigned in_msg_length;   /* Message buffer size */
} fbird_batch;
#endif /* FB_API_VER >= 40 */

enum php_fbird_option {
	PHP_FBIRD_DEFAULT            = 0,
	PHP_FBIRD_CREATE             = 0,
	/* connection flags */
	PHP_FBIRD_CONNECT_FORCE_NEW  = 2,   /* Force new connection, bypass connection reuse (matches PGSQL_CONNECT_FORCE_NEW) */
	/* fetch flags */
	PHP_FBIRD_FETCH_BLOBS        = 1,
	PHP_FBIRD_FETCH_ARRAYS       = 2,
	PHP_FBIRD_UNIXTIME           = 4,
	PHP_FBIRD_FETCH_DATE_OBJ     = 8,  /* Return DATE/TIME/TIMESTAMP as DateTimeImmutable */
	/* transaction access mode */
	PHP_FBIRD_WRITE              = 1,
	PHP_FBIRD_READ               = 2,
	/* transaction isolation level */
	PHP_FBIRD_CONCURRENCY        = 4,
	PHP_FBIRD_COMMITTED          = 8,
		PHP_FBIRD_REC_NO_VERSION = 32,
		PHP_FBIRD_REC_VERSION    = 64,
	PHP_FBIRD_CONSISTENCY        = 16,
	/* transaction lock resolution */
	PHP_FBIRD_WAIT               = 128,
	PHP_FBIRD_NOWAIT             = 256,
		PHP_FBIRD_LOCK_TIMEOUT   = 512,

	/* Table reservation lock types */
	PHP_FBIRD_LOCK_SHARED        = 1024,
	PHP_FBIRD_LOCK_PROTECTED     = 2048,
	PHP_FBIRD_LOCK_EXCLUSIVE     = 4096, // Not used explicitly in legacy, but good for completeness

	/* Table reservation access types */
	PHP_FBIRD_LOCK_READ          = 8192,
	PHP_FBIRD_LOCK_WRITE         = 16384,

	/* Firebird 4.0+ features */
	PHP_FBIRD_READ_CONSISTENCY   = 32768,

	/* Event timeout return value */
	PHP_FBIRD_EVENT_TIMEOUT      = -2
};

#define IBG(v) ZEND_MODULE_GLOBALS_ACCESSOR(fbird, v)

#if defined(ZTS) && defined(COMPILE_DL_FIREBIRD)
#ifdef __cplusplus
extern "C" {
#endif
ZEND_TSRMLS_CACHE_EXTERN()
#ifdef __cplusplus
}
#endif
#endif

#define BLOB_ID_LEN     13
#define BLOB_ID_MASK    "%x:%hx"

#define BLOB_INPUT      1
#define BLOB_OUTPUT     2

#ifdef PHP_WIN32
// Since we target PHP 8.1+ only, always use modern format
#define LL_MASK "ll"
#define LL_LIT(lit) lit ## I64
typedef void (__stdcall *info_func_t)(char*);
#else
#define LL_MASK "ll"
#define LL_LIT(lit) lit ## ll
typedef void (*info_func_t)(char*);
#endif

extern zend_class_entry *firebird_exception_ce;
extern const zend_function_entry firebird_exception_methods[];

void _php_fbird_error(void);
void _php_fbird_module_error(const char *, ...)
	PHP_ATTRIBUTE_FORMAT(printf,1,2);

/* determine if a resource is a link or transaction handle */
#define PHP_FBIRD_LINK_TRANS(zv, lh, th)                                                    \
		do {                                                                                \
			if (!zv) {                                                                      \
				lh = (fbird_db_link *)zend_fetch_resource2(                                 \
					IBG(default_link), "Firebird link", le_link, le_plink);                \
			} else {                                                                        \
				_php_fbird_get_link_trans(INTERNAL_FUNCTION_PARAM_PASSTHRU, zv, &lh, &th);  \
			}                                                                               \
			if (SUCCESS != _php_fbird_def_trans(lh, &th)) { RETURN_FALSE; }                 \
		} while (0)

int _php_fbird_def_trans(fbird_db_link *ib_link, fbird_transaction **trans);
void _php_fbird_get_link_trans(INTERNAL_FUNCTION_PARAMETERS, zval *link_id,
	fbird_db_link **ib_link, fbird_transaction **trans);

/* provided by fbird_query.c */
void php_fbird_query_minit(INIT_FUNC_ARGS);

/* provided by fbird_blobs.c */
void php_fbird_blobs_minit(INIT_FUNC_ARGS);
int _php_fbird_string_to_quad(char const *id, ISC_QUAD *qd);
zend_string *_php_fbird_quad_to_string(ISC_QUAD const qd);
int _php_fbird_blob_get(zval *return_value, fbird_blob *ib_blob, zend_ulong max_len);
int _php_fbird_blob_add(zval *string_arg, fbird_blob *ib_blob);

/* provided by fbird_events.c */
void php_fbird_events_minit(INIT_FUNC_ARGS);
void _php_fbird_free_event(fbird_event *event);

/* provided by fbird_service.c */
void php_fbird_service_minit(INIT_FUNC_ARGS);

#ifdef __cplusplus
extern "C" {
#endif

void _php_fbird_insert_alias(HashTable *ht, const char *alias);

#ifdef __cplusplus
}
#endif

#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif

#ifdef PHP_DEBUG
void fbp_dump_buffer(int len, const unsigned char *buffer);
void fbp_dump_buffer_raw(int len, const unsigned char *buffer);
#endif

void fbp_error_ex(long level, const char *, ...)
    PHP_ATTRIBUTE_FORMAT(printf,2,3);

#ifdef PHP_WIN32
#define fbp_fatal(msg, ...)   fbp_error_ex(E_ERROR,   msg " (%s:%d)\n", ## __VA_ARGS__, __FILE__, __LINE__)
#define fbp_warning(msg, ...) fbp_error_ex(E_WARNING, msg " (%s:%d)\n", ## __VA_ARGS__, __FILE__, __LINE__)
#define fbp_notice(msg, ...)  fbp_error_ex(E_NOTICE,  msg " (%s:%d)\n", ## __VA_ARGS__, __FILE__, __LINE__)
#else
#define fbp_fatal(msg, ...)   fbp_error_ex(E_ERROR,   msg " (%s:%d)\n" __VA_OPT__(,) __VA_ARGS__, __FILE__, __LINE__)
#define fbp_warning(msg, ...) fbp_error_ex(E_WARNING, msg " (%s:%d)\n" __VA_OPT__(,) __VA_ARGS__, __FILE__, __LINE__)
#define fbp_notice(msg, ...)  fbp_error_ex(E_NOTICE,  msg " (%s:%d)\n" __VA_OPT__(,) __VA_ARGS__, __FILE__, __LINE__)
#endif


typedef void* (ISC_EXPORT *fb_get_master_interface_t)(void);

/* Resource Type Validation Macros (TypeError Support)
 *
 * These macros provide consistent error handling when wrong resource types
 * are passed to fbird_* functions. In exception mode (FBIRD_EXCEPTION_MODE_THROW),
 * they throw TypeError. Otherwise, they emit E_WARNING for backward compatibility.
 */

/* Helper function prototype - implementation in firebird.c */
const char *_fbird_res_type_name(int type);

/* Validate connection resource (le_link or le_plink).
 * M3: Also accepts Firebird\Connection objects (weak-ref to same resource). */
#define FBIRD_VALIDATE_LINK_EX(zv, argnum, var) do { \
	/* M3 object path: Firebird\Connection accepted alongside resources */ \
	if (Z_TYPE_P(zv) == IS_OBJECT && \
		instanceof_function(Z_OBJCE_P(zv), fbird_connection_ce)) { \
		zend_resource *_cres = fbird_connection_get_resource(Z_OBJ_P(zv)); \
		if (!_cres) { RETURN_FALSE; } \
		var = (fbird_db_link *)_cres->ptr; \
		if (!var) { RETURN_FALSE; } \
		break; \
	} \
	int _res_type = Z_RES_TYPE_P(zv); \
	if (_res_type != le_link && _res_type != le_plink) { \
		if (IBG(exception_mode) == FBIRD_EXCEPTION_MODE_THROW) { \
			zend_argument_type_error(argnum, \
				"must be a Firebird connection resource, %s resource given", \
				_fbird_res_type_name(_res_type)); \
			RETURN_THROWS(); \
		} else { \
			php_error_docref(NULL, E_WARNING, \
				"Argument #%d must be a Firebird connection resource, %s resource given", \
				argnum, _fbird_res_type_name(_res_type)); \
			RETURN_FALSE; \
		} \
	} \
	var = (fbird_db_link *)zend_fetch_resource2_ex(zv, LE_LINK, le_link, le_plink); \
	if (!var) { RETURN_FALSE; } \
} while(0)

/* Validate transaction resource (le_trans).
 * M3: Also accepts Firebird\Transaction objects (weak-ref to same resource). */
#define FBIRD_VALIDATE_TRANS_EX(zv, argnum, var) do { \
	/* M3 object path: Firebird\Transaction accepted alongside resources */ \
	if (Z_TYPE_P(zv) == IS_OBJECT && \
		instanceof_function(Z_OBJCE_P(zv), fbird_transaction_ce)) { \
		zend_resource *_tres = fbird_transaction_get_resource(Z_OBJ_P(zv)); \
		if (!_tres) { RETURN_FALSE; } \
		var = (fbird_transaction *)_tres->ptr; \
		if (!var) { RETURN_FALSE; } \
		break; \
	} \
	int _res_type = Z_RES_TYPE_P(zv); \
	if (_res_type != le_trans) { \
		if (IBG(exception_mode) == FBIRD_EXCEPTION_MODE_THROW) { \
			zend_argument_type_error(argnum, \
				"must be a Firebird transaction resource, %s resource given", \
				_fbird_res_type_name(_res_type)); \
			RETURN_THROWS(); \
		} else { \
			php_error_docref(NULL, E_WARNING, \
				"Argument #%d must be a Firebird transaction resource, %s resource given", \
				argnum, _fbird_res_type_name(_res_type)); \
			RETURN_FALSE; \
		} \
	} \
	var = (fbird_transaction *)zend_fetch_resource_ex(zv, LE_TRANS, le_trans); \
	if (!var) { RETURN_FALSE; } \
} while(0)

/* Validate query/result resource (le_query).
 * M3 Phase G: Also accepts Firebird\ResultSet objects (weak-ref to same resource). */
/* Issue #297: Exported for OOP Statement::execute() to call directly */
int _php_fbird_exec(INTERNAL_FUNCTION_PARAMETERS, fbird_query *ib_query, zval *args, int bind_n);

#define FBIRD_VALIDATE_QUERY_EX(zv, argnum, var) do { \
	/* M3 object path: Firebird\ResultSet accepted alongside resources */ \
	if (Z_TYPE_P(zv) == IS_OBJECT && \
		instanceof_function(Z_OBJCE_P(zv), fbird_resultset_ce)) { \
		zend_resource *_qres = fbird_resultset_get_resource(Z_OBJ_P(zv)); \
		if (!_qres) { RETURN_FALSE; } \
		var = (fbird_query *)_qres->ptr; \
		if (!var) { \
			if (IBG(exception_mode) == FBIRD_EXCEPTION_MODE_THROW) { \
				zend_argument_type_error(argnum, \
					"must be a valid (non-freed) Firebird query/result resource or Firebird\\ResultSet"); \
				RETURN_THROWS(); \
			} \
			php_error_docref(NULL, E_WARNING, \
				"Argument #%d must be a valid (non-freed) Firebird query/result resource or Firebird\\ResultSet", \
				argnum); \
			RETURN_FALSE; \
		} \
		break; \
	} \
	/* Issue #297: Firebird\Statement (from fbird_prepare/ex) accepted */ \
	if (Z_TYPE_P(zv) == IS_OBJECT && \
		instanceof_function(Z_OBJCE_P(zv), fbird_statement_ce)) { \
		zend_resource *_qres = fbird_statement_get_resource(Z_OBJ_P(zv)); \
		if (!_qres) { RETURN_FALSE; } \
		var = (fbird_query *)_qres->ptr; \
		if (!var) { \
			if (IBG(exception_mode) == FBIRD_EXCEPTION_MODE_THROW) { \
				zend_argument_type_error(argnum, \
					"must be a valid (non-freed) Firebird query resource or Firebird\\Statement"); \
				RETURN_THROWS(); \
			} \
			php_error_docref(NULL, E_WARNING, \
				"Argument #%d must be a valid (non-freed) Firebird query resource or Firebird\\Statement", \
				argnum); \
			RETURN_FALSE; \
		} \
		break; \
	} \
	/* Must be IS_RESOURCE - anything else (object, array, etc.) is a type error */ \
	if (Z_TYPE_P(zv) != IS_RESOURCE) { \
		if (IBG(exception_mode) == FBIRD_EXCEPTION_MODE_THROW) { \
			zend_argument_type_error(argnum, \
				"must be a Firebird query/result resource, %s given", \
				zend_get_type_by_const(Z_TYPE_P(zv))); \
			RETURN_THROWS(); \
		} else { \
			php_error_docref(NULL, E_WARNING, \
				"Argument #%d must be a Firebird query/result resource, %s given", \
				argnum, zend_get_type_by_const(Z_TYPE_P(zv))); \
			RETURN_FALSE; \
		} \
	} \
	int _res_type = Z_RES_TYPE_P(zv); \
	if (_res_type != le_query) { \
		if (IBG(exception_mode) == FBIRD_EXCEPTION_MODE_THROW) { \
			zend_argument_type_error(argnum, \
				"must be a Firebird query/result resource, %s resource given", \
				_fbird_res_type_name(_res_type)); \
			RETURN_THROWS(); \
		} else { \
			php_error_docref(NULL, E_WARNING, \
				"Argument #%d must be a Firebird query/result resource, %s resource given", \
				argnum, _fbird_res_type_name(_res_type)); \
			RETURN_FALSE; \
		} \
	} \
	var = (fbird_query *)zend_fetch_resource_ex(zv, LE_QUERY, le_query); \
	if (!var) { RETURN_FALSE; } \
} while(0)

/* Validate blob resource (le_blob).
 * M3 Phase G: Also accepts Firebird\Blob objects (weak-ref to same resource). */
#define FBIRD_VALIDATE_BLOB_EX(zv, argnum, var) do { \
	/* M3 object path: Firebird\Blob accepted alongside resources */ \
	if (Z_TYPE_P(zv) == IS_OBJECT && \
		instanceof_function(Z_OBJCE_P(zv), fbird_blob_ce)) { \
		zend_resource *_bres = fbird_blob_get_resource(Z_OBJ_P(zv)); \
		if (!_bres) { RETURN_FALSE; } \
		var = (fbird_blob *)_bres->ptr; \
		if (!var) { \
			if (IBG(exception_mode) == FBIRD_EXCEPTION_MODE_THROW) { \
				zend_argument_type_error(argnum, \
					"must be a valid (non-freed) Firebird blob resource or Firebird\\Blob"); \
				RETURN_THROWS(); \
			} \
			php_error_docref(NULL, E_WARNING, \
				"Argument #%d must be a valid (non-freed) Firebird blob resource or Firebird\\Blob", \
				argnum); \
			RETURN_FALSE; \
		} \
		break; \
	} \
	/* Must be IS_RESOURCE - anything else is a type error */ \
	if (Z_TYPE_P(zv) != IS_RESOURCE) { \
		if (IBG(exception_mode) == FBIRD_EXCEPTION_MODE_THROW) { \
			zend_argument_type_error(argnum, \
				"must be a Firebird blob resource, %s given", \
				zend_get_type_by_const(Z_TYPE_P(zv))); \
			RETURN_THROWS(); \
		} else { \
			php_error_docref(NULL, E_WARNING, \
				"Argument #%d must be a Firebird blob resource, %s given", \
				argnum, zend_get_type_by_const(Z_TYPE_P(zv))); \
			RETURN_FALSE; \
		} \
	} \
	int _res_type_b = Z_RES_TYPE_P(zv); \
	if (_res_type_b != le_blob) { \
		if (IBG(exception_mode) == FBIRD_EXCEPTION_MODE_THROW) { \
			zend_argument_type_error(argnum, \
				"must be a Firebird blob resource, %s resource given", \
				_fbird_res_type_name(_res_type_b)); \
			RETURN_THROWS(); \
		} else { \
			php_error_docref(NULL, E_WARNING, \
				"Argument #%d must be a Firebird blob resource, %s resource given", \
				argnum, _fbird_res_type_name(_res_type_b)); \
			RETURN_FALSE; \
		} \
	} \
	var = (fbird_blob *)zend_fetch_resource_ex(zv, LE_BLOB, le_blob); \
	if (!var) { RETURN_FALSE; } \
} while(0)

/* Validate event handle (le_event resource OR Firebird\Event object).
 * M3 Phase H: Firebird\Event objects own fbird_event* directly (no resource indirection).
 * Callers must #include "fbird_classes.h" before using this macro. */
#define FBIRD_VALIDATE_EVENT_EX(zv, argnum, var) do { \
	if (Z_TYPE_P(zv) == IS_OBJECT && \
		instanceof_function(Z_OBJCE_P(zv), fbird_event_ce)) { \
		(var) = fbird_event_get_ptr(Z_OBJ_P(zv)); \
		if (!(var) || (var)->state == DEAD) { \
			zend_argument_type_error((argnum), \
				"must be a valid (non-freed) Firebird\\Event"); \
			RETURN_THROWS(); \
		} \
		break; \
	} \
	if (Z_TYPE_P(zv) != IS_RESOURCE) { \
		zend_argument_type_error((argnum), \
			"must be of type Firebird\\Event or resource"); \
		RETURN_THROWS(); \
	} \
	(var) = (fbird_event *)zend_fetch_resource_ex((zv), LE_EVENT, le_event); \
	if (!(var)) { RETURN_FALSE; } \
} while (0)

#endif /* PHP_FBIRD_INCLUDES_H */
