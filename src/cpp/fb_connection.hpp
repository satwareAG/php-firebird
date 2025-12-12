/*
  +----------------------------------------------------------------------+
  | Copyright (c) The PHP Group                                          |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | https://www.php.net/license/3_01.txt                                 |
  +----------------------------------------------------------------------+
*/

#ifndef FB_CONNECTION_HPP
#define FB_CONNECTION_HPP

/**
 * @file fb_connection.hpp
 * @brief RAII Connection wrapper for Firebird OO API
 *
 * This file provides a modern C++ wrapper for database connections using
 * the Firebird C++ Object-Oriented API (IAttachment interface). It replaces
 * the legacy isc_attach_database() / isc_detach_database() functions.
 *
 * Features:
 * - RAII resource management with automatic cleanup
 * - Version-aware features (timeouts for FB 4.0+, blob caching for FB 5.0+)
 * - DPB builder integration for connection parameters
 * - Exception-based error handling with status preservation
 * - C interop functions for gradual migration
 *
 * @see MODERNIZATION_PLAN_FB3_TO_FB5.md Phase 2: Connection Layer
 */

#include <firebird/Interface.h>
#include <ibase.h>
#include <string>
#include <string_view>
#include <optional>
#include <cstdint>

#include "fb_core.hpp"
#include "fb_status.hpp"
#include "fb_version.hpp"
#include "fb_dpb_builder.hpp"

namespace fb {

/**
 * Forward declaration for getMaster() - defined in firebird_utils.cpp
 * This retrieves the global IMaster instance from the PHP extension globals.
 */
Firebird::IMaster* getMaster() noexcept;

/**
 * Connection parameters structure for simplified connection creation.
 * Mirrors the parameters accepted by _php_fbird_attach_db().
 */
struct ConnectionParams {
    std::string_view database;      ///< Database path or connection string
    std::string_view user;          ///< Username (empty for trusted auth)
    std::string_view password;      ///< Password
    std::string_view charset;       ///< Character set (e.g., "UTF8")
    std::string_view role;          ///< SQL role name
    unsigned short dialect = 3;     ///< SQL dialect (1, 2, or 3)
    unsigned short num_buffers = 0; ///< Page buffer count (0 = server default)
    bool force_write = false;       ///< Force synchronous writes
    bool force_write_set = false;   ///< Whether force_write was explicitly set

#ifdef isc_dpb_session_time_zone
    std::string_view session_timezone;  ///< FB 4.0+: Session timezone
#endif

#ifdef isc_dpb_set_bind
    std::string_view bind_rules;    ///< FB 4.0+: Type binding rules
#endif
};

/**
 * RAII wrapper for Firebird database connections using the OO API.
 *
 * This class encapsulates an IAttachment handle and provides:
 * - Automatic resource cleanup via RAII
 * - Version-aware feature access (timeouts, blob caching)
 * - Integration with DpbBuilder for connection parameters
 * - Status preservation for PHP error reporting
 *
 * Usage example:
 * @code
 *   ConnectionParams params;
 *   params.database = "/var/db/test.fdb";
 *   params.user = "SYSDBA";
 *   params.password = "masterkey";
 *   params.charset = "UTF8";
 *
 *   try {
 *       Connection conn = Connection::create(params);
 *       // Use connection...
 *   } catch (const fb::Exception& e) {
 *       // Handle error, e.sqlcode(), e.gdscode(), e.what()
 *   }
 * @endcode
 */
class Connection {
public:
    /**
     * Factory method to create a new database connection.
     *
     * @param params Connection parameters
     * @return Connection object (throws fb::Exception on failure)
     * @throws fb::Exception if connection fails
     */
    [[nodiscard]] static Connection create(const ConnectionParams& params);

    /**
     * Factory method with explicit IMaster (for testing/advanced use).
     *
     * @param master Firebird master interface
     * @param params Connection parameters
     * @return Connection object
     * @throws fb::Exception if connection fails
     */
    [[nodiscard]] static Connection create(Firebird::IMaster* master,
                                            const ConnectionParams& params);

    /**
     * Default constructor creates an invalid (empty) connection.
     */
    Connection() noexcept = default;

    /**
     * Destructor - automatically detaches if still connected.
     */
    ~Connection();

    // Non-copyable (connection handles are unique)
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    // Movable
    Connection(Connection&& other) noexcept;
    Connection& operator=(Connection&& other) noexcept;

    /**
     * Check if the connection is valid (attached).
     */
    [[nodiscard]] bool isConnected() const noexcept {
        return attachment_ != nullptr;
    }

    /**
     * Explicit conversion to bool for if-statements.
     */
    explicit operator bool() const noexcept {
        return isConnected();
    }

    /**
     * Get the raw IAttachment pointer (for Firebird API calls).
     * @warning Do not release() this pointer; Connection owns it.
     */
    [[nodiscard]] Firebird::IAttachment* get() noexcept {
        return attachment_.get();
    }

    [[nodiscard]] const Firebird::IAttachment* get() const noexcept {
        return attachment_.get();
    }

    /**
     * Get the legacy isc_db_handle for compatibility with existing code.
     * @note This returns 0 for OO API connections; use get() instead.
     * @deprecated Use get() for OO API access.
     */
    [[nodiscard]] isc_db_handle getLegacyHandle() const noexcept {
        return legacy_handle_;
    }

    /**
     * Get version information for this connection's client library.
     */
    [[nodiscard]] const VersionInfo& getVersion() const noexcept {
        return version_;
    }

    /**
     * Explicitly detach from the database.
     * After this call, isConnected() returns false.
     *
     * @throws fb::Exception if detach fails
     */
    void detach();

    /**
     * Detach without throwing exceptions (for use in destructors).
     * @return true if detach succeeded, false otherwise
     */
    bool detachNoThrow() noexcept;

    /**
     * Drop the database (destructive operation).
     * @warning This permanently deletes the database file!
     *
     * @throws fb::Exception if drop fails
     */
    void dropDatabase();

    // -------------------------------------------------------------------------
    // FB 4.0+ Timeout Support
    // -------------------------------------------------------------------------

#ifdef FB_API_VER
#if FB_API_VER >= 40
    /**
     * Set statement execution timeout (FB 4.0+).
     * Statements running longer than this will be cancelled.
     *
     * @param milliseconds Timeout in milliseconds (0 = no timeout)
     * @return true if set successfully, false if not supported
     */
    bool setStatementTimeout(unsigned int milliseconds) noexcept;

    /**
     * Get the current statement timeout setting.
     * @return Timeout in milliseconds (0 = no timeout or not supported)
     */
    [[nodiscard]] unsigned int getStatementTimeout() const noexcept;

    /**
     * Set connection idle timeout (FB 4.0+).
     * Idle connections will be automatically closed.
     *
     * @param seconds Timeout in seconds (0 = no timeout)
     * @return true if set successfully, false if not supported
     */
    bool setIdleTimeout(unsigned int seconds) noexcept;

    /**
     * Get the current idle timeout setting.
     * @return Timeout in seconds (0 = no timeout or not supported)
     */
    [[nodiscard]] unsigned int getIdleTimeout() const noexcept;
#endif // FB_API_VER >= 40
#endif // FB_API_VER

    // -------------------------------------------------------------------------
    // Database Information
    // -------------------------------------------------------------------------

    /**
     * Get the database path/connection string used for this connection.
     */
    [[nodiscard]] const std::string& getDatabasePath() const noexcept {
        return database_path_;
    }

    /**
     * Get the SQL dialect for this connection.
     */
    [[nodiscard]] unsigned short getDialect() const noexcept {
        return dialect_;
    }

    // -------------------------------------------------------------------------
    // Status Access (for PHP error reporting)
    // -------------------------------------------------------------------------

    /**
     * Copy the last error status vector to a legacy ISC_STATUS array.
     * Used for integration with existing PHP error handling.
     *
     * @param dest Destination status vector
     * @param dest_size Size of destination array
     */
    void copyLastStatus(ISC_STATUS* dest, std::size_t dest_size = STATUS_VECTOR_SIZE) const noexcept;

private:
    /**
     * Private constructor from attachment pointer.
     */
    explicit Connection(AttachmentPtr attachment,
                       Firebird::IMaster* master,
                       std::string database_path,
                       unsigned short dialect,
                       unsigned version);

    AttachmentPtr attachment_;              ///< RAII-managed attachment
    Firebird::IMaster* master_ = nullptr;   ///< Master interface (not owned)
    isc_db_handle legacy_handle_ = 0;       ///< Legacy handle (for compatibility)
    std::string database_path_;             ///< Database path for this connection
    unsigned short dialect_ = 3;            ///< SQL dialect
    VersionInfo version_{VersionInfo::FB30}; ///< Client library version
    mutable StatusWrapper last_status_{nullptr}; ///< Last error status

    // Timeout settings (FB 4.0+)
    unsigned int statement_timeout_ms_ = 0;
    unsigned int idle_timeout_sec_ = 0;
};

// =============================================================================
// Implementation
// =============================================================================

inline Connection Connection::create(const ConnectionParams& params) {
    Firebird::IMaster* master = getMaster();
    if (!master) {
        throw Exception("Firebird master interface not available");
    }
    return create(master, params);
}

inline Connection Connection::create(Firebird::IMaster* master,
                                     const ConnectionParams& params) {
    if (!master) {
        throw Exception("Firebird master interface is null");
    }

    // Get provider (dispatcher)
    Firebird::IProvider* provider = master->getDispatcher();
    if (!provider) {
        throw Exception("Failed to get Firebird provider");
    }

    // Build DPB using the modern builder
    DpbBuilder dpb(master);

    if (!params.user.empty()) {
        dpb.setUser(params.user);
    }
    if (!params.password.empty()) {
        dpb.setPassword(params.password);
    }
    if (!params.charset.empty()) {
        dpb.setCharset(params.charset);
    }
    if (!params.role.empty()) {
        dpb.setRole(params.role);
    }
    if (params.dialect > 0) {
        dpb.setDialect(params.dialect);
    }
    if (params.num_buffers > 0) {
        dpb.setNumBuffers(params.num_buffers);
    }
    if (params.force_write_set) {
        dpb.setForceWrite(params.force_write);
    }

#ifdef isc_dpb_session_time_zone
    if (!params.session_timezone.empty()) {
        dpb.setSessionTimeZone(params.session_timezone);
    }
#endif

#ifdef isc_dpb_set_bind
    if (!params.bind_rules.empty()) {
        dpb.setBindRules(params.bind_rules);
    } else {
        // Default bind rules for INT128/DECFLOAT compatibility
        dpb.setBindRules("INT128 TO VARCHAR;DECFLOAT TO VARCHAR");
    }
#endif

    // Create status for attach operation
    StatusWrapper status(master);

    // Create database string (must be null-terminated)
    std::string db_string(params.database);

    // Attach to database using OO API
    Firebird::IAttachment* raw_attachment = provider->attachDatabase(
        status.get(),
        db_string.c_str(),
        dpb.getBufferLength(),
        dpb.getBuffer()
    );

    if (status.hasError() || !raw_attachment) {
        throw Exception(status.get());
    }

    // Detect client version
    unsigned client_version = 0;
    Firebird::IUtil* util = master->getUtilInterface();
    if (util) {
        // Get version from util interface
        // The client version is encoded as major * 256 + minor
        // For simplicity, we detect based on available features
#ifdef FB_API_VER
        client_version = FB_API_VER >= 50 ? VersionInfo::FB50 :
                        FB_API_VER >= 40 ? VersionInfo::FB40 :
                        VersionInfo::FB30;
#else
        client_version = VersionInfo::FB30;
#endif
    }

    // Wrap in RAII pointer
    AttachmentPtr attachment(raw_attachment);

    return Connection(
        std::move(attachment),
        master,
        std::move(db_string),
        params.dialect,
        client_version
    );
}

inline Connection::Connection(AttachmentPtr attachment,
                              Firebird::IMaster* master,
                              std::string database_path,
                              unsigned short dialect,
                              unsigned version)
    : attachment_(std::move(attachment)),
      master_(master),
      legacy_handle_(0),
      database_path_(std::move(database_path)),
      dialect_(dialect),
      version_(version),
      last_status_(master) {
}

inline Connection::~Connection() {
    detachNoThrow();
}

inline Connection::Connection(Connection&& other) noexcept
    : attachment_(std::move(other.attachment_)),
      master_(other.master_),
      legacy_handle_(other.legacy_handle_),
      database_path_(std::move(other.database_path_)),
      dialect_(other.dialect_),
      version_(other.version_),
      last_status_(std::move(other.last_status_)),
      statement_timeout_ms_(other.statement_timeout_ms_),
      idle_timeout_sec_(other.idle_timeout_sec_) {
    other.master_ = nullptr;
    other.legacy_handle_ = 0;
}

inline Connection& Connection::operator=(Connection&& other) noexcept {
    if (this != &other) {
        detachNoThrow();
        attachment_ = std::move(other.attachment_);
        master_ = other.master_;
        legacy_handle_ = other.legacy_handle_;
        database_path_ = std::move(other.database_path_);
        dialect_ = other.dialect_;
        version_ = other.version_;
        last_status_ = std::move(other.last_status_);
        statement_timeout_ms_ = other.statement_timeout_ms_;
        idle_timeout_sec_ = other.idle_timeout_sec_;
        other.master_ = nullptr;
        other.legacy_handle_ = 0;
    }
    return *this;
}

inline void Connection::detach() {
    if (!attachment_) {
        return;
    }

    if (!master_) {
        attachment_.reset();
        return;
    }

    StatusWrapper status(master_);
    attachment_->detach(status.get());

    if (status.hasError()) {
        last_status_ = std::move(status);
        throw Exception(last_status_.get());
    }

    attachment_.reset();
}

inline bool Connection::detachNoThrow() noexcept {
    if (!attachment_) {
        return true;
    }

    try {
        if (master_) {
            StatusWrapper status(master_);
            attachment_->detach(status.get());
            if (status.hasError()) {
                last_status_ = std::move(status);
                attachment_.reset();
                return false;
            }
        }
        attachment_.reset();
        return true;
    } catch (...) {
        attachment_.reset();
        return false;
    }
}

inline void Connection::dropDatabase() {
    if (!attachment_) {
        throw Exception("Connection is not attached");
    }

    if (!master_) {
        throw Exception("Master interface not available");
    }

    StatusWrapper status(master_);
    attachment_->dropDatabase(status.get());

    if (status.hasError()) {
        last_status_ = std::move(status);
        throw Exception(last_status_.get());
    }

    // After drop, the attachment is invalid
    attachment_.release(); // Don't call release() on dropped attachment
}

inline void Connection::copyLastStatus(ISC_STATUS* dest, std::size_t dest_size) const noexcept {
    last_status_.copyTo(dest, dest_size);
}

// FB 4.0+ Timeout implementation
#ifdef FB_API_VER
#if FB_API_VER >= 40
inline bool Connection::setStatementTimeout(unsigned int milliseconds) noexcept {
    if (!attachment_ || !version_.hasTimeouts()) {
        return false;
    }

    try {
        StatusWrapper status(master_);
        attachment_->setStatementTimeout(status.get(), milliseconds);
        if (status.isOk()) {
            statement_timeout_ms_ = milliseconds;
            return true;
        }
        last_status_ = std::move(status);
    } catch (...) {
        // Silently fail
    }
    return false;
}

inline unsigned int Connection::getStatementTimeout() const noexcept {
    return statement_timeout_ms_;
}

inline bool Connection::setIdleTimeout(unsigned int seconds) noexcept {
    if (!attachment_ || !version_.hasTimeouts()) {
        return false;
    }

    try {
        StatusWrapper status(master_);
        attachment_->setIdleTimeout(status.get(), seconds);
        if (status.isOk()) {
            idle_timeout_sec_ = seconds;
            return true;
        }
        last_status_ = std::move(status);
    } catch (...) {
        // Silently fail
    }
    return false;
}

inline unsigned int Connection::getIdleTimeout() const noexcept {
    return idle_timeout_sec_;
}
#endif // FB_API_VER >= 40
#endif // FB_API_VER

} // namespace fb

// =============================================================================
// C Interop Functions (extern "C")
// =============================================================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create a database connection using the OO API.
 * This function is designed to be called from C code (firebird.c).
 *
 * @param master_ptr Pointer to IMaster interface (from IBG(master_instance))
 * @param database Database path (null-terminated)
 * @param db_len Length of database path
 * @param user Username (null-terminated, may be NULL)
 * @param user_len Length of username
 * @param password Password (null-terminated, may be NULL)
 * @param password_len Length of password
 * @param charset Character set (null-terminated, may be NULL)
 * @param charset_len Length of charset
 * @param role SQL role (null-terminated, may be NULL)
 * @param role_len Length of role
 * @param num_buffers Number of page buffers (0 for default)
 * @param dialect SQL dialect (1, 2, or 3)
 * @param force_write Force write flag (-1 = not set, 0 = async, 1 = sync)
 * @param status_vector Output status vector for errors
 * @return Pointer to fb::Connection object, or NULL on failure
 */
void* fbc_connect(
    void* master_ptr,
    const char* database, size_t db_len,
    const char* user, size_t user_len,
    const char* password, size_t password_len,
    const char* charset, size_t charset_len,
    const char* role, size_t role_len,
    int num_buffers,
    int dialect,
    int force_write,
    ISC_STATUS* status_vector
);

/**
 * Detach a connection created with fbc_connect().
 *
 * @param connection Pointer returned by fbc_connect()
 * @param status_vector Output status vector for errors
 * @return 0 on success, non-zero on failure
 */
int fbc_disconnect(void* connection, ISC_STATUS* status_vector);

/**
 * Drop a database (destructive operation).
 *
 * @param connection Pointer returned by fbc_connect()
 * @param status_vector Output status vector for errors
 * @return 0 on success, non-zero on failure
 */
int fbc_drop_database(void* connection, ISC_STATUS* status_vector);

/**
 * Check if a connection is valid.
 *
 * @param connection Pointer returned by fbc_connect()
 * @return 1 if connected, 0 if not
 */
int fbc_is_connected(void* connection);

/**
 * Get the IAttachment pointer from a connection.
 *
 * @param connection Pointer returned by fbc_connect()
 * @return Raw IAttachment pointer, or NULL
 */
void* fbc_get_attachment(void* connection);

#ifdef __cplusplus
} // extern "C"
#endif

// =============================================================================
// C Interop Implementation (in header for simplicity)
// =============================================================================

#ifdef __cplusplus

inline void* fbc_connect(
    void* master_ptr,
    const char* database, size_t db_len,
    const char* user, size_t user_len,
    const char* password, size_t password_len,
    const char* charset, size_t charset_len,
    const char* role, size_t role_len,
    int num_buffers,
    int dialect,
    int force_write,
    ISC_STATUS* status_vector
) {
    try {
        auto* master = static_cast<Firebird::IMaster*>(master_ptr);
        if (!master) {
            if (status_vector) {
                status_vector[0] = isc_arg_gds;
                status_vector[1] = isc_unavailable;
                status_vector[2] = isc_arg_end;
            }
            return nullptr;
        }

        fb::ConnectionParams params;
        params.database = database ? std::string_view(database, db_len) : std::string_view();
        params.user = user ? std::string_view(user, user_len) : std::string_view();
        params.password = password ? std::string_view(password, password_len) : std::string_view();
        params.charset = charset ? std::string_view(charset, charset_len) : std::string_view();
        params.role = role ? std::string_view(role, role_len) : std::string_view();
        params.num_buffers = num_buffers > 0 ? static_cast<unsigned short>(num_buffers) : 0;
        params.dialect = dialect > 0 ? static_cast<unsigned short>(dialect) : 3;

        if (force_write >= 0) {
            params.force_write_set = true;
            params.force_write = (force_write != 0);
        }

        // Create connection - allocate on heap for C interop
        auto* conn = new fb::Connection(fb::Connection::create(master, params));

        // Clear status on success
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = 0;
            status_vector[2] = isc_arg_end;
        }

        return conn;

    } catch (const fb::Exception& e) {
        if (status_vector) {
            const ISC_STATUS* src = e.statusVector();
            if (src) {
                for (size_t i = 0; i < fb::STATUS_VECTOR_SIZE && src[i] != isc_arg_end; ++i) {
                    status_vector[i] = src[i];
                }
            }
        }
        return nullptr;
    } catch (...) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_random;
            status_vector[2] = isc_arg_end;
        }
        return nullptr;
    }
}

inline int fbc_disconnect(void* connection, ISC_STATUS* status_vector) {
    if (!connection) {
        return 0;
    }

    try {
        auto* conn = static_cast<fb::Connection*>(connection);
        conn->detach();
        delete conn;

        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = 0;
            status_vector[2] = isc_arg_end;
        }
        return 0;

    } catch (const fb::Exception& e) {
        if (status_vector) {
            const ISC_STATUS* src = e.statusVector();
            if (src) {
                for (size_t i = 0; i < fb::STATUS_VECTOR_SIZE && src[i] != isc_arg_end; ++i) {
                    status_vector[i] = src[i];
                }
            }
        }
        // Still delete the connection object
        delete static_cast<fb::Connection*>(connection);
        return 1;
    } catch (...) {
        delete static_cast<fb::Connection*>(connection);
        return 1;
    }
}

inline int fbc_drop_database(void* connection, ISC_STATUS* status_vector) {
    if (!connection) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_db_handle;
            status_vector[2] = isc_arg_end;
        }
        return 1;
    }

    try {
        auto* conn = static_cast<fb::Connection*>(connection);
        conn->dropDatabase();
        delete conn;

        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = 0;
            status_vector[2] = isc_arg_end;
        }
        return 0;

    } catch (const fb::Exception& e) {
        if (status_vector) {
            const ISC_STATUS* src = e.statusVector();
            if (src) {
                for (size_t i = 0; i < fb::STATUS_VECTOR_SIZE && src[i] != isc_arg_end; ++i) {
                    status_vector[i] = src[i];
                }
            }
        }
        delete static_cast<fb::Connection*>(connection);
        return 1;
    } catch (...) {
        delete static_cast<fb::Connection*>(connection);
        return 1;
    }
}

inline int fbc_is_connected(void* connection) {
    if (!connection) {
        return 0;
    }
    auto* conn = static_cast<fb::Connection*>(connection);
    return conn->isConnected() ? 1 : 0;
}

inline void* fbc_get_attachment(void* connection) {
    if (!connection) {
        return nullptr;
    }
    auto* conn = static_cast<fb::Connection*>(connection);
    return conn->get();
}

#endif // __cplusplus

#endif // FB_CONNECTION_HPP
