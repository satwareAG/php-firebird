/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

#ifndef PHP_FBIRD_INCLUDES_H
#define PHP_FBIRD_INCLUDES_H

#include <ibase.h>
#ifndef PHP_WIN32
#include <sys/types.h>
#include <unistd.h>
#endif

/* Firebird 3.0+ required - compile-time check */
#if !defined(FB_API_VER) || FB_API_VER < 30
#error "Firebird 3.0 or later is required. Please install Firebird 3.0+ client libraries."
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

#define RESET_ERRMSG do { IBG(errmsg)[0] = '\0'; IBG(sql_code) = 0; } while (0)

#define IB_STATUS (IBG(status))

#ifdef FBIRD_DEBUG
#define FBDEBUG(a) php_printf("::: %s (%s:%d)\n", a, __FILE__, __LINE__);
#endif

#ifndef FBDEBUG
#define FBDEBUG(a)
#endif

extern int le_link, le_plink, le_trans, le_query;
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
	void *get_statement_interface;
	int client_version;
	int client_major_version;
	int client_minor_version;
	pid_t init_pid;                 /* PID at initialization for fork-safety detection */
	int exception_mode;             /* Exception mode: 0=SILENT (default), 1=THROW */
	zend_bool in_mshutdown;         /* Flag: 1 during MSHUTDOWN to prevent EG() access */
ZEND_END_MODULE_GLOBALS(fbird)

ZEND_EXTERN_MODULE_GLOBALS(fbird)

/* Union to safely hold 64-bit handles even if headers define them as 32-bit integers */
typedef union {
	void *ptr;
	isc_db_handle db;
	isc_tr_handle tr;
	isc_stmt_handle stmt;
	isc_blob_handle blob;
} fb_safe_handle;

typedef struct {
	uint32_t magic;           /* UAF guard: FBIRD_MAGIC_LINK on alloc, FBIRD_MAGIC_FREED on free */
	fb_safe_handle handle;
	struct tr_list *tr_list;
	unsigned short dialect;
	struct event *event_head;
	/* Phase 3: OO API connection wrapper (fb::Connection* from fbc_connect())
	 * When non-NULL, this connection was created via the modern OO API.
	 * The handle.ptr may be NULL in this case - use fbc_get_attachment() instead. */
	void *fbc_connection;
	/* Hash key for connection cache lookup (16-byte MD5).
	 * Used by fbird_close to remove stale cache entries from EG(regular_list).
	 * Fixes: Issue #35 - Heap Use-After-Free in fbird_pconnect */
	char hash_key[16];
	/* PID at connection creation for fork-safety validation.
	 * Fixes: Issue #36 - UAF with pcntl_fork/PHPStan parallel mode */
	pid_t created_pid;
} fbird_db_link;

typedef struct {
	uint32_t magic;              /* UAF guard: FBIRD_MAGIC_TRANS on alloc, FBIRD_MAGIC_FREED on free */
	fb_safe_handle handle;
	unsigned short link_cnt;
	unsigned long affected_rows;
	/* OO API transaction wrapper (fb::Transaction* from fbt_start())
	 * When non-NULL, this transaction was created via the modern OO API.
	 * The handle.tr may be 0 in this case - use fbt_get_handle() instead. */
	void *fbt_transaction;
#ifndef PHP_WIN32
	pid_t created_pid;   /* PID when transaction was created (fork detection) */
#endif
	fbird_db_link *db_link[1]; /* last member */
} fbird_transaction;

typedef struct tr_list {
	fbird_transaction *trans;
	struct tr_list *next;
} fbird_tr_list;

typedef struct {
	uint32_t magic;              /* UAF guard: FBIRD_MAGIC_BLOB on alloc, FBIRD_MAGIC_FREED on free */
	fb_safe_handle bl_handle;
	unsigned short type;
	ISC_QUAD bl_qd;
	/* Phase 6: OO API blob wrapper (fb::BlobWrapper* from fbb_create()/fbb_open())
	 * When non-NULL, this blob was created via the modern OO API.
	 * The bl_handle.blob may be 0 in this case - use fbb_* functions instead. */
	void *fbb_blob;
#ifndef PHP_WIN32
	pid_t created_pid;   /* PID when blob was created (fork detection) */
#endif
} fbird_blob;

typedef struct event {
	uint32_t magic;              /* UAF guard: FBIRD_MAGIC_EVENT on alloc, FBIRD_MAGIC_FREED on free */
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
#ifndef PHP_WIN32
	pid_t created_pid;   /* PID when event was created (fork detection) */
#endif
} fbird_event;

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
} fbird_bind_buf;

typedef struct {
	ISC_ARRAY_DESC ar_desc;
	ISC_LONG ar_size; /* size of entire array in bytes */
	unsigned short el_type, el_size;
} fbird_array;

typedef struct _fbird_query {
    uint32_t magic;              /* UAF guard: FBIRD_MAGIC_QUERY on alloc, FBIRD_MAGIC_FREED on free */
    fbird_db_link *link;
    fbird_transaction *trans;
    zend_resource *trans_res;
    zend_resource *res;
    fb_safe_handle stmt;
    XSQLDA *in_sqlda, *out_sqlda;
    fbird_array *in_array, *out_array;
    unsigned short type, has_more_rows, is_open;
    unsigned short in_array_cnt, out_array_cnt;
    unsigned short dialect;
    char *query;
    ISC_UCHAR statement_type;
    fbird_bind_buf *bind_buf;
    ISC_SHORT *in_nullind, *out_nullind;
    ISC_USHORT in_fields_count, out_fields_count;
    HashTable *ht_aliases, *ht_ind; // Precomputed for fbird_fetch_*()
    int was_result_once;
    /* Whether this query instance owns the statement handle and must DSQL_drop it
     * in the destructor. Result resources created for SELECT reuse the parent's
     * statement handle and must NOT drop it to avoid invalidating the prepared
     * statement (fixes: tests/006.phpt, tests/bug45373.phpt, etc.). */
    zend_bool owns_stmt_handle;
    /* Parent/children linkage to allow invalidating dependent results when the
     * prepared statement is freed (ensures TypeError on use-after-free, as
     * expected by tests/use_after_free-002.phpt). */
    struct _fbird_query *parent;
    struct _fbird_query *child_head;
    struct _fbird_query *child_next;
    /* OO API statement wrapper (fb::Statement* from fbs_prepare())
     * When non-NULL, this statement was prepared via the modern OO API.
     * The stmt.ptr may be 0 in this case - use fbs_get_statement() instead. */
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
#ifndef PHP_WIN32
    pid_t created_pid;      /* PID when query was created (fork detection) */
#endif
} fbird_query;

#if FB_API_VER >= 40
/**
 * Batch operation wrapper for Firebird 4.0+ IBatch interface.
 * Provides high-performance bulk INSERT operations.
 */
typedef struct {
    uint32_t magic;              /* UAF guard: FBIRD_MAGIC_BATCH on alloc, FBIRD_MAGIC_FREED on free */
    void *fbbatch_wrapper;    /* OO API batch wrapper (from fbbatch_create()) */
    fbird_transaction *trans; /* Associated transaction */
    fbird_query *query;       /* Parent prepared statement */
    void *in_metadata;        /* IMessageMetadata for input parameters */
    void *in_msg_buffer;      /* Message buffer for row data */
    unsigned in_msg_length;   /* Message buffer size */
#ifndef PHP_WIN32
    pid_t created_pid;        /* PID when batch was created (fork detection) */
#endif
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

typedef ISC_STATUS (ISC_EXPORT *fb_get_statement_interface_t)(
	ISC_STATUS* status_vector, void* db_handle, isc_stmt_handle* stmt_handle
);

typedef void* (ISC_EXPORT *fb_get_master_interface_t)(void);

/* Resource Type Validation Macros (TypeError Support)
 *
 * These macros provide consistent error handling when wrong resource types
 * are passed to fbird_* functions. In exception mode (FBIRD_EXCEPTION_MODE_THROW),
 * they throw TypeError. Otherwise, they emit E_WARNING for backward compatibility.
 */

/* Helper function prototype - implementation in firebird.c */
const char *_fbird_res_type_name(int type);

/* Validate connection resource (le_link or le_plink) */
#define FBIRD_VALIDATE_LINK_EX(zv, argnum, var) do { \
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

/* Validate transaction resource (le_trans) */
#define FBIRD_VALIDATE_TRANS_EX(zv, argnum, var) do { \
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

/* Validate query/result resource (le_query) */
#define FBIRD_VALIDATE_QUERY_EX(zv, argnum, var) do { \
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

/* ============================================================================
 * UAF (Use-After-Free) Detection Guards
 * ============================================================================
 * Magic number validation pattern for memory safety.
 * Each resource struct has a magic field as its first member.
 * On allocation: magic = FBIRD_MAGIC_<TYPE>
 * On free: magic = FBIRD_MAGIC_FREED (poison value)
 *
 * This allows detection of:
 * 1. Use-after-free (magic == FBIRD_MAGIC_FREED)
 * 2. Corrupt/invalid pointers (magic != expected)
 * 3. Type confusion (wrong magic for resource type)
 */

/* Magic number constants - unique per resource type */
#define FBIRD_MAGIC_LINK    0xFB01C0DE  /* Connection/link resource */
#define FBIRD_MAGIC_TRANS   0xFB02C0DE  /* Transaction resource */
#define FBIRD_MAGIC_QUERY   0xFB03C0DE  /* Query/result resource */
#define FBIRD_MAGIC_BLOB    0xFB04C0DE  /* BLOB resource */
#define FBIRD_MAGIC_EVENT   0xFB05C0DE  /* Event resource */
#define FBIRD_MAGIC_SERVICE 0xFB06C0DE  /* Service manager resource */
#define FBIRD_MAGIC_BATCH   0xFB07C0DE  /* Batch operations resource (FB4.0+) */
#define FBIRD_MAGIC_FREED   0xDEADFB1D  /* Poison value after free */

/* Initialize magic number on allocation */
#define FBIRD_INIT_MAGIC(ptr, magic_val) \
	do { (ptr)->magic = (magic_val); } while(0)

/* Poison magic number on free (UAF detection) */
#define FBIRD_POISON_MAGIC(ptr) \
	do { (ptr)->magic = FBIRD_MAGIC_FREED; } while(0)

/* Check if resource was already freed */
#define FBIRD_IS_FREED(ptr) ((ptr)->magic == FBIRD_MAGIC_FREED)

/* Validate magic number - throws on UAF or corruption */
#define FBIRD_VALIDATE_MAGIC(ptr, expected_magic, type_name) \
	do { \
		if ((ptr)->magic == FBIRD_MAGIC_FREED) { \
			zend_throw_exception_ex(zend_ce_error, 0, \
				"Use-after-free: %s resource was already freed", type_name); \
			RETURN_THROWS(); \
		} \
		if ((ptr)->magic != (expected_magic)) { \
			zend_throw_exception_ex(zend_ce_error, 0, \
				"Invalid %s resource (magic number mismatch: 0x%08X != 0x%08X)", \
				type_name, (ptr)->magic, (expected_magic)); \
			RETURN_THROWS(); \
		} \
	} while(0)

/* Validate magic with custom return behavior (for non-RETURN_THROWS contexts) */
#define FBIRD_VALIDATE_MAGIC_EX(ptr, expected_magic, type_name, on_fail) \
	do { \
		if ((ptr)->magic == FBIRD_MAGIC_FREED) { \
			_php_fbird_module_error("Use-after-free: %s resource was already freed", type_name); \
			on_fail; \
		} \
		if ((ptr)->magic != (expected_magic)) { \
			_php_fbird_module_error("Invalid %s resource (magic number mismatch)", type_name); \
			on_fail; \
		} \
	} while(0)

/* Convenience macros for each resource type */
#define FBIRD_VALIDATE_LINK_MAGIC(ptr) \
	FBIRD_VALIDATE_MAGIC(ptr, FBIRD_MAGIC_LINK, "connection")

#define FBIRD_VALIDATE_TRANS_MAGIC(ptr) \
	FBIRD_VALIDATE_MAGIC(ptr, FBIRD_MAGIC_TRANS, "transaction")

#define FBIRD_VALIDATE_QUERY_MAGIC(ptr) \
	FBIRD_VALIDATE_MAGIC(ptr, FBIRD_MAGIC_QUERY, "query")

#define FBIRD_VALIDATE_BLOB_MAGIC(ptr) \
	FBIRD_VALIDATE_MAGIC(ptr, FBIRD_MAGIC_BLOB, "blob")

#define FBIRD_VALIDATE_EVENT_MAGIC(ptr) \
	FBIRD_VALIDATE_MAGIC(ptr, FBIRD_MAGIC_EVENT, "event")

#define FBIRD_VALIDATE_SERVICE_MAGIC(ptr) \
	FBIRD_VALIDATE_MAGIC(ptr, FBIRD_MAGIC_SERVICE, "service")

#define FBIRD_VALIDATE_BATCH_MAGIC(ptr) \
	FBIRD_VALIDATE_MAGIC(ptr, FBIRD_MAGIC_BATCH, "batch")

/* ============================================================================
 * Destructor Guard Macros (Shutdown Segfault Prevention)
 * ============================================================================
 * These macros provide defense-in-depth protection against SIGSEGV during
 * PHP shutdown, addressing Issues #50, #51, #55, #56, #64.
 *
 * Root causes prevented:
 * 1. Use-after-free: Accessing freed memory in destructors
 * 2. EG() access during MSHUTDOWN: Executor globals destroyed before persistent cleanup
 * 3. Resource ordering: Parent resources freed before children
 * 4. Double-free: Pointers freed multiple times
 * 5. NULL dereference: Accessing invalidated handles after explicit close
 *
 * Usage in destructors:
 *   static void _php_fbird_free_XXX(zend_resource *rsrc) {
 *       type *res = (type *)rsrc->ptr;
 *       FBIRD_DESTRUCTOR_GUARD(res, "_php_fbird_free_XXX");
 *       // ... cleanup logic ...
 *   }
 */

/**
 * FBIRD_DESTRUCTOR_GUARD - Guard macro for resource destructor functions
 *
 * Performs two safety checks before destructor logic executes:
 * 1. NULL pointer check: Prevents dereference of already-freed resources
 * 2. MSHUTDOWN flag check: Prevents EG() access during module shutdown
 *
 * @param resource_ptr   Pointer to the resource structure
 * @param resource_name  Debug name for logging (destructor function name)
 */
#define FBIRD_DESTRUCTOR_GUARD(resource_ptr, resource_name) \
	do { \
		/* Guard 1: NULL pointer check */ \
		if ((resource_ptr) == NULL) { \
			FBDEBUG(resource_name ": NULL pointer, skipping"); \
			return; \
		} \
		/* Guard 2: MSHUTDOWN flag check - EG() is invalid during module shutdown */ \
		if (IBG(in_mshutdown)) { \
			FBDEBUG(resource_name ": MSHUTDOWN active, skipping cleanup"); \
			return; \
		} \
	} while(0)

/**
 * FBIRD_DESTRUCTOR_GUARD_EX - Extended guard with rsrc->ptr clearing
 *
 * Same as FBIRD_DESTRUCTOR_GUARD but also clears rsrc->ptr to NULL
 * when MSHUTDOWN is active, preventing double-free on subsequent calls.
 *
 * Note: During MSHUTDOWN, we cannot safely efree() or access EG() lists,
 * so we simply mark the resource as handled and return.
 *
 * @param rsrc           Pointer to the zend_resource
 * @param resource_ptr   Pointer to the resource structure
 * @param resource_name  Debug name for logging
 */
#define FBIRD_DESTRUCTOR_GUARD_EX(rsrc, resource_ptr, resource_name) \
	do { \
		/* Guard 1: NULL pointer check */ \
		if ((resource_ptr) == NULL) { \
			FBDEBUG(resource_name ": NULL pointer, skipping"); \
			return; \
		} \
		/* Guard 2: MSHUTDOWN flag check - mark as handled and skip */ \
		if (IBG(in_mshutdown)) { \
			FBDEBUG(resource_name ": MSHUTDOWN active, marking handled"); \
			(rsrc)->ptr = NULL; \
			return; \
		} \
	} while(0)

/**
 * FBIRD_FUNCTION_GUARD - Guard macro for public API functions
 *
 * Validates handle pointer before function execution.
 * Emits E_WARNING and returns the specified value on invalid handle.
 *
 * @param handle_ptr   Pointer to validate
 * @param handle_name  Human-readable name for error message
 * @param return_val   Value to return on failure
 */
#define FBIRD_FUNCTION_GUARD(handle_ptr, handle_name, return_val) \
	do { \
		if ((handle_ptr) == NULL) { \
			php_error_docref(NULL, E_WARNING, \
				"Supplied %s is not a valid handle", handle_name); \
			return (return_val); \
		} \
	} while(0)

/**
 * FBIRD_FUNCTION_GUARD_EX - Guard with exception mode support
 *
 * Like FBIRD_FUNCTION_GUARD but throws TypeError in exception mode.
 *
 * @param handle_ptr   Pointer to validate
 * @param handle_name  Human-readable name for error message
 */
#define FBIRD_FUNCTION_GUARD_EX(handle_ptr, handle_name) \
	do { \
		if ((handle_ptr) == NULL) { \
			if (IBG(exception_mode) == FBIRD_EXCEPTION_MODE_THROW) { \
				zend_throw_exception_ex(zend_ce_type_error, 0, \
					"Supplied %s is not a valid handle", handle_name); \
				RETURN_THROWS(); \
			} else { \
				php_error_docref(NULL, E_WARNING, \
					"Supplied %s is not a valid handle", handle_name); \
				RETURN_FALSE; \
			} \
		} \
	} while(0)

/**
 * Resource state enumeration (for future enhanced tracking)
 *
 * Can be used to track resource lifecycle state more precisely
 * than just NULL/non-NULL or magic number checks.
 */
typedef enum {
	FBIRD_STATE_ACTIVE = 0,    /* Resource is valid and usable */
	FBIRD_STATE_DETACHED = 1,  /* Resource detached (e.g., service_detach) */
	FBIRD_STATE_CLOSED = 2,    /* Resource explicitly closed */
	FBIRD_STATE_ERROR = 3      /* Resource in error state */
} fbird_resource_state;

#endif /* PHP_FBIRD_INCLUDES_H */
