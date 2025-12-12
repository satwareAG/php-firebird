/*
   +----------------------------------------------------------------------+
   | PHP Version 8.1+                                                     |
   +----------------------------------------------------------------------+
   | Copyright (c) The PHP Group                                          |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Michael Wegener <mw@satware.com>                            |
   +----------------------------------------------------------------------+
 */

/**
 * Phase 7: Event OO API Wrapper
 *
 * RAII wrapper for Firebird OO API IEvents interface.
 *
 * NOTE ON CURRENT IMPLEMENTATION:
 * The php-firebird extension uses a polling-based event model for PHP 8.1+
 * thread safety. This wrapper provides the foundation for:
 * 1. Future async event support with IEventCallback
 * 2. Clean resource management via RAII
 * 3. OO API method access for advanced features
 *
 * The IEventCallback interface is implemented but currently the extension
 * continues to use isc_wait_for_event() for synchronous polling because
 * PHP callbacks MUST execute in the PHP thread context.
 */

#ifndef FB_EVENTS_HPP
#define FB_EVENTS_HPP

#include <firebird/Interface.h>
#include <ibase.h>
#include <atomic>
#include <cstring>
#include <memory>

namespace fb {

/**
 * EventCallback - IEventCallback implementation for async event notification
 *
 * NOTE: This implementation is prepared for future async support.
 * Currently the extension uses synchronous polling (isc_wait_for_event).
 * The callback interface is provided for:
 * - Future async event handling if PHP threading model changes
 * - Proper API compliance with Firebird OO interface
 */
class EventCallback final : public Firebird::IEventCallbackImpl<EventCallback, Firebird::CheckStatusWrapper> {
public:
    EventCallback() : m_eventOccurred(false), m_refCount(1) {}
    ~EventCallback() = default;

    // Non-copyable, non-movable due to atomic members
    EventCallback(const EventCallback&) = delete;
    EventCallback& operator=(const EventCallback&) = delete;
    EventCallback(EventCallback&&) = delete;
    EventCallback& operator=(EventCallback&&) = delete;

    // IReferenceCounted implementation (REQUIRED)
    void addRef() { ++m_refCount; }
    int release() {
        int rc = --m_refCount;
        if (rc == 0) {
            // Note: Don't delete here as we're typically stack/member allocated
        }
        return rc;
    }

    // IEventCallback implementation
    void eventCallbackFunction(unsigned int length, const unsigned char* events) {
        (void)length;
        (void)events;
        m_eventOccurred.store(true, std::memory_order_release);
    }

    bool hasEventOccurred() const {
        return m_eventOccurred.load(std::memory_order_acquire);
    }

    void reset() {
        m_eventOccurred.store(false, std::memory_order_release);
    }

private:
    std::atomic<bool> m_eventOccurred;
    std::atomic<int> m_refCount;
};


/**
 * EventsWrapper - RAII wrapper for IEvents
 *
 * Manages the lifecycle of a queued event registration.
 * Provides cancel() for explicit cleanup.
 */
class EventsWrapper {
public:
    EventsWrapper() : m_events(nullptr), m_callback(nullptr) {}

    ~EventsWrapper() {
        // Note: Events should be cancelled before destruction via cancel()
        // The IEvents release is handled by Firebird internally after cancel
        m_events = nullptr;
        if (m_callback) {
            m_callback->release();
            m_callback = nullptr;
        }
    }

    // Non-copyable
    EventsWrapper(const EventsWrapper&) = delete;
    EventsWrapper& operator=(const EventsWrapper&) = delete;

    // Move semantics
    EventsWrapper(EventsWrapper&& other) noexcept
        : m_events(other.m_events)
        , m_callback(other.m_callback)
    {
        other.m_events = nullptr;
        other.m_callback = nullptr;
    }

    EventsWrapper& operator=(EventsWrapper&& other) noexcept {
        if (this != &other) {
            if (m_callback) {
                m_callback->release();
            }
            m_events = other.m_events;
            m_callback = other.m_callback;
            other.m_events = nullptr;
            other.m_callback = nullptr;
        }
        return *this;
    }

    /**
     * Queue events for async notification
     *
     * @param master     IMaster instance
     * @param attachment IAttachment to queue events on
     * @param length     Length of events buffer
     * @param events     Event buffer from isc_event_block
     * @param status_vector Output status vector for errors
     * @return true on success, false on failure
     *
     * NOTE: Currently not actively used - extension uses isc_wait_for_event()
     * for thread-safe synchronous polling. Prepared for future async support.
     */
    bool queue(Firebird::IMaster* master, Firebird::IAttachment* attachment,
               unsigned int length, const unsigned char* events,
               ISC_STATUS* status_vector) noexcept
    {
        if (!master || !attachment) {
            if (status_vector) {
                status_vector[0] = isc_arg_gds;
                status_vector[1] = isc_bad_db_handle;
                status_vector[2] = isc_arg_end;
            }
            return false;
        }

        // Create callback if not exists
        if (!m_callback) {
            m_callback = new (std::nothrow) EventCallback();
            if (!m_callback) {
                if (status_vector) {
                    status_vector[0] = isc_arg_gds;
                    status_vector[1] = isc_virmemexh;
                    status_vector[2] = isc_arg_end;
                }
                return false;
            }
        }
        m_callback->reset();

        Firebird::CheckStatusWrapper status(master->getStatus());
        try {
            m_events = attachment->queEvents(&status, m_callback, length, events);
            if (status.hasData()) {
                if (status_vector) {
                    const ISC_STATUS* errors = status.getErrors();
                    for (unsigned i = 0; errors[i] != isc_arg_end && i < ISC_STATUS_LENGTH - 1; ++i) {
                        status_vector[i] = errors[i];
                    }
                    status_vector[ISC_STATUS_LENGTH - 1] = isc_arg_end;
                }
                return false;
            }
            return true;
        } catch (...) {
            if (status_vector) {
                status_vector[0] = isc_arg_gds;
                status_vector[1] = isc_except2;
                status_vector[2] = isc_arg_end;
            }
            return false;
        }
    }

    /**
     * Cancel the queued event
     *
     * @param master IMaster instance
     * @param status_vector Output status vector for errors
     * @return true on success, false on failure
     */
    bool cancel(Firebird::IMaster* master, ISC_STATUS* status_vector) noexcept {
        if (!m_events) {
            return true; // Nothing to cancel
        }

        if (!master) {
            if (status_vector) {
                status_vector[0] = isc_arg_gds;
                status_vector[1] = isc_bad_db_handle;
                status_vector[2] = isc_arg_end;
            }
            return false;
        }

        Firebird::CheckStatusWrapper status(master->getStatus());
        try {
            m_events->cancel(&status);
            if (status.hasData()) {
                if (status_vector) {
                    const ISC_STATUS* errors = status.getErrors();
                    for (unsigned i = 0; errors[i] != isc_arg_end && i < ISC_STATUS_LENGTH - 1; ++i) {
                        status_vector[i] = errors[i];
                    }
                    status_vector[ISC_STATUS_LENGTH - 1] = isc_arg_end;
                }
                return false;
            }
            m_events = nullptr;
            return true;
        } catch (...) {
            if (status_vector) {
                status_vector[0] = isc_arg_gds;
                status_vector[1] = isc_except2;
                status_vector[2] = isc_arg_end;
            }
            return false;
        }
    }

    /**
     * Check if an event has occurred (via callback)
     */
    bool hasEventOccurred() const {
        return m_callback && m_callback->hasEventOccurred();
    }

    /**
     * Reset the event occurred flag
     */
    void resetEvent() {
        if (m_callback) {
            m_callback->reset();
        }
    }

    /**
     * Check if events are queued
     */
    bool isActive() const { return m_events != nullptr; }

    /**
     * Get the underlying IEvents pointer (for advanced usage)
     */
    Firebird::IEvents* get() const { return m_events; }

private:
    Firebird::IEvents* m_events;
    EventCallback* m_callback;
};

} // namespace fb

#endif // FB_EVENTS_HPP
