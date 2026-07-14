# Firebird Best Practices for PHP Developers

Full lifecycle guide for production Firebird usage through the php-firebird driver.
Covers Firebird 3.0, 4.0, and 5.0.

---

## 1. Connection Management

### DSN patterns

```php
// Embedded (local file, no server)
$conn = fbird_connect('/data/employee.fdb', 'sysdba', 'masterkey');

// TCP remote (default port 3050)
$conn = fbird_connect('server.example.com:employee', 'sysdba', 'masterkey');

// TCP with explicit port
$conn = fbird_connect('server/3050:/data/employee.fdb', 'sysdba', 'masterkey');

// IPv6
$conn = fbird_connect('[::1]:employee', 'sysdba', 'masterkey');

// PDO DSN
$pdo = new PDO('fbird:dbname=server:employee;charset=UTF8', 'sysdba', 'masterkey');
```

### Connection options

```php
// DPB parameters via options array
$conn = fbird_connect('server:employee', 'sysdba', 'masterkey', [
    'charset'           => 'UTF8',
    'role'              => 'app_role',
    'dialect'           => 3,
    'page_buffers'      => 2048,
    'forced_writes'     => 1,     // Sync writes (durability)
    'wire_crypt'        => true,  // FB3+ wire encryption
    'read_consistency'  => true,  // FB4+ default isolation
    // 'session_time_zone' => 'Europe/Berlin', // FB4+ (tracked in #478)
]);
```

Source: [connection_strings @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/README.connection_strings)

### Persistent connections vs connect

```php
// Use pconnect for web apps (connection survives PHP request lifecycle)
$conn = fbird_pconnect('server:employee', 'sysdba', 'masterkey');

// Use connect for CLI scripts and long-running processes
$conn = fbird_connect('server:employee', 'sysdba', 'masterkey');
```

**Anti-pattern**: Mixing `pconnect` and `connect` to the same database in the
same PHP process causes lock contention.

### Connection pooling (FB4+)

Firebird 4.0 supports server-side connection pooling for external connections
(`EXECUTE STATEMENT ... ON EXTERNAL`). Configure in `firebird.conf`:
```text
ExtConnPoolSize = 100
ExtConnPoolLifeTime = 7200
```

### Idle timeout (FB4+)

```php
// Auto-disconnect idle connections after N seconds
fbird_set_idle_timeout($conn, 300);  // 5 minutes
```

Source: [README.session_idle_timeouts @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/README.session_idle_timeouts)

---

## 2. Transaction Strategy

### Isolation level selection

| Use case | Isolation | Why |
|----------|-----------|-----|
| OLTP (web app, fast reads/writes) | Read Committed + Read Consistency `[FB4]` | No writer blocking, stable statement-level view |
| Reporting / analytics | Snapshot | Stable data view for entire transaction |
| Batch maintenance | Snapshot Table Stability | Exclusive access for DDL/admin |
| Background queue worker | Read Committed + NO WAIT | Fail fast on lock conflict |

```php
// Read Committed + Read Consistency (FB4+) - recommended default
$trx = fbird_trans([
    'read_committed'   => true,
    'read_consistency' => true,
], $conn);

// Snapshot (Consistency / Repeatable Read)
$trx = fbird_trans([
    'concurrency'  => true,   // == SNAPSHOT
    'wait'         => true,
], $conn);

// Read Committed + Record Version (legacy FB3 default)
$trx = fbird_trans([
    'read_committed' => true,
    'rec_version'    => true,
    'no_wait'        => true,
], $conn);
```

Source: [README.read_consistency @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/README.read_consistency.md)

### Commit patterns

```php
// Standard commit (ends transaction, releases locks)
fbird_commit($trx);

// Commit retain (keeps transaction alive with same snapshot)
// Use when you need to commit data but continue reading from the same view
fbird_commit_ret($trx);

// Rollback retain (undo changes but keep transaction context)
fbird_rollback_ret($trx);
```

### Savepoints

```php
// Nested transactions without committing the outer transaction
fbird_savepoint($trx, 'before_risky');
try {
    fbird_query($trx, "INSERT INTO ... ");
} catch (Exception $e) {
    fbird_rollback_savepoint($trx, 'before_risky');
}
fbird_release_savepoint($trx, 'before_risky');
```

### Table reservation locks

```php
// Reserve tables at transaction start to prevent conflicts
$trx = fbird_trans([
    'read_committed' => true,
    'rec_version'    => false,
    'reserving'      => [
        'inventory' => ['shared', 'read'],    // Read lock
        'temp_data' => ['exclusive', 'write'], // Exclusive write
    ],
], $conn);
```

### Multi-database two-phase commit

```php
// php-firebird uniquely supports 2PC across multiple Firebird databases
$coordinator = fbird_trans(false, $conn1, $conn2, $conn3);
// ... work across all three databases ...
fbird_commit($coordinator);  // Atomic commit across all DBs
```

---

## 3. Security

### Authentication

| Method | Version | Recommendation |
|--------|---------|----------------|
| SRP (Secure Remote Password) | FB3+ | **Recommended**. Wire-encrypted by default. |
| Legacy_Auth | FB2.5 compat | Disable in production unless old clients require it. |
| Win_SSPI | Windows only | Use for domain-joined Windows clients. |

```php
// SRP is the default - no special code needed
$conn = fbird_connect('server:employee', 'sysdba', 'masterkey');
// Wire encryption is automatic in FB3+ when both client and server support it
```

### SQL SECURITY (FB4+)

```sql
-- Principle of least privilege: use INVOKER by default for user-facing procedures
CREATE PROCEDURE get_user_data
    SQL SECURITY INVOKER
AS BEGIN ... END;

-- Use DEFINER only for admin tasks that need elevated privileges
CREATE PROCEDURE cleanup_audit_log
    SQL SECURITY DEFINER
AS BEGIN
    DELETE FROM audit_log WHERE log_date < CURRENT_DATE - 365;
END;
```

### Role management

```php
// Connect with a specific role
$conn = fbird_connect('server:employee', 'app_user', 'pass', [
    'role' => 'reader_role',
]);

// FB4+: all granted roles are implicitly active (no SET ROLE needed)
// Check active roles:
$res = fbird_query($conn,
    "SELECT RDB\$ROLE_IN_USE(?) FROM rdb\$database",
    ['editor_role']
);
```

### User management via service API

```php
$svc = fbird_service_attach('localhost', 'sysdba', 'masterkey');

// Create user
fbird_add_user($svc, [
    'user_name'    => 'new_user',
    'password'     => 'secure_pass',
    'first_name'   => 'Jane',
    'last_name'    => 'Doe',
    'middle_name'  => 'Q',
    'user_id'      => 1001,    // UID (optional)
    'group_id'     => 100,     // GID (optional)
]);

// Modify user
fbird_modify_user($svc, [
    'user_name' => 'new_user',
    'password'  => 'new_pass',
]);

// Deactivate (disable login without deleting)
fbird_query($svc, "ALTER USER new_user INACTIVE");

// Delete user
fbird_delete_user($svc, 'new_user');
```

Source: [user_management @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/sql.extensions/README.user_management)

---

## 4. Indexing Strategy

### Do

```sql
-- Index foreign keys (Firebird does NOT auto-index FKs)
CREATE INDEX idx_orders_customer_fk ON orders (customer_id);

-- Index WHERE-clause columns used in common queries
CREATE INDEX idx_products_status ON products (status) WHERE status = 'active';  -- [FB5]

-- Use expression indices for case-insensitive lookups
CREATE INDEX idx_email_lower ON contacts (LOWER(email));

-- Use descending indices for time-series with ORDER BY ... DESC
CREATE DESCENDING INDEX idx_log_ts ON log_table (timestamp);

-- FB4+: GROUP BY can use descending indices
```

### Don't

```sql
-- Don't index small tables (<1000 rows) - sequential scan is faster
-- Don't create redundant indices:
--   CREATE INDEX idx_a ON t(a);     -- redundant if idx_a_b exists
--   CREATE INDEX idx_a_b ON t(a, b);
-- Don't index columns with low cardinality (e.g., BOOLEAN)
--   unless combined with other columns in a composite index
```

### Partial indices (FB5)

```sql
-- Index only active records (smaller index, faster writes)
CREATE INDEX idx_active_customers ON customers (name)
    WHERE status = 'active';

-- Index only recent records (rolling window)
CREATE INDEX idx_recent_orders ON orders (order_date)
    WHERE order_date >= CURRENT_DATE - 90;
```

Source: [partial_indices @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/sql.extensions/README.partial_indices)

### Plan inspection

```sql
-- Enable plan display
SET PLAN ON;

-- Check if your index is being used:
SELECT * FROM contacts WHERE LOWER(email) = 'test@example.com';
-- Plan should show: PLAN (CONTACTS INDEX (IDX_EMAIL_LOWER))
```

```php
// Retrieve plan programmatically
fbird_query($conn, "SET PLAN ON");
$res = fbird_query($conn, "SELECT ...");
// Plan text is in statement metadata
```

---

## 5. PSQL Patterns

### Packages vs standalone procedures

**Do**: Group related procedures in packages.

```sql
CREATE PACKAGE order_mgmt AS
BEGIN
    FUNCTION calculate_total(order_id INT) RETURNS DECFLOAT(16);
    PROCEDURE apply_discounts(order_id INT, pct DECFLOAT(16));
    PROCEDURE ship_order(order_id INT);
END;

CREATE PACKAGE BODY order_mgmt AS
BEGIN
    -- Private sub-functions are invisible outside the package
    FUNCTION validate_inventory(order_id INT) RETURNS BOOLEAN
    AS BEGIN ... END

    FUNCTION calculate_total(order_id INT) RETURNS DECFLOAT(16)
    AS BEGIN ... END

    PROCEDURE apply_discounts(order_id INT, pct DECFLOAT(16))
    AS BEGIN ... END

    PROCEDURE ship_order(order_id INT)
    AS BEGIN ... END
END;
```

**Anti-pattern**: Creating 50 standalone procedures that share no encapsulation.

### EXECUTE STATEMENT pitfalls

```sql
-- GOOD: static SQL (compiled once, cached)
FOR SELECT id, name FROM customers WHERE region = :region
INTO :cid, :cname DO BEGIN ... END

-- ACCEPTABLE: parameterized dynamic SQL
EXECUTE STATEMENT ('SELECT name FROM customers WHERE id = ' || :cid)
INTO :cname;

-- BAD: string concatenation with user input (SQL injection!)
EXECUTE STATEMENT ('SELECT name FROM customers WHERE name = ''' || :input || '''');

-- GOOD: [FB4] use WITH CALLER PRIVILEGES for dynamic SQL
EXECUTE STATEMENT (...)
    WITH CALLER PRIVILEGES;  -- Runs with invoker's rights, not definer's
```

Source: [execute_statement2 @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.execute_statement2)

### Exception handling (FB4+)

```sql
CREATE PROCEDURE safe_operation AS
BEGIN
    BEGIN
        -- Risky operation
        INSERT INTO sensitive_table VALUES (...);
    WHEN ANY DO
    BEGIN
        -- [FB4] Access exception details for logging
        INSERT INTO error_log (exc_name, exc_msg, exc_time)
        VALUES (
            RDB$GET_CONTEXT('EXCEPTION', 'EXCEPTION_NAME'),
            RDB$GET_CONTEXT('EXCEPTION', 'EXCEPTION_MESSAGE'),
            CURRENT_TIMESTAMP
        );
        -- Re-raise if needed
        EXCEPTION;
    END
END;
```

### Autonomous transactions for audit logging

```sql
CREATE PROCEDURE transfer(from_acct INT, to_acct INT, amount DECIMAL(18,2))
AS
BEGIN
    -- Log to audit even if the main transaction rolls back
    IN AUTONOMOUS TRANSACTION DO
    BEGIN
        INSERT INTO transfer_log (from_id, to_id, amount, ts)
        VALUES (:from_acct, :to_acct, :amount, CURRENT_TIMESTAMP);
    END

    -- Main transaction (may fail)
    UPDATE accounts SET balance = balance - :amount WHERE id = :from_acct;
    UPDATE accounts SET balance = balance + :amount WHERE id = :to_acct;
END;
```

---

## 6. Blob Handling

### Writing blobs

```php
// Method 1: stream-based (recommended for large blobs)
$blob = fbird_blob_create_stream($conn);
fwrite($blob, $largeData);
$blobId = fbird_blob_close($blob);
fbird_query($conn, "INSERT INTO docs (content) VALUES (?)", [$blobId]);

// Method 2: chunked
$blob = fbird_blob_create($conn);
$chunkSize = 4096;
for ($i = 0; $i < strlen($data); $i += $chunkSize) {
    fbird_blob_add($blob, substr($data, $i, $chunkSize));
}
$blobId = fbird_blob_close($blob);

// Method 3: import from file (simplest)
$blobId = fbird_blob_import($conn, fopen('/path/to/file.pdf', 'r'));
fbird_query($conn, "INSERT INTO docs (content) VALUES (?)", [$blobId]);
```

### Reading blobs

```php
// Method 1: stream-based read (recommended)
$res = fbird_query($conn, "SELECT content FROM docs WHERE id = ?", [$id]);
$row = fbird_fetch_assoc($res);
$stream = fbird_blob_open_stream($conn, $row['CONTENT']);
while ($chunk = fread($stream, 4096)) {
    echo $chunk;
}
fbird_blob_close($stream);

// Method 2: seekable blob (random access)
$blob = fbird_blob_open_seekable($conn, $row['CONTENT']);
fbird_blob_seek($blob, 1024);  // Seek to offset
$data = fbird_blob_get($blob, 4096);
fbird_blob_close($blob);

// Method 3: echo directly to output (no buffering)
fbird_blob_echo($conn, $row['CONTENT']);
```

### Server-side blob operations (FB5+)

```sql
-- BLOB_APPEND: concatenate blobs server-side (avoids round-trips)
SELECT BLOB_APPEND(header_blob, body_blob, footer_blob)
FROM templates WHERE name = 'invoice';

-- RDB$BLOB_UTIL: server-side blob manipulation
SELECT RDB$BLOB_UTIL.GET_SIZE(blob_col) FROM documents WHERE id = 1;
SELECT RDB$BLOB_UTIL.SUB_BLOB(blob_col, 0, 1024) FROM documents;
```

Source: [blob_append @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/sql.extensions/README.blob_append.md), [blob_util @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/sql.extensions/README.blob_util.md)

---

## 7. Character Sets & Collations

### Choosing a character set

| Set | When to use | Notes |
|-----|-------------|-------|
| `UTF8` | Default for all new databases | Full Unicode support |
| `NONE` | Binary data, raw bytes | No conversion, byte-for-byte |
| `OCTETS` | Binary strings (BINARY alias) | Same as NONE for storage |
| `WIN1252` | Legacy Western European | Migration from old databases |
| `ISO8859_1` | Legacy Western European | Migration |

```sql
-- Create database with UTF8
CREATE DATABASE '/data/mydb.fdb' DEFAULT CHARACTER SET UTF8;

-- Table-level charset
CREATE TABLE messages (
    id INT,
    subject VARCHAR(200) CHARACTER SET UTF8 COLLATE UNICODE_CI_AI,
    body BLOB SUB_TYPE TEXT CHARACTER SET UTF8
);
```

### Collation names

| Suffix | Meaning |
|--------|---------|
| `_CI` | Case-insensitive |
| `_AI` | Accent-insensitive |
| `_CI_AI` | Both case- and accent-insensitive |

```sql
-- Case-insensitive accent-insensitive search
SELECT * FROM customers
WHERE name COLLATE UNICODE_CI_AI LIKE '%müller%';
-- Matches: Muller, Müller, MULLER, muller
```

### SET BIND for type coercion (FB4+)

```php
// PDO: coerce DECFLOAT to VARCHAR at connection level
$pdo->setAttribute(PDO::FBIRD_ATTR_SET_BIND, 'DECFLOAT TO VARCHAR(60)');

// Procedural: via SQL passthrough
fbird_query($conn, "SET BIND OF DECFLOAT TO VARCHAR(60)");
// Now DECFLOAT columns are returned as VARCHAR strings
```

Source: [set_bind @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.set_bind.md)

### DISABLE-COMPRESSIONS in collations (FB5+)

```sql
-- FB5: disable ICU compression for performance with short strings
CREATE COLLATION short_str_ci FOR UTF8 FROM UNICODE
    DISABLE-COMPRESSIONS;
```

---

## 8. Performance Optimization

### Statement timeouts (FB4+)

```php
// Prevent runaway queries from blocking the server
fbird_set_statement_timeout($conn, 5);  // 5 seconds
// The next query will abort if it takes longer than 5 seconds
$res = fbird_query($conn, "SELECT ... complex analytical query ...");
```

### Batch API for bulk operations (FB4+)

```php
// 10-100x faster than individual INSERTs for bulk data
$stmt = fbird_prepare($conn, "INSERT INTO metrics (host, val) VALUES (?, ?)");
$batch = fbird_batch_create($stmt);

foreach ($dataPoints as $dp) {
    fbird_batch_add($batch, [$dp['host'], $dp['val']]);
}

$result = fbird_batch_execute($batch);  // Single network round-trip
```

### Parallel operations (FB5+)

```sql
-- Server config: firebird.conf
-- ParallelWorkers = 4

-- gbak parallel backup
$svc = fbird_service_attach('localhost', 'sysdba', 'masterkey');
fbird_backup($svc, '/data/employee.fdb', '/backup/emp.fbk', [
    FBIRD_BKP_PARALLEL_WORKERS => 4,
]);

-- Query current parallel worker setting
SELECT RDB$GET_CONTEXT('SYSTEM', 'PARALLEL_WORKERS') FROM rdb$database;
```

### Profiler (FB5+)

```sql
-- Profile a specific session's workload
SELECT RDB$PROFILER.START_SESSION('weekly_report_audit') FROM rdb$database;

-- ... run workload ...

SELECT RDB$PROFILER.STOP_SESSION() FROM rdb$database;

-- Find slow routines
SELECT rdb$routine_name, rdb$elapsed_time
FROM plg$prof_record_source_stats_view
ORDER BY rdb$elapsed_time DESC
ROWS 10;
```

Source: [profiler @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/sql.extensions/README.profiler.md)

### Cost-based join selection (FB5+)

FB5's optimizer automatically chooses between nested-loop and hash joins.
No code changes needed. Verify via `SET PLAN ON`:

```sql
SET PLAN ON;
SELECT * FROM large_table l JOIN small_table s ON l.id = s.ref_id;
-- Plan should show "HASH JOIN" for large/small table combinations
```

Source: [README.Optimizer @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/README.Optimizer.txt)

### DECFLOAT vs NUMERIC vs DOUBLE

| Type | Use case | Precision | Performance |
|------|----------|-----------|-------------|
| `DECFLOAT(34)` | Financial calculations | 34 significant digits | Good (hardware-accelerated in FB4+) |
| `NUMERIC(18,2)` | Currency, standard accounting | 18 digits | Fastest (64-bit integer internally) |
| `NUMERIC(38,0)` | Very large integers | 38 digits | Slower (INT128 internally) |
| `DOUBLE PRECISION` | Scientific computing | ~15-17 digits | Fastest float |

---

## 9. Backup, Restore & Replication

### gbak (logical backup)

```php
$svc = fbird_service_attach('localhost', 'sysdba', 'masterkey');

// Full backup
fbird_backup($svc, '/data/employee.fdb', '/backup/emp_' . date('Ymd') . '.fbk');

// Backup with options
fbird_backup($svc, '/data/employee.fdb', '/backup/emp.fbk', [
    FBIRD_BKP_IGNORE_DATA_TBL    => ['AUDIT_LOG'],  // Skip large tables
    FBIRD_BKP_PARALLEL_WORKERS   => 4,               // [FB5]
    FBIRD_BKP_VERBOSE            => true,
    FBIRD_BKP_VERBOSE_THRESHOLD  => 5000,            // [FB3]
]);

// Restore
fbird_restore($svc, '/backup/emp.fbk', '/data/employee_new.fdb', [
    FBIRD_RST_REPLACE            => true,
    FBIRD_RST_PARALLEL_WORKERS   => 4,                // [FB5]
]);
```

Source: [README.gbak @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/README.gbak)

### NBackup (file-level incremental)

```sql
-- Until #476 adds native PHP API, use SQL or CLI:
ALTER DATABASE BEGIN BACKUP;
-- ... OS-level file copy ...
ALTER DATABASE END BACKUP;
```

Source: [CORE-2216 @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/CHANGELOG.md)

### Logical replication (FB4+)

Server-side feature, configured via `databases.conf` and `replication.conf`.

```sql
-- Check replication mode (FB5+)
SELECT MON$REPLICA_MODE FROM MON$DATABASE;

-- Publication status
SELECT * FROM RDB$PUBLICATIONS;
SELECT * FROM RDB$PUBLICATION_TABLES;
```

Source: [README.replication @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/README.replication.md)

---

## 10. Diagnostics & Monitoring

### MON$ monitoring tables

```php
// Find long-running statements
$res = fbird_query($conn, "
    SELECT
        a.MON\$USER,
        a.MON\$REMOTE_ADDRESS,
        s.MON\$SQL_TEXT,
        s.MON\$TIMESTAMP,
        s.MON\$STATEMENT_ID
    FROM MON\$STATEMENTS s
    JOIN MON\$ATTACHMENTS a ON s.MON\$ATTACHMENT_ID = a.MON\$ATTACHMENT_ID
    WHERE s.MON\$STATE = 1                    -- Active
    AND   s.MON\$TIMESTAMP < CURRENT_TIMESTAMP - 1/24  -- Running > 1 hour
    ORDER BY s.MON\$TIMESTAMP
");

// Kill a stuck attachment
fbird_kill_attachment($conn, $stuckAttachmentId);
```

Source: [README.monitoring_tables @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/README.monitoring_tables)

### Online validation

```php
$svc = fbird_service_attach('localhost', 'sysdba', 'masterkey');
fbird_maintain_db($svc, '/data/employee.fdb', [
    'action' => FBIRD_PRP_VALIDATE_DB,
]);
```

Source: [README.online_validation @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/README.online_validation)

### Error handling patterns

```php
// Procedural: check errors after each call
$res = fbird_query($conn, "SELECT ...");
if (!$res) {
    $code = fbird_errcode();
    $msg  = fbird_errmsg();
    $sqlstate = fbird_sqlstate();  // SQL-2003 standard 5-char code
    throw new DatabaseException("Query failed [$sqlstate]: $msg (code $code)");
}

// Exception mode (cleaner)
ini_set('fbird.enable_exceptions', 1);
try {
    $res = fbird_query($conn, "SELECT ...");
} catch (Firebird\DatabaseException $e) {
    printf("SQLSTATE %s: %s\n", $e->getSqlState(), $e->getMessage());
}
```

### Common SQLSTATE codes

| SQLSTATE | Meaning | Action |
|----------|---------|--------|
| `08006` | Connection lost | Reconnect |
| `28000` | Invalid authorization | Check credentials |
| `42S22` | Column not found | Fix query |
| `23000` | Integrity constraint violation | Check FK/unique |
| `40001` | Serialization failure (deadlock) | Retry transaction |
| `40P01` | Deadlock detected | Retry with backoff |
| `HYT00` | Timeout (FB4+ statement timeout) | Increase timeout or optimize query |
