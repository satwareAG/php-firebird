/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

#ifndef FB_BLR_COMPAT_H
#define FB_BLR_COMPAT_H

/**
 * @file fb_blr_compat.h
 * @brief BLR (Binary Language Representation) constants compatibility header
 *
 * This header provides BLR constants needed for array operations.
 *
 * Firebird 4.0+ includes these in <firebird/impl/blr.h>
 * Firebird 3.0 does NOT have this header - constants are internal.
 *
 * This compatibility header allows compilation with both Firebird 3.0 and 4.0+ client libraries.
 *
 * @see https://github.com/satwareAG/php-firebird/issues/19
 */

/*
 * Try to include Firebird 4.0+ header if available.
 * The __has_include() preprocessor feature is available in:
 * - GCC 5+
 * - Clang 3.0+
 * - MSVC 2017+
 *
 * For compilers without __has_include, we fall through to fallback definitions.
 */
#ifdef __has_include
#  if __has_include(<firebird/impl/blr.h>)
#    include <firebird/impl/blr.h>
#    define FB_BLR_COMPAT_HAVE_FB4_HEADER 1
#  endif
#endif

/*
 * Fallback BLR constant definitions for Firebird 3.0 compatibility.
 *
 * These values are stable across Firebird versions (2.5, 3.0, 4.0, 5.0).
 * Source: Firebird source code src/include/firebird/impl/blr.h
 *
 * Only define if not already defined (either by Firebird 4.0+ header or ibase.h).
 */

/* Basic data types */
#ifndef blr_text
#define blr_text                14
#endif

#ifndef blr_text2
#define blr_text2               15
#endif

#ifndef blr_short
#define blr_short               7
#endif

#ifndef blr_long
#define blr_long                8
#endif

#ifndef blr_quad
#define blr_quad                9
#endif

#ifndef blr_float
#define blr_float               10
#endif

#ifndef blr_double
#define blr_double              27
#endif

#ifndef blr_d_float
#define blr_d_float             11
#endif

#ifndef blr_timestamp
#define blr_timestamp           35
#endif

#ifndef blr_varying
#define blr_varying             37
#endif

#ifndef blr_varying2
#define blr_varying2            38
#endif

#ifndef blr_blob
#define blr_blob                261
#endif

#ifndef blr_cstring
#define blr_cstring             40
#endif

#ifndef blr_cstring2
#define blr_cstring2            41
#endif

#ifndef blr_blob_id
#define blr_blob_id             45
#endif

#ifndef blr_sql_date
#define blr_sql_date            12
#endif

#ifndef blr_sql_time
#define blr_sql_time            13
#endif

#ifndef blr_int64
#define blr_int64               16
#endif

/* Firebird 4.0+ types - needed for forward compatibility */
#ifndef blr_int128
#define blr_int128              26
#endif

#ifndef blr_dec64
#define blr_dec64               24
#endif

#ifndef blr_dec128
#define blr_dec128              25
#endif

#ifndef blr_sql_time_tz
#define blr_sql_time_tz         28
#endif

#ifndef blr_timestamp_tz
#define blr_timestamp_tz        29
#endif

#ifndef blr_ex_time_tz
#define blr_ex_time_tz          30
#endif

#ifndef blr_ex_timestamp_tz
#define blr_ex_timestamp_tz     31
#endif

#ifndef blr_bool
#define blr_bool                23
#endif

/* BLR version and structural elements */
#ifndef blr_version4
#define blr_version4            4
#endif

#ifndef blr_version5
#define blr_version5            5
#endif

#ifndef blr_eoc
#define blr_eoc                 76
#endif

#ifndef blr_end
#define blr_end                 255
#endif

/* Message and parameter definitions */
#ifndef blr_message
#define blr_message             4
#endif

#ifndef blr_begin
#define blr_begin               2
#endif

#ifndef blr_send
#define blr_send                3
#endif

#ifndef blr_receive
#define blr_receive             6
#endif

#ifndef blr_loop
#define blr_loop                17
#endif

#ifndef blr_for
#define blr_for                 18
#endif

#ifndef blr_if
#define blr_if                  19
#endif

#ifndef blr_stall
#define blr_stall               20
#endif

#ifndef blr_select
#define blr_select              21
#endif

/* Value fetch operations */
#ifndef blr_field
#define blr_field               50
#endif

#ifndef blr_literal
#define blr_literal             51
#endif

#ifndef blr_dbkey
#define blr_dbkey               52
#endif

#ifndef blr_parameter
#define blr_parameter           53
#endif

#ifndef blr_parameter2
#define blr_parameter2          54
#endif

#ifndef blr_null
#define blr_null                55
#endif

/* Aggregate functions */
#ifndef blr_agg_count
#define blr_agg_count           60
#endif

#ifndef blr_agg_max
#define blr_agg_max             61
#endif

#ifndef blr_agg_min
#define blr_agg_min             62
#endif

#ifndef blr_agg_total
#define blr_agg_total           63
#endif

#ifndef blr_agg_average
#define blr_agg_average         64
#endif

#endif /* FB_BLR_COMPAT_H */
