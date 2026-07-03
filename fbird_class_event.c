/* fbird_class_event.c - Firebird\Event OOP class (Phase H) */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_firebird.h"
#include "php_fbird_includes.h"
#include "firebird_utils.h"
#include "fbird_class_internal.h"

#ifndef PHP_WIN32
#include <signal.h>
#endif

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

/* ── Phase H: Event methods ─────────────────────────────────────────── */

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_event_wait, 0, 0, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_DOUBLE, 0, "-1.0")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_event_cancel, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_event_get_name, 0, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_event_get_count, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

/* Event::wait(float $timeout = -1.0): bool
 * Blocks until event fires or timeout expires. Calls registered callback.
 * Returns true if event fired, false on timeout/error. */
PHP_METHOD(Firebird_Event, wait)
{
	double timeout = -1.0;
	fbird_event_obj *intern;
	fbird_event *event;
	ISC_ULONG occurred_event[15];
	unsigned short i;
	ISC_STATUS wait_result;
#ifndef PHP_WIN32
	struct sigaction sa_new, sa_old;
	unsigned int alarm_remaining = 0;
	int use_timeout = 0;
	int had_old_handler = 0;
#endif

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|d", &timeout) == FAILURE) {
		RETURN_THROWS();
	}

	intern = fbird_event_from_obj(Z_OBJ_P(getThis()));
	event = intern->event;

	if (!event || event->state == DEAD) {
		RETURN_FALSE;
	}

	if (!event->link || !fbc_is_connected(event->link->fbc_connection)) {
		event->state = DEAD;
		RETURN_FALSE;
	}

	if (event->callback_count >= event->max_callbacks) {
		event->state = DEAD;
		_php_fbird_module_error("Event callback limit exceeded");
		RETURN_FALSE;
	}

#ifndef PHP_WIN32
	if (timeout >= 0) {
		use_timeout = 1;
		fbird_timeout_occurred = 0;
		memset(&sa_new, 0, sizeof(sa_new));
		sa_new.sa_handler = fbird_timeout_handler;
		sigemptyset(&sa_new.sa_mask);
		sa_new.sa_flags = 0;

		if (sigaction(SIGALRM, &sa_new, &sa_old) == 0) {
			had_old_handler = 1;
		}
		alarm_remaining = alarm(0);
		if (timeout == 0.0) {
			alarm(1);
		} else {
			unsigned int timeout_sec = (unsigned int)(timeout + 0.999);
			if (timeout_sec == 0) timeout_sec = 1;
			alarm(timeout_sec);
		}
	}
#endif

	/* Zero timeout = immediate check, no blocking */
	if (timeout == 0.0) {
#ifndef PHP_WIN32
		if (use_timeout) {
			alarm(0);
			if (had_old_handler) sigaction(SIGALRM, &sa_old, NULL);
			if (alarm_remaining > 0) alarm(alarm_remaining);
		}
#endif
		RETURN_FALSE;
	}

	/* Baseline initialization on first wait */
	if (event->needs_reregistration) {
		ISC_STATUS init_status[20];
		ISC_ULONG init_counts[15];
		void *attachment_ptr = fbc_get_attachment(event->link->fbc_connection);

		if (fbe_wait_for_event_oo(init_status, attachment_ptr,
				event->buffer_size, event->event_buffer, event->result_buffer)) {
#ifndef PHP_WIN32
			if (use_timeout) {
				alarm(0);
				if (had_old_handler) sigaction(SIGALRM, &sa_old, NULL);
				if (alarm_remaining > 0) alarm(alarm_remaining);
				if (fbird_timeout_occurred) {
					RETURN_FALSE;
				}
			}
#endif
			_php_fbird_error();
			event->state = DEAD;
			RETURN_FALSE;
		}
		fbe_event_counts(init_counts, event->buffer_size,
			event->event_buffer, event->result_buffer);
		event->needs_reregistration = 0;
	}

	wait_result = fbe_wait_for_event_oo(IB_STATUS,
		fbc_get_attachment(event->link->fbc_connection),
		event->buffer_size, event->event_buffer, event->result_buffer);

#ifndef PHP_WIN32
	if (use_timeout) {
		alarm(0);
		if (had_old_handler) sigaction(SIGALRM, &sa_old, NULL);
		if (alarm_remaining > 0) alarm(alarm_remaining);
		if (fbird_timeout_occurred) {
			RETURN_FALSE;
		}
	}
#endif

	if (wait_result != 0) {
		_php_fbird_error();
		event->state = DEAD;
		RETURN_FALSE;
	}

	fbe_event_counts(occurred_event, event->buffer_size,
		event->event_buffer, event->result_buffer);

	for (i = 0; i < event->event_count; ++i) {
		if (occurred_event[i]) {
			zval return_value_cb, args[1];

			ZVAL_UNDEF(&return_value_cb);
			ZVAL_STRING(&args[0], event->events[i]);

			event->callback_count++;

			if (event->last_fired_event) {
				efree(event->last_fired_event);
			}
			event->last_fired_event = estrdup(event->events[i]);

			if (FAILURE == call_user_function(NULL, NULL, &event->callback,
					&return_value_cb, 1, args)) {
				_php_fbird_module_error("Error calling event callback");
				zval_ptr_dtor(&args[0]);
				event->state = DEAD;
				RETURN_FALSE;
			}

			if (Z_TYPE(return_value_cb) != IS_UNDEF && !zend_is_true(&return_value_cb)) {
				event->state = DEAD;
			}

			zval_ptr_dtor(&args[0]);
			zval_ptr_dtor(&return_value_cb);
			RETURN_TRUE;
		}
	}

	RETURN_FALSE;
}

/* Event::cancel(): bool
 * Cancels a pending event wait. */
PHP_METHOD(Firebird_Event, cancel)
{
	fbird_event_obj *intern;

	RESET_ERRMSG;

	if (zend_parse_parameters_none() == FAILURE) {
		RETURN_THROWS();
	}

	intern = fbird_event_from_obj(Z_OBJ_P(getThis()));
	if (!intern->event || intern->event->state == DEAD) {
		RETURN_FALSE;
	}

	intern->event->state = DEAD;
	RETURN_TRUE;
}

/* Event::getName(): string
 * Returns the name of the last event that fired,
 * or the first registered event name if none fired yet. */
PHP_METHOD(Firebird_Event, getName)
{
	fbird_event_obj *intern;

	RESET_ERRMSG;

	if (zend_parse_parameters_none() == FAILURE) {
		RETURN_THROWS();
	}

	intern = fbird_event_from_obj(Z_OBJ_P(getThis()));
	if (!intern->event) {
		RETURN_EMPTY_STRING();
	}

	if (intern->event->last_fired_event) {
		RETURN_STRING(intern->event->last_fired_event);
	}

	if (intern->event->event_count > 0 && intern->event->events[0]) {
		RETURN_STRING(intern->event->events[0]);
	}

	RETURN_EMPTY_STRING();
}

/* Event::getCount(): int
 * Returns the number of times the event callback has been invoked. */
PHP_METHOD(Firebird_Event, getCount)
{
	fbird_event_obj *intern;

	RESET_ERRMSG;

	if (zend_parse_parameters_none() == FAILURE) {
		RETURN_THROWS();
	}

	intern = fbird_event_from_obj(Z_OBJ_P(getThis()));
	if (!intern->event) {
		RETURN_LONG(0);
	}

	RETURN_LONG(intern->event->callback_count);
}

const zend_function_entry fbird_event_methods[] = {
	PHP_ME(Firebird_Event, wait,     arginfo_fbird_event_wait,      ZEND_ACC_PUBLIC)
	PHP_ME(Firebird_Event, cancel,   arginfo_fbird_event_cancel,    ZEND_ACC_PUBLIC)
	PHP_ME(Firebird_Event, getName,  arginfo_fbird_event_get_name,  ZEND_ACC_PUBLIC)
	PHP_ME(Firebird_Event, getCount, arginfo_fbird_event_get_count, ZEND_ACC_PUBLIC)
	PHP_FE_END
};
