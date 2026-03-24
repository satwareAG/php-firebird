#ifndef PHP_FB_INCLUDES_H
#define PHP_FB_INCLUDES_H

#include <ibase.h>
#include <stdint.h>

#ifndef PHP_WIN32
#include <sys/types.h>
#include <unistd.h>
#endif

/* Opaque struct typedefs for type safety in C layer */
typedef struct fbc_master_t fbc_master_t;
typedef struct fbc_connection_t fbc_connection_t;
typedef struct fbt_transaction_t fbt_transaction_t;
typedef struct fbs_statement_t fbs_statement_t;
typedef struct fbb_blob_t fbb_blob_t;
typedef struct fbe_events_t fbe_events_t;
typedef struct fbsvc_service_t fbsvc_service_t;

#define TPB_MAX_SIZE 128

/* Transaction parameter bitmasks (PHP-specific values, NOT isc_tpb_*) */
#define PHP_FBIRD_DEFAULT            0
#define PHP_FBIRD_WRITE              1
#define PHP_FBIRD_READ               2
#define PHP_FBIRD_CONCURRENCY        4
#define PHP_FBIRD_COMMITTED          8
#define PHP_FBIRD_CONSISTENCY        16
#define PHP_FBIRD_REC_NO_VERSION     32
#define PHP_FBIRD_REC_VERSION        64
#define PHP_FBIRD_WAIT               128
#define PHP_FBIRD_NOWAIT             256

/* TPB lock/consistency constants */
#define PHP_FBIRD_LOCK_TIMEOUT       512
#define PHP_FBIRD_LOCK_SHARED        1024
#define PHP_FBIRD_LOCK_PROTECTED     2048
#define PHP_FBIRD_LOCK_EXCLUSIVE     4096
#define PHP_FBIRD_LOCK_READ          8192
#define PHP_FBIRD_LOCK_WRITE         16384
#define PHP_FBIRD_READ_CONSISTENCY   32768

/* Event timeout sentinel value */
#define PHP_FBIRD_EVENT_TIMEOUT      (-2)

/* Connection option: force new connection, bypass reuse */
#define PHP_FBIRD_CONNECT_FORCE_NEW  2

typedef struct {
	struct tr_list *tr_list;
	unsigned short dialect;
	struct event *event_head;
	fbc_connection_t *fbc_connection;
	char hash_key[16];
	pid_t created_pid;
} fbird_db_link;

typedef struct {
	unsigned short link_cnt;
	unsigned long affected_rows;
	fbt_transaction_t *fbt_transaction;
	fbird_db_link *db_link[1];
} fbird_transaction;

typedef struct tr_list {
	fbird_transaction *trans;
	struct tr_list *next;
} fbird_tr_list;

typedef struct {
	unsigned short type;
	ISC_QUAD bl_qd;
	fbb_blob_t *fbb_blob;
} fbird_blob;

typedef struct event {
	fbird_db_link *link;
	zend_resource* link_res;
	ISC_LONG event_id;
	unsigned short event_count;
	char **events;
    unsigned char *event_buffer;
    unsigned char *result_buffer;
	zval callback;
	void *thread_ctx;
	struct event *event_next;
	enum event_state { NEW, ACTIVE, PENDING_REREGISTER, DEAD } state;
	int needs_reregistration;
	unsigned short buffer_size;
	int callback_count;
	int max_callbacks;
	fbe_events_t *fbe_events;
} fbird_event;

typedef struct {
	short nullind;
	union {
		short sval;
		ISC_LONG lval;
		ISC_INT64 i64val;
		float fval;
		double dval;
		ISC_TIMESTAMP tsval;
		ISC_DATE dtval;
		ISC_TIME tmval;
		ISC_QUAD qval;
#if FB_API_VER >= 40
		ISC_TIMESTAMP_TZ tstzval;
		ISC_TIME_TZ tmtzval;
#endif
	} val;
} BIND_BUF;

typedef struct _ib_query {
	struct _ib_query *parent;
	struct _ib_query *child_head;
	struct _ib_query *child_next;
	zend_resource *res;
	fbird_db_link *link;
	fbird_transaction *trans;
	zend_resource *trans_res;
	unsigned short dialect;
	char *query;
	int statement_type;
	int in_fields_count;
	int out_fields_count;
	XSQLDA *in_sqlda;
	XSQLDA *out_sqlda;
	BIND_BUF *bind_buf;
	ISC_SHORT *in_nullind;
	ISC_SHORT *out_nullind;
	void *in_array;
	void *out_array;
	HashTable *ht_aliases;
	HashTable *ht_ind;
	fbs_statement_t *fbs_statement;
	void *fbs_resultset;
	int is_open;
	int has_more_rows;
	int owns_stmt_handle;
	void *out_metadata;
	void *out_msg_buffer;
	unsigned out_msg_length;
	void *in_metadata;
	void *in_msg_buffer;
	unsigned in_msg_length;
} fbird_query;

/* fbird_teb_t: simplified TPB holder per connection. */
typedef struct {
	short tpb_len;
	char *tpb_ptr;
} fbird_teb_t;

ZEND_BEGIN_MODULE_GLOBALS(fbird)
	void *master_instance;
	zend_resource *default_link;
	char errmsg[512];
	long sql_code;
	ISC_STATUS status[ISC_STATUS_LENGTH];
	long num_links, num_persistent;
	zend_long default_trans_params;
	zend_long default_lock_timeout;
	char *default_user;
	char *default_password;
	char *timestampformat;
	char *dateformat;
	char *timeformat;
	int in_mshutdown;
	int exception_mode;
ZEND_END_MODULE_GLOBALS(fbird)

ZEND_EXTERN_MODULE_GLOBALS(fbird)

#define IBG(v) ZEND_MODULE_GLOBALS_ACCESSOR(fbird, v)
#define IB_STATUS (IBG(status))
#define RESET_ERRMSG do { IBG(errmsg)[0] = '\0'; IBG(sql_code) = 0; } while (0)

#ifdef __cplusplus
extern "C" {
#endif
const char *_fbird_res_type_name(int type);
fbird_db_link *_fbird_get_link(zval *link_zv);
void fbird_set_shutdown_active(int active);
void _fbird_process_exit_handler(void);
void php_fbird_query_minit(INIT_FUNC_ARGS);
void php_fbird_blobs_minit(INIT_FUNC_ARGS);
void php_fbird_events_minit(INIT_FUNC_ARGS);
void php_fbird_service_minit(INIT_FUNC_ARGS);
#ifdef __cplusplus
}
#endif

#endif /* PHP_FB_INCLUDES_H */
