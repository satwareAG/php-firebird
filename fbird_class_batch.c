/* fbird_class_batch.c - Firebird\BatchHandle OOP class (FB4+, skeleton) */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_firebird.h"
#include "php_fbird_includes.h"
#include "firebird_utils.h"
#include "fbird_class_internal.h"

#if FB_API_VER >= 40

zend_object_handlers fbird_batch_handlers;

zend_object *fbird_batch_create_obj(zend_class_entry *ce)
{
	fbird_batch_obj *intern = zend_object_alloc(sizeof(fbird_batch_obj), ce);
	intern->batch = NULL;
	zend_object_std_init(&intern->std, ce);
	object_properties_init(&intern->std, ce);
	intern->std.handlers = &fbird_batch_handlers;
	return &intern->std;
}

void fbird_batch_free_obj(zend_object *obj)
{
	fbird_batch_obj *intern = fbird_batch_from_obj(obj);
	if (intern->batch) {
		fbird_batch *batch = intern->batch;
		if (batch->fbbatch_wrapper != NULL) {
			fbbatch_cancel(IBG(master_instance), batch->fbbatch_wrapper, IB_STATUS);
			fbbatch_close(IBG(master_instance), batch->fbbatch_wrapper, IB_STATUS);
			batch->fbbatch_wrapper = NULL;
		}
		if (batch->in_msg_buffer != NULL) {
			efree(batch->in_msg_buffer);
			batch->in_msg_buffer = NULL;
		}
		efree(batch);
		intern->batch = NULL;
	}
	zend_object_std_dtor(obj);
}

void fbird_setup_batch_object(zval *rv, fbird_batch *batch)
{
	object_init_ex(rv, fbird_batch_ce);
	fbird_batch_obj *intern = fbird_batch_from_obj(Z_OBJ_P(rv));
	intern->batch = batch;
}

fbird_batch *fbird_batch_get_ptr(zend_object *obj)
{
	fbird_batch_obj *intern = fbird_batch_from_obj(obj);
	return intern ? intern->batch : NULL;
}

#endif /* FB_API_VER >= 40 */
