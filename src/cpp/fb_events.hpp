/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

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
/* LCOV_EXCL_START: EventWrapper — not yet instantiated; fbird_events.c uses
 * legacy isc_event_* C API. This C++ wrapper is a future Phase 9 placeholder. */

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

/* =============================================================================
 * Extern "C" wrapper functions for fb_events.hpp C++ classes
 * ============================================================================= */
extern "C" {

int fbe_cancel(void* master_ptr, void* events_wrapper, ISC_STATUS* status_vector) {
    if (!events_wrapper) return 0; // Nothing to cancel
    if (!master_ptr) return 1;
    auto* wrapper = static_cast<fb::EventsWrapper*>(events_wrapper);
    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    return wrapper->cancel(master, status_vector) ? 0 : 1;
}

void fbe_free(void* events_wrapper) {
    if (!events_wrapper) return;
    auto* wrapper = static_cast<fb::EventsWrapper*>(events_wrapper);
    delete wrapper;
}

int fbe_has_event_fired(void* events_wrapper) {
    if (!events_wrapper) return 0;
    auto* wrapper = static_cast<fb::EventsWrapper*>(events_wrapper);
    return wrapper->hasEventOccurred() ? 1 : 0;
}

void fbe_reset_event_fired(void* events_wrapper) {
    if (!events_wrapper) return;
    auto* wrapper = static_cast<fb::EventsWrapper*>(events_wrapper);
    wrapper->resetEvent();
}

int fbe_is_queued(void* events_wrapper) {
    if (!events_wrapper) return 0;
    auto* wrapper = static_cast<fb::EventsWrapper*>(events_wrapper);
    return wrapper->isActive() ? 1 : 0;
}

} // extern "C"
/* LCOV_EXCL_STOP */
