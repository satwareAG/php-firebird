/*
 * php_fbird_compat.h - API Mode Detection and Compatibility Layer
 *
 * Provides centralized mode detection and typed accessors for the Firebird
 * OO API (fbc_*, fbt_*, fbs_* wrappers). The satwareAG fork exclusively uses
 * the Firebird 3.0+ OO API.
 *
 * Usage:
 *   #include "php_fbird_compat.h"
 *   if (fbird_link_is_oo(link)) { ... }
 */

#ifndef PHP_FBIRD_COMPAT_H
#define PHP_FBIRD_COMPAT_H

#include "php_fbird_includes.h"

/*
 * API Mode Enumeration
 *
 * Explicit tagging of which API path should be used for operations.
 * This is NOT stored in structs yet (future enhancement) but can be
 * derived from the presence of OO API pointers.
 */
typedef enum fbird_api_mode {
    FBIRD_API_MODE_LEGACY = 0,  /* Use isc_* functions with legacy handles */
    FBIRD_API_MODE_OO = 1       /* Use fb*_* wrapper functions with OO API pointers */
} fbird_api_mode;

/*
 * Mode Detection Helpers
 *
 * These functions determine which API mode is in use by checking
 * the presence of OO API wrapper pointers. If the OO API pointer
 * is non-NULL, the OO API path should be used.
 */

/**
 * Check if a database link uses OO API.
 *
 * @param link Database link to check
 * @return 1 if OO API (fbc_connection is set), 0 if legacy
 */
static inline int fbird_link_is_oo(const fbird_db_link* link)
{
    if (link == NULL) {
        return 0;
    }
    return link->fbc_connection != NULL;
}

/**
 * Check if a transaction uses OO API.
 *
 * @param trans Transaction to check
 * @return 1 if OO API (fbt_transaction is set), 0 if legacy
 */
static inline int fbird_trans_is_oo(const fbird_transaction* trans)
{
    if (trans == NULL) {
        return 0;
    }
    return trans->fbt_transaction != NULL;
}

/**
 * Check if a query uses OO API.
 *
 * @param q Query to check
 * @return 1 if OO API (fbs_statement is set), 0 if legacy
 */
static inline int fbird_query_is_oo(const fbird_query* q)
{
    if (q == NULL) {
        return 0;
    }
    return q->fbs_statement != NULL;
}

/**
 * Get the API mode for a database link.
 *
 * @param link Database link to check
 * @return FBIRD_API_MODE_OO or FBIRD_API_MODE_LEGACY
 */
static inline fbird_api_mode fbird_get_link_mode(const fbird_db_link* link)
{
    return fbird_link_is_oo(link) ? FBIRD_API_MODE_OO : FBIRD_API_MODE_LEGACY;
}

/**
 * Get the API mode for a transaction.
 *
 * @param trans Transaction to check
 * @return FBIRD_API_MODE_OO or FBIRD_API_MODE_LEGACY
 */
static inline fbird_api_mode fbird_get_trans_mode(const fbird_transaction* trans)
{
    return fbird_trans_is_oo(trans) ? FBIRD_API_MODE_OO : FBIRD_API_MODE_LEGACY;
}

/**
 * Get the API mode for a query.
 *
 * @param q Query to check
 * @return FBIRD_API_MODE_OO or FBIRD_API_MODE_LEGACY
 */
static inline fbird_api_mode fbird_get_query_mode(const fbird_query* q)
{
    return fbird_query_is_oo(q) ? FBIRD_API_MODE_OO : FBIRD_API_MODE_LEGACY;
}

/*
 * Typed Accessors for OO API Pointers
 *
 * These provide type-safe access to OO API wrapper pointers.
 * Returns NULL if the object is using legacy mode.
 */

/**
 * Get the OO API connection wrapper (checked).
 *
 * @param link Database link
 * @return fbc_connection pointer or NULL if legacy mode
 */
static inline void* fbird_get_connection(const fbird_db_link* link)
{
    if (link == NULL) {
        return NULL;
    }
    return link->fbc_connection;
}

/**
 * Get the OO API transaction wrapper (checked).
 *
 * @param trans Transaction
 * @return fbt_transaction pointer or NULL if legacy mode
 */
static inline void* fbird_get_transaction(const fbird_transaction* trans)
{
    if (trans == NULL) {
        return NULL;
    }
    return trans->fbt_transaction;
}

/**
 * Get the OO API statement wrapper (checked).
 *
 * @param q Query
 * @return fbs_statement pointer or NULL if legacy mode
 */
static inline void* fbird_get_statement(const fbird_query* q)
{
    if (q == NULL) {
        return NULL;
    }
    return q->fbs_statement;
}

/*
 * Guard Macros
 *
 * These macros help prevent calling legacy APIs with OO API handles.
 * Use at the start of functions that must not be called on OO API objects.
 */

/**
 * Guard: Require legacy link.
 * Returns FAILURE and sets error if link is OO API.
 */
#define FBIRD_REQUIRE_LEGACY_LINK(link, func_name) \
    do { \
        if (fbird_link_is_oo(link)) { \
            _php_fbird_module_error(func_name " is not supported for OO API connections"); \
            return FAILURE; \
        } \
    } while(0)

/**
 * Guard: Require legacy transaction.
 * Returns FAILURE and sets error if transaction is OO API.
 */
#define FBIRD_REQUIRE_LEGACY_TRANS(trans, func_name) \
    do { \
        if (fbird_trans_is_oo(trans)) { \
            _php_fbird_module_error(func_name " is not supported for OO API transactions"); \
            return FAILURE; \
        } \
    } while(0)

/**
 * Guard: Require legacy query.
 * Returns FAILURE and sets error if query is OO API.
 */
#define FBIRD_REQUIRE_LEGACY_QUERY(query, func_name) \
    do { \
        if (fbird_query_is_oo(query)) { \
            _php_fbird_module_error(func_name " is not supported for OO API queries"); \
            return FAILURE; \
        } \
    } while(0)

/**
 * Guard: Require OO API link.
 * Returns FAILURE and sets error if link is legacy.
 */
#define FBIRD_REQUIRE_OO_LINK(link, func_name) \
    do { \
        if (!fbird_link_is_oo(link)) { \
            _php_fbird_module_error(func_name " requires OO API connection"); \
            return FAILURE; \
        } \
    } while(0)

/**
 * Guard: Require OO API transaction.
 * Returns FAILURE and sets error if transaction is legacy.
 */
#define FBIRD_REQUIRE_OO_TRANS(trans, func_name) \
    do { \
        if (!fbird_trans_is_oo(trans)) { \
            _php_fbird_module_error(func_name " requires OO API transaction"); \
            return FAILURE; \
        } \
    } while(0)

/**
 * Guard: Require OO API query.
 * Returns FAILURE and sets error if query is legacy.
 */
#define FBIRD_REQUIRE_OO_QUERY(query, func_name) \
    do { \
        if (!fbird_query_is_oo(query)) { \
            _php_fbird_module_error(func_name " requires OO API query"); \
            return FAILURE; \
        } \
    } while(0)

/*
 * Validation Helpers
 *
 * Check that link/transaction/query are in a consistent state.
 */

/**
 * Check if link has valid handles for its detected mode.
 *
 * @param link Database link to validate
 * @return 1 if valid, 0 if inconsistent
 */
static inline int fbird_link_is_valid(const fbird_db_link* link)
{
    if (link == NULL) {
        return 0;
    }
    if (fbird_link_is_oo(link)) {
        /* OO mode: fbc_connection must be set */
        return link->fbc_connection != NULL;
    } else {
        /* Legacy mode: handle.db must be set */
        return link->handle.db != 0;
    }
}

/**
 * Check if transaction has valid handles for its detected mode.
 *
 * @param trans Transaction to validate
 * @return 1 if valid, 0 if inconsistent
 */
static inline int fbird_trans_is_valid(const fbird_transaction* trans)
{
    if (trans == NULL) {
        return 0;
    }
    if (fbird_trans_is_oo(trans)) {
        /* OO mode: fbt_transaction must be set */
        return trans->fbt_transaction != NULL;
    } else {
        /* Legacy mode: handle.tr must be set */
        return trans->handle.tr != 0;
    }
}

/**
 * Check if query has valid handles for its detected mode.
 *
 * @param q Query to validate
 * @return 1 if valid, 0 if inconsistent
 */
static inline int fbird_query_is_valid(const fbird_query* q)
{
    if (q == NULL) {
        return 0;
    }
    if (fbird_query_is_oo(q)) {
        /* OO mode: fbs_statement must be set */
        return q->fbs_statement != NULL;
    } else {
        /* Legacy mode: stmt.stmt must be set */
        return q->stmt.stmt != 0;
    }
}

#endif /* PHP_FBIRD_COMPAT_H */
