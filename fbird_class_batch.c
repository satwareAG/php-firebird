/* fbird_class_batch.c - Firebird\BatchHandle OOP class (FB4+) */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_firebird.h"
#include "php_fbird_includes.h"
#include "firebird_utils.h"
#include "fbird_class_internal.h"

#if FB_API_VER >= 40

/* {{{ arginfo for BatchHandle methods */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_BatchHandle_getBlobAlignment, 0, 0, MAY_BE_LONG|MAY_BE_FALSE)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_BatchHandle_setDefaultBpb, 0, 1, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, bpb, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_BatchHandle_cancel, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_BatchHandle_execute, 0, 0, MAY_BE_ARRAY|MAY_BE_FALSE)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_BatchHandle_add, 0, 0, _IS_BOOL, 0)
	ZEND_ARG_VARIADIC_INFO(0, args)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_BatchHandle_addBlob, 0, 1, MAY_BE_STRING|MAY_BE_FALSE)
	ZEND_ARG_TYPE_INFO(0, data, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, blob_type, IS_LONG, 0)
ZEND_END_ARG_INFO()
/* }}} */

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
	ISC_STATUS status[256];
	if (intern->batch) {
		fbird_batch *batch = intern->batch;
		if (batch->fbbatch_wrapper != NULL) {
			fbbatch_cancel(FBG(master_instance), batch->fbbatch_wrapper, status);
			fbbatch_close(FBG(master_instance), batch->fbbatch_wrapper, status);
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

/* {{{ BatchHandle::getBlobAlignment(): int|false */
PHP_METHOD(BatchHandle, getBlobAlignment)
{
	ISC_STATUS status[256];
	fbird_batch *fb_batch;
	unsigned alignment;

	RESET_ERRMSG;

	if (zend_parse_parameters_none() == FAILURE) {
		RETURN_THROWS();
	}

	fb_batch = fbird_batch_get_ptr(Z_OBJ_P(getThis()));
	if (!fb_batch || !fb_batch->fbbatch_wrapper) {
		_php_fbird_module_error("Invalid batch handle");
		RETURN_FALSE;
	}

	alignment = fbbatch_get_blob_alignment(FBG(master_instance), fb_batch->fbbatch_wrapper, status);
	if (alignment == 0) {
		_php_fbird_error(status);
		RETURN_FALSE;
	}

	RETURN_LONG((zend_long)alignment);
}

/* {{{ BatchHandle::setDefaultBpb(string $bpb): bool */
PHP_METHOD(BatchHandle, setDefaultBpb)
{
	ISC_STATUS status[256];
	char *bpb;
	size_t bpb_len;
	fbird_batch *fb_batch;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &bpb, &bpb_len) == FAILURE) {
		RETURN_THROWS();
	}

	fb_batch = fbird_batch_get_ptr(Z_OBJ_P(getThis()));
	if (!fb_batch || !fb_batch->fbbatch_wrapper) {
		_php_fbird_module_error("Invalid batch handle");
		RETURN_FALSE;
	}

	if (!fbbatch_set_default_bpb(FBG(master_instance), fb_batch->fbbatch_wrapper,
			(unsigned)bpb_len, (const unsigned char *)bpb, status)) {
		_php_fbird_error(status);
		RETURN_FALSE;
	}

	RETURN_TRUE;
}

/* {{{ BatchHandle::cancel(): bool */
PHP_METHOD(BatchHandle, cancel)
{
	ISC_STATUS status[256];
	fbird_batch *fb_batch;

	RESET_ERRMSG;

	if (zend_parse_parameters_none() == FAILURE) {
		RETURN_THROWS();
	}

	fb_batch = fbird_batch_get_ptr(Z_OBJ_P(getThis()));
	if (!fb_batch || !fb_batch->fbbatch_wrapper) {
		_php_fbird_module_error("Invalid batch handle");
		RETURN_FALSE;
	}

	if (!fbbatch_cancel(FBG(master_instance), fb_batch->fbbatch_wrapper, status)) {
		_php_fbird_error(status);
		RETURN_FALSE;
	}

	fbbatch_close(FBG(master_instance), fb_batch->fbbatch_wrapper, status);
	fb_batch->fbbatch_wrapper = NULL;

	RETURN_TRUE;
}

/* {{{ BatchHandle::execute(): array|false */
PHP_METHOD(BatchHandle, execute)
{
	ISC_STATUS status[256];
	fbird_batch *fb_batch;
	void *trans_ptr;
	unsigned total_processed = 0;
	unsigned error_count = 0;

	RESET_ERRMSG;

	if (zend_parse_parameters_none() == FAILURE) {
		RETURN_THROWS();
	}

	fb_batch = fbird_batch_get_ptr(Z_OBJ_P(getThis()));
	if (!fb_batch || !fb_batch->fbbatch_wrapper) {
		_php_fbird_module_error("Invalid batch handle");
		RETURN_FALSE;
	}

	if (!fb_batch->trans || !fb_batch->trans->fbt_transaction) {
		_php_fbird_module_error("Batch has no valid transaction");
		RETURN_FALSE;
	}

	trans_ptr = fbt_get_handle(fb_batch->trans->fbt_transaction);
	if (!trans_ptr) {
		_php_fbird_module_error("Failed to get transaction handle");
		RETURN_FALSE;
	}

	if (!fbbatch_execute(FBG(master_instance), fb_batch->fbbatch_wrapper, trans_ptr,
			&total_processed, &error_count, status)) {
		_php_fbird_error(status);
		RETURN_FALSE;
	}

	/* Close the batch after execution */
	fbbatch_close(FBG(master_instance), fb_batch->fbbatch_wrapper, status);
	fb_batch->fbbatch_wrapper = NULL;

	unsigned success_count = (total_processed >= error_count) ? (total_processed - error_count) : 0;

	array_init(return_value);
	add_assoc_long(return_value, "total_processed", total_processed);
	add_assoc_long(return_value, "success_count", success_count);
	add_assoc_long(return_value, "error_count", error_count);
}

/* {{{ BatchHandle::add(mixed ...$args): bool
 * Delegates to fbird_batch_add_impl() - shared with procedural API. */
PHP_METHOD(BatchHandle, add)
{
	zval *args = NULL;
	int argc = 0;
	fbird_batch *fb_batch;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "*", &args, &argc) == FAILURE) {
		RETURN_THROWS();
	}

	fb_batch = fbird_batch_get_ptr(Z_OBJ_P(getThis()));

	if (fbird_batch_add_impl(fb_batch, args, argc) == FAILURE) {
		RETURN_FALSE;
	}

	RETURN_TRUE;
}

/* {{{ BatchHandle::addBlob(string $data, int $blob_type = 0): string|false */
PHP_METHOD(BatchHandle, addBlob)
{
	ISC_STATUS status[256];
	char *data;
	size_t data_len;
	zend_long blob_type = 0;
	fbird_batch *fb_batch;
	ISC_QUAD blob_id;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|l", &data, &data_len, &blob_type) == FAILURE) {
		RETURN_THROWS();
	}

	fb_batch = fbird_batch_get_ptr(Z_OBJ_P(getThis()));
	if (!fb_batch || !fb_batch->fbbatch_wrapper) {
		_php_fbird_module_error("Invalid batch handle");
		RETURN_FALSE;
	}

	memset(&blob_id, 0, sizeof(blob_id));

	if (!fbbatch_add_blob(FBG(master_instance), fb_batch->fbbatch_wrapper,
			(unsigned)data_len, (const void *)data, &blob_id, 0, NULL, status)) {
		_php_fbird_error(status);
		RETURN_FALSE;
	}

	RETURN_NEW_STR(_php_fbird_quad_to_string(blob_id));
}

const zend_function_entry fbird_batch_methods[] = {
	PHP_ME(BatchHandle, getBlobAlignment, arginfo_BatchHandle_getBlobAlignment, ZEND_ACC_PUBLIC)
	PHP_ME(BatchHandle, setDefaultBpb,    arginfo_BatchHandle_setDefaultBpb,    ZEND_ACC_PUBLIC)
	PHP_ME(BatchHandle, cancel,           arginfo_BatchHandle_cancel,           ZEND_ACC_PUBLIC)
	PHP_ME(BatchHandle, execute,          arginfo_BatchHandle_execute,          ZEND_ACC_PUBLIC)
	PHP_ME(BatchHandle, add,              arginfo_BatchHandle_add,              ZEND_ACC_PUBLIC)
	PHP_ME(BatchHandle, addBlob,          arginfo_BatchHandle_addBlob,          ZEND_ACC_PUBLIC)
	PHP_FE_END
};

#endif /* FB_API_VER >= 40 */
