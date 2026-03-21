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


/**
 * Build an Event Parameter Block (EPB) — replaces isc_event_block().
 *
 * Constructs the binary EPB format manually:
 *   1 byte version (1), then for each event: 1 byte name-len, name bytes, 4 bytes count (LE).
 *
 * @param event_buf   Output: allocated EPB buffer (caller must call fbe_event_free())
 * @param result_buf  Output: allocated result buffer (same size, zeroed)
 * @param count       Number of event names (1-15)
 * @param names       Array of event name C-strings
 * @return            Buffer length, or 0 on error
 */
unsigned short fbe_event_block(unsigned char** event_buf, unsigned char** result_buf,
                               unsigned short count, const char** names)
{
    if (!event_buf || !result_buf || count == 0 || count > 15 || !names) {
        return 0;
    }

    /* Calculate required buffer size: 1 (version) + sum(1 + namelen + 4) per event */
    unsigned short total = 1;
    for (unsigned short i = 0; i < count; ++i) {
        if (!names[i]) return 0;
        size_t nlen = strlen(names[i]);
        if (nlen > 255) return 0;
        total = (unsigned short)(total + 1 + nlen + 4);
    }

    unsigned char* ebuf = (unsigned char*)malloc(total);
    unsigned char* rbuf = (unsigned char*)malloc(total);
    if (!ebuf || !rbuf) {
        free(ebuf);
        free(rbuf);
        return 0;
    }
    memset(rbuf, 0, total);

    unsigned char* p = ebuf;
    *p++ = 1; /* EPB version */
    for (unsigned short i = 0; i < count; ++i) {
        size_t nlen = strlen(names[i]);
        *p++ = (unsigned char)nlen;
        memcpy(p, names[i], nlen);
        p += nlen;
        /* Initial count = 0, little-endian 4 bytes */
        *p++ = 0; *p++ = 0; *p++ = 0; *p++ = 0;
    }

    *event_buf  = ebuf;
    *result_buf = rbuf;
    return total;
}

/**
 * Wait synchronously for events — wraps isc_wait_for_event() via legacy handle.
 *
 * @param db_handle_ptr  Pointer to isc_db_handle (from fbc_get_legacy_handle_ptr())
 * @param buffer_size    EPB buffer size (from fbe_event_block())
 * @param event_buf      Event buffer
 * @param result_buf     Result buffer (updated on return)
 * @param status_vector  Output status vector
 * @return               0 on success, non-zero on error
 */
ISC_STATUS fbe_wait_for_event(void* db_handle_ptr, unsigned short buffer_size,
                              unsigned char* event_buf, unsigned char* result_buf,
                              ISC_STATUS* status_vector)
{
    if (!db_handle_ptr || !event_buf || !result_buf || !status_vector) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_db_handle;
            status_vector[2] = isc_arg_end;
        }
        return isc_bad_db_handle;
    }
    isc_db_handle* handle = static_cast<isc_db_handle*>(db_handle_ptr);
    return isc_wait_for_event(status_vector, handle, buffer_size, event_buf, result_buf);
}

/**
 * Decode event counts from result buffer — replaces isc_event_counts().
 *
 * Compares event_buf (old counts) with result_buf (new counts) and writes
 * the delta (new - old) into occurred[].
 *
 * @param occurred    Output: array of ISC_ULONG deltas (one per event)
 * @param buffer_size EPB buffer size
 * @param event_buf   Original event buffer (old counts)
 * @param result_buf  Result buffer (new counts)
 */
void fbe_event_counts(ISC_ULONG* occurred, unsigned short buffer_size,
                      unsigned char* event_buf, unsigned char* result_buf)
{
    if (!occurred || !event_buf || !result_buf || buffer_size < 1) return;

    const unsigned char* ep = event_buf  + 1; /* skip version byte */
    const unsigned char* rp = result_buf + 1;
    const unsigned char* ep_end = event_buf + buffer_size;
    unsigned idx = 0;

    while (ep < ep_end) {
        unsigned char nlen = *ep++;
        rp++; /* skip name length in result */
        ep += nlen; /* skip name in event */
        rp += nlen; /* skip name in result */
        if (ep + 4 > ep_end) break;

        /* Read little-endian 4-byte counts */
        ISC_ULONG old_cnt = (ISC_ULONG)ep[0] | ((ISC_ULONG)ep[1] << 8)
                          | ((ISC_ULONG)ep[2] << 16) | ((ISC_ULONG)ep[3] << 24);
        ISC_ULONG new_cnt = (ISC_ULONG)rp[0] | ((ISC_ULONG)rp[1] << 8)
                          | ((ISC_ULONG)rp[2] << 16) | ((ISC_ULONG)rp[3] << 24);
        occurred[idx++] = (new_cnt > old_cnt) ? (new_cnt - old_cnt) : 0;

        /* Advance result pointer past count bytes */
        rp += 4;
        ep += 4;
    }
}

/**
 * Free an EPB buffer allocated by fbe_event_block() — replaces isc_free().
 *
 * @param buf  Buffer to free (may be NULL)
 */
void fbe_event_free(unsigned char* buf)
{
    free(buf);
}

} // extern "C"
