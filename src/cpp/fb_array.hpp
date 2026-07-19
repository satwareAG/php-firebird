/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

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

#include "fb_blr_compat.h"
#include "fb_status.hpp"

// Charset ids for BLR *2 types (blr_text2/blr_varying2).
// 0 = NONE/ASCII in Firebird.
#ifndef FBIRD_ARRAY_DEFAULT_CHARSET_ID
#define FBIRD_ARRAY_DEFAULT_CHARSET_ID 0
#endif

// SDL constants (Slice Description Language)
// Values from Firebird source: jrd/sdl.cpp op_* constants
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
#define isc_sdl_struct      6   // was incorrectly 9
#endif
#ifndef isc_sdl_variable
#define isc_sdl_variable    7   // was incorrectly 15
#endif
#ifndef isc_sdl_scalar
#define isc_sdl_scalar      8   // was incorrectly 18
#endif
#ifndef isc_sdl_tiny_integer
#define isc_sdl_tiny_integer 9  // was incorrectly 19
#endif
#ifndef isc_sdl_long_integer
#define isc_sdl_long_integer 11 // was incorrectly 26
#endif
#ifndef isc_sdl_short_integer
#define isc_sdl_short_integer 10
#endif
#ifndef isc_sdl_do1
#define isc_sdl_do1         35
#endif
#ifndef isc_sdl_do2
#define isc_sdl_do2         34
#endif
#ifndef isc_sdl_do3
#define isc_sdl_do3         33
#endif
#ifndef isc_sdl_element
#define isc_sdl_element     36  // was incorrectly 14
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
        set_status_error(status_vector, isc_bad_req_handle);
        return false;
    }

    // Build SDL from descriptor
    unsigned char sdl_buffer[1024];
    unsigned sdl_length = 0;

    if (!buildSdlFromDesc(desc, sdl_buffer, &sdl_length)) {
        set_status_error(status_vector, isc_random);
        return false;
    }

    try {
        // jane: fixed IStatus leak — was CheckStatusWrapper, now CheckStatusScope (RAII)


        // Call IAttachment::getSlice
        int result = attachment->getSlice(
            check_status.get(),
            transaction,
            array_id,
            static_cast<unsigned>(sdl_length),
            sdl_buffer,
            0,       // param_length (unused for simple slices)
            nullptr, // param_buffer (unused for simple slices)
            static_cast<int>(*buffer_length),
            static_cast<unsigned char*>(buffer)
        );


        if (check_status.hasError()) {
            if (status_vector) {
                copyStatus(check_status.status(), status_vector);
            }
            
            return false;
        }

        // Update buffer_length with actual bytes read
        *buffer_length = static_cast<ISC_LONG>(result);
        
        return true;

    } catch (...) {
        set_status_error(status_vector, isc_except2);
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
        set_status_error(status_vector, isc_bad_req_handle);
        return false;
    }

    // Build SDL from descriptor
    unsigned char sdl_buffer[1024];
    unsigned sdl_length = 0;

    if (!buildSdlFromDesc(desc, sdl_buffer, &sdl_length)) {
        set_status_error(status_vector, isc_random);
        return false;
    }

    try {
        // jane: fixed IStatus leak — was CheckStatusWrapper(raw_status), now CheckStatusScope (RAII)
        fb::CheckStatusScope check_status(master);

        // Call IAttachment::putSlice
        attachment->putSlice(
            check_status.get(),
            transaction,
            array_id,
            static_cast<unsigned>(sdl_length),
            sdl_buffer,
            0,       // param_length (unused for simple slices)
            nullptr, // param_buffer (unused for simple slices)
            static_cast<int>(buffer_length),
            static_cast<unsigned char*>(const_cast<void*>(buffer))
        );

        if (check_status.hasError()) {
            if (status_vector) {
                copyStatus(check_status.status(), status_vector);
            }
            return false;
        }
        return true;

    } catch (...) {
        set_status_error(status_vector, isc_except2);
        return false;
    }
}

/**
 * Build SDL (Slice Description Language) from ISC_ARRAY_DESC.
 *
 * SDL format for whole-array operations:
 * - Version byte (isc_sdl_version1)
 * - Struct definition (isc_sdl_struct, count)
 *   - Scalar type (isc_sdl_scalar, index, dtype)
 * - Relation name (isc_sdl_relation)
 * - Field name (isc_sdl_field)
 * - Dimension loop (isc_sdl_do2, dim, lower, upper)
 * - Element reference (isc_sdl_element, dims, scalar)
 *   - Scalar (isc_sdl_scalar, 0, dtype)
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

    const auto stuffSdlWord = [&](ISC_USHORT word) {
        *sdl++ = static_cast<unsigned char>(word & 0xFF);
        *sdl++ = static_cast<unsigned char>((word >> 8) & 0xFF);
    };

    const auto stuffLiteral = [&](ISC_LONG literal) {
        if (literal >= -128 && literal <= 127) {
            *sdl++ = isc_sdl_tiny_integer;
            *sdl++ = static_cast<unsigned char>(static_cast<int8_t>(literal));
            return;
        }

        if (literal >= -32768 && literal <= 32767) {
            *sdl++ = isc_sdl_short_integer;
            *sdl++ = static_cast<unsigned char>(literal & 0xFF);
            *sdl++ = static_cast<unsigned char>((literal >> 8) & 0xFF);
            return;
        }

        *sdl++ = isc_sdl_long_integer;
        *sdl++ = static_cast<unsigned char>(literal & 0xFF);
        *sdl++ = static_cast<unsigned char>((literal >> 8) & 0xFF);
        *sdl++ = static_cast<unsigned char>((literal >> 16) & 0xFF);
        *sdl++ = static_cast<unsigned char>((literal >> 24) & 0xFF);
    };

    // Canonical layout matches Firebird `isc_array_gen_sdl()` (src/yvalve/array.cpp).

    // 1) Version
    *sdl++ = isc_sdl_version1;

    // 2) Element descriptor: struct(1) + blr dtype [+ scale/len]
    //
    // IMPORTANT:
    // - For arrays of VARCHAR/CHAR with charset info, Firebird supports *2 types:
    //   blr_text2 / blr_varying2 / blr_cstring2 where the BLR stream includes
    //   a charset id (word) before the length (word).
    // - Our descriptor (ISC_ARRAY_DESC) does not carry charset id, but Firebird
    //   uses this information from field metadata.
    //
    // To match Firebird's expectations for varying arrays (and avoid silent
    // corruption where length is preserved but bytes are zeroed), we emit the
    // *2 BLR codes with a default charset id (NONE = 0) when dealing with
    // blr_varying.
    *sdl++ = isc_sdl_struct;
    *sdl++ = 1;

    const unsigned char dtype = static_cast<unsigned char>(desc->array_desc_dtype);

    if (dtype == blr_varying) {
        /*
         * VARCHAR arrays: Use blr_cstring format for user buffer.
         *
         * BACKGROUND: Firebird's sdl_desc() sets dtype_cstring for blr_varying,
         * but internal array storage uses IBVARY (2-byte length + data).
         * This mismatch corrupts data when MOV_move interprets IBVARY as null-terminated.
         *
         * WORKAROUND: Use blr_cstring explicitly in SDL. This tells Firebird to:
         * - Accept null-terminated strings in the user buffer (putSlice)
         * - Return null-terminated strings to the user buffer (getSlice)
         * - Handle IBVARY conversion internally
         *
         * The max length for blr_cstring = declared_length + 1 (for null terminator)
         */
        *sdl++ = blr_cstring;
        /* Length = max chars + 1 for null terminator */
        stuffSdlWord(static_cast<ISC_USHORT>(desc->array_desc_length + 1));
    } else if (dtype == blr_varying2) {
        /*
         * VARCHAR with charset: Use blr_cstring2
         */
        const ISC_USHORT charset_id = static_cast<ISC_USHORT>(desc->array_desc_flags);
        *sdl++ = blr_cstring2;
        stuffSdlWord(charset_id);
        stuffSdlWord(static_cast<ISC_USHORT>(desc->array_desc_length + 1));
    } else {
        *sdl++ = dtype;

        switch (dtype) {
        case blr_short:
        case blr_long:
        case blr_int64:
        case blr_quad:
        case blr_int128:
            *sdl++ = static_cast<unsigned char>(desc->array_desc_scale);
            break;

        case blr_text:
        case blr_cstring:
        case blr_varying:
            stuffSdlWord(static_cast<ISC_USHORT>(desc->array_desc_length));
            break;

        default:
            break;
        }
    }

    // 3) Relation
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

    // 4) Field
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

    // 5) Dimension loops
    for (unsigned short dim = 0; dim < desc->array_desc_dimensions; dim++) {
        const ISC_LONG lower = desc->array_desc_bounds[dim].array_bound_lower;
        const ISC_LONG upper = desc->array_desc_bounds[dim].array_bound_upper;

        if (lower == 1) {
            *sdl++ = isc_sdl_do1;
            *sdl++ = static_cast<unsigned char>(dim);
        } else {
            *sdl++ = isc_sdl_do2;
            *sdl++ = static_cast<unsigned char>(dim);
            stuffLiteral(lower);
        }

        stuffLiteral(upper);
    }

    // 6) Element expression
    // Canonical layout per Firebird's `gen_sdl` in `src/yvalve/array.cpp`:
    //   isc_sdl_element, 1, isc_sdl_scalar, 0, <dims>, isc_sdl_variable, <dim>...
    *sdl++ = isc_sdl_element;
    *sdl++ = 1;
    *sdl++ = isc_sdl_scalar;
    *sdl++ = 0;
    *sdl++ = static_cast<unsigned char>(desc->array_desc_dimensions);

    for (unsigned short dim = 0; dim < desc->array_desc_dimensions; dim++) {
        *sdl++ = isc_sdl_variable;
        *sdl++ = static_cast<unsigned char>(dim);
    }

    // 7) End
    *sdl++ = isc_sdl_eoc;

    *sdl_length = static_cast<unsigned>(sdl - sdl_buffer);


    return true;
}

} // namespace fb

#endif // FB_ARRAY_HPP
