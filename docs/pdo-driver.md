# Firebird PDO Driver (pdo_fbird)

The `pdo_fbird` extension is a separate PDO driver for Firebird databases using the `fbird:` DSN prefix. It avoids collision with PHP's bundled `pdo_firebird` driver (`firebird:` DSN).

## Installation

The `pdo_fbird` driver is built separately from the main `firebird` extension:

```bash
cd pdo_fbird
phpize
./configure --with-pdo-fbird
make
make install
```

Add to `php.ini`:
```ini
extension=firebird.so   ; must be loaded first
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

## Driver-Specific Attributes

These constants are registered on the `PDO` class when `pdo_fbird` is loaded:

| Constant | Value | Description |
|----------|-------|-------------|
| `PDO::FBIRD_ATTR_TRANSACTION_FLAGS` | 1000 | Firebird transaction flags (`PHP_FBIRD_*`) |
| `PDO::FBIRD_ATTR_DIALECT` | 1001 | SQL dialect override (1 or 3) |
| `PDO::FBIRD_ATTR_CHARSET` | 1002 | Connection character set |

```php
$pdo->setAttribute(PDO::FBIRD_ATTR_DIALECT, 3);
$pdo->setAttribute(PDO::FBIRD_ATTR_CHARSET, 'UTF8');
```

## Server Version

```php
echo $pdo->getAttribute(PDO::ATTR_SERVER_VERSION);
// e.g. "WI-V4.0.0.2496 Firebird 4.0"
```

## BLOB / LOB Support

```php
// Write BLOB
$stmt = $pdo->prepare('INSERT INTO docs (content) VALUES (?)');
$fp = fopen('large_file.pdf', 'rb');
$stmt->bindParam(1, $fp, PDO::PARAM_LOB);
$stmt->execute();
fclose($fp);

// Read BLOB
$stmt = $pdo->query('SELECT content FROM docs WHERE id = 1');
$row  = $stmt->fetch(PDO::FETCH_ASSOC);
$data = stream_get_contents($row['CONTENT']);
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
| Minimum Firebird server | 2.5 | 2.5 (via FB 3+ client) |
| DECFLOAT / INT128 / TZ | No | Planned (v8.x) |
| Batch DML | No | Planned (v8.x) |

## Limitations

- `PDO::lastInsertId()` is not supported — use `SELECT GEN_ID(gen, 0) FROM RDB$DATABASE` or a `RETURNING` clause.
- Scrollable cursors (`PDO::CURSOR_SCROLL`) are not yet implemented.
- Multiple active result sets on a single connection are not supported.
