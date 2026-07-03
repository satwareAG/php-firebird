/* fbird_class_event.c - Firebird\Event OOP class (skeleton, Phase H will add methods) */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_firebird.h"
#include "php_fbird_includes.h"
#include "firebird_utils.h"
#include "fbird_class_internal.h"

zend_object_handlers fbird_event_handlers;

zend_object *fbird_event_create_obj(zend_class_entry *ce)
{
	fbird_event_obj *intern = zend_object_alloc(sizeof(fbird_event_obj), ce);
	intern->event = NULL;
	zend_object_std_init(&intern->std, ce);
	object_properties_init(&intern->std, ce);
	intern->std.handlers = &fbird_event_handlers;
	return &intern->std;
}

void fbird_event_free_obj(zend_object *obj)
{
	fbird_event_obj *intern = fbird_event_from_obj(obj);
	if (intern->event) {
		_php_fbird_free_event(intern->event);
		efree(intern->event);
		intern->event = NULL;
	}
	zend_object_std_dtor(obj);
}

void fbird_setup_event_object(zval *rv, fbird_event *ev)
{
	object_init_ex(rv, fbird_event_ce);
	fbird_event_obj *intern = fbird_event_from_obj(Z_OBJ_P(rv));
	intern->event = ev;
}

fbird_event *fbird_event_get_ptr(zend_object *obj)
{
	fbird_event_obj *intern = fbird_event_from_obj(obj);
	return intern ? intern->event : NULL;
}
