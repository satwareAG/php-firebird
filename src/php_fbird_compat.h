/*
 * php_fbird_compat.h - API Mode Detection and Compatibility Layer
 *
 * Provides centralized mode detection and typed accessors for the Firebird
 * OO API (fbc_*, fbt_*, fbs_* wrappers). The satwareAG fork exclusively uses
 * the Firebird 3.0+ OO API.
 *
 * Usage:
 *   #include "src/php_fbird_compat.h"
 *   if (fbird_link_is_oo(link)) { ... }
 */

#ifndef PHP_FBIRD_COMPAT_H
#define PHP_FBIRD_COMPAT_H

#include "php_fbird_includes.h"



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
    /* OO-only: fbc_connection must be set */
    return link->fbc_connection != NULL;
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
    /* OO-only: fbt_transaction must be set */
    return trans->fbt_transaction != NULL;
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
    return q->fbs_statement != NULL;
}

#endif /* PHP_FBIRD_COMPAT_H */
