# Next-Gen Architecture Specification

> Date: 2026-03-22
> Status: Draft
> Scope: 3-layer architecture for deprecation-free, state-of-the-art PHP Firebird driver

---

## 1. Overview

Transform php-firebird into a modern 3-layer architecture:

1. **Layer 1** — Deprecation-free `fbird_*` C function API (existing, modernized)
2. **Layer 2** — Native PHP OOP classes in `Firebird\` namespace (new)
3. **Layer 3** — Thin PDO driver `pdo_fbird` (new, separate `.so`)

All layers use the Firebird OO API (`IAttachment`, `IStatement`, etc.) internally.
No `isc_*` function calls remain except protocol constants and `isc_vax_integer()`.

---

## 2. Layer 1: Deprecation-Free `fbird_*` Function API

### 2.1 Current State

- 151+ tests passing across 12-target matrix
- Connection, transaction, blob handles migrated to OO API wrappers
- Remaining legacy: `fb_safe_handle` for `_ib_query.stmt`, service API (`isc_service_*`),
  events (`isc_wait_for_event`), utility functions (`isc_vax_integer`, `isc_sqlcode`)

### 2.2 Target State

- **Zero `isc_*` function calls** (except constants and `isc_vax_integer()` utility)
- Service API migrated to `IService` via `fb_service.hpp` C++ wrappers
- Statement handle migrated to `IStatement` — `fb_safe_handle` union removed
- All `isc_dsql_*` calls replaced with OO API equivalents
- Events: keep `isc_wait_for_event()` for sync polling (async deferred to Layer 2)

### 2.3 Public API (unchanged)

All existing `fbird_*` functions remain with identical signatures:
- `fbird_connect()`, `fbird_pconnect()`, `fbird_close()`
- `fbird_query()`, `fbird_prepare()`, `fbird_execute()`
- `fbird_fetch_row()`, `fbird_fetch_assoc()`, `fbird_fetch_object()`
- `fbird_blob_*()`, `fbird_trans()`, `fbird_service_*()`, etc.

No new `ibase_*` aliases. No signature changes. Full backward compatibility.

---

## 3. Layer 2: Native PHP OOP Classes (`Firebird\` Namespace)

### 3.1 Design Principles

- Registered via `zend_class_entry` in C (not PHP userland classes)
- Each class wraps the corresponding OO API interface
- Exposes Firebird-specific features not available in PDO
- Implements standard PHP interfaces where applicable (`Traversable`, `Stringable`)
- Type-safe: uses PHP 8.x typed properties and union types

### 3.2 Class Hierarchy

```
Firebird\Connection          → wraps IAttachment
Firebird\Transaction         → wraps ITransaction
Firebird\Statement           → wraps IStatement
Firebird\ResultSet           → wraps IResultSet (implements Traversable)
Firebird\Blob                → wraps IBlob (implements Stringable)
Firebird\Batch               → wraps IBatch (FB4+ only)
Firebird\Service             → wraps IService
Firebird\Events              → wraps IEvents + queEvents()
Firebird\Exception           → extends \RuntimeException
Firebird\ConnectionException → extends Firebird\Exception
Firebird\QueryException      → extends Firebird\Exception
Firebird\ServiceException    → extends Firebird\Exception
```

### 3.3 Key Class APIs

#### `Firebird\Connection`

```php
class Connection {
    public function __construct(string $database, ?string $user = null,
        ?string $password = null, ?string $role = null, ?string $charset = null,
        ?int $dialect = 3, ?array $dpb = null);

    public function close(): void;
    public function ping(): bool;
    public function isConnected(): bool;

    // Transactions
    public function beginTransaction(?array $tpb = null): Transaction;
    public function getDefaultTransaction(): Transaction;

    // Statements
    public function prepare(string $sql): Statement;
    public function execute(string $sql, array $params = []): int|ResultSet;
    public function query(string $sql, array $params = []): ResultSet;

    // Blobs
    public function createBlob(): Blob;
    public function openBlob(string $blobId): Blob;

    // Batch (FB4+)
    public function createBatch(string $sql, ?array $params = null): Batch;

    // Events
    public function waitForEvent(array $events, float $timeout = 0.0): ?array;
    public function createEventHandler(array $events, callable $callback): Events;

    // Info
    public function getServerVersion(): string;
    public function getInfo(int $item): mixed;
    public function getDatabaseInfo(): array;

    // Timeouts (FB4+)
    public function setIdleTimeout(int $seconds): void;
    public function setStatementTimeout(int $seconds): void;

    // Maintenance
    public function cancelOperation(int $kind = CANCEL_RAISE): void;
}
```

#### `Firebird\Statement`

```php
class Statement {
    public function execute(array $params = []): int|ResultSet;
    public function openCursor(array $params = [], ?string $cursorName = null): ResultSet;
    public function getAffectedRows(): int;
    public function getPlan(bool $detailed = false): string;
    public function getType(): int;
    public function getInputMetadata(): array;
    public function getOutputMetadata(): array;
    public function free(): void;

    // Batch (FB4+)
    public function createBatch(): Batch;

    // Timeouts (FB4+)
    public function setTimeout(int $seconds): void;
}
```

#### `Firebird\ResultSet`

```php
class ResultSet implements \IteratorAggregate, \Countable {
    public function fetchNext(): ?array;
    public function fetchAssoc(): ?array;
    public function fetchObject(?string $class = 'stdClass'): ?object;
    public function fetchAll(int $mode = FETCH_ASSOC): array;
    public function fetchColumn(int $column = 0): mixed;

    // Scrollable cursors
    public function fetchFirst(): ?array;
    public function fetchLast(): ?array;
    public function fetchAbsolute(int $position): ?array;
    public function fetchRelative(int $offset): ?array;
    public function isEof(): bool;
    public function isBof(): bool;

    public function getMetadata(): array;
    public function close(): void;
    public function getIterator(): \Traversable;
    public function count(): int;
}
```

#### `Firebird\Transaction`

```php
class Transaction {
    public function commit(): void;
    public function commitRetaining(): void;
    public function rollback(): void;
    public function rollbackRetaining(): void;
    public function isActive(): bool;
    public function getInfo(): array;

    // Savepoints (via RELEASE SAVEPOINT / ROLLBACK TO SAVEPOINT SQL)
    public function savepoint(string $name): void;
    public function releaseSavepoint(string $name): void;
    public function rollbackToSavepoint(string $name): void;
}
```

#### `Firebird\Batch` (FB4+)

```php
class Batch {
    public function add(array $params): void;
    public function addBlob(string $blobId, string $data): void;
    public function execute(): array; // returns per-row status
    public function cancel(): void;
    public function close(): void;
    public function getMetadata(): array;
}
```

### 3.4 Firebird-Specific Features (not in PDO)

- **Named/scrollable cursors** via `ResultSet::fetchAbsolute()` etc.
- **Batch DML** via `Batch` class (FB4+)
- **Events** via `Connection::waitForEvent()` and `Events` class
- **DECFLOAT/INT128/TZ types** with proper PHP type mapping
- **Service API** (backup, restore, user management, statistics)
- **Transaction info** (reads, writes, fetches, marks)
- **Database info** (ODS version, page size, attachment count)
- **Cancel operation** for long-running queries
- **Statement/idle timeouts** (FB4+)

---

## 4. Layer 3: PDO Driver (`pdo_fbird`)

### 4.1 Design

- Separate shared module: `pdo_fbird.so` / `php_pdo_fbird.dll`
- DSN prefix: `fbird:` (avoids collision with bundled `pdo_firebird` using `firebird:`)
- Thin wrapper delegating to Layer 2 classes internally
- Implements standard PDO interfaces: `PDO`, `PDOStatement`

### 4.2 DSN Format

```
fbird:dbname=localhost:/path/to/database.fdb;charset=UTF8;dialect=3;role=MYROLE
```

### 4.3 PDO Attributes (Firebird-specific)

```php
PDO::FBIRD_ATTR_DIALECT        // SQL dialect (1 or 3)
PDO::FBIRD_ATTR_CHARSET        // Connection charset
PDO::FBIRD_ATTR_ROLE           // SQL role
PDO::FBIRD_ATTR_IDLE_TIMEOUT   // Idle timeout (FB4+)
PDO::FBIRD_ATTR_STMT_TIMEOUT   // Statement timeout (FB4+)
PDO::FBIRD_ATTR_BATCH_SIZE     // Default batch size (FB4+)
```

### 4.4 Limitations vs Layer 2

PDO cannot expose:
- Scrollable cursor methods (PDO has no scrollable cursor API)
- Event handling (no PDO equivalent)
- Service API (backup/restore/user management)
- Batch DML (PDO has no batch API)
- Transaction info queries

Users needing these features should use Layer 2 directly.

---

## 5. Type Mapping

| Firebird Type | PHP Type (Layer 1) | PHP Type (Layer 2/3) |
|---------------|--------------------|-----------------------|
| SMALLINT | int | int |
| INTEGER | int | int |
| BIGINT | int (64-bit) / string (32-bit) | int |
| FLOAT | float | float |
| DOUBLE PRECISION | float | float |
| NUMERIC/DECIMAL | string (scaled) | string |
| CHAR/VARCHAR | string | string |
| BLOB (text) | string | string |
| BLOB (binary) | string (raw) | string (raw) |
| DATE | string (Y-m-d) | string or DateTimeImmutable |
| TIME | string (H:i:s) | string or DateTimeImmutable |
| TIMESTAMP | string (Y-m-d H:i:s) | string or DateTimeImmutable |
| BOOLEAN | bool | bool |
| DECFLOAT(16/34) | string | string |
| INT128 | string | string |
| TIMESTAMP WITH TZ | string | string or DateTimeImmutable |
| TIME WITH TZ | string | string or DateTimeImmutable |
| ARRAY | array | array |

---

## 6. Build Configuration

### 6.1 Extension Module

Single `firebird.so` contains Layer 1 + Layer 2.
`pdo_fbird.so` is a separate module depending on `firebird.so` + `pdo.so`.

### 6.2 config.m4 Changes

```m4
PHP_ARG_WITH([firebird], ...)      # Layer 1 + 2
PHP_ARG_WITH([pdo-fbird], ...)     # Layer 3 (optional)
```

### 6.3 Stubs

- `stubs/firebird-stubs.php` — Layer 1 function stubs (existing)
- `stubs/firebird-classes.php` — Layer 2 class stubs (new/expanded)
- `stubs/pdo-fbird-stubs.php` — Layer 3 PDO stubs (new)

---

## 7. Compatibility

- **PHP**: 8.2+ (minimum), 8.3/8.4/8.5 supported
- **Firebird Client**: 3.0+ (compile-time minimum)
- **Firebird Server**: 2.5+ via TCP/IP (runtime, feature-dependent)
- **OS**: Linux x86_64, Windows x64 (CI matrix)
- **Thread Safety**: NTS and ZTS builds supported
