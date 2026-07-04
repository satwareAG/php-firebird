# Firebird PDO Driver (pdo_fbird)

The `pdo_fbird` extension is a **standalone** PDO driver for Firebird databases
using the `fbird:` DSN prefix. It avoids collision with PHP's bundled
`pdo_firebird` driver (`firebird:` DSN) and provides deep integration with
Firebird-specific features via the modern OO API.

> **v12.0.0 Breaking Change**: `pdo_fbird` is no longer compiled into
> `firebird.so`. It must be loaded as a separate extension after `firebird.so`.

## Installation

The `pdo_fbird` driver is built as a separate extension from the main
`firebird` extension. Both extensions are built from the same source tree:

```bash
# From the repository root — builds both firebird.so and pdo_fbird.so
phpize
./configure --with-firebird --with-pdo-fbird
make
make install
```

Or build pdo_fbird standalone:

```bash
cd pdo_fbird
phpize
./configure --with-pdo-fbird
make
make install
```

Add to `php.ini` (order matters — `pdo_fbird.so` must load AFTER `firebird.so`):
```ini
extension=firebird.so
extension=pdo_fbird.so
```

## DSN Format

```
fbird:host=<host>;dbname=<path>[;charset=<charset>][;dialect=<1|3>][;role=<role>]
```

| Parameter | Default | Description |
|-----------|---------|-------------|
| `host`    | `localhost` | Server hostname or IP |
| `dbname`  | *(required)* | Full path to database file |
| `charset` | `UTF8` | Connection character set |
| `dialect` | `3` | SQL dialect (1 or 3) |
| `role`    | *(none)* | SQL role name |

## Basic Usage

```php
$pdo = new PDO(
    'fbird:host=localhost;dbname=/var/lib/firebird/data/mydb.fdb',
    'SYSDBA',
    'masterkey',
    [
        PDO::ATTR_ERRMODE            => PDO::ERRMODE_EXCEPTION,
        PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
    ]
);
```

## Queries

```php
// Simple query
$stmt = $pdo->query('SELECT id, name FROM users');
foreach ($stmt->fetchAll(PDO::FETCH_ASSOC) as $row) {
    echo $row['NAME'] . "\n";
}

// Positional parameters
$stmt = $pdo->prepare('SELECT * FROM users WHERE id = ?');
$stmt->execute([42]);
$row = $stmt->fetch(PDO::FETCH_ASSOC);

// Named parameters
$stmt = $pdo->prepare('INSERT INTO users (name, email) VALUES (:name, :email)');
$stmt->execute([':name' => 'Alice', ':email' => 'alice@example.com']);
```

## Transactions

The driver supports standard PDO transaction methods and custom isolation levels.

```php
$pdo->beginTransaction();
try {
    $pdo->exec("UPDATE accounts SET balance = balance - 100 WHERE id = 1");
    $pdo->exec("UPDATE accounts SET balance = balance + 100 WHERE id = 2");
    $pdo->commit();
} catch (\PDOException $e) {
    $pdo->rollBack();
    throw $e;
}
```

### Isolation Levels

Use `PDO::ATTR_TRANSACTION_ISOLATION_LEVEL` to set the isolation level for the next `beginTransaction()` call:

- `PDO::FBIRD_TXN_READ_COMMITTED`
- `PDO::FBIRD_TXN_REPEATABLE_READ`
- `PDO::FBIRD_TXN_SERIALIZABLE`

```php
$pdo->setAttribute(PDO::ATTR_TRANSACTION_ISOLATION_LEVEL, PDO::FBIRD_TXN_READ_COMMITTED);
$pdo->beginTransaction();
```

## Driver-Specific Attributes

These constants are registered on the `PDO` class when `pdo_fbird` is loaded:

| Constant | Description |
|----------|-------------|
| `PDO::FBIRD_ATTR_TRANSACTION_FLAGS` | Firebird transaction flags (`PHP_FBIRD_*`) |
| `PDO::FBIRD_ATTR_DIALECT` | SQL dialect override (1 or 3) |
| `PDO::FBIRD_ATTR_CHARSET` | Connection character set |
| `PDO::FBIRD_ATTR_WRITABLE_TRANSACTION` | Boolean: set transaction to READ WRITE or READ ONLY |
| `PDO::FBIRD_ATTR_FETCH_TABLE_NAMES` | Boolean: prepend table name to column names in fetch result |
| `PDO::FBIRD_ATTR_DATE_FORMAT` | Format string for `DATE` columns (e.g., `%Y-%m-%d`) |
| `PDO::FBIRD_ATTR_TIME_FORMAT` | Format string for `TIME` columns (e.g., `%H:%M:%S`) |
| `PDO::FBIRD_ATTR_TIMESTAMP_FORMAT` | Format string for `TIMESTAMP` columns |
| `PDO::FBIRD_ATTR_SET_BIND` | Set bind configuration (FB 4.0+, e.g., `NATIVE_CHARACTER_SET`) |

## Advanced Features

### Scrollable Cursors

Requires Firebird 5.0+ client and server.

```php
$stmt = $pdo->prepare("SELECT * FROM users", [PDO::ATTR_CURSOR => PDO::CURSOR_SCROLL]);
$stmt->execute();

$row = $stmt->fetch(PDO::FETCH_ASSOC, PDO::FETCH_ORI_LAST);
$row = $stmt->fetch(PDO::FETCH_ASSOC, PDO::FETCH_ORI_PRIOR);
$row = $stmt->fetch(PDO::FETCH_ASSOC, PDO::FETCH_ORI_ABS, 5);
```

### Last Insert ID

Firebird does not have an implicit last-insert-id. The driver implements `PDO::lastInsertId($name)` by calling `GEN_ID(name, 0)`. You must provide the sequence/generator name.

```php
$id = $pdo->lastInsertId('MY_SEQUENCE');
```

### Array Fields

Supports reading and writing Firebird array columns.

```php
// Insert array
$stmt = $pdo->prepare("INSERT INTO items (id, tags) VALUES (?, ?)");
$stmt->execute([1, ["electronics", "sale", "new"]]);

// Fetch array
$stmt = $pdo->query("SELECT tags FROM items WHERE id = 1");
$row = $stmt->fetch();
print_r($row['TAGS']); // Array ( [0] => electronics [1] => sale [2] => new )
```

### Service API

Expose Firebird Service Manager operations via `setAttribute` and `getAttribute`.

```php
// Get server version via Service API
echo $pdo->getAttribute(PDO::FBIRD_ATTR_SERVER_VERSION);

// Perform backup
$pdo->setAttribute(PDO::FBIRD_ATTR_SERVICE_BACKUP, [
    'db' => '/path/to/mydb.fdb',
    'file' => '/path/to/backup.fbk'
]);
```

### Event Polling

```php
// Register events
$pdo->setAttribute(PDO::FBIRD_ATTR_EVENT_NAMES, ['NEW_ORDER', 'CANCEL_ORDER']);

// Wait for any of the registered events (blocking)
$pdo->setAttribute(PDO::FBIRD_ATTR_EVENT_WAIT, true);

// Get counts of triggered events
$counts = $pdo->getAttribute(PDO::FBIRD_ATTR_EVENT_COUNT);
if ($counts['NEW_ORDER'] > 0) {
    echo "Processing new orders...";
}
```

## BLOB / LOB Support

```php
// Write BLOB
$stmt = $pdo->prepare('INSERT INTO docs (content) VALUES (?)');
$fp = fopen('large_file.pdf', 'rb');
$stmt->bindParam(1, $fp, PDO::PARAM_LOB);
$stmt->execute();
fclose($fp);

// Read BLOB as Stream
$stmt = $pdo->query('SELECT content FROM docs WHERE id = 1');
$stmt->bindColumn(1, $lob, PDO::PARAM_LOB);
$stmt->fetch(PDO::FETCH_BOUND);
fpassthru($lob);
```

## Error Handling

The driver maps Firebird GDS error codes to standard SQLSTATE codes:

| GDS Code | SQLSTATE | Meaning |
|----------|----------|---------|
| `isc_unique_key_violation` | `23000` | Integrity constraint violation |
| `isc_lock_conflict` | `40001` | Serialization failure |
| `isc_deadlock` | `40001` | Deadlock |
| `isc_bad_tpb_*` | `25000` | Invalid transaction state |
| `isc_dsql_*` | `42000` | Syntax error / access violation |
| *(other)* | `HY000` | General error |

## Differences from `pdo_firebird`

| Feature | `pdo_firebird` (bundled) | `pdo_fbird` (this driver) |
|---------|--------------------------|---------------------------|
| DSN prefix | `firebird:` | `fbird:` |
| Firebird client API | Legacy `isc_*` | OO API (FB 3.0+) |
| Minimum Firebird client | 2.5 | 3.0 |
| DECFLOAT / INT128 / TZ | Coercion only | **Native Support** |
| Batch DML | No | **Planned (v10.x)** |
| Service API | No | **✅ Yes (Attributes)** |
| Event Polling | No | **✅ Yes (Attributes)** |
| Array Fields | No | **✅ Yes (Read/Write)** |
| lastInsertId() | No | **✅ Yes (via sequence)** |
| Scrollable Cursors | No | **✅ Yes (FB 5.0+)** |
