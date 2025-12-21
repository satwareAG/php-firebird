<?php

/**
 * Helper wrappers for non-variadic execution.
 * They are defined in userland src/Firebird/functions.php but stubbed here to ensure PHPStan sees them.
 */

namespace Firebird;

/**
 * @param resource $link
 * @param string $sql
 * @param array<mixed> $params
 * @return resource|int|bool
 */
function fbird_query_params(mixed $link, string $sql, array $params = []): mixed {}

/**
 * @param resource $link
 * @param resource $transaction
 * @param string $sql
 * @param array<mixed> $params
 * @return resource|int|bool
 */
function fbird_query_params_tx(mixed $link, mixed $transaction, string $sql, array $params = []): mixed {}

/**
 * @param resource $statement
 * @param array<mixed> $params
 * @return resource|int|bool
 */
function fbird_execute_params(mixed $statement, array $params = []): mixed {}

/**
 * @param resource $link
 * @param int $flags
 * @param int|null $lockTimeout
 * @return resource|false
 */
function fbird_trans_begin(mixed $link, int $flags = FBIRD_DEFAULT, ?int $lockTimeout = null): mixed {}

/**
 * Get list of limbo (in-doubt) transaction IDs from a database connection.
 * Limbo transactions occur from failed two-phase commits requiring manual recovery.
 *
 * @param resource|null $link Database connection resource (default link if null)
 * @param int $maxCount Maximum number of transaction IDs to return (default 100, max 10000)
 * @return array<int, int>|false Array of transaction IDs or false on error
 */
function fbird_get_limbo_transactions(mixed $link = null, int $maxCount = 100): array|false {}

/**
 * Reconnect to a limbo transaction for recovery (commit or rollback).
 * Used to resolve in-doubt transactions from failed two-phase commits.
 *
 * @param resource $link Database connection resource
 * @param int $transactionId Transaction ID from fbird_get_limbo_transactions()
 * @return resource|false Transaction resource or false on error
 */
function fbird_reconnect_transaction(mixed $link, int $transactionId): mixed {}
