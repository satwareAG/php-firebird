/* fbird_class_internal.h - Internal types and accessors for OOP class split.
 * Not part of public API; included only by fbird_class_*.c and fbird_classes.c. */
#ifndef FBIRD_CLASS_INTERNAL_H
#define FBIRD_CLASS_INTERNAL_H

#include "php.h"
#include "php_fbird_includes.h"
#include "fbird_classes.h"
#include "fbird_service_types.h"

extern zend_class_entry *fbird_connection_exception_ce;
extern zend_class_entry *fbird_query_exception_ce;
extern zend_class_entry *fbird_service_exception_ce;

/* Connection */
typedef struct {
	zend_resource *conn_res;
	zend_object    std;
} fbird_connection_obj;
static inline fbird_connection_obj *fbird_connection_from_obj(zend_object *obj) {
	return (fbird_connection_obj *)((char *)obj - XtOffsetOf(fbird_connection_obj, std));
}
#define Z_FBIRD_CONNECTION_P(zv) fbird_connection_from_obj(Z_OBJ_P(zv))
extern zend_object_handlers fbird_connection_handlers;
zend_object *fbird_connection_create(zend_class_entry *ce);
void fbird_connection_free(zend_object *obj);
extern const zend_function_entry fbird_connection_methods[];
fbird_db_link *fbird_get_link_from_conn(zval *conn_zv);

/* Transaction */
typedef struct {
	void          *fbt_trans;
	zend_resource *trans_res;
	zend_object    std;
} fbird_transaction_obj;
static inline fbird_transaction_obj *fbird_transaction_from_obj(zend_object *obj) {
	return (fbird_transaction_obj *)((char *)obj - XtOffsetOf(fbird_transaction_obj, std));
}
#define Z_FBIRD_TRANSACTION_P(zv) fbird_transaction_from_obj(Z_OBJ_P(zv))
extern zend_object_handlers fbird_transaction_handlers;
zend_object *fbird_transaction_create(zend_class_entry *ce);
void fbird_transaction_free(zend_object *obj);
extern const zend_function_entry fbird_transaction_methods[];

/* Statement */
typedef struct {
	zend_resource *query_res;
	zend_object    std;
} fbird_statement_obj;
static inline fbird_statement_obj *fbird_statement_from_obj(zend_object *obj) {
	return (fbird_statement_obj *)((char *)obj - XtOffsetOf(fbird_statement_obj, std));
}
#define Z_FBIRD_STATEMENT_P(zv) fbird_statement_from_obj(Z_OBJ_P(zv))
extern zend_object_handlers fbird_statement_handlers;
zend_object *fbird_statement_create(zend_class_entry *ce);
void fbird_statement_free(zend_object *obj);
extern const zend_function_entry fbird_statement_methods[];

/* ResultSet */
typedef struct {
	zend_resource *query_res;
	zend_object    std;
} fbird_resultset_obj;
static inline fbird_resultset_obj *fbird_resultset_from_obj(zend_object *obj) {
	return (fbird_resultset_obj *)((char *)obj - XtOffsetOf(fbird_resultset_obj, std));
}
#define Z_FBIRD_RESULTSET_P(zv) fbird_resultset_from_obj(Z_OBJ_P(zv))
extern zend_object_handlers fbird_resultset_handlers;
zend_object *fbird_resultset_create(zend_class_entry *ce);
void fbird_resultset_free(zend_object *obj);
extern const zend_function_entry fbird_resultset_methods[];

/* Blob */
typedef struct {
	void          *fbb_wrap;
	ISC_QUAD       blob_id;
	zend_resource *blob_res;
	zend_object    std;
} fbird_blob_obj;
static inline fbird_blob_obj *fbird_blob_from_obj(zend_object *obj) {
	return (fbird_blob_obj *)((char *)obj - XtOffsetOf(fbird_blob_obj, std));
}
#define Z_FBIRD_BLOB_P(zv) fbird_blob_from_obj(Z_OBJ_P(zv))
extern zend_object_handlers fbird_blob_handlers;
zend_object *fbird_blob_create_obj(zend_class_entry *ce);
void fbird_blob_free_obj(zend_object *obj);
extern const zend_function_entry fbird_blob_methods[];

/* Service — unified struct in fbird_service_types.h */
extern zend_object_handlers fbird_service_handlers;
zend_object *fbird_service_create_obj(zend_class_entry *ce);
void fbird_service_free_obj(zend_object *obj);
extern const zend_function_entry fbird_service_methods[];

/* Event */
typedef struct {
	fbird_event *event;
	zend_object  std;
} fbird_event_obj;
static inline fbird_event_obj *fbird_event_from_obj(zend_object *obj) {
	return (fbird_event_obj *)((char *)obj - XtOffsetOf(fbird_event_obj, std));
}
#define Z_FBIRD_EVENT_P(zv) fbird_event_from_obj(Z_OBJ_P(zv))
extern zend_object_handlers fbird_event_handlers;
zend_object *fbird_event_create_obj(zend_class_entry *ce);
void fbird_event_free_obj(zend_object *obj);
extern const zend_function_entry fbird_event_methods[];

/* Signal-based timeout (shared between fbird_events.c and fbird_class_event.c) */
#ifndef PHP_WIN32
extern volatile sig_atomic_t fbird_timeout_occurred;
void fbird_timeout_handler(int sig);
#endif

/* Batch (FB4+) */
#if FB_API_VER >= 40
typedef struct {
	fbird_batch *batch;
	zend_object  std;
} fbird_batch_obj;
static inline fbird_batch_obj *fbird_batch_from_obj(zend_object *obj) {
	return (fbird_batch_obj *)((char *)obj - XtOffsetOf(fbird_batch_obj, std));
}
#define Z_FBIRD_BATCH_P(zv) fbird_batch_from_obj(Z_OBJ_P(zv))
extern zend_object_handlers fbird_batch_handlers;
zend_object *fbird_batch_create_obj(zend_class_entry *ce);
void fbird_batch_free_obj(zend_object *obj);
#endif

#endif /* FBIRD_CLASS_INTERNAL_H */
