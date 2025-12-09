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

        constexpr bool is_valid() const noexcept {
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

        constexpr bool is_valid() const noexcept {
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
        unsigned year, month, day;
        unsigned hours, minutes, seconds, fractions;
        std::string timeZone;

        // C++17: Enable structured binding assignment
        auto tie() const noexcept {
            return std::tie(year, month, day, hours, minutes, seconds, fractions);
        }

        bool is_valid() const noexcept {
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

            // Temporary storage for decode operation
            unsigned year, month, day, hours, minutes, seconds, fractions;
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
    int insert_aliases_modern(void* master_ptr, ISC_STATUS* status_vec, ibase_query* ib_query,
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
                _php_ibase_insert_alias(ib_query->ht_aliases, alias_str.c_str());
            }

            return 0;

        } catch (...) {
            return -1;
        }
    }
#endif // FB_API_VER >= 40
}

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

extern "C" int fbu_insert_aliases(void *master_ptr, ISC_STATUS* status, ibase_query *ib_query, void *statement_ptr)
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

#endif // FB_API_VER >= 40
