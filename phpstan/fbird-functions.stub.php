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
