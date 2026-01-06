/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

#include <ibase.h>

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

/* PHP headers must be wrapped in extern "C" when included from C++
 * to ensure proper symbol linkage (especially for TSRM in ZTS builds) */
extern "C" {
#include "php.h"
#include "php_fbird_includes.h"
}

#include "firebird_utils.h"
#include "firebird_utils_internal.h"

namespace string_utils {
    inline void add_php_string_from_view(zval* array, const char* key, std::string_view value) noexcept {
        add_assoc_stringl(array, key, value.data(), value.size());
    }

    inline void add_php_string_indexed(zval* array, int index, std::string_view value) noexcept {
        add_index_stringl(array, index, value.data(), value.size());
    }

    constexpr bool is_valid_time_component(unsigned hours, unsigned minutes, unsigned seconds, unsigned fractions) noexcept {
        return hours <= 23 && minutes <= 59 && seconds <= 59 && fractions <= 9999;
    }

    constexpr bool is_valid_date_component(unsigned year, unsigned month, unsigned day) noexcept {
        return year >= 1 && year <= 9999 && month >= 1 && month <= 12 && day >= 1 && day <= 31;
    }
}

namespace {
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
            return std::nullopt;
        }
    }

    struct TimeComponents {
        unsigned hours, minutes, seconds, fractions;

        [[nodiscard]] constexpr bool is_valid() const noexcept {
            return hours <= 23 &&
                   minutes <= 59 &&
                   seconds <= 59 &&
                   fractions <= 9999;
        }
    };

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

    struct DateComponents {
        unsigned year, month, day;

        [[nodiscard]] constexpr bool is_valid() const noexcept {
            return year >= 1 && year <= 9999 &&
                   month >= 1 && month <= 12 &&
                   day >= 1 && day <= 31;
        }
    };

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
    struct DecodedTimestampTz {
        unsigned year{0}, month{0}, day{0};
        unsigned hours{0}, minutes{0}, seconds{0}, fractions{0};
        std::string timeZone;

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

            return DecodedTimestampTz{
                year, month, day,
                hours, minutes, seconds, fractions,
                std::string(tz_buffer.data())
            };

        } catch (...) {
            return std::nullopt;
        }
    }

    int insert_field_info_modern(void* master_ptr, ISC_STATUS* status_vec,
                                bool is_output_var, int field_num, zval* target_array,
                                Firebird::IStatement* statement) noexcept {
        try {
            FirebirdMasterWrapper master(master_ptr);
            FirebirdStatusManager status_mgr(status_vec, master.getMaster());

            auto* metadata = is_output_var
                ? statement->getOutputMetadata(status_mgr.get())
                : statement->getInputMetadata(status_mgr.get());

            if (!metadata) {
                return -1;
            }

            FirebirdMetadataWrapper meta_wrapper(metadata, true);

            const auto field_name = meta_wrapper.getFieldName(status_mgr.get(), field_num);
            const auto alias_name = meta_wrapper.getAlias(status_mgr.get(), field_num);
            const auto relation_name = meta_wrapper.getRelation(status_mgr.get(), field_num);

            if (status_mgr.hasError()) {
                return -1;
            }

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

            for (unsigned i = 0; i < cols; ++i) {
                const auto alias = meta_wrapper.getAlias(status_mgr.get(), i);
                if (status_mgr.hasError()) {
                    return -1;
                }

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

// Undefine min/max macros from php_fbird_includes.h to avoid C++ STL conflicts
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include "src/cpp/fb_core.hpp"
#include "src/cpp/fb_connection.hpp"

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
    auto version = get_client_version_impl(master_ptr);
    return version.value_or(0);
}

extern "C" ISC_TIME fbu_encode_time(void *master_ptr, unsigned hours, unsigned minutes, unsigned seconds, unsigned fractions)
{
    TimeComponents components{hours, minutes, seconds, fractions};
    auto result = encode_time_impl(master_ptr, components);
    return result.value_or(0);
}

extern "C" ISC_DATE fbu_encode_date(void *master_ptr, unsigned year, unsigned month, unsigned day)
{
    DateComponents components{year, month, day};
    auto result = encode_date_impl(master_ptr, components);
    return result.value_or(0);
}

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

static void fbu_copy_status(const ISC_STATUS* from, ISC_STATUS* to, size_t maxLength)
{
    if (!from || !to || maxLength == 0) return;

    copy_status_vector(from, maxLength, to, maxLength);
}

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

        // Extract database path from CREATE DATABASE statement for Connection
        // The SQL contains the database path, e.g., "CREATE SCHEMA 'path' ..."
        std::string db_path;
        const char* quote_start = strchr(create_sql, '\'');
        if (quote_start) {
            const char* quote_end = strchr(quote_start + 1, '\'');
            if (quote_end) {
                db_path = std::string(quote_start + 1, quote_end - quote_start - 1);
            }
        }

        // Use the new factory method to create a proper Connection from the attachment
        // This transfers ownership of the attachment to the Connection object
        fb::Connection conn = fb::Connection::createFromAttachment(
            master,
            attachment,
            db_path,
            static_cast<unsigned short>(dialect)
        );

        // Move to heap and return as opaque pointer (compatible with fbc_get_attachment)
        return reinterpret_cast<void*>(new fb::Connection(std::move(conn)));

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
        // Copy error status from exception to output status vector
        if (status_vector) {
            const ISC_STATUS* exc_status = e.statusVector();
            if (exc_status) {
                for (size_t i = 0; i < ISC_STATUS_LENGTH; ++i) {
                    status_vector[i] = exc_status[i];
                }
            } else {
                // No status available - set generic error
                status_vector[0] = isc_arg_gds;
                status_vector[1] = isc_random;
                status_vector[2] = isc_arg_end;
            }
        }
        return nullptr;
    } catch (...) {
        // Unknown exception - set generic error
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_random;
            status_vector[2] = isc_arg_end;
        }
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

        /* Do NOT delete wrapper here - let PHP resource destructor call fbt_free().
         * The wrapper stays valid (but inactive) so any PHP code holding a reference
         * won't dereference freed memory. fbt_is_active() will return false.
         * This fixes Issue #9: segfault in transaction cleanup after DDL commit. */
        return 0;

    } catch (const fb::Exception&) {
        auto* trans = reinterpret_cast<fb::Transaction*>(transaction);
        if (status_vector) {
            trans->copyLastStatus(status_vector, ISC_STATUS_LENGTH);
        }
        /* On error, still don't delete - let PHP handle cleanup */
        return -1;
    } catch (...) {
        /* On unknown exception, still don't delete */
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

        /* Do NOT delete wrapper here - let PHP resource destructor call fbt_free().
         * Consistent with fbt_commit() behavior for Issue #9 fix. */
        return 0;

    } catch (const fb::Exception&) {
        auto* trans = reinterpret_cast<fb::Transaction*>(transaction);
        if (status_vector) {
            trans->copyLastStatus(status_vector, ISC_STATUS_LENGTH);
        }
        /* On error, still don't delete - let PHP handle cleanup */
        return -1;
    } catch (...) {
        /* On unknown exception, still don't delete */
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

extern "C" int fbt_get_info(
    void* master_ptr,
    void* transaction_ptr,
    unsigned items_length,
    const unsigned char* items,
    unsigned buffer_length,
    unsigned char* buffer,
    ISC_STATUS* status_vector
) {
    if (!master_ptr || !transaction_ptr || !items || !buffer) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_req_handle;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* transaction = static_cast<Firebird::ITransaction*>(transaction_ptr);

    try {
        Firebird::IStatus* fb_status = master->getStatus();
        Firebird::CheckStatusWrapper status(fb_status);

        transaction->getInfo(&status, items_length, items, buffer_length, buffer);

        if (fb::statusHasError(fb_status)) {
            if (status_vector) {
                copy_status_vector(fb_status->getErrors(), ISC_STATUS_LENGTH, status_vector, ISC_STATUS_LENGTH);
            }
            return 0;
        }

        if (status_vector) {
            status_vector[0] = 1;
            status_vector[1] = 0;
        }
        return 1;

    } catch (...) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_except2;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }
}

extern "C" int fbc_get_info(
    void* master_ptr,
    void* attachment_ptr,
    unsigned items_length,
    const unsigned char* items,
    unsigned buffer_length,
    unsigned char* buffer,
    ISC_STATUS* status_vector
) {
    if (!master_ptr || !attachment_ptr || !items || !buffer) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_db_handle;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* attachment = static_cast<Firebird::IAttachment*>(attachment_ptr);

    try {
        Firebird::IStatus* fb_status = master->getStatus();
        Firebird::CheckStatusWrapper status(fb_status);

        attachment->getInfo(&status, items_length, items, buffer_length, buffer);

        if (fb::statusHasError(fb_status)) {
            if (status_vector) {
                copy_status_vector(fb_status->getErrors(), ISC_STATUS_LENGTH, status_vector, ISC_STATUS_LENGTH);
            }
            return 0;
        }

        if (status_vector) {
            status_vector[0] = 1;
            status_vector[1] = 0;
        }
        return 1;

    } catch (...) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_except2;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }
}

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
    auto decoded = decode_timestamp_tz_impl(master_ptr, timestamp_tz);

    if (decoded.has_value()) {
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
    if (master_ptr == nullptr || ib_query == nullptr || statement_ptr == nullptr) {
        return -1;
    }

    auto* statement = static_cast<Firebird::IStatement*>(statement_ptr);
    return insert_aliases_modern(master_ptr, status, ib_query, statement);
}

extern "C" int fbu_insert_field_info(void *master_ptr, ISC_STATUS* status, int is_outvar, int num,
	zval *into_array, void *statement_ptr)
{
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

#include "src/cpp/fb_statement.hpp"

extern "C" void* fbs_prepare(
    void* master_ptr,
    void* attachment_ptr,
    void* transaction_ptr,
    const char* sql,
    unsigned sql_length,
    unsigned dialect,
    ISC_STATUS* status_vector
) {
    /* CRITICAL: Always initialize the caller-provided ISC_STATUS vector.
     * Some callers (and our C layer) treat a non-zero status[1] as an error.
     * If a previous call set an error, and a later call succeeds but doesn't
     * touch the status vector, stale errors can leak into subsequent operations.
     *
     * Fixes: tests/003.phpt (stale "validation error" after suppressed query)
     */
    if (status_vector) {
        status_vector[0] = 1;
        status_vector[1] = 0;
    }

    if (!master_ptr || !attachment_ptr || !transaction_ptr || !sql) {
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
    if (status_vector) {
        status_vector[0] = 1;
        status_vector[1] = 0;
    }

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
    if (status_vector) {
        status_vector[0] = 1;
        status_vector[1] = 0;
    }

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
    if (status_vector) {
        status_vector[0] = 1;
        status_vector[1] = 0;
    }

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
    if (status_vector) {
        status_vector[0] = 1;
        status_vector[1] = 0;
    }
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
    if (status_vector) {
        status_vector[0] = 1;
        status_vector[1] = 0;
    }
    if (!master_ptr || !statement_ptr) {
        return nullptr;
    }

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* wrapper = static_cast<fb::StatementWrapper*>(statement_ptr);

    return wrapper->getInputMetadata(master, status_vector);
}

extern "C" void* fbs_get_output_metadata(void* master_ptr, void* statement_ptr, ISC_STATUS* status_vector) {
    if (status_vector) {
        status_vector[0] = 1;
        status_vector[1] = 0;
    }
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

extern "C" int fbs_set_cursor_name(void* master_ptr, void* statement_ptr, const char* cursor_name, ISC_STATUS* status_vector) {
    if (!master_ptr || !statement_ptr || !cursor_name) {
        if (status_vector) {
            status_vector[0] = 1;
            status_vector[1] = isc_arg_gds;
            status_vector[2] = isc_bad_stmt_handle;
            status_vector[3] = isc_arg_end;
        }
        return 0;
    }

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* wrapper = static_cast<fb::StatementWrapper*>(statement_ptr);

    try {
        Firebird::ThrowStatusWrapper status(master->getStatus());
        Firebird::IStatement* stmt = wrapper->getStatement();
        if (!stmt) {
            if (status_vector) {
                status_vector[0] = 1;
                status_vector[1] = isc_arg_gds;
                status_vector[2] = isc_bad_stmt_handle;
                status_vector[3] = isc_arg_end;
            }
            return 0;
        }

        stmt->setCursorName(&status, cursor_name);

        if (status_vector) {
            status_vector[0] = 0;
            status_vector[1] = 0;
        }
        return 1;
    } catch (const Firebird::FbException& e) {
        if (status_vector) {
            const ISC_STATUS* errors = e.getStatus()->getErrors();
            if (errors) {
                for (size_t i = 0; i < ISC_STATUS_LENGTH; ++i) {
                    status_vector[i] = errors[i];
                    if (errors[i] == isc_arg_end) break;
                }
            }
        }
        return 0;
    }
}

extern "C" ISC_INT64 fbs_execute_singleton_int64(
    void* master_ptr,
    void* statement_ptr,
    void* transaction_ptr,
    ISC_STATUS* status_vector
) {
    if (!master_ptr || !statement_ptr || !transaction_ptr) {
        if (status_vector) {
            status_vector[0] = 1;
            status_vector[1] = isc_arg_gds;
            status_vector[2] = isc_bad_db_handle;
            status_vector[3] = isc_arg_end;
        }
        return 0;
    }

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* wrapper = static_cast<fb::StatementWrapper*>(statement_ptr);
    auto* transaction = static_cast<Firebird::ITransaction*>(transaction_ptr);

    Firebird::CheckStatusWrapper status(master->getStatus());

    /* Get output metadata to determine buffer size and field offset */
    auto* outMetadata = wrapper->getOutputMetadata(master, status_vector);
    if (!outMetadata) {
        return 0;
    }

    unsigned msgLen = outMetadata->getMessageLength(&status);
    unsigned fieldOffset = outMetadata->getOffset(&status, 0);
    unsigned nullOffset = outMetadata->getNullOffset(&status, 0);

    /* Allocate message buffer */
    auto* outMsg = new unsigned char[msgLen];
    memset(outMsg, 0, msgLen);

    /* Open cursor */
    if (!wrapper->openCursor(master, transaction, nullptr, nullptr, 0, status_vector)) {
        outMetadata->release();
        delete[] outMsg;
        return 0;
    }

    /* Fetch the single row */
    int fetchResult = wrapper->fetchNext(master, outMsg, status_vector);
    if (fetchResult != 1) {
        /* fetchResult: 1 = success, 0 = EOF, -1 = error */
        wrapper->closeCursor(status_vector);
        outMetadata->release();
        delete[] outMsg;
        return 0;
    }

    /* Check null indicator */
    ISC_SHORT nullFlag = *reinterpret_cast<ISC_SHORT*>(outMsg + nullOffset);
    ISC_INT64 result = 0;

    if (nullFlag == 0) {
        /* Field is not null - extract the INT64 value */
        result = *reinterpret_cast<ISC_INT64*>(outMsg + fieldOffset);
    }

    /* Close cursor and cleanup */
    wrapper->closeCursor(status_vector);
    outMetadata->release();
    delete[] outMsg;

    /* Clear any error status since we succeeded */
    if (status_vector) {
        status_vector[0] = 1;
        status_vector[1] = 0;
    }

    return result;
}

extern "C" unsigned fbs_get_input_count(void* master_ptr, void* statement_ptr, ISC_STATUS* status_vector) {
    if (status_vector) {
        status_vector[0] = 1;
        status_vector[1] = 0;
    }
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
    if (status_vector) {
        status_vector[0] = 1;
        status_vector[1] = 0;
    }
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

/* =============================================================================
 * OO API Blob Functions (fbb_*)
 * ============================================================================= */
#define FBB_NO_INLINE_IMPL
#include "src/cpp/fb_blob.hpp"

/* =============================================================================
 * OO API Event Functions (fbe_*)
 * ============================================================================= */
#include "src/cpp/fb_events.hpp"

/* =============================================================================
 * OO API Service Functions (fbsvc_*)
 * ============================================================================= */
#include "src/cpp/fb_service.hpp"

/* =============================================================================
 * OO API Array Functions (fba_*)
 * ============================================================================= */
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

extern "C" int fba_lookup_bounds(
    void* master_ptr,
    void* attachment_ptr,
    void* transaction_ptr,
    const char* relation_name,
    const char* field_name,
    ISC_ARRAY_DESC* desc,
    ISC_STATUS* status_vector
) {
#ifdef FBIRD_ARRAY_DEBUG
    fprintf(stderr, "fba_lookup_bounds: master=%p attach=%p trans=%p rel='%s' field='%s'\n",
        master_ptr, attachment_ptr, transaction_ptr,
        relation_name ? relation_name : "NULL",
        field_name ? field_name : "NULL");
#endif

    if (!master_ptr || !attachment_ptr || !transaction_ptr || !relation_name || !field_name || !desc) {
    #ifdef FBIRD_ARRAY_DEBUG
    fprintf(stderr, "fba_lookup_bounds: NULL parameter check failed\n");
#endif
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_req_handle;
            status_vector[2] = isc_arg_end;
        }
        return 1;
    }

    /*
     * Array descriptor lookup via OO API.
     *
     * There is no direct IAttachment method for lookup_bounds(). We query
     * Firebird system tables to reconstruct ISC_ARRAY_DESC.
     *
     * This implementation supports the extension test suite use case:
     * - one-dimensional arrays with lower bound = 1
     * - element type: blr_text / blr_varying / blr_long / blr_short / blr_int64
     *
     * It can be extended to multi-dimensional arrays later.
     */

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* attachment = static_cast<Firebird::IAttachment*>(attachment_ptr);
    auto* transaction = static_cast<Firebird::ITransaction*>(transaction_ptr);

    try {
        Firebird::IStatus* raw_status = master->getStatus();
        Firebird::CheckStatusWrapper st(raw_status);

        /* Query 1: array field source + relation field id.
         * We need RDB$FIELD_SOURCE (domain name) and RDB$FIELD_ID (field position in relation).
         * Note: RDB$FIELD_DIMENSIONS links via (RDB$FIELD_NAME, RDB$DIMENSION) only, so we
         * can use RDB$FIELD_ID later only to sanity-check, not as join key. */
        const char* sql1 =
            "SELECT rf.RDB$FIELD_SOURCE, rf.RDB$FIELD_ID "
            "FROM RDB$RELATION_FIELDS rf "
            "WHERE rf.RDB$RELATION_NAME = ? AND rf.RDB$FIELD_NAME = ?";

#ifdef FBIRD_ARRAY_DEBUG
        fprintf(stderr, "fba_lookup_bounds: preparing Query1...\n");
#endif
        Firebird::IStatement* stmt1 = attachment->prepare(&st, transaction, 0, sql1, 3, 0);
        if (fb::statusHasError(raw_status) || !stmt1) {
    #ifdef FBIRD_ARRAY_DEBUG
        fprintf(stderr, "fba_lookup_bounds: Query1 prepare FAILED\n");
#endif
            if (status_vector) {
                copy_status_vector(raw_status->getErrors(), ISC_STATUS_LENGTH, status_vector, ISC_STATUS_LENGTH);
            }
            return 1;
        }
#ifdef FBIRD_ARRAY_DEBUG
        fprintf(stderr, "fba_lookup_bounds: Query1 prepare OK, stmt1=%p\n", (void*)stmt1);
#endif

        Firebird::IMessageMetadata* inMeta1 = stmt1->getInputMetadata(&st);
        Firebird::IMessageMetadata* outMeta1 = stmt1->getOutputMetadata(&st);
        if (fb::statusHasError(raw_status) || !inMeta1 || !outMeta1) {
            if (status_vector) {
                copy_status_vector(raw_status->getErrors(), ISC_STATUS_LENGTH, status_vector, ISC_STATUS_LENGTH);
            }
            if (inMeta1) inMeta1->release();
            if (outMeta1) outMeta1->release();
            stmt1->free(&st);
            return 1;
        }

        const unsigned inLen1 = inMeta1->getMessageLength(&st);
        const unsigned outLen1 = outMeta1->getMessageLength(&st);
        std::unique_ptr<unsigned char[]> inBuf1(new unsigned char[inLen1]());
        std::unique_ptr<unsigned char[]> outBuf1(new unsigned char[outLen1]());

        /* Bind relation_name + field_name (both SQL_TEXT/VARYING) */
        for (unsigned p = 0; p < inMeta1->getCount(&st); ++p) {
            const unsigned type = inMeta1->getType(&st, p) & ~1u;
            const unsigned off = inMeta1->getOffset(&st, p);
            const unsigned nullOff = inMeta1->getNullOffset(&st, p);
            *reinterpret_cast<ISC_SHORT*>(inBuf1.get() + nullOff) = 0;

            const char* val = (p == 0) ? relation_name : field_name;
            const unsigned maxLen = inMeta1->getLength(&st, p);
            const unsigned valLen = (unsigned) std::min<size_t>(std::strlen(val), maxLen);

            if (type == SQL_VARYING) {
                *reinterpret_cast<ISC_SHORT*>(inBuf1.get() + off) = (ISC_SHORT)valLen;
                std::memcpy(inBuf1.get() + off + sizeof(ISC_SHORT), val, valLen);
            } else {
                std::memcpy(inBuf1.get() + off, val, valLen);
                if (valLen < maxLen) {
                    std::memset(inBuf1.get() + off + valLen, ' ', maxLen - valLen);
                }
            }
        }

#ifdef FBIRD_ARRAY_DEBUG
        fprintf(stderr, "fba_lookup_bounds: Query1 openCursor...\n");
#endif
        Firebird::IResultSet* rs1 = stmt1->openCursor(&st, transaction, inMeta1, inBuf1.get(), outMeta1, 0);
        if (fb::statusHasError(raw_status) || !rs1) {
    #ifdef FBIRD_ARRAY_DEBUG
        fprintf(stderr, "fba_lookup_bounds: Query1 openCursor FAILED\n");
#endif
            if (status_vector) {
                copy_status_vector(raw_status->getErrors(), ISC_STATUS_LENGTH, status_vector, ISC_STATUS_LENGTH);
            }
            inMeta1->release();
            outMeta1->release();
            stmt1->free(&st);
            return 1;
        }

#ifdef FBIRD_ARRAY_DEBUG
        fprintf(stderr, "fba_lookup_bounds: Query1 fetchNext...\n");
#endif
        const int fetch1 = rs1->fetchNext(&st, outBuf1.get());
#ifdef FBIRD_ARRAY_DEBUG
        fprintf(stderr, "fba_lookup_bounds: Query1 fetch1=%d\n", fetch1);
#endif
        if (fb::statusHasError(raw_status) || fetch1 != 0) {
#ifdef FBIRD_ARRAY_DEBUG
            fprintf(stderr, "fba_lookup_bounds: Query1 fetch FAILED (fetch1=%d, hasError=%d)\n", fetch1, fb::statusHasError(raw_status));
#endif
            if (status_vector) {
                copy_status_vector(raw_status->getErrors(), ISC_STATUS_LENGTH, status_vector, ISC_STATUS_LENGTH);
            }
            rs1->close(&st);
            inMeta1->release();
            outMeta1->release();
            stmt1->free(&st);
            return 1;
        }

        /* Extract RDB$FIELD_SOURCE (TEXT) and RDB$FIELD_ID (SHORT) */
        const unsigned fsOff = outMeta1->getOffset(&st, 0);
        const unsigned fsNullOff = outMeta1->getNullOffset(&st, 0);
        const unsigned fsLen = outMeta1->getLength(&st, 0);
        const unsigned fidOff = outMeta1->getOffset(&st, 1);
        const unsigned fidNullOff = outMeta1->getNullOffset(&st, 1);

        if (*reinterpret_cast<ISC_SHORT*>(outBuf1.get() + fsNullOff) != 0 ||
            *reinterpret_cast<ISC_SHORT*>(outBuf1.get() + fidNullOff) != 0) {
            if (status_vector) {
                status_vector[0] = isc_arg_gds;
                status_vector[1] = isc_bad_req_handle;
                status_vector[2] = isc_arg_end;
            }
            rs1->close(&st);
            inMeta1->release();
            outMeta1->release();
            stmt1->free(&st);
            return 1;
        }

        std::string fieldSource(reinterpret_cast<char*>(outBuf1.get() + fsOff), fsLen);
        /* Trim trailing spaces */
        while (!fieldSource.empty() && fieldSource.back() == ' ') {
            fieldSource.pop_back();
        }

        const ISC_SHORT fieldId = *reinterpret_cast<ISC_SHORT*>(outBuf1.get() + fidOff);

        rs1->close(&st);
        inMeta1->release();
        outMeta1->release();
        stmt1->free(&st);

#ifdef FBIRD_ARRAY_DEBUG
        fprintf(stderr, "fba_lookup_bounds: fieldSource='%s' fieldId=%d\n", fieldSource.c_str(), (int)fieldId);
#endif

        /* Query 2: field type/len/scale/subtype/charset */
        const char* sql2 =
            "SELECT f.RDB$FIELD_TYPE, f.RDB$FIELD_LENGTH, f.RDB$FIELD_SCALE, f.RDB$FIELD_SUB_TYPE, f.RDB$CHARACTER_SET_ID "
            "FROM RDB$FIELDS f WHERE f.RDB$FIELD_NAME = ?";

#ifdef FBIRD_ARRAY_DEBUG
        fprintf(stderr, "fba_lookup_bounds: Query2 prepare...\n");
#endif
        Firebird::IStatement* stmt2 = attachment->prepare(&st, transaction, 0, sql2, 3, 0);
#ifdef FBIRD_ARRAY_DEBUG
        fprintf(stderr, "fba_lookup_bounds: Query2 prepare hasError=%d stmt2=%p\n", fb::statusHasError(raw_status), (void*)stmt2);
#endif
        if (fb::statusHasError(raw_status) || !stmt2) {
#ifdef FBIRD_ARRAY_DEBUG
            fprintf(stderr, "fba_lookup_bounds: Query2 prepare FAILED\n");
#endif
            if (status_vector) {
                copy_status_vector(raw_status->getErrors(), ISC_STATUS_LENGTH, status_vector, ISC_STATUS_LENGTH);
            }
            return 1;
        }

#ifdef FBIRD_ARRAY_DEBUG
        fprintf(stderr, "fba_lookup_bounds: Query2 getMetadata...\n");
#endif
        Firebird::IMessageMetadata* inMeta2 = stmt2->getInputMetadata(&st);
        Firebird::IMessageMetadata* outMeta2 = stmt2->getOutputMetadata(&st);
#ifdef FBIRD_ARRAY_DEBUG
        fprintf(stderr, "fba_lookup_bounds: Query2 inMeta2=%p outMeta2=%p\n", (void*)inMeta2, (void*)outMeta2);
#endif
        if (fb::statusHasError(raw_status) || !inMeta2 || !outMeta2) {
#ifdef FBIRD_ARRAY_DEBUG
            fprintf(stderr, "fba_lookup_bounds: Query2 metadata FAILED\n");
#endif
            if (status_vector) {
                copy_status_vector(raw_status->getErrors(), ISC_STATUS_LENGTH, status_vector, ISC_STATUS_LENGTH);
            }
            if (inMeta2) inMeta2->release();
            if (outMeta2) outMeta2->release();
            stmt2->free(&st);
            return 1;
        }

        const unsigned inLen2 = inMeta2->getMessageLength(&st);
        const unsigned outLen2 = outMeta2->getMessageLength(&st);
        std::unique_ptr<unsigned char[]> inBuf2(new unsigned char[inLen2]());
        std::unique_ptr<unsigned char[]> outBuf2(new unsigned char[outLen2]());

        /* Bind fieldSource */
        {
            const unsigned type = inMeta2->getType(&st, 0) & ~1u;
            const unsigned off = inMeta2->getOffset(&st, 0);
            const unsigned nullOff = inMeta2->getNullOffset(&st, 0);
            const unsigned maxLen = inMeta2->getLength(&st, 0);
            *reinterpret_cast<ISC_SHORT*>(inBuf2.get() + nullOff) = 0;

            const unsigned valLen = (unsigned) std::min<size_t>(fieldSource.size(), maxLen);
            if (type == SQL_VARYING) {
                *reinterpret_cast<ISC_SHORT*>(inBuf2.get() + off) = (ISC_SHORT)valLen;
                std::memcpy(inBuf2.get() + off + sizeof(ISC_SHORT), fieldSource.data(), valLen);
            } else {
                std::memcpy(inBuf2.get() + off, fieldSource.data(), valLen);
                if (valLen < maxLen) {
                    std::memset(inBuf2.get() + off + valLen, ' ', maxLen - valLen);
                }
            }
        }

#ifdef FBIRD_ARRAY_DEBUG
        fprintf(stderr, "fba_lookup_bounds: Query2 openCursor...\n");
#endif
        Firebird::IResultSet* rs2 = stmt2->openCursor(&st, transaction, inMeta2, inBuf2.get(), outMeta2, 0);
        if (fb::statusHasError(raw_status) || !rs2) {
#ifdef FBIRD_ARRAY_DEBUG
            fprintf(stderr, "fba_lookup_bounds: Query2 openCursor FAILED\n");
#endif
            if (status_vector) {
                copy_status_vector(raw_status->getErrors(), ISC_STATUS_LENGTH, status_vector, ISC_STATUS_LENGTH);
            }
            inMeta2->release();
            outMeta2->release();
            stmt2->free(&st);
            return 1;
        }

#ifdef FBIRD_ARRAY_DEBUG
        fprintf(stderr, "fba_lookup_bounds: Query2 fetchNext...\n");
#endif
        const int fetch2 = rs2->fetchNext(&st, outBuf2.get());
#ifdef FBIRD_ARRAY_DEBUG
        fprintf(stderr, "fba_lookup_bounds: Query2 fetch2=%d\n", fetch2);
#endif
        if (fb::statusHasError(raw_status) || fetch2 != 0) {
#ifdef FBIRD_ARRAY_DEBUG
            fprintf(stderr, "fba_lookup_bounds: Query2 fetch FAILED\n");
#endif
            if (status_vector) {
                copy_status_vector(raw_status->getErrors(), ISC_STATUS_LENGTH, status_vector, ISC_STATUS_LENGTH);
            }
            rs2->close(&st);
            inMeta2->release();
            outMeta2->release();
            stmt2->free(&st);
            return 1;
        }

        const ISC_SHORT fieldType = *reinterpret_cast<ISC_SHORT*>(
            outBuf2.get() + outMeta2->getOffset(&st, 0));
        const ISC_SHORT fieldLen = *reinterpret_cast<ISC_SHORT*>(
            outBuf2.get() + outMeta2->getOffset(&st, 1));
        const ISC_SHORT fieldScale = *reinterpret_cast<ISC_SHORT*>(
            outBuf2.get() + outMeta2->getOffset(&st, 2));
        const ISC_SHORT fieldSubType = *reinterpret_cast<ISC_SHORT*>(
            outBuf2.get() + outMeta2->getOffset(&st, 3));
        const ISC_SHORT fieldCharsetId = *reinterpret_cast<ISC_SHORT*>(
            outBuf2.get() + outMeta2->getOffset(&st, 4));

        rs2->close(&st);
        inMeta2->release();
        outMeta2->release();
        stmt2->free(&st);

#ifdef FBIRD_ARRAY_DEBUG
        fprintf(stderr, "fba_lookup_bounds: fieldType=%d fieldLen=%d fieldScale=%d fieldSubType=%d charsetId=%d\n",
            (int)fieldType, (int)fieldLen, (int)fieldScale, (int)fieldSubType, (int)fieldCharsetId);
#endif

        /* Query 3: bounds per dimension.
         * Firebird 4.0 RDB$FIELD_DIMENSIONS only has:
         *   RDB$FIELD_NAME, RDB$DIMENSION, RDB$LOWER_BOUND, RDB$UPPER_BOUND.
         * There is no FIELD_ID/RELATION_FIELD/RELATION_NAME column.
         * The FIELD_NAME here is the domain name (RDB$FIELD_SOURCE).
         */
        const char* sql3 =
            "SELECT fd.RDB$DIMENSION, fd.RDB$LOWER_BOUND, fd.RDB$UPPER_BOUND "
            "FROM RDB$FIELD_DIMENSIONS fd "
            "WHERE fd.RDB$FIELD_NAME = ? "
            "ORDER BY fd.RDB$DIMENSION";

#ifdef FBIRD_ARRAY_DEBUG
        fprintf(stderr, "fba_lookup_bounds: Query3 prepare...\n");
#endif
        Firebird::IStatement* stmt3 = attachment->prepare(&st, transaction, 0, sql3, 3, 0);
#ifdef FBIRD_ARRAY_DEBUG
        fprintf(stderr, "fba_lookup_bounds: Query3 prepare hasError=%d stmt3=%p\n", fb::statusHasError(raw_status), (void*)stmt3);
#endif
        if (fb::statusHasError(raw_status) || !stmt3) {
#ifdef FBIRD_ARRAY_DEBUG
            fprintf(stderr, "fba_lookup_bounds: Query3 prepare FAILED\n");
#endif
            if (status_vector) {
                copy_status_vector(raw_status->getErrors(), ISC_STATUS_LENGTH, status_vector, ISC_STATUS_LENGTH);
            }
            return 1;
        }

#ifdef FBIRD_ARRAY_DEBUG
        fprintf(stderr, "fba_lookup_bounds: Query3 getMetadata...\n");
#endif
        Firebird::IMessageMetadata* inMeta3 = stmt3->getInputMetadata(&st);
        Firebird::IMessageMetadata* outMeta3 = stmt3->getOutputMetadata(&st);
#ifdef FBIRD_ARRAY_DEBUG
        fprintf(stderr, "fba_lookup_bounds: Query3 inMeta3=%p outMeta3=%p\n", (void*)inMeta3, (void*)outMeta3);
#endif
        if (fb::statusHasError(raw_status) || !inMeta3 || !outMeta3) {
#ifdef FBIRD_ARRAY_DEBUG
            fprintf(stderr, "fba_lookup_bounds: Query3 metadata FAILED\n");
#endif
            if (status_vector) {
                copy_status_vector(raw_status->getErrors(), ISC_STATUS_LENGTH, status_vector, ISC_STATUS_LENGTH);
            }
            if (inMeta3) inMeta3->release();
            if (outMeta3) outMeta3->release();
            stmt3->free(&st);
            return 1;
        }

        const unsigned inLen3 = inMeta3->getMessageLength(&st);
        const unsigned outLen3 = outMeta3->getMessageLength(&st);
        std::unique_ptr<unsigned char[]> inBuf3(new unsigned char[inLen3]());
        std::unique_ptr<unsigned char[]> outBuf3(new unsigned char[outLen3]());

        /* Bind: fieldSource */
        {
            const unsigned type0 = inMeta3->getType(&st, 0) & ~1u;
            const unsigned off0 = inMeta3->getOffset(&st, 0);
            const unsigned null0 = inMeta3->getNullOffset(&st, 0);
            const unsigned max0 = inMeta3->getLength(&st, 0);
            *reinterpret_cast<ISC_SHORT*>(inBuf3.get() + null0) = 0;
            const unsigned valLen0 = (unsigned) std::min<size_t>(fieldSource.size(), max0);
            if (type0 == SQL_VARYING) {
                *reinterpret_cast<ISC_SHORT*>(inBuf3.get() + off0) = (ISC_SHORT)valLen0;
                std::memcpy(inBuf3.get() + off0 + sizeof(ISC_SHORT), fieldSource.data(), valLen0);
            } else {
                std::memcpy(inBuf3.get() + off0, fieldSource.data(), valLen0);
                if (valLen0 < max0) {
                    std::memset(inBuf3.get() + off0 + valLen0, ' ', max0 - valLen0);
                }
            }
        }

#ifdef FBIRD_ARRAY_DEBUG
        fprintf(stderr, "fba_lookup_bounds: Query3 openCursor...\n");
#endif
        Firebird::IResultSet* rs3 = stmt3->openCursor(&st, transaction, inMeta3, inBuf3.get(), outMeta3, 0);
#ifdef FBIRD_ARRAY_DEBUG
        fprintf(stderr, "fba_lookup_bounds: Query3 openCursor hasError=%d rs3=%p\n", fb::statusHasError(raw_status), (void*)rs3);
#endif
        if (fb::statusHasError(raw_status) || !rs3) {
#ifdef FBIRD_ARRAY_DEBUG
            fprintf(stderr, "fba_lookup_bounds: Query3 openCursor FAILED\n");
#endif
            if (status_vector) {
                copy_status_vector(raw_status->getErrors(), ISC_STATUS_LENGTH, status_vector, ISC_STATUS_LENGTH);
            }
            inMeta3->release();
            outMeta3->release();
            stmt3->free(&st);
            return 1;
        }

        /* Start filling descriptor */
        std::memset(desc, 0, sizeof(*desc));
        std::memcpy(desc->array_desc_relation_name, relation_name,
                    std::min<size_t>(std::strlen(relation_name), sizeof(desc->array_desc_relation_name)));
        std::memcpy(desc->array_desc_field_name, field_name,
                    std::min<size_t>(std::strlen(field_name), sizeof(desc->array_desc_field_name)));

        /* Map RDB$FIELD_TYPE to blr_* (minimal mapping for test suite) */
        switch (fieldType) {
            case 7:  /* SMALLINT */
                desc->array_desc_dtype = blr_short;
                break;
            case 8:  /* INTEGER */
                desc->array_desc_dtype = blr_long;
                break;
            case 10: /* FLOAT */
                desc->array_desc_dtype = blr_float;
                break;
            case 12: /* DATE */
                desc->array_desc_dtype = blr_sql_date;
                break;
            case 13: /* TIME */
                desc->array_desc_dtype = blr_sql_time;
                break;
            case 14: /* CHAR */
                desc->array_desc_dtype = blr_text;
                break;
            case 16: /* BIGINT / NUMERIC / DECIMAL */
                desc->array_desc_dtype = blr_int64;
                break;
            case 27: /* DOUBLE PRECISION */
                desc->array_desc_dtype = blr_double;
                break;
            case 35: /* TIMESTAMP */
                desc->array_desc_dtype = blr_timestamp;
                break;
            case 37: /* VARCHAR */
                desc->array_desc_dtype = blr_varying;
                break;
            default:
                /* Unsupported element type for now */
                if (status_vector) {
                    status_vector[0] = isc_arg_gds;
                    status_vector[1] = isc_dsql_datatype_err;
                    status_vector[2] = isc_arg_end;
                }
                rs3->close(&st);
                inMeta3->release();
                outMeta3->release();
                stmt3->free(&st);
                return 1;
        }

        desc->array_desc_length = fieldLen;
        desc->array_desc_scale = fieldScale;
        desc->array_desc_dimensions = 0;
        /* Store charset id in flags field for VARCHAR/CHAR types.
         * This is used by buildSdlFromDesc() to emit blr_varying2 with correct charset.
         * The flags field is otherwise unused in our implementation. */
        desc->array_desc_flags = fieldCharsetId;

        /* Iterate bounds rows */
#ifdef FBIRD_ARRAY_DEBUG
        fprintf(stderr, "fba_lookup_bounds: Query3 fetch loop starting...\n");
#endif
        int dim_count = 0;
        while (true) {
            const int f = rs3->fetchNext(&st, outBuf3.get());
#ifdef FBIRD_ARRAY_DEBUG
            fprintf(stderr, "fba_lookup_bounds: Query3 fetch=%d hasError=%d\n", f, fb::statusHasError(raw_status));
#endif
            if (fb::statusHasError(raw_status)) {
                if (status_vector) {
                    copy_status_vector(raw_status->getErrors(), ISC_STATUS_LENGTH, status_vector, ISC_STATUS_LENGTH);
                }
                rs3->close(&st);
                inMeta3->release();
                outMeta3->release();
                stmt3->free(&st);
                return 1;
            }
            if (f != 0) {
                break; /* EOF */
            }

            const unsigned dimOff = outMeta3->getOffset(&st, 0);
            const unsigned lowOff = outMeta3->getOffset(&st, 1);
            const unsigned upOff = outMeta3->getOffset(&st, 2);

            const ISC_SHORT dim = *reinterpret_cast<ISC_SHORT*>(outBuf3.get() + dimOff);
            const ISC_LONG low = *reinterpret_cast<ISC_LONG*>(outBuf3.get() + lowOff);
            const ISC_LONG up = *reinterpret_cast<ISC_LONG*>(outBuf3.get() + upOff);

#ifdef FBIRD_ARRAY_DEBUG
            fprintf(stderr, "fba_lookup_bounds: dim=%d low=%d up=%d (offs: dim=%u low=%u up=%u)\n",
                (int)dim, (int)low, (int)up, dimOff, lowOff, upOff);
#endif

            /* Firebird stores RDB$DIMENSION as 0-based (0..N-1) */
            if (dim < 0 || dim >= 16) {
#ifdef FBIRD_ARRAY_DEBUG
                fprintf(stderr, "fba_lookup_bounds: skipping dim=%d (out of range 0-15)\n", (int)dim);
#endif
                continue;
            }

            /* RDB$DIMENSION is already 0-based; ISC_ARRAY_DESC uses 0-based array_desc_bounds */
            const auto idx = static_cast<unsigned>(dim);
            desc->array_desc_bounds[idx].array_bound_lower = low;
            desc->array_desc_bounds[idx].array_bound_upper = up;

            if (idx + 1 > desc->array_desc_dimensions) {
                desc->array_desc_dimensions = (ISC_USHORT)(idx + 1);
            }
        }

        rs3->close(&st);
        inMeta3->release();
        outMeta3->release();
        stmt3->free(&st);

        if (desc->array_desc_dimensions == 0) {
#ifdef FBIRD_ARRAY_DEBUG
            fprintf(stderr, "fba_lookup_bounds: no dimensions found!\n");
#endif
            if (status_vector) {
                status_vector[0] = isc_arg_gds;
                status_vector[1] = isc_bad_req_handle;
                status_vector[2] = isc_arg_end;
            }
            return 1;
        }

#ifdef FBIRD_ARRAY_DEBUG
        fprintf(stderr, "fba_lookup_bounds: SUCCESS dtype=%d len=%d dims=%d bounds[0]=%d-%d\n",
            desc->array_desc_dtype, desc->array_desc_length, desc->array_desc_dimensions,
            desc->array_desc_bounds[0].array_bound_lower, desc->array_desc_bounds[0].array_bound_upper);
#endif

        if (status_vector) {
            status_vector[0] = 1;
            status_vector[1] = 0;
        }
        return 0;

    } catch (...) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_except2;
            status_vector[2] = isc_arg_end;
        }
        return 1;
    }
}

// Non-inline implementations needed for C linkage (inline functions in headers
// don't get proper linkage when called from C code)
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

extern "C" int fbb_seek(void* master_ptr, void* blob_wrapper, int whence, int offset,
                        int* result_position, ISC_STATUS* status_vector) {
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

    return wrapper->seek(master, whence, offset, result_position, status_vector) ? 1 : 0;
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

/* =============================================================================
 * IXpbBuilder-based TPB Construction (FB 3.0+)
 *
 * These functions use IXpbBuilder for clean construction of Transaction
 * Parameter Blocks, replacing manual byte array assembly.
 * ============================================================================= */

extern "C" unsigned char* fbxpb_build_tpb(
    void* master_ptr,
    zend_long trans_flags,
    zend_long lock_timeout,
    unsigned* buffer_length,
    ISC_STATUS* status_vector
) {
    if (!master_ptr || !buffer_length) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_req_handle;
            status_vector[2] = isc_arg_end;
        }
        return nullptr;
    }

    *buffer_length = 0;

    try {
        auto* master = static_cast<Firebird::IMaster*>(master_ptr);
        Firebird::IStatus* raw_status = master->getStatus();
        Firebird::CheckStatusWrapper status(raw_status);
        Firebird::IUtil* util = master->getUtilInterface();

        if (!util) {
            if (status_vector) {
                status_vector[0] = isc_arg_gds;
                status_vector[1] = isc_unavailable;
                status_vector[2] = isc_arg_end;
            }
            return nullptr;
        }

        // Create IXpbBuilder for TPB construction
        // IXpbBuilder::TPB = 1 (Transaction Parameter Block)
        Firebird::IXpbBuilder* tpb = util->getXpbBuilder(&status, Firebird::IXpbBuilder::TPB, nullptr, 0);

        if (fb::statusHasError(raw_status) || !tpb) {
            if (status_vector) {
                copy_status_vector(raw_status->getErrors(), ISC_STATUS_LENGTH, status_vector, ISC_STATUS_LENGTH);
            }
            return nullptr;
        }

        // Note: IXpbBuilder for TPB automatically includes isc_tpb_version3
        // Do NOT manually insert it or you'll get a duplicate!

        // Access mode: READ or WRITE (default WRITE)
        if (trans_flags & PHP_FBIRD_READ) {
            tpb->insertTag(&status, isc_tpb_read);
        } else {
            tpb->insertTag(&status, isc_tpb_write);
        }

        // Isolation level (mutually exclusive - check in order of specificity)
        if (trans_flags & PHP_FBIRD_COMMITTED) {
            tpb->insertTag(&status, isc_tpb_read_committed);

            // Record versioning for READ COMMITTED
            if (trans_flags & PHP_FBIRD_REC_VERSION) {
                tpb->insertTag(&status, isc_tpb_rec_version);
            } else if (trans_flags & PHP_FBIRD_REC_NO_VERSION) {
                tpb->insertTag(&status, isc_tpb_no_rec_version);
            }
#if FB_API_VER >= 40
            // FB 4.0+ READ CONSISTENCY for snapshot isolation within READ COMMITTED
            if (trans_flags & PHP_FBIRD_READ_CONSISTENCY) {
                tpb->insertTag(&status, isc_tpb_read_consistency);
            }
#endif
        } else if (trans_flags & PHP_FBIRD_CONSISTENCY) {
            tpb->insertTag(&status, isc_tpb_consistency);
        } else if (trans_flags & PHP_FBIRD_CONCURRENCY) {
            tpb->insertTag(&status, isc_tpb_concurrency);
        } else {
            // Default: SNAPSHOT (concurrency)
            tpb->insertTag(&status, isc_tpb_concurrency);
        }

        // Lock resolution: WAIT, NOWAIT, or LOCK_TIMEOUT
        if (trans_flags & PHP_FBIRD_NOWAIT) {
            tpb->insertTag(&status, isc_tpb_nowait);
        } else if (trans_flags & PHP_FBIRD_LOCK_TIMEOUT) {
            // Lock timeout requires wait + timeout value
            tpb->insertTag(&status, isc_tpb_wait);
            // Insert timeout value as 4-byte integer
            tpb->insertInt(&status, isc_tpb_lock_timeout, static_cast<int>(lock_timeout));
        } else if (trans_flags & PHP_FBIRD_WAIT) {
            tpb->insertTag(&status, isc_tpb_wait);
        }
        // Note: if none specified, Firebird defaults to WAIT

        // Check for errors during construction
        if (fb::statusHasError(raw_status)) {
            if (status_vector) {
                copy_status_vector(raw_status->getErrors(), ISC_STATUS_LENGTH, status_vector, ISC_STATUS_LENGTH);
            }
            tpb->dispose();
            return nullptr;
        }

        // Get buffer length and copy to allocated memory
        unsigned len = tpb->getBufferLength(&status);
        const unsigned char* buf = tpb->getBuffer(&status);

        if (fb::statusHasError(raw_status) || !buf || len == 0) {
            if (status_vector) {
                copy_status_vector(raw_status->getErrors(), ISC_STATUS_LENGTH, status_vector, ISC_STATUS_LENGTH);
            }
            tpb->dispose();
            return nullptr;
        }

        // Allocate and copy buffer (caller must free with fbxpb_free_tpb)
        auto* result = new unsigned char[len];
        std::memcpy(result, buf, len);
        *buffer_length = len;

        // Dispose the builder (we've copied the buffer)
        tpb->dispose();

        // Success
        if (status_vector) {
            status_vector[0] = 1;
            status_vector[1] = 0;
        }

        return result;

    } catch (const Firebird::FbException& e) {
        if (status_vector) {
            const ISC_STATUS* errors = e.getStatus()->getErrors();
            if (errors) {
                for (size_t i = 0; i < ISC_STATUS_LENGTH; ++i) {
                    status_vector[i] = errors[i];
                    if (errors[i] == isc_arg_end) break;
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

extern "C" void fbxpb_free_tpb(unsigned char* buffer) {
    if (buffer) {
        delete[] buffer;
    }
}

/* =============================================================================
 * Limbo Transaction Functions (Two-Phase Commit Recovery)
 * ============================================================================= */

extern "C" int fbt_get_limbo_transactions(
    void* master_ptr,
    void* attachment_ptr,
    ISC_INT64* trans_ids,
    unsigned max_ids,
    ISC_STATUS* status_vector
) {
    if (!master_ptr || !attachment_ptr || !trans_ids || max_ids == 0) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_db_handle;
            status_vector[2] = isc_arg_end;
        }
        return -1;
    }

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* attachment = static_cast<Firebird::IAttachment*>(attachment_ptr);

    try {
        Firebird::IStatus* raw_status = master->getStatus();
        Firebird::CheckStatusWrapper status(raw_status);

        // Request isc_info_limbo to get limbo transaction IDs
        const unsigned char info_items[] = { isc_info_limbo };
        constexpr unsigned BUFFER_SIZE = 4096;
        unsigned char buffer[BUFFER_SIZE];

        attachment->getInfo(&status, sizeof(info_items), info_items, BUFFER_SIZE, buffer);

        if (fb::statusHasError(raw_status)) {
            if (status_vector) {
                copy_status_vector(raw_status->getErrors(), ISC_STATUS_LENGTH, status_vector, ISC_STATUS_LENGTH);
            }
            return -1;
        }

        // Parse the response buffer
        // Format: isc_info_limbo, length (2 bytes), data...
        // Each limbo transaction is: type(1), length(2), transaction_id (4/8 bytes)
        unsigned count = 0;
        unsigned pos = 0;

        while (pos < BUFFER_SIZE && count < max_ids) {
            unsigned char item = buffer[pos++];

            if (item == isc_info_end) {
                break;
            }

            if (item != isc_info_limbo) {
                // Skip unknown items
                if (pos + 2 <= BUFFER_SIZE) {
                    unsigned len = buffer[pos] | (buffer[pos + 1] << 8);
                    pos += 2 + len;
                }
                continue;
            }

            // isc_info_limbo found - get length and transaction ID
            if (pos + 2 > BUFFER_SIZE) break;
            unsigned cluster_len = buffer[pos] | (buffer[pos + 1] << 8);
            pos += 2;

            // Parse transaction IDs within this cluster
            unsigned cluster_end = pos + cluster_len;
            while (pos < cluster_end && count < max_ids) {
                // Transaction ID is a 32-bit or 64-bit value (depends on Firebird version)
                // Typically stored as 4-byte integers
                if (pos + 4 <= cluster_end) {
                    const auto trans_id = static_cast<ISC_INT64>(
                        buffer[pos] | (buffer[pos + 1] << 8) |
                        (buffer[pos + 2] << 16) | (buffer[pos + 3] << 24)
                    );
                    if (trans_id > 0) {
                        trans_ids[count++] = trans_id;
                    }
                    pos += 4;
                } else {
                    pos = cluster_end;
                }
            }
        }

        if (status_vector) {
            status_vector[0] = 1;
            status_vector[1] = 0;
        }

        return static_cast<int>(count);

    } catch (const Firebird::FbException& e) {
        if (status_vector) {
            const ISC_STATUS* errors = e.getStatus()->getErrors();
            if (errors) {
                for (size_t i = 0; i < ISC_STATUS_LENGTH; ++i) {
                    status_vector[i] = errors[i];
                    if (errors[i] == isc_arg_end) break;
                }
            }
        }
        return -1;
    } catch (...) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_random;
            status_vector[2] = isc_arg_end;
        }
        return -1;
    }
}

extern "C" void* fbt_reconnect(
    void* master_ptr,
    void* attachment_ptr,
    ISC_INT64 trans_id,
    ISC_STATUS* status_vector
) {
    if (!master_ptr || !attachment_ptr) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_db_handle;
            status_vector[2] = isc_arg_end;
        }
        return nullptr;
    }

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* attachment = static_cast<Firebird::IAttachment*>(attachment_ptr);

    try {
        Firebird::IStatus* raw_status = master->getStatus();
        Firebird::CheckStatusWrapper status(raw_status);

        // Prepare the transaction ID as bytes (4 bytes for standard trans ID)
        unsigned char id_bytes[4];
        id_bytes[0] = static_cast<unsigned char>(trans_id & 0xFF);
        id_bytes[1] = static_cast<unsigned char>((trans_id >> 8) & 0xFF);
        id_bytes[2] = static_cast<unsigned char>((trans_id >> 16) & 0xFF);
        id_bytes[3] = static_cast<unsigned char>((trans_id >> 24) & 0xFF);

        // Reconnect to the limbo transaction
        Firebird::ITransaction* transaction = attachment->reconnectTransaction(
            &status, sizeof(id_bytes), id_bytes
        );

        if (fb::statusHasError(raw_status) || !transaction) {
            if (status_vector) {
                copy_status_vector(raw_status->getErrors(), ISC_STATUS_LENGTH, status_vector, ISC_STATUS_LENGTH);
            }
            return nullptr;
        }

        // Wrap in fb::Transaction and return using factory method
        // Note: We create a Transaction wrapper that owns this ITransaction
        auto* trans_wrapper = new fb::Transaction(fb::Transaction::fromRaw(master, transaction));

        if (status_vector) {
            status_vector[0] = 1;
            status_vector[1] = 0;
        }

        return trans_wrapper;

    } catch (const Firebird::FbException& e) {
        if (status_vector) {
            const ISC_STATUS* errors = e.getStatus()->getErrors();
            if (errors) {
                for (size_t i = 0; i < ISC_STATUS_LENGTH; ++i) {
                    status_vector[i] = errors[i];
                    if (errors[i] == isc_arg_end) break;
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

/* =============================================================================
 * IBatch API Functions (Firebird 4.0+ Bulk Operations)
 * ============================================================================= */

#if FB_API_VER >= 40

// Batch wrapper class to manage IBatch lifecycle
namespace {
    struct BatchWrapper {
        Firebird::IBatch* batch = nullptr;
        Firebird::IMessageMetadata* metadata = nullptr;

        ~BatchWrapper() {
            // CRITICAL: Do NOT release metadata here!
            // The metadata is owned by the PHP-side fbird_batch structure
            // (batch->in_metadata) and will be released by _php_fbird_free_batch().
            // Releasing it here causes double-free during PHP shutdown.
            // Fixes: Segfault in batch_edge_cases.phpt cleanup (si_addr=0x2c4)
            
            // Only close the IBatch handle if still open
            // (fbbatch_close() should have already done this)
            if (batch) {
                // Log warning - batch should have been closed explicitly
                // This is a cleanup safety net only
                batch = nullptr;
            }
        }
    };
}

extern "C" void* fbbatch_create(
    void* master_ptr,
    void* statement_ptr,
    unsigned buffer_size,
    ISC_STATUS* status_vector
) {
    if (!master_ptr || !statement_ptr) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_stmt_handle;
            status_vector[2] = isc_arg_end;
        }
        return nullptr;
    }

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* statement = static_cast<Firebird::IStatement*>(statement_ptr);

    try {
        Firebird::IStatus* raw_status = master->getStatus();
        Firebird::CheckStatusWrapper status(raw_status);

        // Get input metadata from statement
        Firebird::IMessageMetadata* inMetadata = statement->getInputMetadata(&status);
        if (fb::statusHasError(raw_status) || !inMetadata) {
            if (status_vector) {
                copy_status_vector(raw_status->getErrors(), ISC_STATUS_LENGTH, status_vector, ISC_STATUS_LENGTH);
            }
            return nullptr;
        }

        // Use default buffer size if not specified (16MB is a good default)
        const unsigned buffer_bytes_size = (buffer_size == 0) ? (16U * 1024U * 1024U) : buffer_size;

        // Build batch parameter block using IXpbBuilder::BATCH
        Firebird::IUtil* util = master->getUtilInterface();
        if (!util) {
            inMetadata->release();
            if (status_vector) {
                status_vector[0] = isc_arg_gds;
                status_vector[1] = isc_unavailable;
                status_vector[2] = isc_arg_end;
            }
            return nullptr;
        }

        Firebird::IXpbBuilder* batchPpb = util->getXpbBuilder(&status, Firebird::IXpbBuilder::BATCH, nullptr, 0);
        if (fb::statusHasError(raw_status) || !batchPpb) {
            inMetadata->release();
            if (status_vector) {
                copy_status_vector(raw_status->getErrors(), ISC_STATUS_LENGTH, status_vector, ISC_STATUS_LENGTH);
            }
            return nullptr;
        }

        batchPpb->insertInt(&status, Firebird::IBatch::TAG_BUFFER_BYTES_SIZE, static_cast<int>(buffer_bytes_size));
        batchPpb->insertInt(&status, Firebird::IBatch::TAG_BLOB_POLICY, Firebird::IBatch::BLOB_ID_ENGINE);
        batchPpb->insertTag(&status, Firebird::IBatch::TAG_MULTIERROR);
        batchPpb->insertTag(&status, Firebird::IBatch::TAG_DETAILED_ERRORS);

        const unsigned parLength = batchPpb->getBufferLength(&status);
        const unsigned char* par = batchPpb->getBuffer(&status);

        // Get batch from statement with the input metadata
        Firebird::IBatch* batch = statement->createBatch(
            &status,
            inMetadata,
            parLength,
            par
        );

        batchPpb->dispose();

        if (fb::statusHasError(raw_status) || !batch) {
            inMetadata->release();
            if (status_vector) {
                copy_status_vector(raw_status->getErrors(), ISC_STATUS_LENGTH, status_vector, ISC_STATUS_LENGTH);
            }
            return nullptr;
        }

        // Create wrapper
        auto* wrapper = new BatchWrapper();
        wrapper->batch = batch;
        wrapper->metadata = inMetadata;  // Keep reference for later use

        if (status_vector) {
            status_vector[0] = 1;
            status_vector[1] = 0;
        }

        return wrapper;

    } catch (const Firebird::FbException& e) {
        if (status_vector) {
            const ISC_STATUS* errors = e.getStatus()->getErrors();
            if (errors) {
                for (size_t i = 0; i < ISC_STATUS_LENGTH; ++i) {
                    status_vector[i] = errors[i];
                    if (errors[i] == isc_arg_end) break;
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

extern "C" int fbbatch_add(
    void* master_ptr,
    void* batch_wrapper,
    unsigned count,
    const void* in_buffer,
    ISC_STATUS* status_vector
) {
    if (!master_ptr || !batch_wrapper || !in_buffer) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_req_handle;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* wrapper = static_cast<BatchWrapper*>(batch_wrapper);

    if (!wrapper->batch) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_req_handle;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }

    try {
        Firebird::IStatus* raw_status = master->getStatus();
        Firebird::CheckStatusWrapper status(raw_status);

        // Add messages to batch
        wrapper->batch->add(&status, count, in_buffer);

        if (fb::statusHasError(raw_status)) {
            if (status_vector) {
                copy_status_vector(raw_status->getErrors(), ISC_STATUS_LENGTH, status_vector, ISC_STATUS_LENGTH);
            }
            return 0;
        }

        if (status_vector) {
            status_vector[0] = 1;
            status_vector[1] = 0;
        }

        return 1;

    } catch (const Firebird::FbException& e) {
        if (status_vector) {
            const ISC_STATUS* errors = e.getStatus()->getErrors();
            if (errors) {
                for (size_t i = 0; i < ISC_STATUS_LENGTH; ++i) {
                    status_vector[i] = errors[i];
                    if (errors[i] == isc_arg_end) break;
                }
            }
        }
        return 0;
    } catch (...) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_random;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }
}

extern "C" int fbbatch_execute(
    void* master_ptr,
    void* batch_wrapper,
    void* transaction_ptr,
    unsigned* total_processed,
    unsigned* error_count,
    ISC_STATUS* status_vector
) {
    if (!master_ptr || !batch_wrapper || !transaction_ptr) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_req_handle;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* wrapper = static_cast<BatchWrapper*>(batch_wrapper);
    auto* transaction = static_cast<Firebird::ITransaction*>(transaction_ptr);

    if (!wrapper->batch) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_req_handle;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }

    try {
        Firebird::IStatus* raw_status = master->getStatus();
        Firebird::CheckStatusWrapper status(raw_status);

        // Execute the batch
        Firebird::IBatchCompletionState* completion = wrapper->batch->execute(&status, transaction);

        if (fb::statusHasError(raw_status)) {
            if (status_vector) {
                copy_status_vector(raw_status->getErrors(), ISC_STATUS_LENGTH, status_vector, ISC_STATUS_LENGTH);
            }
            if (completion) {
                completion->dispose();
            }
            return 0;
        }

        // Get completion statistics
        if (completion) {
            if (total_processed) {
                *total_processed = completion->getSize(&status);
            }

            // Count errors
            unsigned errors = 0;
            unsigned size = completion->getSize(&status);
            for (unsigned i = 0; i < size; ++i) {
                int state = completion->getState(&status, i);
                if (state != Firebird::IBatchCompletionState::EXECUTE_FAILED) {
                    // Success
                } else {
                    errors++;
                }
            }
            if (error_count) {
                *error_count = errors;
            }

            completion->dispose();
        } else {
            if (total_processed) *total_processed = 0;
            if (error_count) *error_count = 0;
        }

        // NOTE: Do NOT call batch->close() here!
        // The batch data is added to the transaction by execute(), but the
        // transaction has not committed yet. Calling close() before commit
        // discards the uncommitted batch data.
        // The batch handle will be released when:
        // 1. PHP cleanup calls fbbatch_close() during shutdown
        // 2. Firebird auto-releases when transaction commits/rollbacks

        if (status_vector) {
            status_vector[0] = 1;
            status_vector[1] = 0;
        }

        return 1;

    } catch (const Firebird::FbException& e) {
        if (status_vector) {
            const ISC_STATUS* errors = e.getStatus()->getErrors();
            if (errors) {
                for (size_t i = 0; i < ISC_STATUS_LENGTH; ++i) {
                    status_vector[i] = errors[i];
                    if (errors[i] == isc_arg_end) break;
                }
            }
        }
        return 0;
    } catch (...) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_random;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }
}

extern "C" int fbbatch_cancel(
    void* master_ptr,
    void* batch_wrapper,
    ISC_STATUS* status_vector
) {
    if (!master_ptr || !batch_wrapper) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_req_handle;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* wrapper = static_cast<BatchWrapper*>(batch_wrapper);

    if (!wrapper->batch) {
        if (status_vector) {
            status_vector[0] = 1;
            status_vector[1] = 0;
        }
        return 1;  // Already cancelled/closed
    }

    try {
        Firebird::IStatus* raw_status = master->getStatus();
        Firebird::CheckStatusWrapper status(raw_status);

        wrapper->batch->cancel(&status);

        if (fb::statusHasError(raw_status)) {
            if (status_vector) {
                copy_status_vector(raw_status->getErrors(), ISC_STATUS_LENGTH, status_vector, ISC_STATUS_LENGTH);
            }
            return 0;
        }

        if (status_vector) {
            status_vector[0] = 1;
            status_vector[1] = 0;
        }

        return 1;

    } catch (const Firebird::FbException& e) {
        if (status_vector) {
            const ISC_STATUS* errors = e.getStatus()->getErrors();
            if (errors) {
                for (size_t i = 0; i < ISC_STATUS_LENGTH; ++i) {
                    status_vector[i] = errors[i];
                    if (errors[i] == isc_arg_end) break;
                }
            }
        }
        return 0;
    } catch (...) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_random;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }
}

extern "C" int fbbatch_close(
    void* master_ptr,
    void* batch_wrapper,
    ISC_STATUS* status_vector
) {
    if (!batch_wrapper) {
        if (status_vector) {
            status_vector[0] = 1;
            status_vector[1] = 0;
        }
        return 1;
    }

    auto* wrapper = static_cast<BatchWrapper*>(batch_wrapper);

    /* CRITICAL: Do NOT call wrapper->batch->close() here!
     * When this function is called during PHP shutdown (from _php_fbird_free_batch),
     * the IBatch handle may already be invalid - Firebird internally frees batch
     * handles when the associated transaction commits/rollbacks.
     * Calling close() on an invalid handle causes SIGSEGV (si_addr=0x2c4).
     * 
     * Instead, just NULL the pointer and delete the wrapper.
     * Firebird has already cleaned up the batch resources via transaction end.
     * Fixes: Segfault in batch_edge_cases.phpt during shutdown. */
    wrapper->batch = nullptr;

    delete wrapper;

    if (status_vector) {
        status_vector[0] = 1;
        status_vector[1] = 0;
    }

    return 1;
}

extern "C" void* fbbatch_get_metadata(
    void* master_ptr,
    void* batch_wrapper,
    ISC_STATUS* status_vector
) {
    if (!master_ptr || !batch_wrapper) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_req_handle;
            status_vector[2] = isc_arg_end;
        }
        return nullptr;
    }

    auto* wrapper = static_cast<BatchWrapper*>(batch_wrapper);

    if (!wrapper->batch) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_req_handle;
            status_vector[2] = isc_arg_end;
        }
        return nullptr;
    }

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);

    try {
        Firebird::IStatus* raw_status = master->getStatus();
        Firebird::CheckStatusWrapper status(raw_status);

        Firebird::IMessageMetadata* metadata = wrapper->batch->getMetadata(&status);

        if (fb::statusHasError(raw_status) || !metadata) {
            if (status_vector) {
                copy_status_vector(raw_status->getErrors(), ISC_STATUS_LENGTH, status_vector, ISC_STATUS_LENGTH);
            }
            return nullptr;
        }

        if (status_vector) {
            status_vector[0] = 1;
            status_vector[1] = 0;
        }

        return metadata;

    } catch (const Firebird::FbException& e) {
        if (status_vector) {
            const ISC_STATUS* errors = e.getStatus()->getErrors();
            if (errors) {
                for (size_t i = 0; i < ISC_STATUS_LENGTH; ++i) {
                    status_vector[i] = errors[i];
                    if (errors[i] == isc_arg_end) break;
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

extern "C" unsigned fbbatch_get_blob_alignment(
    void* master_ptr,
    void* batch_wrapper,
    ISC_STATUS* status_vector
) {
    if (!master_ptr || !batch_wrapper) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_req_handle;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }

    auto* wrapper = static_cast<BatchWrapper*>(batch_wrapper);

    if (!wrapper->batch) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_req_handle;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);

    try {
        Firebird::IStatus* raw_status = master->getStatus();
        Firebird::CheckStatusWrapper status(raw_status);

        unsigned alignment = wrapper->batch->getBlobAlignment(&status);

        if (fb::statusHasError(raw_status)) {
            if (status_vector) {
                copy_status_vector(raw_status->getErrors(), ISC_STATUS_LENGTH, status_vector, ISC_STATUS_LENGTH);
            }
            return 0;
        }

        if (status_vector) {
            status_vector[0] = 1;
            status_vector[1] = 0;
        }

        return alignment;

    } catch (const Firebird::FbException& e) {
        if (status_vector) {
            const ISC_STATUS* errors = e.getStatus()->getErrors();
            if (errors) {
                for (size_t i = 0; i < ISC_STATUS_LENGTH; ++i) {
                    status_vector[i] = errors[i];
                    if (errors[i] == isc_arg_end) break;
                }
            }
        }
        return 0;
    } catch (...) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_random;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }
}

/* =============================================================================
 * IBatch BLOB Handling Functions
 * ============================================================================= */

extern "C" int fbbatch_add_blob(
    void* master_ptr,
    void* batch_wrapper,
    unsigned length,
    const void* data,
    ISC_QUAD* blob_id_out,
    unsigned bpb_length,
    const unsigned char* bpb,
    ISC_STATUS* status_vector
) {
    if (!master_ptr || !batch_wrapper || !blob_id_out) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_req_handle;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* wrapper = static_cast<BatchWrapper*>(batch_wrapper);

    if (!wrapper->batch) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_req_handle;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }

    try {
        Firebird::IStatus* raw_status = master->getStatus();
        Firebird::CheckStatusWrapper status(raw_status);

        // Add BLOB to batch using IBatch::addBlob()
        // addBlob() modifies blob_id_out in-place
        wrapper->batch->addBlob(
            &status,
            length,
            data,
            blob_id_out,
            bpb_length,
            bpb
        );

        if (fb::statusHasError(raw_status)) {
            if (status_vector) {
                copy_status_vector(raw_status->getErrors(), ISC_STATUS_LENGTH, status_vector, ISC_STATUS_LENGTH);
            }
            return 0;
        }

        if (status_vector) {
            status_vector[0] = 1;
            status_vector[1] = 0;
        }

        return 1;

    } catch (const Firebird::FbException& e) {
        if (status_vector) {
            const ISC_STATUS* errors = e.getStatus()->getErrors();
            if (errors) {
                for (size_t i = 0; i < ISC_STATUS_LENGTH; ++i) {
                    status_vector[i] = errors[i];
                    if (errors[i] == isc_arg_end) break;
                }
            }
        }
        return 0;
    } catch (...) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_random;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }
}

extern "C" int fbbatch_append_blob_data(
    void* master_ptr,
    void* batch_wrapper,
    unsigned length,
    const void* data,
    ISC_STATUS* status_vector
) {
    if (!master_ptr || !batch_wrapper) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_req_handle;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* wrapper = static_cast<BatchWrapper*>(batch_wrapper);

    if (!wrapper->batch) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_req_handle;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }

    try {
        Firebird::IStatus* raw_status = master->getStatus();
        Firebird::CheckStatusWrapper status(raw_status);

        // Append data to the current BLOB being constructed
        wrapper->batch->appendBlobData(&status, length, data);

        if (fb::statusHasError(raw_status)) {
            if (status_vector) {
                copy_status_vector(raw_status->getErrors(), ISC_STATUS_LENGTH, status_vector, ISC_STATUS_LENGTH);
            }
            return 0;
        }

        if (status_vector) {
            status_vector[0] = 1;
            status_vector[1] = 0;
        }

        return 1;

    } catch (const Firebird::FbException& e) {
        if (status_vector) {
            const ISC_STATUS* errors = e.getStatus()->getErrors();
            if (errors) {
                for (size_t i = 0; i < ISC_STATUS_LENGTH; ++i) {
                    status_vector[i] = errors[i];
                    if (errors[i] == isc_arg_end) break;
                }
            }
        }
        return 0;
    } catch (...) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_random;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }
}

extern "C" int fbbatch_add_blob_stream(
    void* master_ptr,
    void* batch_wrapper,
    unsigned length,
    const void* data,
    ISC_STATUS* status_vector
) {
    if (!master_ptr || !batch_wrapper) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_req_handle;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* wrapper = static_cast<BatchWrapper*>(batch_wrapper);

    if (!wrapper->batch) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_req_handle;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }

    try {
        Firebird::IStatus* raw_status = master->getStatus();
        Firebird::CheckStatusWrapper status(raw_status);

        // Stream BLOB data using addBlobStream
        wrapper->batch->addBlobStream(&status, length, data);

        if (fb::statusHasError(raw_status)) {
            if (status_vector) {
                copy_status_vector(raw_status->getErrors(), ISC_STATUS_LENGTH, status_vector, ISC_STATUS_LENGTH);
            }
            return 0;
        }

        if (status_vector) {
            status_vector[0] = 1;
            status_vector[1] = 0;
        }

        return 1;

    } catch (const Firebird::FbException& e) {
        if (status_vector) {
            const ISC_STATUS* errors = e.getStatus()->getErrors();
            if (errors) {
                for (size_t i = 0; i < ISC_STATUS_LENGTH; ++i) {
                    status_vector[i] = errors[i];
                    if (errors[i] == isc_arg_end) break;
                }
            }
        }
        return 0;
    } catch (...) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_random;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }
}

extern "C" int fbbatch_register_blob(
    void* master_ptr,
    void* batch_wrapper,
    const ISC_QUAD* existing_blob,
    ISC_QUAD* batch_blob_id,
    ISC_STATUS* status_vector
) {
    if (!master_ptr || !batch_wrapper || !existing_blob || !batch_blob_id) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_req_handle;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* wrapper = static_cast<BatchWrapper*>(batch_wrapper);

    if (!wrapper->batch) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_req_handle;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }

    try {
        Firebird::IStatus* raw_status = master->getStatus();
        Firebird::CheckStatusWrapper status(raw_status);

        // Register an existing BLOB for use in the batch
        // registerBlob() modifies batch_blob_id in-place
        wrapper->batch->registerBlob(&status, existing_blob, batch_blob_id);

        if (fb::statusHasError(raw_status)) {
            if (status_vector) {
                copy_status_vector(raw_status->getErrors(), ISC_STATUS_LENGTH, status_vector, ISC_STATUS_LENGTH);
            }
            return 0;
        }

        if (status_vector) {
            status_vector[0] = 1;
            status_vector[1] = 0;
        }

        return 1;

    } catch (const Firebird::FbException& e) {
        if (status_vector) {
            const ISC_STATUS* errors = e.getStatus()->getErrors();
            if (errors) {
                for (size_t i = 0; i < ISC_STATUS_LENGTH; ++i) {
                    status_vector[i] = errors[i];
                    if (errors[i] == isc_arg_end) break;
                }
            }
        }
        return 0;
    } catch (...) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_random;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }
}

extern "C" int fbbatch_set_default_bpb(
    void* master_ptr,
    void* batch_wrapper,
    unsigned bpb_length,
    const unsigned char* bpb,
    ISC_STATUS* status_vector
) {
    if (!master_ptr || !batch_wrapper) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_req_handle;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* wrapper = static_cast<BatchWrapper*>(batch_wrapper);

    if (!wrapper->batch) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_req_handle;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }

    try {
        Firebird::IStatus* raw_status = master->getStatus();
        Firebird::CheckStatusWrapper status(raw_status);

        // Set default BPB for BLOB operations
        wrapper->batch->setDefaultBpb(&status, bpb_length, bpb);

        if (fb::statusHasError(raw_status)) {
            if (status_vector) {
                copy_status_vector(raw_status->getErrors(), ISC_STATUS_LENGTH, status_vector, ISC_STATUS_LENGTH);
            }
            return 0;
        }

        if (status_vector) {
            status_vector[0] = 1;
            status_vector[1] = 0;
        }

        return 1;

    } catch (const Firebird::FbException& e) {
        if (status_vector) {
            const ISC_STATUS* errors = e.getStatus()->getErrors();
            if (errors) {
                for (size_t i = 0; i < ISC_STATUS_LENGTH; ++i) {
                    status_vector[i] = errors[i];
                    if (errors[i] == isc_arg_end) break;
                }
            }
        }
        return 0;
    } catch (...) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_random;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }
}

/* =============================================================================
 * IBatch Detailed Error Reporting Functions
 * ============================================================================= */

extern "C" int fbbatch_execute_detailed(
    void* master_ptr,
    void* batch_wrapper,
    void* transaction_ptr,
    fbbatch_completion_result* result,
    ISC_STATUS* status_vector
) {
    if (!master_ptr || !batch_wrapper || !transaction_ptr || !result) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_req_handle;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }

    // Initialize result
    result->total_count = 0;
    result->success_count = 0;
    result->error_count = 0;
    result->errors = nullptr;

    auto* master = static_cast<Firebird::IMaster*>(master_ptr);
    auto* wrapper = static_cast<BatchWrapper*>(batch_wrapper);
    auto* transaction = static_cast<Firebird::ITransaction*>(transaction_ptr);

    if (!wrapper->batch) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_req_handle;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }

    try {
        Firebird::IStatus* raw_status = master->getStatus();
        Firebird::CheckStatusWrapper status(raw_status);

        // Execute the batch
        Firebird::IBatchCompletionState* completion = wrapper->batch->execute(&status, transaction);

        if (fb::statusHasError(raw_status)) {
            if (status_vector) {
                copy_status_vector(raw_status->getErrors(), ISC_STATUS_LENGTH, status_vector, ISC_STATUS_LENGTH);
            }
            if (completion) {
                completion->dispose();
            }
            return 0;
        }

        if (!completion) {
            if (status_vector) {
                status_vector[0] = 1;
                status_vector[1] = 0;
            }
            return 1;
        }

        // Get total count
        unsigned total = completion->getSize(&status);
        result->total_count = total;

        // First pass: count errors
        unsigned error_count = 0;
        for (unsigned i = 0; i < total; ++i) {
            int state = completion->getState(&status, i);
            if (state == Firebird::IBatchCompletionState::EXECUTE_FAILED) {
                error_count++;
            }
        }

        result->error_count = error_count;
        result->success_count = total - error_count;

        // Second pass: collect error details if any
        if (error_count > 0) {
            result->errors = static_cast<fbbatch_error_entry*>(
                calloc(error_count, sizeof(fbbatch_error_entry))
            );

            if (result->errors) {
                unsigned error_idx = 0;
                unsigned search_pos = 0;

                while (error_idx < error_count) {
                    // Find next error position
                    unsigned error_pos = completion->findError(&status, search_pos);
                    if (error_pos == static_cast<unsigned>(Firebird::IBatchCompletionState::NO_MORE_ERRORS)) {
                        break;  // No more errors
                    }

                    fbbatch_error_entry* entry = &result->errors[error_idx];
                    entry->position = error_pos;
                    entry->state = FBBATCH_EXECUTE_FAILED;

                    // Get error status for this position
                    Firebird::IStatus* error_status = master->getStatus();
                    completion->getStatus(&status, error_status, error_pos);

                    // Extract SQLSTATE from the status (stored in errors array)
                    // The errors array may contain isc_arg_sql_state followed by the state string
                    const ISC_STATUS* errors_vec = error_status->getErrors();
                    bool found_sqlstate = false;
                    if (errors_vec) {
                        for (int i = 0; errors_vec[i] != isc_arg_end && i < ISC_STATUS_LENGTH - 1; ++i) {
                            if (errors_vec[i] == isc_arg_sql_state && errors_vec[i + 1] != 0) {
                                const char* sqlstate_str = reinterpret_cast<const char*>(errors_vec[i + 1]);
                                if (sqlstate_str && strlen(sqlstate_str) >= 5) {
                                    strncpy(entry->sqlstate, sqlstate_str, 5);
                                    entry->sqlstate[5] = '\0';
                                    found_sqlstate = true;
                                    break;
                                }
                            }
                        }
                    }
                    if (!found_sqlstate) {
                        strcpy(entry->sqlstate, "HY000");  // Default SQLSTATE
                    }

                    // Build error message from status vector
                    const ISC_STATUS* errors = error_status->getErrors();
                    if (errors && errors[0] != 0) {
                        // Use Firebird's IUtil to interpret the status
                        Firebird::IUtil* util = master->getUtilInterface();
                        if (util) {
                            char msg_buffer[1024] = {0};
                            unsigned msg_len = util->formatStatus(msg_buffer, sizeof(msg_buffer) - 1, error_status);
                            if (msg_len > 0) {
                                entry->message = strdup(msg_buffer);
                            }
                        }

                        // Fallback if formatStatus didn't work
                        if (!entry->message) {
                            char fallback[256];
                            snprintf(fallback, sizeof(fallback), "Batch execution failed at row %u", error_pos);
                            entry->message = strdup(fallback);
                        }
                    } else {
                        char fallback[256];
                        snprintf(fallback, sizeof(fallback), "Batch execution failed at row %u", error_pos);
                        entry->message = strdup(fallback);
                    }

                    error_status->dispose();
                    error_idx++;
                    search_pos = error_pos + 1;  // Continue searching from next position
                }

                // Update error_count in case we got fewer than expected
                result->error_count = error_idx;
                result->success_count = total - error_idx;
            }
        }

        completion->dispose();

        if (status_vector) {
            status_vector[0] = 1;
            status_vector[1] = 0;
        }

        return 1;

    } catch (const Firebird::FbException& e) {
        if (status_vector) {
            const ISC_STATUS* errors = e.getStatus()->getErrors();
            if (errors) {
                for (size_t i = 0; i < ISC_STATUS_LENGTH; ++i) {
                    status_vector[i] = errors[i];
                    if (errors[i] == isc_arg_end) break;
                }
            }
        }
        return 0;
    } catch (...) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_random;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }
}

extern "C" void fbbatch_free_errors(fbbatch_error_entry* errors, unsigned count) {
    if (!errors) {
        return;
    }

    for (unsigned i = 0; i < count; ++i) {
        if (errors[i].message) {
            free(errors[i].message);
            errors[i].message = nullptr;
        }
    }

    free(errors);
}

extern "C" void fbbatch_free_result(fbbatch_completion_result* result) {
    if (!result) {
        return;
    }

    if (result->errors) {
        fbbatch_free_errors(result->errors, result->error_count);
        result->errors = nullptr;
    }

    result->total_count = 0;
    result->success_count = 0;
    result->error_count = 0;
}

#endif // FB_API_VER >= 40 (IBatch API)
