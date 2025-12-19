<?php

declare(strict_types=1);

namespace Firebird;

/**
 * Non-variadic helper wrappers around variadic `fbird_*` extension functions.
 *
 * Goal:
 * - Provide array-based, statically typable alternatives to `fbird_query(...$params)`
 *   and `fbird_execute(...$params)` call sites.
 * - Keep runtime behaviour identical by delegating to the extension functions.
 *
 * Note: We intentionally use call_user_func_array() here to avoid `...$args`
 * spreading in higher-level wrapper code.
 */

/**
 * Execute a query on a connection with positional parameters.
 *
 * Mirrors:
 *   fbird_query($link, $sql, ...$params)
 *
 * @param resource $link
 * @param list<mixed> $params
 * @return resource|int|bool
 */
function fbird_query_params(mixed $link, string $sql, array $params = []): mixed
{
    /** @var list<mixed> $args */
    $args = array_merge([$link, $sql], $params);

    /** @var resource|int|bool $result */
    $result = \call_user_func_array('fbird_query', $args);

    return $result;
}

/**
 * Execute a query on a connection within an explicit transaction with positional parameters.
 *
 * Mirrors:
 *   fbird_query($link, $trans, $sql, ...$params)
 *
 * @param resource $link
 * @param resource $transaction
 * @param list<mixed> $params
 * @return resource|int|bool
 */
function fbird_query_params_tx(mixed $link, mixed $transaction, string $sql, array $params = []): mixed
{
    /** @var list<mixed> $args */
    $args = array_merge([$link, $transaction, $sql], $params);

    /** @var resource|int|bool $result */
    $result = \call_user_func_array('fbird_query', $args);

    return $result;
}

/**
 * Execute a prepared statement with positional parameters.
 *
 * Mirrors:
 *   fbird_execute($statement, ...$params)
 *
 * @param resource $statement
 * @param list<mixed> $params
 * @return resource|int|bool
 */
function fbird_execute_params(mixed $statement, array $params = []): mixed
{
    /** @var list<mixed> $args */
    $args = array_merge([$statement], $params);

    /** @var resource|int|bool $result */
    $result = \call_user_func_array('fbird_execute', $args);

    return $result;
}

/**
 * Start a transaction with optional lock timeout on a single connection.
 *
 * Mirrors common patterns:
 * - fbird_trans(FBIRD_DEFAULT, $link)
 * - fbird_trans($flags, $link)
 * - fbird_trans($flags|FBIRD_WAIT|FBIRD_LOCK_TIMEOUT, $timeout, $link)
 *
 * @param resource $link
 * @return resource|false
 */
function fbird_trans_begin(mixed $link, int $flags = FBIRD_DEFAULT, ?int $lockTimeout = null): mixed
{
    $args = [$flags];

    if ($lockTimeout !== null) {
        // The C implementation only accepts lock timeout when WAIT is set and NOWAIT is not set.
        $flags |= FBIRD_WAIT | FBIRD_LOCK_TIMEOUT;
        $args[0] = $flags;
        $args[] = $lockTimeout;
    }

    $args[] = $link;

    /** @var resource|false $result */
    $result = \call_user_func_array('fbird_trans', $args);

    return $result;
}
