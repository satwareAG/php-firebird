#ifndef FBIRD_CLASSES_INTERNAL_H
#define FBIRD_CLASSES_INTERNAL_H

#include "php.h"
#include "php_fbird_includes.h"

/* Internal connection object structure */
typedef struct {
	fbc_connection_t *fbc_connection;      /* fbc_connection_t* — owns the connection */
	char hash_key[16];         /* cache key if cached */
	zend_bool persistent;
	zend_object std;
} fbird_connection_obj;

static inline fbird_connection_obj *fbird_connection_from_obj(zend_object *obj)
{
	return (fbird_connection_obj *)((char *)obj - XtOffsetOf(fbird_connection_obj, std));
}

#define Z_FB_CONN_P(zv) fbird_connection_from_obj(Z_OBJ_P(zv))

/* Internal transaction object structure */
typedef struct {
	fbt_transaction_t *fbt_trans;  /* fbt_transaction_t* from fbt_start() */
	zend_object  std;
} fbird_transaction_obj;

static inline fbird_transaction_obj *fbird_transaction_from_obj(zend_object *obj)
{
	return (fbird_transaction_obj *)((char *)obj - XtOffsetOf(fbird_transaction_obj, std));
}

#define Z_FB_TRANS_P(zv) fbird_transaction_from_obj(Z_OBJ_P(zv))

/* Internal statement object structure */
typedef struct {
	zend_resource *query_res; /* result of fbird_prepare() */
	zend_object    std;
} fbird_statement_obj;

static inline fbird_statement_obj *fbird_statement_from_obj(zend_object *obj)
{
	return (fbird_statement_obj *)((char *)obj - XtOffsetOf(fbird_statement_obj, std));
}

#define Z_FB_STMT_P(zv) fbird_statement_from_obj(Z_OBJ_P(zv))

/* Internal resultset object structure */
typedef struct {
	zend_resource *query_res; /* result of fbird_execute() */
	zend_object    std;
} fbird_resultset_obj;

static inline fbird_resultset_obj *fbird_resultset_from_obj(zend_object *obj)
{
	return (fbird_resultset_obj *)((char *)obj - XtOffsetOf(fbird_resultset_obj, std));
}

#define Z_FB_RESULTSET_P(zv) fbird_resultset_from_obj(Z_OBJ_P(zv))

/* Internal blob object structure */
typedef struct {
	fbb_blob_t *fbb_wrap;   /* BlobWrapper* from fbb_create/fbb_open */
	ISC_QUAD    blob_id;    /* blob ID (set after create/close) */
	zend_object std;
} fbird_blob_obj;

static inline fbird_blob_obj *fbird_blob_from_obj(zend_object *obj)
{
	return (fbird_blob_obj *)((char *)obj - XtOffsetOf(fbird_blob_obj, std));
}

#define Z_FB_BLOB_P(zv) fbird_blob_from_obj(Z_OBJ_P(zv))

/* Internal service object structure */
typedef struct {
	zend_resource *svc_res;      /* weak ref to le_service when wrapped from procedural API */
	fbsvc_service_t *fbsvc;   /* fbsvc_service pointer from fbsvc_attach() */
	zend_object  std;
} fbird_service_obj;

static inline fbird_service_obj *fbird_service_from_obj(zend_object *obj)
{
	return (fbird_service_obj *)((char *)obj - XtOffsetOf(fbird_service_obj, std));
}

#define Z_FB_SERVICE_P(zv) fbird_service_from_obj(Z_OBJ_P(zv))

/* Internal event object structure */
typedef struct {
	fbird_event *event;
	zend_object  std;
} fbird_event_obj;

static inline fbird_event_obj *fbird_event_from_obj(zend_object *obj)
{
	return (fbird_event_obj *)((char *)obj - XtOffsetOf(fbird_event_obj, std));
}

#define Z_FB_EVENT_P(zv) fbird_event_from_obj(Z_OBJ_P(zv))

/* Internal batch object structure */
typedef struct {
	fbird_batch *batch;
	zend_object  std;
} fbird_batch_obj;

static inline fbird_batch_obj *fbird_batch_from_obj(zend_object *obj)
{
	return (fbird_batch_obj *)((char *)obj - XtOffsetOf(fbird_batch_obj, std));
}

#define Z_FB_BATCH_P(zv) fbird_batch_from_obj(Z_OBJ_P(zv))

#endif /* FBIRD_CLASSES_INTERNAL_H */
