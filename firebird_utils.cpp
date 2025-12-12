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
        (void)e; // suppress unused variable warning
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
