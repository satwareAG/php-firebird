/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

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

#ifdef isc_dpb_parallel_workers
    unsigned int parallel_workers = 0; ///< FB 5.0+: Parallel workers (0 = default)
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
     * Factory method to wrap an existing IAttachment* (e.g., from CREATE DATABASE).
     * Takes ownership of the attachment pointer.
     *
     * @param master Firebird master interface
     * @param attachment Existing IAttachment* (ownership transferred)
     * @param database_path Database path string
     * @param dialect SQL dialect
     * @return Connection object wrapping the attachment
     */
    [[nodiscard]] static Connection createFromAttachment(
        Firebird::IMaster* master,
        Firebird::IAttachment* attachment,
        std::string database_path,
        unsigned short dialect);

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
    bool dropped_ = false;                  ///< True after dropDatabase() — attachment is invalid
    std::string database_path_;             ///< Database path for this connection
    unsigned short dialect_ = 3;            ///< SQL dialect
    VersionInfo version_{VersionInfo::FB30}; ///< Client library version
    mutable StatusWrapper last_status_{static_cast<Firebird::IStatus*>(nullptr)}; ///< Last error status

    /**
     * Probe the server-side attachment with a lightweight getInfo() roundtrip.
     *
     * Used by detachNoThrow() to verify the attachment is alive before
     * calling detach(). If the database was dropped by another connection
     * (e.g., test harness cleanup_db()), the server-side attachment is dead
     * and calling detach() on it SIGSEGVs on FB 3.0 (issue #260).
     *
     * The dropped_ flag only covers same-connection dropDatabase(); this
     * method covers the cross-connection drop scenario.
     *
     * @param master Master interface for status allocation (must not be null)
     * @return true if attachment responds, false if dead or unreachable
     */
    bool pingAttachment(Firebird::IMaster* master) noexcept;

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
        // Default bind rules: INT128 still coerced to VARCHAR (no native PHP type yet).
        // DECFLOAT returns as native Firebird\DecFloat object (v13.0.0 breaking change).
        dpb.setBindRules("INT128 TO VARCHAR");
    }
#endif

#ifdef isc_dpb_parallel_workers
    if (params.parallel_workers > 0) {
        dpb.setParallelWorkers(params.parallel_workers);
    }
#endif

    // Create database string (must be null-terminated)
    std::string db_string(params.database);

    // Attach to database using OO API
    CheckStatusScope status(master);
    Firebird::IAttachment* raw_attachment = provider->attachDatabase(
        status.get(),
        db_string.c_str(),
        dpb.getBufferLength(),
        dpb.getBuffer()
    );

    // Note: Use hasError() instead of isDirty() for FB3 compatibility.
    // In FB3, isDirty() returns true even on success (it means "status was touched").
    // hasError() correctly checks for actual errors (STATE_ERRORS flag).
    if (status.hasError() || !raw_attachment) {
        throw Exception(status.status());
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

inline Connection Connection::createFromAttachment(
    Firebird::IMaster* master,
    Firebird::IAttachment* raw_attachment,
    std::string database_path,
    unsigned short dialect) {

    if (!master) {
        throw Exception("Firebird master interface is null");
    }

    if (!raw_attachment) {
        throw Exception("Attachment pointer is null");
    }

    // Detect client version
    unsigned client_version = 0;
#ifdef FB_API_VER
    client_version = FB_API_VER >= 50 ? VersionInfo::FB50 :
                    FB_API_VER >= 40 ? VersionInfo::FB40 :
                    VersionInfo::FB30;
#else
    client_version = VersionInfo::FB30;
#endif

    // Wrap in RAII pointer (takes ownership)
    AttachmentPtr attachment(raw_attachment);

    return Connection(
        std::move(attachment),
        master,
        std::move(database_path),
        dialect,
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
      database_path_(std::move(other.database_path_)),
      dialect_(other.dialect_),
      version_(other.version_),
      last_status_(std::move(other.last_status_)),
      statement_timeout_ms_(other.statement_timeout_ms_),
      idle_timeout_sec_(other.idle_timeout_sec_) {
    other.master_ = nullptr;
}

inline Connection& Connection::operator=(Connection&& other) noexcept {
    if (this != &other) {
        detachNoThrow();
        attachment_ = std::move(other.attachment_);
        master_ = other.master_;
        database_path_ = std::move(other.database_path_);
        dialect_ = other.dialect_;
        version_ = other.version_;
        last_status_ = std::move(other.last_status_);
        statement_timeout_ms_ = other.statement_timeout_ms_;
        idle_timeout_sec_ = other.idle_timeout_sec_;
        other.master_ = nullptr;
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

    CheckStatusScope status(master_);
    attachment_->detach(status.get());

    if (status.hasError()) {
        last_status_ = StatusWrapper(master_);
        throw Exception(status.status());
    }

    attachment_.reset();
}

inline bool Connection::pingAttachment(Firebird::IMaster* master) noexcept {
    if (!attachment_ || !master) {
        return false;
    }

    try {
        CheckStatusScope status(master);
        unsigned char info_request[] = { isc_info_ods_version };
        unsigned char info_buffer[32] = {0};
        attachment_->getInfo(status.get(),
                             sizeof(info_request), info_request,
                             sizeof(info_buffer), info_buffer);
        return !status.hasError();
    } catch (...) {
        return false;
    }
}

inline bool Connection::detachNoThrow() noexcept {
    if (!attachment_) {
        return true;
    }

    // If the database was dropped on this connection, skip detach() — attachment is already invalid.
    if (dropped_) {
        attachment_.reset();
        return true;
    }

    try {
        // Use getMaster() to get the current global master instead of the cached master_
        // pointer, which may be dangling if MSHUTDOWN already released the Firebird
        // client library (persistent connections destroyed after module shutdown).
        // getMaster() returns nullptr during MSHUTDOWN - in that case, skip the
        // Firebird API entirely. Calling pingAttachment()/detach() on a dead
        // attachment (dropped by cleanup_db() in RSHUTDOWN) SIGSEGVs with
        // opcache protect_memory=1 (issue #264). The Firebird client library's
        // atexit handler cleans up server-side state on process exit.
        // This matches the pattern already used in BlobWrapper::~BlobWrapper().
        Firebird::IMaster* master = getMaster();
        if (!master) {
            attachment_.reset();
            return true;
        }

        // Liveness check: if the DB was dropped by another connection
        // (e.g., test harness cleanup_db()), the server-side attachment
        // is dead. Calling detach() on it SIGSEGVs on FB 3.0 (issue #260).
        // The dropped_ flag only covers same-connection dropDatabase().
        if (!pingAttachment(master)) {
            attachment_.reset();
            return true;
        }

        // Attachment is alive - safe to detach
        CheckStatusScope status(master);
        attachment_->detach(status.get());
        if (status.hasError()) {
            attachment_.reset();
            return false;
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

    CheckStatusScope status(master_);
    attachment_->dropDatabase(status.get());

    if (status.hasError()) {
        throw Exception(status.status());
    }

    // After drop, the attachment is invalid — mark dropped so detachNoThrow() skips detach()
    dropped_ = true;
    attachment_.reset();
}

inline void Connection::copyLastStatus(ISC_STATUS* dest, std::size_t dest_size) const noexcept {
    last_status_.copyTo(dest, dest_size);
}

// FB 4.0+ Timeout implementation
#ifdef FB_API_VER
#if FB_API_VER >= 40
inline bool Connection::setStatementTimeout(unsigned int milliseconds) noexcept {
    if (!attachment_ || !master_ || !version_.hasTimeouts()) {
        return false;
    }

    try {
        CheckStatusScope status(master_);
        attachment_->setStatementTimeout(status.get(), milliseconds);
        if (!status.hasError()) {
            statement_timeout_ms_ = milliseconds;
            return true;
        }
        /* Save error status for C wrapper to extract via copyLastStatus() */
        last_status_ = StatusWrapper(master_);
        last_status_.get()->setErrors(status.status()->getErrors());
    } catch (...) {
        // Silently fail
    }
    return false;
}

inline unsigned int Connection::getStatementTimeout() const noexcept {
    return statement_timeout_ms_;
}

inline bool Connection::setIdleTimeout(unsigned int seconds) noexcept {
    if (!attachment_ || !master_ || !version_.hasTimeouts()) {
        return false;
    }

    try {
        CheckStatusScope status(master_);
        attachment_->setIdleTimeout(status.get(), seconds);
        if (!status.hasError()) {
            idle_timeout_sec_ = seconds;
            return true;
        }
        /* Save error status for C wrapper to extract via copyLastStatus() */
        last_status_ = StatusWrapper(master_);
        last_status_.get()->setErrors(status.status()->getErrors());
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

// C interop function declarations (implementations in firebird_utils.cpp)
// Note: These are declared extern "C" in firebird_utils.h

#endif // FB_CONNECTION_HPP
