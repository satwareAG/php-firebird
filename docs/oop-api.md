# Firebird OOP API (Layer 2)

The `Firebird\*` namespace provides a native object-oriented interface to Firebird databases, registered directly as C classes by the php-firebird extension. All classes are available when the `firebird` extension is loaded.

## Classes

### `Firebird\Connection`

Opens and manages a database connection.

```php
$conn = new Firebird\Connection(
    database: 'localhost:/var/lib/firebird/data/mydb.fdb',
    username: 'SYSDBA',
    password: 'masterkey',
    charset:  'UTF8',
    dialect:  3
);

if ($conn->isConnected()) {
    $conn->ping();
}
$conn->close();
```

**Methods:**

| Method | Description |
|--------|-------------|
| `__construct(string $database, string $username, string $password, string $charset = 'UTF8', int $buffers = 0, int $dialect = 3, string $role = '', bool $persistent = false)` | Open connection |
| `close(): bool` | Close the connection |
| `isConnected(): bool` | Check connection state |
| `ping(): bool` | Verify server is reachable |
| `beginTransaction(int $flags = 0): Transaction` | Start a transaction |
| `prepare(string $sql, ?Transaction $tx = null, int $dialect = 3): Statement` | Prepare a statement |

---

### `Firebird\Transaction`

Represents an active transaction. Obtained via `Connection::beginTransaction()`.

```php
$tx = $conn->beginTransaction();
try {
    $stmt = $conn->prepare('INSERT INTO t (col) VALUES (?)', $tx);
    $stmt->execute('hello');
    $tx->commit();
} catch (\Throwable $e) {
    $tx->rollback();
    throw $e;
}
```

**Methods:**

| Method | Description |
|--------|-------------|
| `commit(): bool` | Commit the transaction |
| `rollback(): bool` | Roll back the transaction |
| `isActive(): bool` | Check if transaction is active |

---

### `Firebird\Statement`

A prepared SQL statement. Obtained via `Connection::prepare()`.

```php
$stmt = $conn->prepare('SELECT id, name FROM users WHERE active = ?', $tx);
$rs   = $stmt->execute(1);
while ($row = $rs->fetch()) {
    echo $row['NAME'] . "\n";
}
$rs->close();
```

**Methods:**

| Method | Description |
|--------|-------------|
| `execute(mixed ...$params): ResultSet\|bool` | Execute; returns `ResultSet` for SELECT, `true` for DML/DDL |

---

### `Firebird\ResultSet`

Cursor over a SELECT result. Obtained via `Statement::execute()`.

**Methods:**

| Method | Description |
|--------|-------------|
| `fetch(): array\|false` | Fetch next row as associative array (uppercase keys), or `false` |
| `close(): bool` | Close the cursor |

---

### `Firebird\Blob`

Read/write access to BLOB columns.

```php
// Write
$blob = Firebird\Blob::create($conn, $tx);
$blob->write($binaryData);
$blobId = $blob->getId();
$blob->close();

// Read
$blob = Firebird\Blob::open($conn, $tx, $blobId);
$data = '';
while (($chunk = $blob->read(8192)) !== false) {
    $data .= $chunk;
}
$blob->close();
```

**Methods:**

| Method | Description |
|--------|-------------|
| `static create(Connection $conn, Transaction $tx): static` | Create new BLOB for writing |
| `static open(Connection $conn, Transaction $tx, string $blobId): static` | Open existing BLOB for reading |
| `write(string $data): bool` | Write data segment |
| `read(int $length = 8192): string\|false` | Read data segment |
| `close(): bool` | Close BLOB handle |
| `getId(): string` | Get BLOB quad identifier |

---

### `Firebird\Service`

Access to the Firebird Services API (backup, restore, server info).

```php
$svc = new Firebird\Service('localhost', 'SYSDBA', 'masterkey');
echo $svc->getServerVersion() . "\n";
$svc->detach();
```

**Methods:**

| Method | Description |
|--------|-------------|
| `__construct(string $host = 'localhost', string $username = 'SYSDBA', string $password = 'masterkey')` | Attach to Services Manager |
| `detach(): bool` | Detach |
| `isAttached(): bool` | Check attachment state |
| `getServerVersion(): string` | Get server version string |

---

### Exception Hierarchy

```
\RuntimeException
  └── Firebird\Exception          (base; has getSqlState(): string)
        ├── Firebird\DatabaseException   (connection errors)
        └── Firebird\TransactionException (transaction errors)
```

Exceptions are thrown when `fbird_set_exception_mode(FBIRD_EXCEPTION_MODE_THROW)` is active.

---

## Notes

- Column names in `ResultSet::fetch()` are returned in **uppercase** (Firebird default).
- All classes are registered by the C extension — do not instantiate via `new` except `Connection` and `Service`.
- For Firebird 4.0+ features (DECFLOAT, INT128, time zones, batch DML), use the procedural `fbird_*` API or the `pdo_fbird` driver.
