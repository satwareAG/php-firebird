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

#ifndef FB_ARRAY_HPP
#define FB_ARRAY_HPP

/**
 * @file fb_array.hpp
 * @brief Array slice operations for Firebird OO API
 *
 * This file provides C interop functions for array operations using
 * the Firebird C++ Object-Oriented API. Array operations in Firebird
 * involve:
 *
 * 1. isc_array_lookup_bounds() → Query system tables for array metadata
 *    Note: No direct OO API equivalent - uses SQL query against RDB$FIELD_DIMENSIONS
 *
 * 2. isc_array_get_slice() → IAttachment::getSlice() (FB 3.0+)
 *    Read array data from database
 *
 * 3. isc_array_put_slice() → IAttachment::putSlice() (FB 3.0+)
 *    Write array data to database
 *
 * Unlike blobs/transactions, arrays don't need a persistent wrapper object -
 * operations are atomic through IAttachment methods.
 *
 * @see MODERNIZATION_PLAN_FB3_TO_FB5.md Phase 9: Array OO API Wrapper
 */

#include <firebird/Interface.h>
#include <ibase.h>
#include <cstring>
#include <cstdint>

#include "fb_status.hpp"

// SDL constants (Slice Description Language)
// These are internal Firebird constants for array slice operations
#ifndef isc_sdl_version1
#define isc_sdl_version1    1
#endif
#ifndef isc_sdl_eoc
#define isc_sdl_eoc         255
#endif
#ifndef isc_sdl_relation
#define isc_sdl_relation    2
#endif
#ifndef isc_sdl_field
#define isc_sdl_field       4
#endif
#ifndef isc_sdl_struct
#define isc_sdl_struct      9
#endif
#ifndef isc_sdl_do2
#define isc_sdl_do2         12
#endif
#ifndef isc_sdl_element
#define isc_sdl_element     14
#endif
#ifndef isc_sdl_variable
#define isc_sdl_variable    15
#endif
#ifndef isc_sdl_scalar
#define isc_sdl_scalar      18
#endif
#ifndef isc_sdl_tiny_integer
#define isc_sdl_tiny_integer 19
#endif
#ifndef isc_sdl_long_integer
#define isc_sdl_long_integer 26
#endif

namespace fb {

/**
 * Array slice operations using Firebird OO API.
 *
 * These are stateless utility functions that operate through IAttachment.
 * Unlike blobs or statements, array operations don't maintain persistent
 * state - each operation is atomic.
 */
class ArrayUtils {
public:
    /**
     * Get an array slice from the database using OO API.
     *
     * @param master IMaster interface
     * @param attachment IAttachment pointer (from fbc_get_attachment)
     * @param transaction ITransaction pointer (from fbt_get_transaction)
     * @param array_id The ISC_QUAD array identifier
     * @param desc Array descriptor (ISC_ARRAY_DESC)
     * @param buffer Output buffer to receive array data
     * @param buffer_length Buffer length (updated with actual bytes read)
     * @param status_vector Output status vector for errors
     * @return true on success, false on failure
     */
    static bool getSlice(
        Firebird::IMaster* master,
        Firebird::IAttachment* attachment,
        Firebird::ITransaction* transaction,
        ISC_QUAD* array_id,
        const ISC_ARRAY_DESC* desc,
        void* buffer,
        ISC_LONG* buffer_length,
        ISC_STATUS* status_vector
    ) noexcept;

    /**
     * Put an array slice to the database using OO API.
     *
     * @param master IMaster interface
     * @param attachment IAttachment pointer (from fbc_get_attachment)
     * @param transaction ITransaction pointer (from fbt_get_transaction)
     * @param array_id The ISC_QUAD array identifier (output for new arrays)
     * @param desc Array descriptor (ISC_ARRAY_DESC)
     * @param buffer Input buffer containing array data
     * @param buffer_length Buffer length
     * @param status_vector Output status vector for errors
     * @return true on success, false on failure
     */
    static bool putSlice(
        Firebird::IMaster* master,
        Firebird::IAttachment* attachment,
        Firebird::ITransaction* transaction,
        ISC_QUAD* array_id,
        const ISC_ARRAY_DESC* desc,
        const void* buffer,
        ISC_LONG buffer_length,
        ISC_STATUS* status_vector
    ) noexcept;

private:
    /**
     * Build an SDLBuffer from ISC_ARRAY_DESC.
     *
     * SDL (Slice Description Language) is used by getSlice/putSlice to
     * describe the array structure. This helper converts the legacy
     * ISC_ARRAY_DESC format to SDL.
     *
     * @param desc Array descriptor
     * @param sdl_buffer Output buffer for SDL
     * @param sdl_length Output: actual SDL length
     * @return true on success
     */
    static bool buildSdlFromDesc(
        const ISC_ARRAY_DESC* desc,
        unsigned char* sdl_buffer,
        unsigned* sdl_length
    ) noexcept;

    /**
     * Helper to copy status from IStatus to ISC_STATUS array.
     */
    static void copyStatus(Firebird::IStatus* status, ISC_STATUS* status_vector) noexcept {
        if (!status || !status_vector) return;

        // Get errors from IStatus
        const ISC_STATUS* errors = status->getErrors();
        if (errors) {
            // Copy status vector (up to first isc_arg_end)
            size_t i = 0;
            while (i < ISC_STATUS_LENGTH - 1 && errors[i] != isc_arg_end) {
                status_vector[i] = errors[i];
                i++;
            }
            status_vector[i] = isc_arg_end;
        } else {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = 0;
            status_vector[2] = isc_arg_end;
        }
    }
};

// =============================================================================
// Implementation
// =============================================================================

inline bool ArrayUtils::getSlice(
    Firebird::IMaster* master,
    Firebird::IAttachment* attachment,
    Firebird::ITransaction* transaction,
    ISC_QUAD* array_id,
    const ISC_ARRAY_DESC* desc,
    void* buffer,
    ISC_LONG* buffer_length,
    ISC_STATUS* status_vector
) noexcept {
    if (!master || !attachment || !transaction || !array_id || !desc || !buffer || !buffer_length) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_req_handle;
            status_vector[2] = isc_arg_end;
        }
        return false;
    }

    // Build SDL from descriptor
    unsigned char sdl_buffer[1024];
    unsigned sdl_length = 0;

    if (!buildSdlFromDesc(desc, sdl_buffer, &sdl_length)) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_random; // Generic error for SDL build failure
            status_vector[2] = isc_arg_end;
        }
        return false;
    }

    try {
        // Use CheckStatusWrapper for Firebird template API
        Firebird::IStatus* raw_status = master->getStatus();
        Firebird::CheckStatusWrapper check_status(raw_status);

        // Call IAttachment::getSlice
        int result = attachment->getSlice(
            &check_status,
            transaction,
            array_id,
            static_cast<unsigned>(sdl_length),
            sdl_buffer,
            0,       // param_length (unused for simple slices)
            nullptr, // param_buffer (unused for simple slices)
            static_cast<int>(*buffer_length),
            static_cast<unsigned char*>(buffer)
        );

        if (statusHasError(raw_status)) {
            if (status_vector) {
                copyStatus(raw_status, status_vector);
            }
            raw_status->dispose();
            return false;
        }

        // Update buffer_length with actual bytes read
        *buffer_length = static_cast<ISC_LONG>(result);
        raw_status->dispose();
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

inline bool ArrayUtils::putSlice(
    Firebird::IMaster* master,
    Firebird::IAttachment* attachment,
    Firebird::ITransaction* transaction,
    ISC_QUAD* array_id,
    const ISC_ARRAY_DESC* desc,
    const void* buffer,
    ISC_LONG buffer_length,
    ISC_STATUS* status_vector
) noexcept {
    if (!master || !attachment || !transaction || !array_id || !desc || !buffer) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_bad_req_handle;
            status_vector[2] = isc_arg_end;
        }
        return false;
    }

    // Build SDL from descriptor
    unsigned char sdl_buffer[1024];
    unsigned sdl_length = 0;

    if (!buildSdlFromDesc(desc, sdl_buffer, &sdl_length)) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_random; // Generic error for SDL build failure
            status_vector[2] = isc_arg_end;
        }
        return false;
    }

    try {
        // Use CheckStatusWrapper for Firebird template API
        Firebird::IStatus* raw_status = master->getStatus();
        Firebird::CheckStatusWrapper check_status(raw_status);

        // Call IAttachment::putSlice
        attachment->putSlice(
            &check_status,
            transaction,
            array_id,
            static_cast<unsigned>(sdl_length),
            sdl_buffer,
            0,       // param_length (unused for simple slices)
            nullptr, // param_buffer (unused for simple slices)
            static_cast<int>(buffer_length),
            static_cast<unsigned char*>(const_cast<void*>(buffer))
        );

        if (statusHasError(raw_status)) {
            if (status_vector) {
                copyStatus(raw_status, status_vector);
            }
            raw_status->dispose();
            return false;
        }

        raw_status->dispose();
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
 * Build SDL (Slice Description Language) from ISC_ARRAY_DESC.
 *
 * SDL format (simplified for whole-array operations):
 * - Version byte (isc_sdl_version1)
 * - Struct info (isc_sdl_struct, 1 element)
 * - Relation name (isc_sdl_relation)
 * - Field name (isc_sdl_field)
 * - Dimension loop (isc_sdl_do2, isc_sdl_variable, lower, upper)
 * - Element reference (isc_sdl_element, 1)
 *   - Variable reference for each dimension
 * - End (isc_sdl_eoc)
 */
inline bool ArrayUtils::buildSdlFromDesc(
    const ISC_ARRAY_DESC* desc,
    unsigned char* sdl_buffer,
    unsigned* sdl_length
) noexcept {
    if (!desc || !sdl_buffer || !sdl_length) {
        return false;
    }

    unsigned char* sdl = sdl_buffer;

    // Version
    *sdl++ = isc_sdl_version1;

    // Struct with 1 element
    *sdl++ = isc_sdl_struct;
    *sdl++ = 1;

    // Data type definition using isc_sdl_scalar
    // (simplified - uses scalar for basic type, then actual type byte)
    *sdl++ = isc_sdl_scalar;
    *sdl++ = 0;  // Index 0 in struct
    *sdl++ = desc->array_desc_dtype;

    // Relation name
    unsigned char rname_len = 0;
    while (rname_len < sizeof(desc->array_desc_relation_name) &&
           desc->array_desc_relation_name[rname_len] &&
           desc->array_desc_relation_name[rname_len] != ' ') {
        rname_len++;
    }
    *sdl++ = isc_sdl_relation;
    *sdl++ = rname_len;
    std::memcpy(sdl, desc->array_desc_relation_name, rname_len);
    sdl += rname_len;

    // Field name
    unsigned char fname_len = 0;
    while (fname_len < sizeof(desc->array_desc_field_name) &&
           desc->array_desc_field_name[fname_len] &&
           desc->array_desc_field_name[fname_len] != ' ') {
        fname_len++;
    }
    *sdl++ = isc_sdl_field;
    *sdl++ = fname_len;
    std::memcpy(sdl, desc->array_desc_field_name, fname_len);
    sdl += fname_len;

    // Dimension loops (for each dimension)
    for (unsigned short dim = 0; dim < desc->array_desc_dimensions; dim++) {
        ISC_LONG lower = desc->array_desc_bounds[dim].array_bound_lower;
        ISC_LONG upper = desc->array_desc_bounds[dim].array_bound_upper;

        *sdl++ = isc_sdl_do2;
        *sdl++ = static_cast<unsigned char>(dim);

        // Lower bound
        if (lower >= -128 && lower <= 127) {
            *sdl++ = isc_sdl_tiny_integer;
            *sdl++ = static_cast<unsigned char>(lower);
        } else {
            *sdl++ = isc_sdl_long_integer;
            *sdl++ = static_cast<unsigned char>(lower & 0xFF);
            *sdl++ = static_cast<unsigned char>((lower >> 8) & 0xFF);
            *sdl++ = static_cast<unsigned char>((lower >> 16) & 0xFF);
            *sdl++ = static_cast<unsigned char>((lower >> 24) & 0xFF);
        }

        // Upper bound
        if (upper >= -128 && upper <= 127) {
            *sdl++ = isc_sdl_tiny_integer;
            *sdl++ = static_cast<unsigned char>(upper);
        } else {
            *sdl++ = isc_sdl_long_integer;
            *sdl++ = static_cast<unsigned char>(upper & 0xFF);
            *sdl++ = static_cast<unsigned char>((upper >> 8) & 0xFF);
            *sdl++ = static_cast<unsigned char>((upper >> 16) & 0xFF);
            *sdl++ = static_cast<unsigned char>((upper >> 24) & 0xFF);
        }
    }

    // Element reference
    *sdl++ = isc_sdl_element;
    *sdl++ = static_cast<unsigned char>(desc->array_desc_dimensions);
    for (unsigned short dim = 0; dim < desc->array_desc_dimensions; dim++) {
        *sdl++ = isc_sdl_variable;
        *sdl++ = static_cast<unsigned char>(dim);
    }

    // End of SDL
    *sdl++ = isc_sdl_eoc;

    *sdl_length = static_cast<unsigned>(sdl - sdl_buffer);
    return true;
}

} // namespace fb

#endif // FB_ARRAY_HPP
