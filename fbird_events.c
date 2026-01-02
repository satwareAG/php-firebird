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

#ifndef PHP_WIN32
#include <signal.h>
#include <setjmp.h>
#include <errno.h>
#endif

static int le_event;

/**
 * ============================================================================
 * THREAD-SAFETY REDESIGN (PHP 8.1+)
 * ============================================================================
 *
 * PROBLEM:
 * The old implementation used isc_que_events() with a C callback that was
 * invoked from Firebird's internal thread. This callback called PHP functions
 * (call_user_function) from a non-PHP thread, which is fundamentally unsafe:
 * - No PHP request context exists on the Firebird thread
 * - TSRM macros are empty in PHP 8.1+ (TSRMLS_FETCH_FROM_CTX does nothing)
 * - Causes random crashes, memory corruption, and undefined behavior
 *
 * SOLUTION (Polling Model):
 * - fbird_set_event_handler() registers the event and stores the callback
 *   but does NOT use async callbacks from isc_que_events()
 * - fbird_poll_event() uses isc_wait_for_event() synchronously to check
 *   for events and calls the PHP callback from the PHP thread (safe!)
 * - fbird_wait_event() continues to work as before (blocking sync)
 *
 * This ensures all PHP callbacks execute in the correct PHP thread context.
 * ============================================================================
 */

static void _php_fbird_event_free(unsigned char *event_buf, unsigned char *result_buf)
{
	if (event_buf) {
		isc_free((ISC_SCHAR *)event_buf);
	}
	if (result_buf) {
		isc_free((ISC_SCHAR *)result_buf);
	}
}

void _php_fbird_free_event(fbird_event *event)
{
	unsigned short i;

	event->state = DEAD;

#if FB_API_VER >= 30
	/* Phase 7: Free OO API event wrapper if present */
	if (event->fbe_events) {
		fbe_cancel(IBG(master_instance), event->fbe_events, NULL);
		fbe_free(event->fbe_events);
		event->fbe_events = NULL;
	}
#endif

	if (event->link != NULL) {
		fbird_event **node;

		/* Remove this event from the link's event list */
		for (node = &event->link->event_head; *node && *node != event; node = &(*node)->event_next) {
			/* iterate */
		}
		if (*node == event) {
			*node = event->event_next;
		}

		/* Release reference to the DB link resource */
		if (event->link_res) {
			if (GC_DELREF(event->link_res) == 0) {
				zend_list_delete(event->link_res);
			}
			event->link_res = NULL;
		}

		event->link = NULL;
	}

	if (Z_TYPE(event->callback) != IS_UNDEF) {
		zval_ptr_dtor(&event->callback);
		ZVAL_UNDEF(&event->callback);

		if (event->event_buffer || event->result_buffer) {
			_php_fbird_event_free(event->event_buffer, event->result_buffer);
			event->event_buffer = NULL;
			event->result_buffer = NULL;
		}

		for (i = 0; i < event->event_count; ++i) {
			if (event->events[i]) {
				efree(event->events[i]);
			}
		}
		efree(event->events);
	}
}

static void _php_fbird_free_event_rsrc(zend_resource *rsrc)
{
	fbird_event *e = (fbird_event *) rsrc->ptr;
	_php_fbird_free_event(e);
	efree(e);
}

void php_fbird_events_minit(INIT_FUNC_ARGS)
{
	le_event = zend_register_list_destructors_ex(_php_fbird_free_event_rsrc, NULL,
		LE_EVENT, module_number);
}

/**
 * Build event buffers for event operations.
 * This creates the event_buffer and result_buffer needed for isc_wait_for_event.
 */
static void _php_fbird_event_block(unsigned short count, char **events,
	unsigned short *l, unsigned char **event_buf, unsigned char **result_buf)
{
	/**
	 * The Interbase API uses variadic arguments which we can't easily
	 * construct at runtime, but the maximum is 15 events.
	 */
	*l = (unsigned short) isc_event_block(event_buf, result_buf, count,
		events[0], events[1], events[2], events[3], events[4],
		events[5], events[6], events[7], events[8], events[9],
		events[10], events[11], events[12], events[13], events[14]);
}

PHP_FUNCTION(fbird_wait_event)
{
	zval *args;
	fbird_db_link *ib_link;
	int num_args;
	unsigned char *event_buffer, *result_buffer;
	char *events[15];
	uint32_t i = 0;
	unsigned short event_count = 0, buffer_size;
	ISC_ULONG occurred_event[15];

	RESET_ERRMSG;

	/* Validate argument count: 1-16 (optional link + 1-15 event names) */
	if (ZEND_NUM_ARGS() < 1 || ZEND_NUM_ARGS() > 16) {
		WRONG_PARAM_COUNT;
	}

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "+", &args, &num_args) == FAILURE) {
		return;
	}

	/* Determine if first argument is a link resource */
	if (Z_TYPE(args[0]) == IS_RESOURCE) {
		if ((ib_link = (fbird_db_link *)zend_fetch_resource2_ex(&args[0], "Firebird link", le_link, le_plink)) == NULL) {
			RETURN_FALSE;
		}
		i = 1;
	} else {
		if (ZEND_NUM_ARGS() > 15) {
			WRONG_PARAM_COUNT;
		}
		if ((ib_link = (fbird_db_link *)zend_fetch_resource2(IBG(default_link), "Firebird link", le_link, le_plink)) == NULL) {
			RETURN_FALSE;
		}
	}

	/* Initialize the events array to NULL */
	for (unsigned short j = 0; j < 15; ++j) {
		events[j] = NULL;
	}

	/* Collect event names */
	for (; i < ZEND_NUM_ARGS(); ++i) {
		convert_to_string_ex(&args[i]);
		events[event_count++] = Z_STRVAL(args[i]);
	}

	/* Build event buffers */
	_php_fbird_event_block(event_count, events, &buffer_size, &event_buffer, &result_buffer);

	/**
	 * Initialize the event buffers with a preliminary wait.
	 * This is required because isc_event_block() initializes counters to 0,
	 * and isc_wait_for_event() returns immediately if counters are 0.
	 * The first wait/count cycle establishes the baseline.
	 */
	{
		ISC_STATUS init_status[20];
		ISC_ULONG init_counts[15];
		if (isc_wait_for_event(init_status, &ib_link->handle.db, buffer_size, event_buffer, result_buffer)) {
			/* Initial wait failed - likely connection issue */
			_php_fbird_error();
			_php_fbird_event_free(event_buffer, result_buffer);
			RETURN_FALSE;
		}
		isc_event_counts(init_counts, buffer_size, event_buffer, result_buffer);
	}

	/* Now wait for actual events */
	if (isc_wait_for_event(IB_STATUS, &ib_link->handle.db, buffer_size, event_buffer, result_buffer)) {
		_php_fbird_error();
		_php_fbird_event_free(event_buffer, result_buffer);
		RETURN_FALSE;
	}

	/* Determine which event fired */
	isc_event_counts(occurred_event, buffer_size, event_buffer, result_buffer);
	for (i = 0; i < event_count; ++i) {
		if (occurred_event[i]) {
			zend_string *result = zend_string_init(events[i], strlen(events[i]), 0);
			_php_fbird_event_free(event_buffer, result_buffer);
			RETURN_STR(result);
		}
	}

	/* No event detected (should not happen) */
	_php_fbird_event_free(event_buffer, result_buffer);
	RETURN_FALSE;
}

PHP_FUNCTION(fbird_set_event_handler)
{
	zval *args, *cb_arg;
	fbird_db_link *ib_link;
	fbird_event *event;
	unsigned short i = 1, buffer_size;
	int num_args;
	zend_resource *link_res;

	RESET_ERRMSG;

	/* Validate argument count */
	if (ZEND_NUM_ARGS() < 2 || ZEND_NUM_ARGS() > 17) {
		WRONG_PARAM_COUNT;
	}

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "+", &args, &num_args) == FAILURE) {
		return;
	}

	/* Determine argument layout: [link,] callback, event, [event, ...] */
	if (Z_TYPE(args[0]) != IS_STRING) {
		/* First argument is resource, second is callback */
		if (ZEND_NUM_ARGS() < 3 || ZEND_NUM_ARGS() > 17) {
			WRONG_PARAM_COUNT;
		}

		cb_arg = &args[1];
		i = 2;

		if ((ib_link = (fbird_db_link *)zend_fetch_resource2_ex(&args[0], "Firebird link", le_link, le_plink)) == NULL) {
			RETURN_FALSE;
		}
		link_res = Z_RES(args[0]);
	} else {
		/* First argument is callback (use default link) */
		if (ZEND_NUM_ARGS() < 2 || ZEND_NUM_ARGS() > 16) {
			WRONG_PARAM_COUNT;
		}

		cb_arg = &args[0];

		if ((ib_link = (fbird_db_link *)zend_fetch_resource2(IBG(default_link), "Firebird link", le_link, le_plink)) == NULL) {
			RETURN_FALSE;
		}
		link_res = IBG(default_link);
	}

	/* Validate callback is callable */
	if (!zend_is_callable(cb_arg, 0, NULL)) {
		zend_string *cb_name = zend_get_callable_name(cb_arg);
		_php_fbird_module_error("Callback argument %s is not a callable function", ZSTR_VAL(cb_name));
		zend_string_release_ex(cb_name, 0);
		RETURN_FALSE;
	}

	/* Allocate and initialize event structure */
	event = (fbird_event *) safe_emalloc(sizeof(fbird_event), 1, 0);
	event->link_res = link_res;
	GC_ADDREF(link_res);
	event->link = ib_link;
	event->event_count = 0;
	event->state = NEW;
	event->needs_reregistration = 0;
	event->buffer_size = 0;
	event->callback_count = 0;
	event->max_callbacks = 1000; /* Safety limit for polling */
	event->event_id = 0;
	event->event_buffer = NULL;
	event->result_buffer = NULL;
	event->thread_ctx = NULL; /* Not used in polling model */
	event->fbe_events = NULL; /* Phase 7: OO API event wrapper (future async support) */
	event->events = (char **) safe_emalloc(sizeof(char *), 15, 0);

	/* Store callback reference */
	ZVAL_DUP(&event->callback, cb_arg);

	/* Collect event names */
	for (; i < 15; ++i) {
		if (i < ZEND_NUM_ARGS()) {
			convert_to_string_ex(&args[i]);
			event->events[event->event_count++] = estrdup(Z_STRVAL(args[i]));
		} else {
			event->events[i] = NULL;
		}
	}

	/* Build event buffers */
	_php_fbird_event_block(event->event_count, event->events,
		&buffer_size, &event->event_buffer, &event->result_buffer);
	event->buffer_size = buffer_size;

	/**
	 * NOTE: We do NOT call isc_wait_for_event() here because it blocks.
	 * The baseline initialization will happen on the first fbird_poll_event() call.
	 * The event handler is ready to use immediately.
	 */
	event->needs_reregistration = 1; /* Flag: first poll needs baseline init */

	/* Mark as active and add to link's event list */
	event->state = ACTIVE;
	event->event_next = ib_link->event_head;
	ib_link->event_head = event;

	RETVAL_RES(zend_register_resource(event, le_event));
	Z_TRY_ADDREF_P(return_value);
}

#ifndef PHP_WIN32
/* Signal handler for alarm-based timeout */
static volatile sig_atomic_t fbird_timeout_occurred = 0;
static void fbird_timeout_handler(int sig) {
	(void)sig;
	fbird_timeout_occurred = 1;
}
#endif

PHP_FUNCTION(fbird_poll_event)
{
	zval *event_arg;
	zend_long timeout_ms = -1;  /* Default: block forever */
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

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r|l", &event_arg, &timeout_ms) == FAILURE) {
		RETURN_FALSE;
	}

	event = (fbird_event *)zend_fetch_resource_ex(event_arg, "Firebird event", le_event);
	if (!event) {
		RETURN_FALSE;
	}

	/* Check if event handler is still valid */
	if (event->state == DEAD) {
		RETURN_NULL(); /* Handler was cancelled */
	}

	if (!event->link || event->link->handle.ptr == 0) {
		event->state = DEAD;
		RETURN_FALSE; /* Connection lost */
	}

	/* Safety limit check */
	if (event->callback_count >= event->max_callbacks) {
		event->state = DEAD;
		_php_fbird_module_error("Event callback limit exceeded");
		RETURN_FALSE;
	}

	/**
	 * Handle baseline initialization on first poll.
	 * isc_event_block() initializes counters to 0, and isc_wait_for_event()
	 * returns immediately if counters are 0. We need to do a first wait/count
	 * cycle to establish the baseline before waiting for actual events.
	 */
	if (event->needs_reregistration) {
		ISC_STATUS init_status[20];
		ISC_ULONG init_counts[15];

		if (isc_wait_for_event(init_status, &event->link->handle.db,
				event->buffer_size, event->event_buffer, event->result_buffer)) {
			/* Initial wait failed - likely connection issue */
			_php_fbird_error();
			event->state = DEAD;
			RETURN_FALSE;
		}
		isc_event_counts(init_counts, event->buffer_size,
			event->event_buffer, event->result_buffer);
		event->needs_reregistration = 0;
	}

#ifndef PHP_WIN32
	/**
	 * Set up alarm-based timeout for Unix systems.
	 * We use SIGALRM to interrupt isc_wait_for_event() after the specified timeout.
	 *
	 * Strategy:
	 * 1. Save any existing alarm state
	 * 2. Install our signal handler
	 * 3. Set alarm for timeout duration
	 * 4. Call isc_wait_for_event()
	 * 5. On return: cancel alarm, restore previous state
	 * 6. Check if timeout occurred
	 */
	if (timeout_ms >= 0) {
		use_timeout = 1;
		fbird_timeout_occurred = 0;

		/* Set up our signal handler, saving the old one */
		memset(&sa_new, 0, sizeof(sa_new));
		sa_new.sa_handler = fbird_timeout_handler;
		sigemptyset(&sa_new.sa_mask);
		sa_new.sa_flags = 0;  /* No SA_RESTART - we want EINTR */

		if (sigaction(SIGALRM, &sa_new, &sa_old) == 0) {
			had_old_handler = 1;
		}

		/* Cancel any pending alarm and save remaining time */
		alarm_remaining = alarm(0);

		/* Set our timeout alarm (convert ms to seconds, round up, minimum 1s) */
		if (timeout_ms == 0) {
			/* For 0ms timeout, we still need to set alarm to interrupt immediately */
			/* Use the smallest possible alarm (1 second) but check the flag first */
			alarm(1);
		} else {
			unsigned int timeout_sec = (unsigned int)((timeout_ms + 999) / 1000);
			if (timeout_sec == 0) {
				timeout_sec = 1;
			}
			alarm(timeout_sec);
		}
	}
#endif

	/**
	 * Use isc_wait_for_event() synchronously.
	 * This blocks until an event fires OR until interrupted by SIGALRM.
	 */
	wait_result = isc_wait_for_event(IB_STATUS, &event->link->handle.db, event->buffer_size,
			event->event_buffer, event->result_buffer);

#ifndef PHP_WIN32
	/* Clean up timeout handling */
	if (use_timeout) {
		/* Cancel our alarm */
		alarm(0);

		/* Restore previous signal handler */
		if (had_old_handler) {
			sigaction(SIGALRM, &sa_old, NULL);
		}

		/* Restore any previous alarm that was pending */
		if (alarm_remaining > 0) {
			alarm(alarm_remaining);
		}

		/* Check if timeout occurred */
		if (fbird_timeout_occurred) {
			RETURN_LONG(PHP_FBIRD_EVENT_TIMEOUT);
		}
	}
#endif

	/* Check for errors from isc_wait_for_event */
	if (wait_result != 0) {
#ifndef PHP_WIN32
		/* On Unix, EINTR from timeout is handled above via fbird_timeout_occurred flag.
		 * If we get here with an error, it's a real error. */
#endif
		_php_fbird_error();
		event->state = DEAD;
		RETURN_FALSE;
	}

	/* Get event counts to determine which event fired */
	isc_event_counts(occurred_event, event->buffer_size,
		event->event_buffer, event->result_buffer);

	/* Find the event that occurred */
	for (i = 0; i < event->event_count; ++i) {
		if (occurred_event[i]) {
			zval return_value_cb, args[1];

			ZVAL_UNDEF(&return_value_cb);
			ZVAL_STRING(&args[0], event->events[i]);

			event->callback_count++;

			/**
			 * Call the PHP callback - THREAD-SAFE!
			 * We're executing in the PHP thread, so call_user_function is safe.
			 */
			if (FAILURE == call_user_function(NULL, NULL, &event->callback,
					&return_value_cb, 1, args)) {
				_php_fbird_module_error("Error calling event callback");
				zval_ptr_dtor(&args[0]);
				event->state = DEAD;
				RETURN_FALSE;
			}

			/* Check if callback wants to cancel future events */
			if (Z_TYPE(return_value_cb) != IS_UNDEF && !zend_is_true(&return_value_cb)) {
				event->state = DEAD;
			}

			/* Clean up */
			zval_ptr_dtor(&args[0]);
			zval_ptr_dtor(&return_value_cb);

			/* Return the event name that fired */
			RETURN_STRING(event->events[i]);
		}
	}

	/* No event detected in this poll cycle */
	RETURN_NULL();
}

PHP_FUNCTION(fbird_free_event_handler)
{
	zval *event_arg;

	RESET_ERRMSG;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "r", &event_arg)) {
		fbird_event *event;

		event = (fbird_event *)zend_fetch_resource_ex(event_arg, "Firebird event", le_event);
		if (!event) {
			RETURN_FALSE;
		}

		event->state = DEAD;

		zend_list_delete(Z_RES_P(event_arg));
		RETURN_TRUE;
	} else {
		RETURN_FALSE;
	}
}

#endif /* HAVE_FIREBIRD */
