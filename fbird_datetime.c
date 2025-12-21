/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

/**
 * Cross-platform date/time parsing utilities for Firebird extension.
 *
 * This implementation replaces the non-portable strptime() function with
 * cross-platform sscanf()-based parsing. The parsing functions work
 * consistently across Linux (glibc/musl), Windows, and macOS.
 *
 * Design decisions:
 * 1. Auto-detect format based on separator characters (-, ., /)
 * 2. Support multiple common date formats without configuration
 * 3. Validation of parsed components before returning
 * 4. Clear fallback behavior when parsing fails
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "fbird_datetime.h"

/* Days in each month (non-leap year) */
static const unsigned days_in_month[] = {
    0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

/**
 * Check if year is a leap year.
 */
static int is_leap_year(unsigned year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

/**
 * Get days in a specific month, accounting for leap years.
 */
static unsigned get_days_in_month(unsigned year, unsigned month) {
    if (month < 1 || month > 12) return 0;
    if (month == 2 && is_leap_year(year)) return 29;
    return days_in_month[month];
}

void fbird_datetime_init(fbird_datetime_components* out) {
    if (!out) return;

    out->year = 0;
    out->month = 0;
    out->day = 0;
    out->hours = 0;
    out->minutes = 0;
    out->seconds = 0;
    out->fractions = 0;
    out->timezone[0] = '\0';
    out->has_date = 0;
    out->has_time = 0;
    out->has_timezone = 0;
}

static int fbird_validate_date(unsigned year, unsigned month, unsigned day) {
    /* Year range check */
    if (year < 1 || year > 9999) return 0;

    /* Month range check */
    if (month < 1 || month > 12) return 0;

    /* Day range check for specific month */
    unsigned max_days = get_days_in_month(year, month);
    if (day < 1 || day > max_days) return 0;

    return 1;
}

static int fbird_validate_time(unsigned hours, unsigned minutes, unsigned seconds, unsigned fractions) {
    if (hours > 23) return 0;
    if (minutes > 59) return 0;
    if (seconds > 59) return 0;
    if (fractions > 9999) return 0;
    return 1;
}

static int fbird_extract_timezone(const char* str, fbird_datetime_components* out) {
    if (!str || !out) return 0;

    /* Find the last space in the string */
    const char* last_space = strrchr(str, ' ');
    if (!last_space || last_space == str) return 0;

    const char* potential_tz = last_space + 1;
    size_t tz_len = strlen(potential_tz);

    if (tz_len == 0 || tz_len >= sizeof(out->timezone)) return 0;

    /* Check if it looks like a timezone */
    char first = potential_tz[0];

    /* Timezone offset: +HH:MM, -HH:MM, +HHMM, -HHMM */
    if (first == '+' || first == '-') {
        /* Validate offset format */
        if (tz_len >= 5) {
            strncpy(out->timezone, potential_tz, sizeof(out->timezone) - 1);
            out->timezone[sizeof(out->timezone) - 1] = '\0';
            out->has_timezone = 1;
            return 1;
        }
        return 0;
    }

    /* Named timezone: starts with letter (GMT, UTC, Europe/Berlin, etc.) */
    if ((first >= 'A' && first <= 'Z') || (first >= 'a' && first <= 'z')) {
        /* Basic validation: at least 2 characters, alphanumeric or / or _ */
        if (tz_len >= 2) {
            int valid = 1;
            for (size_t i = 0; i < tz_len && valid; i++) {
                char c = potential_tz[i];
                if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                      (c >= '0' && c <= '9') || c == '/' || c == '_' || c == '-')) {
                    valid = 0;
                }
            }
            if (valid) {
                strncpy(out->timezone, potential_tz, sizeof(out->timezone) - 1);
                out->timezone[sizeof(out->timezone) - 1] = '\0';
                out->has_timezone = 1;
                return 1;
            }
        }
    }

    return 0;
}

int fbird_parse_timestamp(const char* str, fbird_datetime_components* out) {
    if (!str || !out) return 0;

    fbird_datetime_init(out);

    /* Skip leading whitespace */
    while (*str && isspace((unsigned char)*str)) str++;

    if (!*str) return 0;

    /* First try to extract timezone from end of string */
    fbird_extract_timezone(str, out);

    /* Try different timestamp formats */
    int y, mo, d, h, mi, s;
    int frac = 0;
    int n;

    /* Format 1: YYYY-MM-DD HH:MM:SS.FFFF (ISO 8601) */
    n = sscanf(str, "%d-%d-%d %d:%d:%d.%d", &y, &mo, &d, &h, &mi, &s, &frac);
    if (n >= 6 && y > 100) {
        out->year = (unsigned)y;
        out->month = (unsigned)mo;
        out->day = (unsigned)d;
        out->hours = (unsigned)h;
        out->minutes = (unsigned)mi;
        out->seconds = (unsigned)s;

        if (n == 7 && frac > 0) {
            /* Normalize fractions */
            int temp = frac;
            int digits = 0;
            while (temp > 0) {
                temp /= 10;
                digits++;
            }
            while (digits < 4) {
                frac *= 10;
                digits++;
            }
            while (digits > 4) {
                frac /= 10;
                digits--;
            }
            out->fractions = (unsigned)frac;
        }

        if (fbird_validate_date(out->year, out->month, out->day) &&
            fbird_validate_time(out->hours, out->minutes, out->seconds, out->fractions)) {
            out->has_date = 1;
            out->has_time = 1;
            return 1;
        }
    }

    /* Format 2: YYYY-MM-DDTHH:MM:SS (ISO 8601 with T separator) */
    n = sscanf(str, "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &s);
    if (n == 6 && y > 100) {
        out->year = (unsigned)y;
        out->month = (unsigned)mo;
        out->day = (unsigned)d;
        out->hours = (unsigned)h;
        out->minutes = (unsigned)mi;
        out->seconds = (unsigned)s;

        if (fbird_validate_date(out->year, out->month, out->day) &&
            fbird_validate_time(out->hours, out->minutes, out->seconds, out->fractions)) {
            out->has_date = 1;
            out->has_time = 1;
            return 1;
        }
    }

    /* Format 3: DD.MM.YYYY HH:MM:SS (European) */
    n = sscanf(str, "%d.%d.%d %d:%d:%d", &d, &mo, &y, &h, &mi, &s);
    if (n == 6) {
        out->day = (unsigned)d;
        out->month = (unsigned)mo;
        out->year = (unsigned)y;
        if (out->year < 100) {
            out->year += (out->year > 50) ? 1900 : 2000;
        }
        out->hours = (unsigned)h;
        out->minutes = (unsigned)mi;
        out->seconds = (unsigned)s;

        if (fbird_validate_date(out->year, out->month, out->day) &&
            fbird_validate_time(out->hours, out->minutes, out->seconds, out->fractions)) {
            out->has_date = 1;
            out->has_time = 1;
            return 1;
        }
    }

    /* Format 4: MM/DD/YYYY HH:MM:SS (US) */
    n = sscanf(str, "%d/%d/%d %d:%d:%d", &mo, &d, &y, &h, &mi, &s);
    if (n == 6) {
        out->month = (unsigned)mo;
        out->day = (unsigned)d;
        out->year = (unsigned)y;
        if (out->year < 100) {
            out->year += (out->year > 50) ? 1900 : 2000;
        }
        out->hours = (unsigned)h;
        out->minutes = (unsigned)mi;
        out->seconds = (unsigned)s;

        if (fbird_validate_date(out->year, out->month, out->day) &&
            fbird_validate_time(out->hours, out->minutes, out->seconds, out->fractions)) {
            out->has_date = 1;
            out->has_time = 1;
            return 1;
        }
    }

    /* Try date-only parse followed by time-only */
    if (fbird_parse_date(str, out)) {
        /* Find time portion after date */
        const char* time_start = str;
        while (*time_start && !isspace((unsigned char)*time_start)) time_start++;
        while (*time_start && isspace((unsigned char)*time_start)) time_start++;

        if (*time_start) {
            fbird_parse_time(time_start, out);
        }
        return 1;
    }

    return 0;
}

int fbird_parse_date(const char* str, fbird_datetime_components* out) {
    if (!str || !out) return 0;

    fbird_datetime_init(out);

    /* Skip leading whitespace */
    while (*str && isspace((unsigned char)*str)) str++;

    if (!*str) return 0;

    int v1, v2, v3;
    int n;

    /* Try ISO 8601 / SQL format: YYYY-MM-DD */
    n = sscanf(str, "%d-%d-%d", &v1, &v2, &v3);
    if (n == 3 && v1 > 100) {
        /* v1 is year (> 100), this is ISO format */
        out->year = (unsigned)v1;
        out->month = (unsigned)v2;
        out->day = (unsigned)v3;

        if (fbird_validate_date(out->year, out->month, out->day)) {
            out->has_date = 1;
            return 1;
        }
    }

    /* Try European format: DD.MM.YYYY */
    n = sscanf(str, "%d.%d.%d", &v1, &v2, &v3);
    if (n == 3) {
        out->day = (unsigned)v1;
        out->month = (unsigned)v2;
        out->year = (unsigned)v3;

        /* Handle 2-digit years */
        if (out->year < 100) {
            out->year += (out->year > 50) ? 1900 : 2000;
        }

        if (fbird_validate_date(out->year, out->month, out->day)) {
            out->has_date = 1;
            return 1;
        }
    }

    /* Try US format: MM/DD/YYYY */
    n = sscanf(str, "%d/%d/%d", &v1, &v2, &v3);
    if (n == 3) {
        out->month = (unsigned)v1;
        out->day = (unsigned)v2;
        out->year = (unsigned)v3;

        /* Handle 2-digit years */
        if (out->year < 100) {
            out->year += (out->year > 50) ? 1900 : 2000;
        }

        if (fbird_validate_date(out->year, out->month, out->day)) {
            out->has_date = 1;
            return 1;
        }
    }

    return 0;
}

int fbird_parse_time(const char* str, fbird_datetime_components* out) {
    if (!str || !out) return 0;

    /* Don't reinitialize if called from fbird_parse_timestamp */
    int init_needed = !out->has_date;
    if (init_needed) {
        fbird_datetime_init(out);
    }

    /* Skip leading whitespace */
    while (*str && isspace((unsigned char)*str)) str++;

    if (!*str) return 0;

    /* First try to extract timezone from end of string */
    fbird_extract_timezone(str, out);

    int h, m, s;
    int frac = 0;
    int n;

    /* Try HH:MM:SS.FFFF format */
    n = sscanf(str, "%d:%d:%d.%d", &h, &m, &s, &frac);
    if (n >= 3) {
        out->hours = (unsigned)h;
        out->minutes = (unsigned)m;
        out->seconds = (unsigned)s;

        /* Normalize fractions to tenths of milliseconds (0-9999 range) */
        if (n == 4 && frac > 0) {
            /* Count digits in fraction to normalize */
            int temp = frac;
            int digits = 0;
            while (temp > 0) {
                temp /= 10;
                digits++;
            }

            /* Convert to 4-digit precision (tenths of milliseconds) */
            while (digits < 4) {
                frac *= 10;
                digits++;
            }
            while (digits > 4) {
                frac /= 10;
                digits--;
            }
            out->fractions = (unsigned)frac;
        }

        if (fbird_validate_time(out->hours, out->minutes, out->seconds, out->fractions)) {
            out->has_time = 1;
            return 1;
        }
    }

    return 0;
}
