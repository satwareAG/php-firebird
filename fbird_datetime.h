/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

#ifndef PHP_FBIRD_DATETIME_H
#define PHP_FBIRD_DATETIME_H

/**
 * Cross-platform date/time parsing utilities for Firebird extension.
 *
 * These utilities replace the non-portable strptime() function with
 * cross-platform sscanf()-based parsing that works consistently on:
 * - Linux (glibc and musl libc)
 * - Windows (no strptime available)
 * - macOS
 *
 * The implementation uses the Firebird 3.0+ OO API (IUtil interface)
 * for encoding date/time values into ISC_DATE, ISC_TIME, ISC_TIMESTAMP,
 * and timezone-aware variants.
 *
 * Supported date/time formats (auto-detected):
 * - ISO 8601: "YYYY-MM-DD HH:MM:SS" or "YYYY-MM-DD"
 * - European: "DD.MM.YYYY HH:MM:SS" or "DD.MM.YYYY"
 * - US: "MM/DD/YYYY HH:MM:SS" or "MM/DD/YYYY"
 * - SQL DATE: "YYYY-MM-DD"
 * - SQL TIME: "HH:MM:SS" or "HH:MM:SS.FFFF"
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Parsed date/time components structure.
 * All values are normalized (1-based months, full year).
 */
typedef struct {
    unsigned year;        /* Full year (e.g., 2025) */
    unsigned month;       /* Month (1-12) */
    unsigned day;         /* Day of month (1-31) */
    unsigned hours;       /* Hours (0-23) */
    unsigned minutes;     /* Minutes (0-59) */
    unsigned seconds;     /* Seconds (0-59) */
    unsigned fractions;   /* Fractions of second (0-9999, tenths of milliseconds) */
    char timezone[64];    /* Timezone name or offset (e.g., "Europe/Berlin", "+02:00", "GMT") */
    int has_date;         /* 1 if date components were parsed */
    int has_time;         /* 1 if time components were parsed */
    int has_timezone;     /* 1 if timezone was parsed */
} fbird_datetime_components;

/**
 * Parse a date string into components.
 *
 * Attempts to parse the string using multiple format patterns in order of preference:
 * 1. ISO 8601 / SQL format: YYYY-MM-DD
 * 2. European format: DD.MM.YYYY
 * 3. US format: MM/DD/YYYY
 *
 * The format is auto-detected based on separator characters.
 *
 * @param str Input date string
 * @param out Output: parsed components (year, month, day filled; time components zeroed)
 * @return 1 on success, 0 on failure
 */
int fbird_parse_date(const char* str, fbird_datetime_components* out);

/**
 * Parse a time string into components.
 *
 * Supported formats:
 * - HH:MM:SS
 * - HH:MM:SS.FFFF (with fractions)
 * - HH:MM:SS TIMEZONE
 * - HH:MM:SS.FFFF TIMEZONE
 *
 * @param str Input time string
 * @param out Output: parsed components (time fields filled; date components zeroed)
 * @return 1 on success, 0 on failure
 */
int fbird_parse_time(const char* str, fbird_datetime_components* out);

/**
 * Parse a timestamp string (date + time) into components.
 *
 * Supported formats (date part auto-detected):
 * - YYYY-MM-DD HH:MM:SS
 * - YYYY-MM-DD HH:MM:SS.FFFF
 * - YYYY-MM-DD HH:MM:SS TIMEZONE
 * - DD.MM.YYYY HH:MM:SS
 * - MM/DD/YYYY HH:MM:SS
 * - SQL TIMESTAMP format variants
 *
 * @param str Input timestamp string
 * @param out Output: parsed components (all fields if present)
 * @return 1 on success, 0 on partial/failure
 */
int fbird_parse_timestamp(const char* str, fbird_datetime_components* out);

/**
 * Initialize datetime components structure to safe defaults.
 *
 * @param out Structure to initialize
 */
void fbird_datetime_init(fbird_datetime_components* out);

#ifdef __cplusplus
}
#endif

#endif /* PHP_FBIRD_DATETIME_H */
