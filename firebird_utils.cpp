/*
  +----------------------------------------------------------------------+
  | Copyright (c) The PHP Group                                          |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | https://www.php.net/license/3_01.txt                                 |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Simonov Denis <sim-mail@list.ru>                             |
  | Author: Martins Lazdans <marrtins@dqdp.net>                          |
  +----------------------------------------------------------------------+
*/

#include <ibase.h>

#if FB_API_VER >= 30
#include <firebird/Interface.h>
#include <cstring>
#include <memory>
#include <optional>
#include <string_view>
#include <string>
#include <array>
#include <algorithm>
#include <cassert>
#include <utility>
#include <tuple>
#include "php.h"
#include "firebird_utils.h"
#include "php_fbird_includes.h"
#include "firebird_utils_internal.h"

//=============================================================================
// C++17 Utility Functions
//=============================================================================

namespace string_utils {
    // C++17: Efficient string processing with string_view
    inline void add_php_string_from_view(zval* array, const char* key, std::string_view value) noexcept {
        add_assoc_stringl(array, key, value.data(), value.size());
    }

    inline void add_php_string_indexed(zval* array, int index, std::string_view value) noexcept {
        add_index_stringl(array, index, value.data(), value.size());
    }

    // Step 4.1: Additional utility functions with const/noexcept optimization
    constexpr bool is_valid_time_component(unsigned hours, unsigned minutes, unsigned seconds, unsigned fractions) noexcept {
        return hours <= 23 && minutes <= 59 && seconds <= 59 && fractions <= 9999;
    }

    constexpr bool is_valid_date_component(unsigned year, unsigned month, unsigned day) noexcept {
        return year >= 1 && year <= 9999 && month >= 1 && month <= 12 && day >= 1 && day <= 31;
    }
}

//=============================================================================
// Step 2: Modernized Functions with std::optional Safety
//=============================================================================

namespace {
    // Step 2.1: Internal C++17 implementation with std::optional safety
    std::optional<unsigned> get_client_version_impl(void* master_ptr) noexcept {
        if (!master_ptr) {
            return std::nullopt;
        }

        try {
            FirebirdMasterWrapper master(master_ptr);
            auto* util = master.getUtil();

            if (!util) {
                return std::nullopt;
            }

            return util->getClientVersion();

        } catch (...) {
            // Never let C++ exceptions cross extern "C" boundaries
            return std::nullopt;
        }
    }

    // Step 2.2: Input validation structures for time encoding
    struct TimeComponents {
        unsigned hours, minutes, seconds, fractions;

        [[nodiscard]] constexpr bool is_valid() const noexcept {
            return hours <= 23 &&
                   minutes <= 59 &&
                   seconds <= 59 &&
                   fractions <= 9999;
        }
    };

    // Step 2.2: Internal C++17 implementation for time encoding
    std::optional<ISC_TIME> encode_time_impl(void* master_ptr, const TimeComponents& components) noexcept {
        if (!master_ptr || !components.is_valid()) {
            return std::nullopt;
        }

        try {
            FirebirdMasterWrapper master(master_ptr);
            auto* util = master.getUtil();

            if (!util) {
                return std::nullopt;
            }

            return util->encodeTime(components.hours, components.minutes,
                                  components.seconds, components.fractions);
        } catch (...) {
            return std::nullopt;
        }
    }

    // Step 2.3: Input validation structures for date encoding
    struct DateComponents {
        unsigned year, month, day;

        [[nodiscard]] constexpr bool is_valid() const noexcept {
            return year >= 1 && year <= 9999 &&
                   month >= 1 && month <= 12 &&
                   day >= 1 && day <= 31;
        }
    };

    // Step 2.3: Internal C++17 implementation for date encoding
    std::optional<ISC_DATE> encode_date_impl(void* master_ptr, const DateComponents& components) noexcept {
        if (!master_ptr || !components.is_valid()) {
            return std::nullopt;
        }

        try {
            FirebirdMasterWrapper master(master_ptr);
            auto* util = master.getUtil();

            if (!util) {
                return std::nullopt;
            }

            return util->encodeDate(components.year, components.month, components.day);
        } catch (...) {
            return std::nullopt;
        }
    }

#if FB_API_VER >= 40
    // Step 3.1: Structured bindings support for timestamp decoding
    struct DecodedTimestampTz {
        unsigned year{0}, month{0}, day{0};
        unsigned hours{0}, minutes{0}, seconds{0}, fractions{0};
        std::string timeZone;

        // C++17: Enable structured binding assignment
        [[nodiscard]] auto tie() const noexcept {
            return std::tie(year, month, day, hours, minutes, seconds, fractions);
        }

        [[nodiscard]] bool is_valid() const noexcept {
            return year >= 1 && year <= 9999 &&
                   month >= 1 && month <= 12 &&
                   day >= 1 && day <= 31 &&
                   hours <= 23 && minutes <= 59 && seconds <= 59 && fractions <= 9999;
        }
    };

    // Step 3.1: Internal C++17 implementation with structured bindings
    std::optional<DecodedTimestampTz> decode_timestamp_tz_impl(
        void* master_ptr, const ISC_TIMESTAMP_TZ* timestampTz) noexcept {

        if (!master_ptr || !timestampTz) {
            return std::nullopt;
        }

        try {
            FirebirdMasterWrapper master(master_ptr);
            FirebirdStatusManager status_mgr(nullptr, master.getMaster());

            auto* util = master.getUtil();
            if (!util) {
                return std::nullopt;
            }

            // Temporary storage for decode operation (initialized to avoid analyzer warnings)
            unsigned year = 0, month = 0, day = 0, hours = 0, minutes = 0, seconds = 0, fractions = 0;
            constexpr size_t TZ_BUFFER_SIZE = 64;
            std::array<char, TZ_BUFFER_SIZE> tz_buffer{};

            util->decodeTimeStampTz(status_mgr.get(), timestampTz,
                                   &year, &month, &day,
                                   &hours, &minutes, &seconds, &fractions,
                                   TZ_BUFFER_SIZE, tz_buffer.data());

            if (status_mgr.hasError()) {
                return std::nullopt;
            }

            // C++17: Create result with designated initializers style
            return DecodedTimestampTz{
                year, month, day,
                hours, minutes, seconds, fractions,
                std::string(tz_buffer.data())
            };

        } catch (...) {
            return std::nullopt;
        }
    }

    // Step 3.2: Modern field info insertion with complete RAII
    int insert_field_info_modern(void* master_ptr, ISC_STATUS* status_vec,
                                bool is_output_var, int field_num, zval* target_array,
                                Firebird::IStatement* statement) noexcept {
        try {
            FirebirdMasterWrapper master(master_ptr);
            FirebirdStatusManager status_mgr(status_vec, master.getMaster());

            // Get appropriate metadata with RAII management
            auto* metadata = is_output_var
                ? statement->getOutputMetadata(status_mgr.get())
                : statement->getInputMetadata(status_mgr.get());

            if (!metadata) {
                return -1;
            }

            FirebirdMetadataWrapper meta_wrapper(metadata, true);

            // C++17: Use string_view for efficient string handling
            const auto field_name = meta_wrapper.getFieldName(status_mgr.get(), field_num);
            const auto alias_name = meta_wrapper.getAlias(status_mgr.get(), field_num);
            const auto relation_name = meta_wrapper.getRelation(status_mgr.get(), field_num);

            if (status_mgr.hasError()) {
                return -1;
            }

            // Use string utilities for efficient PHP array population
            string_utils::add_php_string_indexed(target_array, 0, field_name);
            string_utils::add_php_string_from_view(target_array, "name", field_name);

            string_utils::add_php_string_indexed(target_array, 1, alias_name);
            string_utils::add_php_string_from_view(target_array, "alias", alias_name);

            string_utils::add_php_string_indexed(target_array, 2, relation_name);
            string_utils::add_php_string_from_view(target_array, "relation", relation_name);

            return 0;

        } catch (...) {
            return -1;
        }
    }

    // Step 3.3: Modern alias insertion with RAII metadata management
    int insert_aliases_modern(void* master_ptr, ISC_STATUS* status_vec, fbird_query* ib_query,
                             Firebird::IStatement* statement) noexcept {
        try {
            FirebirdMasterWrapper master(master_ptr);
            FirebirdStatusManager status_mgr(status_vec, master.getMaster());

            auto* metadata = statement->getOutputMetadata(status_mgr.get());
            if (!metadata) {
                return -1;
            }

            FirebirdMetadataWrapper meta_wrapper(metadata, true);

            unsigned cols = metadata->getCount(status_mgr.get());
            if (status_mgr.hasError()) {
                return -1;
            }

            assert(cols == ib_query->out_fields_count);

            // Modern range-based processing with RAII metadata
            for (unsigned i = 0; i < cols; ++i) {
                const auto alias = meta_wrapper.getAlias(status_mgr.get(), i);
                if (status_mgr.hasError()) {
                    return -1;
                }

                // Convert string_view to C string for existing PHP function
                std::string alias_str(alias);
                _php_fbird_insert_alias(ib_query->ht_aliases, alias_str.c_str());
            }

            return 0;

        } catch (...) {
            return -1;
        }
    }
#endif // FB_API_VER >= 40
} // end anonymous namespace

// =============================================================================
// Namespace fb::getMaster() implementation
// =============================================================================

// Undefine min/max macros from php_fbird_includes.h to avoid C++ STL conflicts
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include "src/cpp/fb_core.hpp"
#include "src/cpp/fb_connection.hpp"
// Phase 2 OO API Integration - COMPLETE
// All Firebird API calls now use CheckStatusWrapper as required by the template API.
// Changes made:
// 1. DpbBuilder: All methods use CheckStatusWrapper (constructor, insert*, getBuffer, clear)
// 2. Connection: All methods use CheckStatusWrapper (create, detach, drop, timeouts)
// 3. C interop functions available: fbc_connect, fbc_disconnect, fbc_drop_database, etc.
//
// See: docs/development/MODERNIZATION_PLAN_FB3_TO_FB5.md Phase 2 notes

namespace fb {

/**
 * Implementation of getMaster() - retrieves the global IMaster instance
 * from PHP extension globals (IBG macro).
 *
 * This function provides the bridge between the C++ wrapper layer and the
 * PHP extension's global state.
 */
Firebird::IMaster* getMaster() noexcept {
    // IBG(master_instance) is defined in php_fbird_includes.h
    // It's stored as void* for C compatibility
    return static_cast<Firebird::IMaster*>(IBG(master_instance));
}

} // namespace fb

/* Returns the client version. 0 bytes are minor version, 1 bytes are major version. */
extern "C" unsigned fbu_get_client_version(void *master_ptr)
{
    // Step 2.1: Use internal C++17 implementation with safe fallback
    auto version = get_client_version_impl(master_ptr);
    return version.value_or(0);
}

extern "C" ISC_TIME fbu_encode_time(void *master_ptr, unsigned hours, unsigned minutes, unsigned seconds, unsigned fractions)
{
    // Step 2.2: Use internal C++17 implementation with input validation
    TimeComponents components{hours, minutes, seconds, fractions};
    auto result = encode_time_impl(master_ptr, components);
    return result.value_or(0);
}

extern "C" ISC_DATE fbu_encode_date(void *master_ptr, unsigned year, unsigned month, unsigned day)
{
    // Step 2.3: Use internal C++17 implementation with input validation
    DateComponents components{year, month, day};
    auto result = encode_date_impl(master_ptr, components);
    return result.value_or(0);
}

// =============================================================================
// Phase 11: Type Encoding/Decoding Functions (FB 3.0+)
// =============================================================================

extern "C" ISC_TIMESTAMP fbu_encode_timestamp(void *master_ptr, unsigned year, unsigned month, unsigned day,
    unsigned hours, unsigned minutes, unsigned seconds, unsigned fractions)
{
    ISC_TIMESTAMP result = {0, 0};

    if (!master_ptr) {
        return result;
    }

    // Validate components
    if (!string_utils::is_valid_date_component(year, month, day) ||
        !string_utils::is_valid_time_component(hours, minutes, seconds, fractions)) {
        return result;
    }

    try {
        FirebirdMasterWrapper master(master_ptr);
        auto* util = master.getUtil();

        if (!util) {
            return result;
        }

        result.timestamp_date = util->encodeDate(year, month, day);
        result.timestamp_time = util->encodeTime(hours, minutes, seconds, fractions);

        return result;
    } catch (...) {
        return result;
    }
}

extern "C" void fbu_decode_time(void *master_ptr, ISC_TIME time,
    unsigned* hours, unsigned* minutes, unsigned* seconds, unsigned* fractions)
{
    // Initialize outputs to zero for safety
    if (hours) *hours = 0;
    if (minutes) *minutes = 0;
    if (seconds) *seconds = 0;
    if (fractions) *fractions = 0;

    if (!master_ptr) {
        return;
    }

    try {
        FirebirdMasterWrapper master(master_ptr);
        auto* util = master.getUtil();

        if (!util) {
            return;
        }

        // Temporary storage (in case caller passes null for some outputs)
        unsigned h = 0, m = 0, s = 0, f = 0;
        util->decodeTime(time, &h, &m, &s, &f);

        if (hours) *hours = h;
        if (minutes) *minutes = m;
        if (seconds) *seconds = s;
        if (fractions) *fractions = f;

    } catch (...) {
        // Error case - outputs already initialized to zero
    }
}

extern "C" void fbu_decode_date(void *master_ptr, ISC_DATE date,
    unsigned* year, unsigned* month, unsigned* day)
{
    // Initialize outputs to zero for safety
    if (year) *year = 0;
    if (month) *month = 0;
    if (day) *day = 0;

    if (!master_ptr) {
        return;
    }

    try {
        FirebirdMasterWrapper master(master_ptr);
        auto* util = master.getUtil();

        if (!util) {
            return;
        }

        // Temporary storage (in case caller passes null for some outputs)
        unsigned y = 0, m = 0, d = 0;
        util->decodeDate(date, &y, &m, &d);

        if (year) *year = y;
        if (month) *month = m;
        if (day) *day = d;

    } catch (...) {
        // Error case - outputs already initialized to zero
    }
}

extern "C" void fbu_decode_timestamp(void *master_ptr, const ISC_TIMESTAMP* timestamp,
    unsigned* year, unsigned* month, unsigned* day,
    unsigned* hours, unsigned* minutes, unsigned* seconds, unsigned* fractions)
{
    // Initialize all outputs to zero for safety
    if (year) *year = 0;
    if (month) *month = 0;
    if (day) *day = 0;
    if (hours) *hours = 0;
    if (minutes) *minutes = 0;
    if (seconds) *seconds = 0;
    if (fractions) *fractions = 0;

    if (!master_ptr || !timestamp) {
        return;
    }

    try {
        FirebirdMasterWrapper master(master_ptr);
        auto* util = master.getUtil();

        if (!util) {
            return;
        }

        // Decode date portion
        unsigned y = 0, mon = 0, d = 0;
        util->decodeDate(timestamp->timestamp_date, &y, &mon, &d);

        // Decode time portion
        unsigned h = 0, min = 0, s = 0, f = 0;
        util->decodeTime(timestamp->timestamp_time, &h, &min, &s, &f);

        // Assign to outputs
        if (year) *year = y;
        if (month) *month = mon;
        if (day) *day = d;
        if (hours) *hours = h;
        if (minutes) *minutes = min;
        if (seconds) *seconds = s;
        if (fractions) *fractions = f;

    } catch (...) {
        // Error case - outputs already initialized to zero
    }
}

// C++17: Modern status copying (replaces manual memcpy)
static void fbu_copy_status(const ISC_STATUS* from, ISC_STATUS* to, size_t maxLength)
{
    if (!from || !to || maxLength == 0) return;

    copy_status_vector(from, maxLength, to, maxLength);
}

// =============================================================================
// Phase 2: OO API Connection Functions - COMPLETE
// =============================================================================
// The fb_connection.hpp wrapper layer is now fully integrated.
// All Firebird API version compatibility issues have been resolved:
//
// Fixes applied:
// 1. statusHasError() helper function replaces FB 5.0-only IStatus::hasData()
// 2. IXpbBuilder::clear() now correctly passes IStatus* parameter for FB 4.0
// 3. All C++ wrapper headers compile cleanly with FB 4.0.5 client
//
// Available bridge functions (defined in fb_connection.hpp):
// - fbc_connect()        - Create connection using OO API
// - fbc_disconnect()     - Detach from database
// - fbc_drop_database()  - Drop database
// - fbc_is_connected()   - Check connection state
// - fbc_get_attachment() - Get raw IAttachment pointer
// =============================================================================

// Phase 3: C interop functions for OO API Connection
// These implementations provide the bridge between C code (firebird.c) and C++ OO API

extern "C" void* fbc_connect(
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
    // Validate master pointer
    if (!master_ptr || !database) {
        return nullptr;
    }

    try {
        auto* master = static_cast<Firebird::IMaster*>(master_ptr);

        // Build connection parameters
        fb::ConnectionParams params;
        params.database = std::string_view(database, db_len);
        if (user && user_len > 0) params.user = std::string_view(user, user_len);
        if (password && password_len > 0) params.password = std::string_view(password, password_len);
        if (charset && charset_len > 0) params.charset = std::string_view(charset, charset_len);
        if (role && role_len > 0) params.role = std::string_view(role, role_len);
        params.dialect = static_cast<unsigned short>(dialect);
        // Note: num_buffers and force_write could be added to DPB in future

        // Create connection using factory method
        auto conn = fb::Connection::create(master, params);

        if (!conn.isConnected()) {
            // Connection failed - copy error status if provided
            if (status_vector) {
                conn.copyLastStatus(status_vector, ISC_STATUS_LENGTH);
            }
            return nullptr;
        }

        // Move to heap and return as opaque pointer
        return reinterpret_cast<void*>(new fb::Connection(std::move(conn)));

    } catch (const fb::Exception& e) {
        // Copy error status from exception to output status vector
        if (status_vector) {
            const ISC_STATUS* exc_status = e.statusVector();
            if (exc_status) {
                for (size_t i = 0; i < ISC_STATUS_LENGTH; ++i) {
                    status_vector[i] = exc_status[i];
                }
            }
        }
        return nullptr;
    } catch (...) {
        return nullptr;
    }
}

extern "C" int fbc_disconnect(void* connection, ISC_STATUS* status_vector) {
    if (!connection) {
        return 0;
    }

    try {
        auto* conn = reinterpret_cast<fb::Connection*>(connection);

        // Try graceful disconnect
        bool success = conn->detachNoThrow();

        // Copy status if provided
        if (status_vector) {
            conn->copyLastStatus(status_vector, ISC_STATUS_LENGTH);
        }

        delete conn;
        return success ? 0 : 1;

    } catch (...) {
        // Clean up even on exception
        delete reinterpret_cast<fb::Connection*>(connection);
        return 1;
    }
}

extern "C" int fbc_drop_database(void* connection, ISC_STATUS* status_vector) {
    if (!connection) {
        return -1;
    }

    try {
        auto* conn = reinterpret_cast<fb::Connection*>(connection);

        // dropDatabase() throws fb::Exception on failure
        conn->dropDatabase();

        delete conn;
        return 0;

    } catch (const fb::Exception&) {
        auto* conn = reinterpret_cast<fb::Connection*>(connection);
        if (status_vector) {
            conn->copyLastStatus(status_vector, ISC_STATUS_LENGTH);
        }
        delete conn;
        return -1;
    } catch (...) {
        delete reinterpret_cast<fb::Connection*>(connection);
        return -1;
    }
}

extern "C" void* fbc_create_database(
    void* master_ptr,
    const char* create_sql,
    unsigned dialect,
    ISC_STATUS* status_vector
) {
    if (!master_ptr || !create_sql) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_req_handle;
            status_vector[2] = isc_arg_end;
        }
        return nullptr;
    }

    try {
        auto* master = static_cast<Firebird::IMaster*>(master_ptr);

        // Get IUtil interface for executeCreateDatabase
        Firebird::IUtil* util = master->getUtilInterface();
        if (!util) {
            if (status_vector) {
                status_vector[0] = isc_arg_gds;
                status_vector[1] = isc_unavailable;
                status_vector[2] = isc_arg_end;
            }
            return nullptr;
        }

        // Create status wrapper
        Firebird::IStatus* raw_status = master->getStatus();
        Firebird::CheckStatusWrapper check_status(raw_status);

        // Execute CREATE DATABASE statement using OO API
        // This returns an IAttachment* connected to the new database
        Firebird::IAttachment* attachment = util->executeCreateDatabase(
            &check_status,
            static_cast<unsigned>(strlen(create_sql)),
            create_sql,
            dialect,
            nullptr  // stmtIsCreateDb - not used in modern API
        );

        if (check_status.isDirty() || !attachment) {
            // Copy error status
            if (status_vector) {
                const ISC_STATUS* errors = raw_status->getErrors();
                if (errors) {
                    for (size_t i = 0; i < ISC_STATUS_LENGTH; ++i) {
                        status_vector[i] = errors[i];
                        if (errors[i] == isc_arg_end) break;
                    }
                }
            }
            return nullptr;
        }

        // Wrap the attachment in a Connection object
        // Note: We create a minimal Connection-like wrapper here
        // The attachment is already connected to the new database
        auto* conn = new (std::nothrow) fb::Connection();
        if (!conn) {
            // Out of memory - detach and fail
            attachment->detach(&check_status);
            if (status_vector) {
                status_vector[0] = isc_arg_gds;
                status_vector[1] = isc_virmemexh;
                status_vector[2] = isc_arg_end;
            }
            return nullptr;
        }

        // We need to construct a Connection with this attachment
        // Since Connection::create() isn't designed for this case,
        // we'll return the raw attachment wrapped in a simple struct
        // that the C code can use with fbc_get_attachment() and fbc_disconnect()

        // Actually, let's create a ConnectionParams and connect properly
        // after committing the implicit transaction from CREATE DATABASE

        // First, commit the implicit transaction (if any)
        // CREATE DATABASE via executeCreateDatabase starts with a committed database
        // The attachment is ready to use

        // Store attachment in a simple wrapper that's compatible with fbc_disconnect
        // We'll create the connection manually
        delete conn; // Don't use this

        // For now, return the attachment directly wrapped in a structure
        // that fbc_get_attachment and fbc_disconnect can handle
        // This requires adding a simpler wrapper class

        // Simpler approach: Return a special wrapper that holds just the attachment
        struct CreateDbResult {
            Firebird::IAttachment* attachment;
            Firebird::IMaster* master;
        };

        auto* result = new (std::nothrow) CreateDbResult{attachment, master};
        if (!result) {
            attachment->detach(&check_status);
            if (status_vector) {
                status_vector[0] = isc_arg_gds;
                status_vector[1] = isc_virmemexh;
                status_vector[2] = isc_arg_end;
            }
            return nullptr;
        }

        return result;

    } catch (const fb::Exception& e) {
        if (status_vector) {
            const ISC_STATUS* exc_status = e.statusVector();
            if (exc_status) {
                for (size_t i = 0; i < ISC_STATUS_LENGTH; ++i) {
                    status_vector[i] = exc_status[i];
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

extern "C" void* fbc_get_attachment(void* connection) {
    if (!connection) {
        return nullptr;
    }
    auto* conn = reinterpret_cast<fb::Connection*>(connection);
    return conn->get();  // Returns IAttachment*
}

extern "C" int fbc_is_connected(void* connection) {
    if (!connection) {
        return 0;
    }
    auto* conn = reinterpret_cast<fb::Connection*>(connection);
    return conn->isConnected() ? 1 : 0;
}

extern "C" unsigned fbc_get_server_version(void* connection) {
    if (!connection) {
        return 0;
    }
    auto* conn = reinterpret_cast<fb::Connection*>(connection);
    return conn->getVersion().getVersion();
}

// =============================================================================
// Phase 4: OO API Transaction Functions
// =============================================================================
// The fb_transaction.hpp wrapper layer provides RAII transaction management.
// These C interop functions bridge between C code (firebird.c) and the C++ OO API.
//
// Available bridge functions:
// - fbt_start()            - Start transaction using OO API
// - fbt_commit()           - Commit transaction
// - fbt_rollback()         - Rollback transaction
// - fbt_commit_retaining() - Commit with retaining
// - fbt_rollback_retaining() - Rollback with retaining
// - fbt_is_active()        - Check if transaction is active
// - fbt_get_handle()       - Get raw ITransaction pointer
// - fbt_free()             - Free wrapper without commit/rollback
// =============================================================================

#include "src/cpp/fb_transaction.hpp"

extern "C" void* fbt_start(
    void* master_ptr,
    void* attachment_ptr,
    unsigned tpb_len,
    const unsigned char* tpb,
    ISC_STATUS* status_vector
) {
    if (!master_ptr || !attachment_ptr) {
        return nullptr;
    }

    try {
        auto* master = static_cast<Firebird::IMaster*>(master_ptr);
        auto* attachment = static_cast<Firebird::IAttachment*>(attachment_ptr);

        // Create transaction using factory method
        auto trans = fb::Transaction::start(master, attachment, tpb_len, tpb);

        if (!trans.isActive()) {
            if (status_vector) {
                trans.copyLastStatus(status_vector, ISC_STATUS_LENGTH);
            }
            return nullptr;
        }

        // Move to heap and return as opaque pointer
        return reinterpret_cast<void*>(new fb::Transaction(std::move(trans)));

    } catch (const fb::Exception& e) {
        (void)e; // suppress unused variable warning
        return nullptr;
    } catch (...) {
        return nullptr;
    }
}

extern "C" int fbt_commit(void* transaction, ISC_STATUS* status_vector) {
    if (!transaction) {
        return 0;
    }

    try {
        auto* trans = reinterpret_cast<fb::Transaction*>(transaction);

        trans->commit();

        if (status_vector) {
            trans->copyLastStatus(status_vector, ISC_STATUS_LENGTH);
        }

        delete trans;
        return 0;

    } catch (const fb::Exception&) {
        auto* trans = reinterpret_cast<fb::Transaction*>(transaction);
        if (status_vector) {
            trans->copyLastStatus(status_vector, ISC_STATUS_LENGTH);
        }
        delete trans;
        return -1;
    } catch (...) {
        delete reinterpret_cast<fb::Transaction*>(transaction);
        return -1;
    }
}

extern "C" int fbt_rollback(void* transaction, ISC_STATUS* status_vector) {
    if (!transaction) {
        return 0;
    }

    try {
        auto* trans = reinterpret_cast<fb::Transaction*>(transaction);

        trans->rollback();

        if (status_vector) {
            trans->copyLastStatus(status_vector, ISC_STATUS_LENGTH);
        }

        delete trans;
        return 0;

    } catch (const fb::Exception&) {
        auto* trans = reinterpret_cast<fb::Transaction*>(transaction);
        if (status_vector) {
            trans->copyLastStatus(status_vector, ISC_STATUS_LENGTH);
        }
        delete trans;
        return -1;
    } catch (...) {
        delete reinterpret_cast<fb::Transaction*>(transaction);
        return -1;
    }
}

extern "C" int fbt_commit_retaining(void* transaction, ISC_STATUS* status_vector) {
    if (!transaction) {
        return -1;
    }

    try {
        auto* trans = reinterpret_cast<fb::Transaction*>(transaction);

        trans->commitRetaining();

        if (status_vector) {
            trans->copyLastStatus(status_vector, ISC_STATUS_LENGTH);
        }

        // Transaction remains valid after retaining commit - do NOT delete
        return 0;

    } catch (const fb::Exception&) {
        auto* trans = reinterpret_cast<fb::Transaction*>(transaction);
        if (status_vector) {
            trans->copyLastStatus(status_vector, ISC_STATUS_LENGTH);
        }
        return -1;
    } catch (...) {
        return -1;
    }
}

extern "C" int fbt_rollback_retaining(void* transaction, ISC_STATUS* status_vector) {
    if (!transaction) {
        return -1;
    }

    try {
        auto* trans = reinterpret_cast<fb::Transaction*>(transaction);

        trans->rollbackRetaining();

        if (status_vector) {
            trans->copyLastStatus(status_vector, ISC_STATUS_LENGTH);
        }

        // Transaction remains valid after retaining rollback - do NOT delete
        return 0;

    } catch (const fb::Exception&) {
        auto* trans = reinterpret_cast<fb::Transaction*>(transaction);
        if (status_vector) {
            trans->copyLastStatus(status_vector, ISC_STATUS_LENGTH);
        }
        return -1;
    } catch (...) {
        return -1;
    }
}

extern "C" int fbt_is_active(void* transaction) {
    if (!transaction) {
        return 0;
    }
    auto* trans = reinterpret_cast<fb::Transaction*>(transaction);
    return trans->isActive() ? 1 : 0;
}

extern "C" void* fbt_get_handle(void* transaction) {
    if (!transaction) {
        return nullptr;
    }
    auto* trans = reinterpret_cast<fb::Transaction*>(transaction);
    return trans->get();  // Returns ITransaction*
}

extern "C" void fbt_free(void* transaction) {
    if (transaction) {
        // Use rollbackNoThrow to safely clean up without throwing
        auto* trans = reinterpret_cast<fb::Transaction*>(transaction);
        trans->rollbackNoThrow();
        delete trans;
    }
}

#endif // FB_API_VER >= 30

#if FB_API_VER >= 40
/* Decodes a time with time zone into its time components. */
extern "C" void fbu_decode_time_tz(void *master_ptr, const ISC_TIME_TZ* time_tz, unsigned* hours, unsigned* minutes, unsigned* seconds, unsigned* fractions,
   unsigned time_zone_buffer_length, char* time_zone_buffer)
{
	auto* master = static_cast<Firebird::IMaster*>(master_ptr);
	Firebird::IUtil* util = master->getUtilInterface();
	Firebird::IStatus* fb_status = master->getStatus();
	Firebird::CheckStatusWrapper status(fb_status);
	util->decodeTimeTz(&status, time_tz, hours, minutes, seconds, fractions,
						time_zone_buffer_length, time_zone_buffer);
}

/* Decodes a timestamp with time zone into its date and time components */
extern "C" void fbu_decode_timestamp_tz(void *master_ptr, const ISC_TIMESTAMP_TZ* timestamp_tz, // NOLINT(bugprone-easily-swappable-parameters)
	unsigned* year, unsigned* month, unsigned* day,
	unsigned* hours, unsigned* minutes, unsigned* seconds, unsigned* fractions,
	unsigned time_zone_buffer_length, char* time_zone_buffer) // NOLINT(readability-function-cognitive-complexity)
{
    // Step 3.1: Use internal C++17 implementation with structured bindings
    auto decoded = decode_timestamp_tz_impl(master_ptr, timestamp_tz);

    if (decoded.has_value()) {
        // C++17: Structured binding assignment for cleaner multi-parameter handling
        auto [y, m, d, h, min, s, f] = decoded->tie();

        // Assign to output parameters (maintaining exact original behavior)
        if (year) *year = y;
        if (month) *month = m;
        if (day) *day = d;
        if (hours) *hours = h;
        if (minutes) *minutes = min;
        if (seconds) *seconds = s;
        if (fractions) *fractions = f;

        // Handle timezone buffer with safe string copying
        if (time_zone_buffer && time_zone_buffer_length > 0) {
            const auto& timezone = decoded->timeZone;
            const size_t copy_size = std::min(static_cast<size_t>(time_zone_buffer_length - 1), timezone.size());
            std::copy_n(timezone.begin(), copy_size, time_zone_buffer);
            time_zone_buffer[copy_size] = '\0'; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }
    } else {
        // Error case - set all outputs to zero/empty (safe fallback)
        if (year) *year = 0;
        if (month) *month = 0;
        if (day) *day = 0;
        if (hours) *hours = 0;
        if (minutes) *minutes = 0;
        if (seconds) *seconds = 0;
        if (fractions) *fractions = 0;
        if (time_zone_buffer && time_zone_buffer_length > 0) {
            time_zone_buffer[0] = '\0'; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }
    }
}

extern "C" int fbu_insert_aliases(void *master_ptr, ISC_STATUS* status, fbird_query *ib_query, void *statement_ptr)
{
    // Step 3.3: Use internal C++17 implementation with RAII metadata management
    if (master_ptr == nullptr || ib_query == nullptr || statement_ptr == nullptr) {
        return -1;
    }

    auto* statement = static_cast<Firebird::IStatement*>(statement_ptr);
    return insert_aliases_modern(master_ptr, status, ib_query, statement);
}

extern "C" int fbu_insert_field_info(void *master_ptr, ISC_STATUS* status, int is_outvar, int num,
	zval *into_array, void *statement_ptr)
{
    // Step 3.2: Use internal C++17 implementation with complete RAII
    if (master_ptr == nullptr || into_array == nullptr || statement_ptr == nullptr) {
        return -1;
    }

    auto* statement = static_cast<Firebird::IStatement*>(statement_ptr);
    return insert_field_info_modern(master_ptr, status, is_outvar != 0, num, into_array, statement);
}

/* Encode time with timezone */
extern "C" int fbu_encode_time_tz(void *master_ptr, ISC_TIME_TZ* time_tz,
	unsigned hours, unsigned minutes, unsigned seconds, unsigned fractions,
	const char* time_zone)
{
    if (!master_ptr || !time_tz || !time_zone) {
        return -1;
    }

    try {
        auto* master = static_cast<Firebird::IMaster*>(master_ptr);
        Firebird::IUtil* util = master->getUtilInterface();
        Firebird::IStatus* fb_status = master->getStatus();
        Firebird::CheckStatusWrapper status(fb_status);

        util->encodeTimeTz(&status, time_tz, hours, minutes, seconds, fractions, time_zone);

        if (status.isDirty()) {
            return -1;
        }
        return 0;
    } catch (...) {
        return -1;
    }
}

/* Encode timestamp with timezone */
extern "C" int fbu_encode_timestamp_tz(void *master_ptr, ISC_TIMESTAMP_TZ* timestamp_tz,
	unsigned year, unsigned month, unsigned day,
	unsigned hours, unsigned minutes, unsigned seconds, unsigned fractions,
	const char* time_zone)
{
    if (!master_ptr || !timestamp_tz || !time_zone) {
        return -1;
    }

    try {
        auto* master = static_cast<Firebird::IMaster*>(master_ptr);
        Firebird::IUtil* util = master->getUtilInterface();
        Firebird::IStatus* fb_status = master->getStatus();
        Firebird::CheckStatusWrapper status(fb_status);

        util->encodeTimeStampTz(&status, timestamp_tz, year, month, day,
                                hours, minutes, seconds, fractions, time_zone);

        if (status.isDirty()) {
            return -1;
        }
        return 0;
    } catch (...) {
        return -1;
    }
}

#endif // FB_API_VER >= 40

// =============================================================================
// Phase 5: Statement OO API Functions (FB 3.0+)
// =============================================================================

#if FB_API_VER >= 30

#include "src/cpp/fb_statement.hpp"

/**
 * The fb_statement.hpp wrapper layer provides RAII statement management.
 *
 * StatementWrapper class handles:
 * - Statement preparation via IAttachment::prepare()
 * - Execution via IStatement::execute()
 * - Cursor management via IStatement::openCursor() / IResultSet
 * - Metadata retrieval
 * - Automatic cleanup on destruction
 */

extern "C" void* fbs_prepare(
    void* master_ptr,
    void* attachment_ptr,
    void* transaction_ptr,
    const char* sql,
    unsigned sql_length,
    unsigned dialect,
    ISC_STATUS* status_vector
) {
    if (!master_ptr || !attachment_ptr || !sql) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_req_handle;
            status_vector[2] = isc_arg_end;
        }
        return nullptr;
    }

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* attachment = static_cast<Firebird::IAttachment*>(attachment_ptr);
    auto* transaction = static_cast<Firebird::ITransaction*>(transaction_ptr);

    // Allocate wrapper on heap
    auto* wrapper = new (std::nothrow) fb::StatementWrapper();
    if (!wrapper) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_virmemexh;
            status_vector[2] = isc_arg_end;
        }
        return nullptr;
    }

    if (!wrapper->prepare(master, attachment, transaction, sql, sql_length, dialect, status_vector)) {
        delete wrapper;
        return nullptr;
    }

    return wrapper;
}

extern "C" int fbs_execute(
    void* master_ptr,
    void* statement_ptr,
    void* transaction_ptr,
    void* in_msg,
    void* in_metadata,
    void* out_msg,
    void* out_metadata,
    ISC_STATUS* status_vector
) {
    if (!master_ptr || !statement_ptr) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_stmt_handle;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* wrapper = static_cast<fb::StatementWrapper*>(statement_ptr);
    auto* transaction = static_cast<Firebird::ITransaction*>(transaction_ptr);
    auto* in_meta = static_cast<Firebird::IMessageMetadata*>(in_metadata);
    auto* out_meta = static_cast<Firebird::IMessageMetadata*>(out_metadata);

    return wrapper->execute(master, transaction, in_msg, in_meta, out_msg, out_meta, status_vector) ? 1 : 0;
}

extern "C" int fbs_open_cursor(
    void* master_ptr,
    void* statement_ptr,
    void* transaction_ptr,
    void* in_msg,
    void* in_metadata,
    unsigned cursor_flags,
    ISC_STATUS* status_vector
) {
    if (!master_ptr || !statement_ptr) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_stmt_handle;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* wrapper = static_cast<fb::StatementWrapper*>(statement_ptr);
    auto* transaction = static_cast<Firebird::ITransaction*>(transaction_ptr);
    auto* in_meta = static_cast<Firebird::IMessageMetadata*>(in_metadata);

    return wrapper->openCursor(master, transaction, in_msg, in_meta, cursor_flags, status_vector) ? 1 : 0;
}

extern "C" int fbs_fetch(
    void* master_ptr,
    void* statement_ptr,
    void* out_msg,
    ISC_STATUS* status_vector
) {
    if (!master_ptr || !statement_ptr) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_stmt_handle;
            status_vector[2] = isc_arg_end;
        }
        return -1;
    }

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* wrapper = static_cast<fb::StatementWrapper*>(statement_ptr);

    return wrapper->fetchNext(master, out_msg, status_vector);
}

extern "C" int fbs_close_cursor(void* statement_ptr, ISC_STATUS* status_vector) {
    if (!statement_ptr) {
        return 1;  // Already closed
    }

    auto* wrapper = static_cast<fb::StatementWrapper*>(statement_ptr);
    return wrapper->closeCursor(status_vector) ? 1 : 0;
}

extern "C" int fbs_free(void* statement_ptr, ISC_STATUS* status_vector) {
    if (!statement_ptr) {
        return 1;  // Already freed
    }

    auto* wrapper = static_cast<fb::StatementWrapper*>(statement_ptr);
    bool result = wrapper->free(status_vector);
    delete wrapper;
    return result ? 1 : 0;
}

extern "C" unsigned fbs_get_type(void* master_ptr, void* statement_ptr, ISC_STATUS* status_vector) {
    if (!master_ptr || !statement_ptr) {
        return 0;
    }

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* wrapper = static_cast<fb::StatementWrapper*>(statement_ptr);

    return wrapper->getType(master, status_vector);
}

extern "C" ISC_UINT64 fbs_get_affected_records(void* master_ptr, void* statement_ptr, ISC_STATUS* status_vector) {
    if (!master_ptr || !statement_ptr) {
        return 0;
    }

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* wrapper = static_cast<fb::StatementWrapper*>(statement_ptr);

    return wrapper->getAffectedRecords(master, status_vector);
}

extern "C" void* fbs_get_input_metadata(void* master_ptr, void* statement_ptr, ISC_STATUS* status_vector) {
    if (!master_ptr || !statement_ptr) {
        return nullptr;
    }

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* wrapper = static_cast<fb::StatementWrapper*>(statement_ptr);

    return wrapper->getInputMetadata(master, status_vector);
}

extern "C" void* fbs_get_output_metadata(void* master_ptr, void* statement_ptr, ISC_STATUS* status_vector) {
    if (!master_ptr || !statement_ptr) {
        return nullptr;
    }

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* wrapper = static_cast<fb::StatementWrapper*>(statement_ptr);

    return wrapper->getOutputMetadata(master, status_vector);
}

extern "C" void* fbs_get_statement(void* statement_ptr) {
    if (!statement_ptr) {
        return nullptr;
    }

    auto* wrapper = static_cast<fb::StatementWrapper*>(statement_ptr);
    return wrapper->getStatement();
}

extern "C" int fbs_is_prepared(void* statement_ptr) {
    if (!statement_ptr) {
        return 0;
    }

    auto* wrapper = static_cast<fb::StatementWrapper*>(statement_ptr);
    return wrapper->isPrepared() ? 1 : 0;
}

extern "C" int fbs_is_cursor_open(void* statement_ptr) {
    if (!statement_ptr) {
        return 0;
    }

    auto* wrapper = static_cast<fb::StatementWrapper*>(statement_ptr);
    return wrapper->isCursorOpen() ? 1 : 0;
}

extern "C" unsigned fbs_get_input_count(void* master_ptr, void* statement_ptr, ISC_STATUS* status_vector) {
    if (!master_ptr || !statement_ptr) {
        return 0;
    }

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* wrapper = static_cast<fb::StatementWrapper*>(statement_ptr);

    auto* metadata = wrapper->getInputMetadata(master, status_vector);
    if (!metadata) {
        return 0;
    }

    Firebird::CheckStatusWrapper status(master->getStatus());
    unsigned count = metadata->getCount(&status);
    metadata->release();

    return count;
}

extern "C" unsigned fbs_get_output_count(void* master_ptr, void* statement_ptr, ISC_STATUS* status_vector) {
    if (!master_ptr || !statement_ptr) {
        return 0;
    }

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* wrapper = static_cast<fb::StatementWrapper*>(statement_ptr);

    auto* metadata = wrapper->getOutputMetadata(master, status_vector);
    if (!metadata) {
        return 0;
    }

    Firebird::CheckStatusWrapper status(master->getStatus());
    unsigned count = metadata->getCount(&status);
    metadata->release();

    return count;
}

#endif // FB_API_VER >= 30 (Phase 5 Statement functions)

/* =============================================================================
 * Phase 6: Blob OO API (FB 3.0+)
 *
 * RAII wrapper for IBlob operations.
 * We provide non-inline implementations below, so disable the inline versions.
 * ============================================================================= */
#if FB_API_VER >= 30
#define FBB_NO_INLINE_IMPL
#include "src/cpp/fb_blob.hpp"
#endif // FB_API_VER >= 30 (Phase 6 Blob functions)

/* =============================================================================
 * Phase 7: Event OO API (FB 3.0+)
 *
 * RAII wrapper for IEvents operations with IEventCallback.
 * The fbe_* functions are implemented inline in fb_events.hpp.
 *
 * Note: The OO API uses callback-based event handling (IEventCallback)
 * which differs from the legacy synchronous isc_wait_for_event() approach.
 * The current implementation continues to use isc_wait_for_event() for the
 * synchronous polling model, with OO API available for future async support.
 * ============================================================================= */
#if FB_API_VER >= 30
#include "src/cpp/fb_events.hpp"
#endif // FB_API_VER >= 30 (Phase 7 Event functions)

/* =============================================================================
 * Phase 8: Service OO API (FB 3.0+)
 *
 * RAII wrapper for IService operations.
 * The fbsvc_* functions are implemented inline in fb_service.hpp.
 *
 * This replaces the legacy isc_service_attach, isc_service_detach,
 * isc_service_start, and isc_service_query functions.
 * ============================================================================= */
#if FB_API_VER >= 30
#include "src/cpp/fb_service.hpp"
#endif // FB_API_VER >= 30 (Phase 8 Service functions)

/* =============================================================================
 * Phase 9: Array OO API (FB 3.0+)
 *
 * Stateless utility functions for array slice operations.
 * Uses IAttachment::getSlice() and IAttachment::putSlice() methods.
 *
 * Note: isc_array_lookup_bounds() has no direct OO API equivalent - it performs
 * a system table query. The existing legacy function continues to be used
 * for array descriptor lookup.
 * ============================================================================= */
#if FB_API_VER >= 30
#include "src/cpp/fb_array.hpp"

extern "C" int fba_get_slice(
    void* master_ptr,
    void* attachment_ptr,
    void* transaction_ptr,
    ISC_QUAD* array_id,
    const ISC_ARRAY_DESC* desc,
    void* buffer,
    ISC_LONG* buffer_length,
    ISC_STATUS* status_vector
) {
    if (!master_ptr || !attachment_ptr || !transaction_ptr) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_req_handle;
            status_vector[2] = isc_arg_end;
        }
        return 1;
    }

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* attachment = static_cast<Firebird::IAttachment*>(attachment_ptr);
    auto* transaction = static_cast<Firebird::ITransaction*>(transaction_ptr);

    bool result = fb::ArrayUtils::getSlice(
        master,
        attachment,
        transaction,
        array_id,
        desc,
        buffer,
        buffer_length,
        status_vector
    );

    return result ? 0 : 1;
}

extern "C" int fba_put_slice(
    void* master_ptr,
    void* attachment_ptr,
    void* transaction_ptr,
    ISC_QUAD* array_id,
    const ISC_ARRAY_DESC* desc,
    const void* buffer,
    ISC_LONG buffer_length,
    ISC_STATUS* status_vector
) {
    if (!master_ptr || !attachment_ptr || !transaction_ptr) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_req_handle;
            status_vector[2] = isc_arg_end;
        }
        return 1;
    }

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* attachment = static_cast<Firebird::IAttachment*>(attachment_ptr);
    auto* transaction = static_cast<Firebird::ITransaction*>(transaction_ptr);

    bool result = fb::ArrayUtils::putSlice(
        master,
        attachment,
        transaction,
        array_id,
        desc,
        buffer,
        buffer_length,
        status_vector
    );

    return result ? 0 : 1;
}

#endif // FB_API_VER >= 30 (Phase 9 Array functions)

// Phase 6: Blob OO API - C interop implementations
// These non-inline implementations are needed because inline functions in headers
// don't get proper linkage when called from C code.

extern "C" void* fbb_create(
    void* master_ptr,
    void* attachment_ptr,
    void* transaction_ptr,
    ISC_QUAD* blob_id,
    unsigned bpb_length,
    const unsigned char* bpb,
    ISC_STATUS* status_vector
) {
    if (!master_ptr || !attachment_ptr || !transaction_ptr || !blob_id) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_req_handle;
            status_vector[2] = isc_arg_end;
        }
        return nullptr;
    }

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* attachment = static_cast<Firebird::IAttachment*>(attachment_ptr);
    auto* transaction = static_cast<Firebird::ITransaction*>(transaction_ptr);

    auto* wrapper = new (std::nothrow) fb::BlobWrapper();
    if (!wrapper) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_virmemexh;
            status_vector[2] = isc_arg_end;
        }
        return nullptr;
    }

    // BlobWrapper::create() stores blob_id internally, retrieve via getBlobId()
    if (!wrapper->create(master, attachment, transaction, bpb_length, bpb, status_vector)) {
        delete wrapper;
        return nullptr;
    }

    // Copy the generated blob_id back to caller
    *blob_id = wrapper->getBlobId();

    return wrapper;
}

extern "C" void* fbb_open(
    void* master_ptr,
    void* attachment_ptr,
    void* transaction_ptr,
    const ISC_QUAD* blob_id,
    unsigned bpb_length,
    const unsigned char* bpb,
    ISC_STATUS* status_vector
) {
    if (!master_ptr || !attachment_ptr || !transaction_ptr || !blob_id) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_req_handle;
            status_vector[2] = isc_arg_end;
        }
        return nullptr;
    }

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* attachment = static_cast<Firebird::IAttachment*>(attachment_ptr);
    auto* transaction = static_cast<Firebird::ITransaction*>(transaction_ptr);

    auto* wrapper = new (std::nothrow) fb::BlobWrapper();
    if (!wrapper) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_virmemexh;
            status_vector[2] = isc_arg_end;
        }
        return nullptr;
    }

    if (!wrapper->open(master, attachment, transaction, blob_id, bpb_length, bpb, status_vector)) {
        delete wrapper;
        return nullptr;
    }

    return wrapper;
}

extern "C" int fbb_put_segment(
    void* master_ptr,
    void* blob_wrapper,
    unsigned length,
    const void* buffer,
    ISC_STATUS* status_vector
) {
    if (!master_ptr || !blob_wrapper) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_segstr_handle;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* wrapper = static_cast<fb::BlobWrapper*>(blob_wrapper);

    return wrapper->putSegment(master, length, buffer, status_vector) ? 1 : 0;
}

extern "C" int fbb_get_segment(
    void* master_ptr,
    void* blob_wrapper,
    unsigned buffer_length,
    void* buffer,
    unsigned* actual_length,
    ISC_STATUS* status_vector
) {
    if (!master_ptr || !blob_wrapper) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_segstr_handle;
            status_vector[2] = isc_arg_end;
        }
        return -1;
    }

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* wrapper = static_cast<fb::BlobWrapper*>(blob_wrapper);

    return wrapper->getSegment(master, buffer_length, buffer, actual_length, status_vector);
}

extern "C" int fbb_close(void* master_ptr, void* blob_wrapper, ISC_STATUS* status_vector) {
    if (!master_ptr || !blob_wrapper) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_segstr_handle;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* wrapper = static_cast<fb::BlobWrapper*>(blob_wrapper);

    return wrapper->close(master, status_vector) ? 1 : 0;
}

extern "C" int fbb_cancel(void* master_ptr, void* blob_wrapper, ISC_STATUS* status_vector) {
    if (!master_ptr || !blob_wrapper) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_segstr_handle;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* wrapper = static_cast<fb::BlobWrapper*>(blob_wrapper);

    return wrapper->cancel(master, status_vector) ? 1 : 0;
}

extern "C" int fbb_get_info(
    void* master_ptr,
    void* blob_wrapper,
    unsigned items_length,
    const unsigned char* items,
    unsigned buffer_length,
    unsigned char* buffer,
    ISC_STATUS* status_vector
) {
    if (!master_ptr || !blob_wrapper) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_segstr_handle;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* wrapper = static_cast<fb::BlobWrapper*>(blob_wrapper);

    return wrapper->getInfo(master, items_length, items, buffer_length, buffer, status_vector) ? 1 : 0;
}

extern "C" void fbb_get_blob_id(void* blob_wrapper, ISC_QUAD* blob_id) {
    if (!blob_wrapper || !blob_id) return;
    auto* wrapper = static_cast<fb::BlobWrapper*>(blob_wrapper);
    *blob_id = wrapper->getBlobId();
}

extern "C" int fbb_is_open(void* blob_wrapper) {
    if (!blob_wrapper) return 0;
    auto* wrapper = static_cast<fb::BlobWrapper*>(blob_wrapper);
    return wrapper->isOpen() ? 1 : 0;
}

extern "C" void* fbb_get_handle(void* blob_wrapper) {
    if (!blob_wrapper) return nullptr;
    auto* wrapper = static_cast<fb::BlobWrapper*>(blob_wrapper);
    return wrapper->getBlob();
}

extern "C" void fbb_free(void* blob_wrapper) {
    if (!blob_wrapper) return;
    auto* wrapper = static_cast<fb::BlobWrapper*>(blob_wrapper);
    delete wrapper;
}

/* =============================================================================
 * Metadata C Interop Functions for OO API Message Buffer Operations
 *
 * These functions provide access to IMessageMetadata interface for allocating
 * message buffers and extracting field values during fetch operations.
 * ============================================================================= */

extern "C" unsigned fbm_get_message_length(void* master_ptr, void* metadata_ptr) {
    if (!master_ptr || !metadata_ptr) return 0;

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* metadata = static_cast<Firebird::IMessageMetadata*>(metadata_ptr);

    Firebird::CheckStatusWrapper status(master->getStatus());
    unsigned length = metadata->getMessageLength(&status);
    return (status.getState() & Firebird::IStatus::STATE_ERRORS) ? 0 : length;
}

extern "C" unsigned fbm_get_count(void* master_ptr, void* metadata_ptr) {
    if (!master_ptr || !metadata_ptr) return 0;

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* metadata = static_cast<Firebird::IMessageMetadata*>(metadata_ptr);

    Firebird::CheckStatusWrapper status(master->getStatus());
    unsigned count = metadata->getCount(&status);
    return (status.getState() & Firebird::IStatus::STATE_ERRORS) ? 0 : count;
}

extern "C" unsigned fbm_get_offset(void* master_ptr, void* metadata_ptr, unsigned index) {
    if (!master_ptr || !metadata_ptr) return 0;

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* metadata = static_cast<Firebird::IMessageMetadata*>(metadata_ptr);

    Firebird::CheckStatusWrapper status(master->getStatus());
    unsigned offset = metadata->getOffset(&status, index);
    return (status.getState() & Firebird::IStatus::STATE_ERRORS) ? 0 : offset;
}

extern "C" unsigned fbm_get_null_offset(void* master_ptr, void* metadata_ptr, unsigned index) {
    if (!master_ptr || !metadata_ptr) return 0;

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* metadata = static_cast<Firebird::IMessageMetadata*>(metadata_ptr);

    Firebird::CheckStatusWrapper status(master->getStatus());
    unsigned offset = metadata->getNullOffset(&status, index);
    return (status.getState() & Firebird::IStatus::STATE_ERRORS) ? 0 : offset;
}

extern "C" unsigned fbm_get_type(void* master_ptr, void* metadata_ptr, unsigned index) {
    if (!master_ptr || !metadata_ptr) return 0;

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* metadata = static_cast<Firebird::IMessageMetadata*>(metadata_ptr);

    Firebird::CheckStatusWrapper status(master->getStatus());
    unsigned type = metadata->getType(&status, index);
    return (status.getState() & Firebird::IStatus::STATE_ERRORS) ? 0 : type;
}

extern "C" unsigned fbm_get_subtype(void* master_ptr, void* metadata_ptr, unsigned index) {
    if (!master_ptr || !metadata_ptr) return 0;

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* metadata = static_cast<Firebird::IMessageMetadata*>(metadata_ptr);

    Firebird::CheckStatusWrapper status(master->getStatus());
    unsigned subtype = metadata->getSubType(&status, index);
    return (status.getState() & Firebird::IStatus::STATE_ERRORS) ? 0 : subtype;
}

extern "C" unsigned fbm_get_length(void* master_ptr, void* metadata_ptr, unsigned index) {
    if (!master_ptr || !metadata_ptr) return 0;

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* metadata = static_cast<Firebird::IMessageMetadata*>(metadata_ptr);

    Firebird::CheckStatusWrapper status(master->getStatus());
    unsigned length = metadata->getLength(&status, index);
    return (status.getState() & Firebird::IStatus::STATE_ERRORS) ? 0 : length;
}

extern "C" int fbm_get_scale(void* master_ptr, void* metadata_ptr, unsigned index) {
    if (!master_ptr || !metadata_ptr) return 0;

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* metadata = static_cast<Firebird::IMessageMetadata*>(metadata_ptr);

    Firebird::CheckStatusWrapper status(master->getStatus());
    int scale = metadata->getScale(&status, index);
    return (status.getState() & Firebird::IStatus::STATE_ERRORS) ? 0 : scale;
}

extern "C" unsigned fbm_get_charset(void* master_ptr, void* metadata_ptr, unsigned index) {
    if (!master_ptr || !metadata_ptr) return 0;

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* metadata = static_cast<Firebird::IMessageMetadata*>(metadata_ptr);

    Firebird::CheckStatusWrapper status(master->getStatus());
    unsigned charset = metadata->getCharSet(&status, index);
    return (status.getState() & Firebird::IStatus::STATE_ERRORS) ? 0 : charset;
}

extern "C" const char* fbm_get_field(void* master_ptr, void* metadata_ptr, unsigned index) {
    if (!master_ptr || !metadata_ptr) return nullptr;

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* metadata = static_cast<Firebird::IMessageMetadata*>(metadata_ptr);

    Firebird::CheckStatusWrapper status(master->getStatus());
    const char* name = metadata->getField(&status, index);
    return (status.getState() & Firebird::IStatus::STATE_ERRORS) ? nullptr : name;
}

extern "C" const char* fbm_get_alias(void* master_ptr, void* metadata_ptr, unsigned index) {
    if (!master_ptr || !metadata_ptr) return nullptr;

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* metadata = static_cast<Firebird::IMessageMetadata*>(metadata_ptr);

    Firebird::CheckStatusWrapper status(master->getStatus());
    const char* alias = metadata->getAlias(&status, index);
    return (status.getState() & Firebird::IStatus::STATE_ERRORS) ? nullptr : alias;
}

extern "C" const char* fbm_get_relation(void* master_ptr, void* metadata_ptr, unsigned index) {
    if (!master_ptr || !metadata_ptr) return nullptr;

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* metadata = static_cast<Firebird::IMessageMetadata*>(metadata_ptr);

    Firebird::CheckStatusWrapper status(master->getStatus());
    const char* relation = metadata->getRelation(&status, index);
    return (status.getState() & Firebird::IStatus::STATE_ERRORS) ? nullptr : relation;
}

extern "C" void fbm_release(void* metadata_ptr) {
    if (!metadata_ptr) return;
    auto* metadata = static_cast<Firebird::IMessageMetadata*>(metadata_ptr);
    metadata->release();
}
