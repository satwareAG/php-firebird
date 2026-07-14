# Cheatsheet: Performance & Concurrency

Performance, concurrency, and optimization features in Firebird 3.0, 4.0, and 5.0.

**Source inventory**: [feature-inventory.md rows #66-#80](../feature-inventory.md#6-performance--concurrency)

---

## True SMP Support for SuperServer

- **Introduced**: Firebird 3.0 (CORE-775)
- **Status**: N/A (engine-internal)

SuperServer in FB3+ uses true symmetric multiprocessing: multiple CPU cores
process requests concurrently within a single server process. No driver-side
configuration needed - this is transparent to PHP applications.

Configure via `firebird.conf`:
```text
ServerMode = Super
# CPU affinity and thread count are auto-detected
```

---

## READ CONSISTENCY Isolation Level

- **Introduced**: Firebird 4.0 (CORE-5953)
- **Source**: [README.read_consistency @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/README.read_consistency.md)
- **OOP tracked**: [#425](https://github.com/satwareAG/php-firebird/issues/425)

Read consistency gives each statement a stable view of the database without
locking writers, eliminating the "garbage collection" overhead of traditional
read-committed transactions.

**SQL**:
```sql
SET TRANSACTION READ COMMITTED READ CONSISTENCY;
```

**PHP**:
```php
// Procedural - native constant
$trx = fbird_trans(FBIRD_READ_CONSISTENCY, $cxn);
// or via options array:
$trx = fbird_trans([
    'read_committed'      => true,
    'read_consistency'    => true,
    'rec_version'         => false,
], $cxn);

// OOP - PARTIAL (#425)
$conn->beginTransaction([
    'read_committed'   => true,
    'read_consistency' => true,
]);

// PDO - via SQL passthrough
$pdo->exec("SET TRANSACTION READ COMMITTED READ CONSISTENCY");
```

**Driver matrix**:

| Layer | Status | Note |
|-------|--------|------|
| `fbird_*` | Y (native) | `FBIRD_READ_CONSISTENCY` constant + `read_consistency` option |
| `Firebird\*` | P | Native method tracked in [#425](https://github.com/satwareAG/php-firebird/issues/425) |
| `pdo_fbird` | P | [#425](https://github.com/satwareAG/php-firebird/issues/425) |

---

## Statement-Level Timeouts

- **Introduced**: Firebird 4.0 (CORE-5488)
- **Source**: [README.statement_timeouts @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/README.statement_timeouts)

**SQL** (via API, not SQL syntax):
```sql
-- Statement timeout is set via the API, not SQL.
-- It applies to the next query executed on that statement.
```

**PHP**:
```php
// Procedural - native API
fbird_set_statement_timeout($cxn, 5);  // 5 seconds for next statement
$res = fbird_query($cxn, "SELECT ... complex query ...");
// Timeout resets after each query. Check current:
$timeout = fbird_get_statement_timeout($cxn);

// OOP
$conn->setStatementTimeout(5);
$rs = $conn->prepare("SELECT ...")->execute();

// Query the current timeout:
echo $conn->getStatementTimeout(); // 5
```

**Driver matrix**:

| Layer | Status | Note |
|-------|--------|------|
| `fbird_*` | Y (native) | `fbird_set_statement_timeout` / `fbird_get_statement_timeout` |
| `Firebird\*` | Y (native) | `Connection::setStatementTimeout` / `getStatementTimeout` |
| `pdo_fbird` | N | [#464](https://github.com/satwareAG/php-firebird/issues/464) |

---

## Session Idle Timeouts

- **Introduced**: Firebird 4.0 (CORE-5488)
- **Source**: [README.session_idle_timeouts @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/README.session_idle_timeouts)

**PHP**:
```php
// Procedural - set idle timeout (connection auto-disconnects after N seconds idle)
fbird_set_idle_timeout($cxn, 300);  // 5 minutes idle
$remaining = fbird_get_idle_timeout($cxn);

// OOP
$conn->setIdleTimeout(300);
echo $conn->getIdleTimeout(); // 300
```

**Driver matrix**:

| Layer | Status | Note |
|-------|--------|------|
| `fbird_*` | Y (native) | `fbird_set_idle_timeout` / `fbird_get_idle_timeout` |
| `Firebird\*` | Y (native) | `Connection::setIdleTimeout` / `getIdleTimeout` |
| `pdo_fbird` | N | [#422](https://github.com/satwareAG/php-firebird/issues/422) |

---

## Parallel Sweeping and Index Creation

- **Introduced**: Firebird 5.0 (#7447)
- **Source**: [README.parallel_features @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/README.parallel_features)
- **Status**: N/A (engine-internal, configurable via `firebird.conf`)
- **Config tracked**: [#427](https://github.com/satwareAG/php-firebird/issues/427)

Configure via `firebird.conf`:
```text
ParallelWorkers = 4
```

FB5 uses multiple threads for sweep and index creation automatically.

---

## Compiled Statement Cache

- **Introduced**: Firebird 5.0 (#7144)
- **Status**: N/A (engine-internal)

FB5 caches compiled statements server-side. Repeated execution of the same SQL
text reuses the cached plan without re-parsing. Transparent to the driver - no
PHP code changes needed.

---

## Cost-Based Hash Join

- **Introduced**: Firebird 5.0 (#7331)
- **Status**: N/A (optimizer-internal)

FB5's optimizer can choose between nested-loop and hash joins based on cost
estimation. Visible in the query plan output:

**SQL**:
```sql
SET PLAN ON;
SELECT * FROM large_table l JOIN small_table s ON l.id = s.ref_id;
-- Plan shows "HASH JOIN" or "NESTED LOOP JOIN" based on cost
```

**PHP**:
```php
// Enable plan display via SQL passthrough
fbird_query($cxn, "SET PLAN ON");
// Read plan from sql_info (existing API):
$res = fbird_query($cxn, "SELECT ... ");
// The plan is available via isc_info_sql_get_plan metadata
```

**Driver matrix**: N/A on all three layers. Optimizer is server-side.

---

## PSQL/SQL Profiler (RDB$PROFILER)

- **Introduced**: Firebird 5.0 (#7086)
- **Source**: [profiler @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/sql.extensions/README.profiler.md)
- **Plugin API tracked**: [#428](https://github.com/satwareAG/php-firebird/issues/428)

**SQL**:
```sql
-- Start profiling the current session
SELECT RDB$PROFILER.START_SESSION('weekly_report_audit');

-- ... run your workload ...

-- Stop and flush
SELECT RDB$PROFILER.STOP_SESSION();

-- Query results:
SELECT
    pps.rdb$package_name,
    pps.rdb$routine_name,
    pps.rdb$source_type,
    ppstats.rdb$elapsed_time,
    ppstats.rdb$page_reads,
    ppstats.rdb$record_reads
FROM plg$prof_record_sources pps
JOIN plg$prof_record_source_stats_view ppstats
    ON pps.rdb$profile_id = ppstats.rdb$profile_id
ORDER BY ppstats.rdb$elapsed_time DESC;
```

**PHP**:
```php
// Start profiling before running your workload
fbird_query($cxn, "SELECT RDB\$PROFILER.START_SESSION('debug_2026_07_14') FROM rdb\$database");

// Run workload
fbird_query($cxn, "EXECUTE PROCEDURE heavy_report_proc");

// Stop profiling
fbird_query($cxn, "SELECT RDB\$PROFILER.STOP_SESSION() FROM rdb\$database");

// Read the profile results
$res = fbird_query($cxn, "
    SELECT rdb$routine_name, rdb$elapsed_time
    FROM plg\$prof_record_source_stats_view
    ORDER BY rdb\$elapsed_time DESC
    ROWS 10
");
while ($row = fbird_fetch_assoc($res)) {
    printf("%-40s %d ms\n", $row['RDB$ROUTINE_NAME'], $row['RDB$ELAPSED_TIME']);
}
```

**Driver matrix**:

| Layer | Status | Note |
|-------|--------|------|
| `fbird_*` | Y (SQL) | Profile via SQL queries to `RDB$PROFILER` / `PLG$PROF_*` |
| `Firebird\*` | Y (SQL) | Same. Plugin API in [#428](https://github.com/satwareAG/php-firebird/issues/428) |
| `pdo_fbird` | Y (SQL) | Same |

---

## Database Linger

- **Introduced**: Firebird 3.0 (CORE-4263)
- **Source**: [linger @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.linger)

**SQL**:
```sql
-- Keep the database in cache for N seconds after last disconnect
ALTER DATABASE SET LINGER TO 30;
-- Disable:
ALTER DATABASE DROP LINGER;
```

**Driver matrix**: Y (SQL) on all three layers.

---

## Online Table/Index Validation

- **Introduced**: Firebird 3.0 (CORE-4707)
- **Source**: [README.online_validation @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/README.online_validation)

**PHP**:
```php
// Procedural - via service manager
$svc = fbird_service_attach('localhost', 'sysdba', 'masterkey');

// Validate table online (no exclusive lock needed)
fbird_maintain_db($svc, '/data/employee.fdb', [
    'action'  => FBIRD_PRP_VALIDATE_TABLE,
    'table'   => 'CUSTOMERS',
]);

// OOP
$svc = new Firebird\Service('localhost', 'sysdba', 'masterkey');
$svc->validateTable('/data/employee.fdb', 'CUSTOMERS');
```

**Driver matrix**:

| Layer | Status | Note |
|-------|--------|------|
| `fbird_*` | Y (native) | `fbird_maintain_db` with validation options |
| `Firebird\*` | Y (native) | `Service` class |
| `pdo_fbird` | N | Service API not available via PDO |

---

## Detailed Query Execution Plan

- **Introduced**: Firebird 3.0 (CORE-3332)
- **Source**: [plan @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.plan)

**SQL**:
```sql
-- Enable plan display (isql)
SET PLAN ON;
-- FB5: also show BLR (Binary Language Representation)
SET EXEC_PATH_DISPLAY BLR;

-- Explicitly specify a plan (advanced)
SELECT * FROM customers WHERE city = 'Berlin'
PLAN (customers INDEX (idx_city));
```

**PHP**:
```php
// Retrieve the plan programmatically
$res = fbird_query($cxn, "SELECT * FROM customers WHERE city = 'Berlin'");
// The plan is available via fbird_field_info / statement info metadata
// For full plan text, query MON$STATEMENTS:
$stmtInfo = fbird_query($cxn, "
    SELECT MON$SQL_TEXT, MON$COMPILED_STATEMENT_ID
    FROM MON$STATEMENTS
    WHERE MON$ATTACHMENT_ID = CURRENT_CONNECTION
");
```

**Driver matrix**: Y (SQL/native) on all three layers.

---

## Expression Indices

- **Introduced**: Firebird 3.0
- **Source**: [expression_indices @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.expression_indices)

**SQL**:
```sql
-- Index on an expression (not just a column)
CREATE INDEX idx_email_lower ON contacts (LOWER(email));
CREATE INDEX idx_full_name ON employees (UPPER(last_name), UPPER(first_name));
CREATE DESCENDING INDEX idx_created ON orders (EXTRACT(YEAR FROM created_at), created_at);

-- Used automatically by the optimizer:
SELECT * FROM contacts WHERE LOWER(email) = 'alice@example.com';
```

**Driver matrix**: Y (SQL) on all three layers.

---

## Partial Indices (WHERE clause)

- **Introduced**: Firebird 5.0 (#3750)
- **Source**: [partial_indices @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/sql.extensions/README.partial_indices)

**SQL**:
```sql
-- Index only active records (smaller index, faster writes)
CREATE INDEX idx_active_customers ON customers (name)
WHERE status = 'active';

-- Index only recent orders
CREATE INDEX idx_recent_orders ON orders (customer_id, order_date)
WHERE order_date >= CURRENT_DATE - 90;

-- The optimizer uses partial indices automatically:
SELECT * FROM customers WHERE status = 'active' AND name LIKE 'A%';
```

**PHP**:
```php
// Create partial index via DDL passthrough
fbird_query($cxn, "
    CREATE INDEX idx_active_customers ON customers (name)
    WHERE status = 'active'
");

// Queries that match the WHERE clause use the index automatically
$res = fbird_query($cxn,
    "SELECT * FROM customers WHERE status = 'active' AND name LIKE ?",
    ['A%']
);
```

**Driver matrix**: Y (SQL) on all three layers.

---

## Optimization Modes Surfaced

- **Introduced**: Firebird 5.0 (#7405)
- **Source**: [CHANGELOG @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/CHANGELOG.md)

**SQL**:
```sql
-- Control optimizer behavior at session level
SET OPTIMIZE FOR FIRST_ROWS;    -- Optimize for quick first row (interactive UIs)
SET OPTIMIZE FOR ALL_ROWS;      -- Optimize for full result set (batch/reporting)

-- Or via firebird.conf:
-- OptimizerModeForRegression = true  (for regression testing)
```

**Driver matrix**: Y (SQL) on all three layers.

---

## ParallelWorkers Config Default (FB5)

- **Introduced**: Firebird 5.0 (#7682)
- **Source**: [CHANGELOG @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/CHANGELOG.md)
- **Status**: N/A (server config), [#427](https://github.com/satwareAG/php-firebird/issues/427)

Configure via `firebird.conf`:
```text
ParallelWorkers = 4
```

Query the active setting per attachment:
```sql
SELECT MON$PARALLEL_WORKERS FROM MON$ATTACHMENTS WHERE MON$ATTACHMENT_ID = CURRENT_CONNECTION;
```
