/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

/**
 * Phase 8: Service OO API Wrapper
 *
 * RAII wrapper for Firebird OO API IService interface.
 *
 * The IService interface provides service manager operations:
 * - attachServiceManager() - connect to service manager
 * - detach() - disconnect from service manager
 * - start() - start a service task (backup, restore, etc.)
 * - query() - query service status/results
 *
 * This wrapper replaces the legacy isc_service_attach, isc_service_detach,
 * isc_service_start, and isc_service_query functions.
 */

#ifndef FB_SERVICE_HPP
#define FB_SERVICE_HPP

#include <firebird/Interface.h>
#include <ibase.h>
#include <cstring>
#include <memory>

namespace fb {

/**
 * ServiceWrapper - RAII wrapper for IService
 *
 * Manages the lifecycle of a service manager connection.
 * Provides methods matching the legacy isc_service_* API.
 */
class ServiceWrapper {
public:
    ServiceWrapper() : m_service(nullptr), m_master(nullptr) {}

    ~ServiceWrapper() {
        // Note: Service should be detached before destruction via detach()
        // We don't auto-detach as that could hide errors
        m_service = nullptr;
        m_master = nullptr;
    }

    // Non-copyable
    ServiceWrapper(const ServiceWrapper&) = delete;
    ServiceWrapper& operator=(const ServiceWrapper&) = delete;

    // Move semantics
    ServiceWrapper(ServiceWrapper&& other) noexcept
        : m_service(other.m_service)
        , m_master(other.m_master)
    {
        other.m_service = nullptr;
        other.m_master = nullptr;
    }

    ServiceWrapper& operator=(ServiceWrapper&& other) noexcept {
        if (this != &other) {
            m_service = other.m_service;
            m_master = other.m_master;
            other.m_service = nullptr;
            other.m_master = nullptr;
        }
        return *this;
    }

    /**
     * Attach to the service manager
     *
     * @param master IMaster instance
     * @param service_name Service name (e.g., "localhost:service_mgr")
     * @param spb_length Service parameter buffer length
     * @param spb Service parameter buffer
     * @param status_vector Output status vector for errors
     * @return true on success, false on failure
     */
    bool attach(Firebird::IMaster* master, const char* service_name,
                unsigned spb_length, const unsigned char* spb,
                ISC_STATUS* status_vector) noexcept
    {
        if (!master) {
            set_status_error(status_vector, isc_bad_svc_handle);
            return false;
        }

        // Detach existing connection if any
        if (m_service) {
            detach(status_vector);
        }

        m_master = master;
        fb::CheckStatusScope scope(master);
        auto* status = scope.get();

        try {
            Firebird::IProvider* provider = master->getDispatcher();
            if (!provider) {
                set_status_error(status_vector, isc_unavailable);
                return false;
            }

            m_service = provider->attachServiceManager(
                status,
                service_name,
                spb_length,
                spb
            );

            if (status->hasData()) {
                copy_status_to_sv(status_vector, status);
                m_service = nullptr;
                return false;
            }

            return m_service != nullptr;
        } catch (...) {
            set_status_error(status_vector, isc_except2);
            m_service = nullptr;
            return false;
        }
    }

    /**
     * Detach from the service manager
     *
     * @param status_vector Output status vector for errors
     * @return true on success, false on failure
     */
    bool detach(ISC_STATUS* status_vector) noexcept {
        if (!m_service) {
            return true; // Already detached
        }

        if (!m_master) {
            set_status_error(status_vector, isc_bad_svc_handle);
            return false;
        }

        fb::CheckStatusScope scope(m_master);
        auto* status = scope.get();
        try {
            m_service->detach(status);
            if (status->hasData()) {
                copy_status_to_sv(status_vector, status);
                return false;
            }
            m_service = nullptr;
            return true;
        } catch (...) {
            set_status_error(status_vector, isc_except2);
            return false;
        }
    }

    /**
     * Start a service task
     *
     * @param spb_length Service parameter buffer length
     * @param spb Service parameter buffer
     * @param status_vector Output status vector for errors
     * @return true on success, false on failure
     */
    bool start(unsigned spb_length, const unsigned char* spb,
               ISC_STATUS* status_vector) noexcept
    {
        if (!m_service || !m_master) {
            set_status_error(status_vector, isc_bad_svc_handle);
            return false;
        }

        fb::CheckStatusScope scope(m_master);
        auto* status = scope.get();
        try {
            m_service->start(status, spb_length, spb);
            if (status->hasData()) {
                copy_status_to_sv(status_vector, status);
                return false;
            }
            return true;
        } catch (...) {
            set_status_error(status_vector, isc_except2);
            return false;
        }
    }

    /**
     * Query service status/results
     *
     * @param send_length Send buffer length
     * @param send_items Send buffer
     * @param recv_length Receive items length
     * @param recv_items Receive items
     * @param buffer_length Output buffer length
     * @param buffer Output buffer
     * @param status_vector Output status vector for errors
     * @return true on success, false on failure
     */
    bool query(unsigned send_length, const unsigned char* send_items,
               unsigned recv_length, const unsigned char* recv_items,
               unsigned buffer_length, unsigned char* buffer,
               ISC_STATUS* status_vector) noexcept
    {
        if (!m_service || !m_master) {
            set_status_error(status_vector, isc_bad_svc_handle);
            return false;
        }

        fb::CheckStatusScope scope(m_master);
        auto* status = scope.get();
        try {
            m_service->query(status, send_length, send_items,
                            recv_length, recv_items, buffer_length, buffer);
            if (status->hasData()) {
                copy_status_to_sv(status_vector, status);
                return false;
            }
            return true;
        } catch (...) {
            set_status_error(status_vector, isc_except2);
            return false;
        }
    }

    /**
     * Check if attached to service manager
     */
    bool isAttached() const { return m_service != nullptr; }

private:
    Firebird::IService* m_service;
    Firebird::IMaster* m_master;
};

} // namespace fb

/* =============================================================================
 * C Interface Functions for Service Manager Operations
 *
 * These extern "C" functions provide the C-compatible interface declared in
 * firebird_utils.h. They wrap the C++ ServiceWrapper class.
 * ============================================================================= */

extern "C" {

/**
 * Attach to the service manager using OO API.
 */
void* fbsvc_attach(void* master_ptr,
                          const char* service_name,
                          unsigned spb_length,
                          const unsigned char* spb,
                          ISC_STATUS* status_vector)
{
    if (!master_ptr || !service_name) {
        set_status_error(status_vector, isc_bad_svc_handle);
        return nullptr;
    }

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* wrapper = new (std::nothrow) fb::ServiceWrapper();
    if (!wrapper) {
        set_status_error(status_vector, isc_virmemexh);
        return nullptr;
    }

    if (!wrapper->attach(master, service_name, spb_length, spb, status_vector)) {
        delete wrapper;
        return nullptr;
    }

    return wrapper;
}

/**
 * Detach from the service manager.
 */
int fbsvc_detach(void* master_ptr, void* service_wrapper, ISC_STATUS* status_vector)
{
    (void)master_ptr; // Unused, kept for API consistency

    if (!service_wrapper) {
        return 1; // Already detached
    }

    auto* wrapper = static_cast<fb::ServiceWrapper*>(service_wrapper);
    return wrapper->detach(status_vector) ? 1 : 0;
}

/**
 * Start a service task.
 */
int fbsvc_start(void* master_ptr,
                       void* service_wrapper,
                       unsigned spb_length,
                       const unsigned char* spb,
                       ISC_STATUS* status_vector)
{
    (void)master_ptr; // Unused, kept for API consistency

    if (!service_wrapper) {
        set_status_error(status_vector, isc_bad_svc_handle);
        return 0;
    }

    auto* wrapper = static_cast<fb::ServiceWrapper*>(service_wrapper);
    return wrapper->start(spb_length, spb, status_vector) ? 1 : 0;
}

/**
 * Query service status/results.
 */
int fbsvc_query(void* master_ptr,
                       void* service_wrapper,
                       unsigned send_length,
                       const unsigned char* send_items,
                       unsigned recv_length,
                       const unsigned char* recv_items,
                       unsigned buffer_length,
                       unsigned char* buffer,
                       ISC_STATUS* status_vector)
{
    (void)master_ptr; // Unused, kept for API consistency

    if (!service_wrapper) {
        set_status_error(status_vector, isc_bad_svc_handle);
        return 0;
    }

    auto* wrapper = static_cast<fb::ServiceWrapper*>(service_wrapper);
    return wrapper->query(send_length, send_items, recv_length, recv_items,
                         buffer_length, buffer, status_vector) ? 1 : 0;
}

/**
 * Check if attached to service manager.
 */
int fbsvc_is_attached(void* service_wrapper)
{
    if (!service_wrapper) {
        return 0;
    }

    auto* wrapper = static_cast<fb::ServiceWrapper*>(service_wrapper);
    return wrapper->isAttached() ? 1 : 0;
}

/**
 * Free service wrapper.
 */
void fbsvc_free(void* service_wrapper)
{
    if (service_wrapper) {
        auto* wrapper = static_cast<fb::ServiceWrapper*>(service_wrapper);
        delete wrapper;
    }
}

} // extern "C"

#endif // FB_SERVICE_HPP
