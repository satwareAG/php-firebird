# Cheatsheet: Administration & Operations

Administration, backup, monitoring, and diagnostic features in Firebird 3.0, 4.0, and 5.0.

**Source inventory**: [feature-inventory.md rows #95-#115](../feature-inventory.md#8-administration--operations)

---

## Online Table/Index Validation

- **Introduced**: Firebird 3.0 (CORE-4707)
- **Source**: [README.online_validation @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/README.online_validation)

**PHP**:
```php
$svc = fbird_service_attach('localhost', 'sysdba', 'masterkey');
// Validate full database online (no exclusive lock)
fbird_maintain_db($svc, '/data/employee.fdb', [
    'action' => FBIRD_PRP_VALIDATE_DB,
]);
// Validate a specific table
fbird_maintain_db($svc, '/data/employee.fdb', [
    'action' => FBIRD_PRP_VALIDATE_TABLE,
    'table'  => 'CUSTOMERS',
]);
```

**Driver matrix**:

| Layer | Status | Note |
|-------|--------|------|
| `fbird_*` | Y (native) | `fbird_maintain_db` with validation options |
| `Firebird\*` | Y (native) | `Service` class |
| `pdo_fbird` | N | Service API not available via PDO |

---

## Database Linger

- **Introduced**: Firebird 3.0 (CORE-4263)
- **Source**: [linger @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.linger)

**SQL**:
```sql
ALTER DATABASE SET LINGER TO 30;   -- Keep in memory 30s after last disconnect
ALTER DATABASE DROP LINGER;        -- Disable linger
```

**Driver matrix**: Y (SQL) on all three layers.

---

## gbak: Ignore Specific Tables Data

- **Introduced**: Firebird 3.0 (CORE-2208)
- **Source**: [README.gbak @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/README.gbak)

**PHP**:
```php
$svc = fbird_service_attach('localhost', 'sysdba', 'masterkey');

// Backup schema only for large log tables (skip data)
fbird_backup($svc, '/data/employee.fdb', '/backup/emp.fbk', [
    FBIRD_BKP_IGNORE_DATA_TBL  => ['AUDIT_LOG', 'TEMP_DATA'],
]);
```

**Driver matrix**:

| Layer | Status | Note |
|-------|--------|------|
| `fbird_*` | Y (native) | `fbird_backup` with options |
| `Firebird\*` | Y (native) | `Service::backup()` |
| `pdo_fbird` | N | Service API only |

---

## gbak: Backup Encrypted Databases

- **Introduced**: Firebird 4.0 (CORE-5808)
- **Source**: [README.gbak @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/README.gbak)

**PHP**:
```php
$svc = fbird_service_attach('localhost', 'sysdba', 'masterkey');
// gbak handles encrypted DBs transparently (key must be available to server)
fbird_backup($svc, '/data/encrypted.fdb', '/backup/enc.fbk');
```

**Driver matrix**: Y (native) on procedural and OOP layers.

---

## gbak: Parallel Backup/Restore (-PARALLEL)

- **Introduced**: Firebird 5.0 (#1783)
- **Source**: [README.gbak @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/README.gbak)

**PHP**:
```php
$svc = fbird_service_attach('localhost', 'sysdba', 'masterkey');

// FB5: parallel backup uses multiple threads
fbird_backup($svc, '/data/employee.fdb', '/backup/emp.fbk', [
    FBIRD_BKP_PARALLEL_WORKERS => 4,
]);

// Parallel restore
fbird_restore($svc, '/backup/emp.fbk', '/data/employee_new.fdb', [
    FBIRD_RST_PARALLEL_WORKERS => 4,
]);
```

**Driver matrix**: Y (native) on procedural and OOP layers.

---

## gbak: Custom Verbose Interval (-VT)

- **Introduced**: Firebird 3.0 (CORE-462)
- **Source**: [README.gbak @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/README.gbak)

**PHP**:
```php
$svc = fbird_service_attach('localhost', 'sysdba', 'masterkey');
// Verbose output every 5000 records (instead of every record)
fbird_backup($svc, '/data/employee.fdb', '/backup/emp.fbk', [
    FBIRD_BKP_VERBOSE_THRESHOLD => 5000,
    FBIRD_BKP_VERBOSE           => true,
]);
```

**Driver matrix**: Y (native) on procedural and OOP layers.

---

## gbak: Enhanced Restore Using Batch API

- **Introduced**: Firebird 4.0 (CORE-5952)
- **Source**: [README.gbak @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/README.gbak)

The restore engine internally uses the batch API for faster data loading.
Transparent to the driver - no API changes needed.

**Driver matrix**: Y (native, engine-level) on all layers.

---

## NBackup Online Dump

- **Introduced**: Firebird 4.0 (CORE-2216)
- **Gap tracked**: [#476](https://github.com/satwareAG/php-firebird/issues/476)

NBackup provides file-level incremental backups (`ALTER DATABASE BEGIN BACKUP`).

**SQL** (workaround until #476 is implemented):
```sql
-- Lock the database for file-level copy (FB4+)
ALTER DATABASE BEGIN BACKUP;
-- ... OS-level copy of the .fdb file ...
ALTER DATABASE END BACKUP;

-- Incremental level-N backup (requires nbackup CLI or service API)
-- Not available via SQL - requires nbackup utility
```

**PHP** (current workaround - shell out to nbackup):
```php
// Until #476 adds native API, use exec():
exec('nbackup -U sysdba -P masterkey -L /data/employee.fdb 2>&1', $output);
// Copy the locked file
copy('/data/employee.fdb', "/backup/emp_{$timestamp}.fdb");
exec('nbackup -U sysdba -P masterkey -N /data/employee.fdb 2>&1', $output);
```

**Driver matrix**:

| Layer | Status | Note |
|-------|--------|------|
| `fbird_*` | N | No nbackup API. Tracked in [#476](https://github.com/satwareAG/php-firebird/issues/476) |
| `Firebird\*` | N | [#476](https://github.com/satwareAG/php-firebird/issues/476) |
| `pdo_fbird` | N | Service API only |

---

## Logical Replication (Built-in)

- **Introduced**: Firebird 4.0 (CORE-2022)
- **Source**: [README.replication @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/README.replication.md)
- **Status**: N/A (server-side configuration)

Replication is configured via `databases.conf` and `replication.conf` on the
server. No PHP driver surface needed.

**SQL** (querying replication state):
```sql
-- Check replication mode (FB5+)
SET HEADER ON; SHOW DATABASE;

-- Query replication-related monitoring
SELECT MON$REPLICA_MODE FROM MON$DATABASE;

-- Publication status (FB5+)
SELECT * FROM RDB$PUBLICATIONS;
SELECT * FROM RDB$PUBLICATION_TABLES;
```

**Driver matrix**: N/A on all three layers. Server-side feature.

---

## Monitoring Tables (MON$*)

- **Introduced**: Firebird 3.0+ (extended in FB4 and FB5)
- **Source**: [README.monitoring_tables @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/README.monitoring_tables)

**SQL**:
```sql
-- Active connections
SELECT MON$USER, MON$REMOTE_PROTOCOL, MON$REMOTE_ADDRESS,
       MON$TIMESTAMP, MON$STATE
FROM MON$ATTACHMENTS;

-- Running statements
SELECT MON$ATTACHMENT_ID, MON$SQL_TEXT, MON$TIMESTAMP, MON$STAT_ID
FROM MON$STATEMENTS
WHERE MON$STATE = 1;  -- Active

-- Lock contention
SELECT MON$ATTACHMENT_ID, MON$LOCK_NAME, MON$LOCK_TIMEOUT
FROM MON$LOCK_STATS;
```

**PHP**:
```php
// Monitor active connections
$res = fbird_query($cxn, "
    SELECT MON\$USER, MON\$REMOTE_ADDRESS, MON\$TIMESTAMP, MON\$STATE
    FROM MON\$ATTACHMENTS
    ORDER BY MON\$TIMESTAMP DESC
");
while ($row = fbird_fetch_assoc($res)) {
    printf("%-20s %-20s %s state=%d\n",
        $row['MON$USER'],
        $row['MON$REMOTE_ADDRESS'],
        $row['MON$TIMESTAMP'],
        $row['MON$STATE']);
}

// Kill a stuck connection
fbird_kill_attachment($cxn, $stuckAttachmentId);
```

**Driver matrix**: Y (SQL) on all three layers.

---

## MON$COMPILED_STATEMENTS (FB5)

- **Introduced**: Firebird 5.0 (#7050)
- **Source**: [README.monitoring_tables @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/README.monitoring_tables)

**SQL**:
```sql
-- FB5: inspect compiled statement cache entries
SELECT cs.MON$COMPILED_STATEMENT_ID, cs.MON$POOL_ID,
       cs.MON$OBJECT_NAME, cs.MON$OBJECT_TYPE,
       s.MON$SQL_TEXT
FROM MON$COMPILED_STATEMENTS cs
LEFT JOIN MON$STATEMENTS s ON s.MON$COMPILED_STATEMENT_ID = cs.MON$COMPILED_STATEMENT_ID;
```

**Driver matrix**: Y (SQL) on all three layers.

---

## Connection Status in MON$ATTACHMENTS (FB4)

- **Introduced**: Firebird 4.0 (CORE-5536)
- **Source**: [README.monitoring_tables @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/README.monitoring_tables)

**SQL**:
```sql
-- FB4: check if connection is encrypted/compressed
SELECT MON$USER,
       MON$REMOTE_PROTOCOL,
       MON$ENCRYPTED,   -- FB4+
       MON$COMPRESSED   -- FB4+
FROM MON$ATTACHMENTS;
```

**Driver matrix**: Y (SQL) on all three layers.

---

## MON$SESSION_TIMEZONE (FB5)

- **Introduced**: Firebird 5.0 (#6794)
- **Source**: [README.monitoring_tables @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/README.monitoring_tables)

**SQL**:
```sql
-- FB5: view session timezone per connection
SELECT MON$USER, MON$SESSION_TIMEZONE FROM MON$ATTACHMENTS;
```

**Driver matrix**: Y (SQL) on all three layers.

---

## Trace Services (Firebird Trace)

- **Introduced**: Firebird 3.0+
- **Source**: [README.trace_services @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/README.trace_services)
- **Gap tracked**: [#477](https://github.com/satwareAG/php-firebird/issues/477)

Firebird trace sessions monitor SQL execution, connection events, and query
plans in real time.

**Current workaround** (shell out to fbtracemgr):
```php
// Until #477 adds native API, use exec():
$config = <<<TRACE
enabled = true
time_threshold = 100
max_sql_length = 1024
TRACE;
exec("echo '$config' | fbtracemgr -U sysdba -P masterkey -C -CONF -SE service_mgr 2>&1", $output);
```

**Driver matrix**:

| Layer | Status | Note |
|-------|--------|------|
| `fbird_*` | N | No trace service API. Tracked in [#477](https://github.com/satwareAG/php-firebird/issues/477). Wire-protocol trace is separate (#377) |
| `Firebird\*` | N | [#477](https://github.com/satwareAG/php-firebird/issues/477) |
| `pdo_fbird` | N | Service API only |

---

## External Connections Pool (Server-Side)

- **Introduced**: Firebird 4.0 (CORE-5990)
- **Source**: [external_connections_pool @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.external_connections_pool)
- **Status**: N/A (server-side configuration)

Server-side connection pooling for `EXECUTE STATEMENT ... ON EXTERNAL`.
Configured via `firebird.conf`:
```text
ExtConnPoolSize = 100
ExtConnPoolLifeTime = 7200
```

**Driver matrix**: N/A on all three layers.

---

## RDB$RECORD_VERSION Pseudocolumn

- **Introduced**: Firebird 3.0 (CORE-3291)
- **Source**: [WhatsNew @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/WhatsNew)

**SQL**:
```sql
-- Get the transaction ID that last modified each row
SELECT id, name, RDB$RECORD_VERSION AS rec_ver
FROM customers;

-- Use in optimistic locking:
SELECT id FROM customers WHERE id = 42 AND RDB$RECORD_VERSION = 12345
FOR UPDATE;
```

**Driver matrix**: Y (SQL) on all three layers.

---

## RDB$KEYWORDS System Table (FB5)

- **Introduced**: Firebird 5.0 (#6713)
- **Source**: [ddl @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/sql.extensions/README.ddl.txt)

**SQL**:
```sql
-- FB5: query reserved keywords
SELECT RDB$KEYWORD FROM RDB$KEYWORDS ORDER BY RDB$KEYWORD;

-- Check if a word is reserved
SELECT RDB$KEYWORD FROM RDB$KEYWORDS WHERE RDB$KEYWORD = 'BOOLEAN';
```

**PHP**:
```php
// Validate identifiers before DDL generation
$res = fbird_query($cxn, "SELECT RDB\$KEYWORD FROM RDB\$KEYWORDS");
$reserved = [];
while ($row = fbird_fetch_assoc($res)) {
    $reserved[] = $row['RDB$KEYWORD'];
}
if (in_array(strtoupper($newColumnName), $reserved)) {
    throw new InvalidArgumentException("$newColumnName is a reserved keyword");
}
```

**Driver matrix**: Y (SQL) on all three layers.

---

## Pseudo-Table of Users

- **Introduced**: Firebird 3.0 (CORE-2639)
- **Source**: [WhatsNew @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/WhatsNew)

**SQL**:
```sql
-- List users visible to the current user
SELECT * FROM RDB$USER_PRIVILEGES WHERE RDB$USER_TYPE = 8;
```

**Driver matrix**: Y (SQL) on all three layers.

---

## Inline Minor ODS Upgrade (FB5)

- **Introduced**: Firebird 5.0 (#7397)
- **Status**: N/A (engine-internal)

FB5 performs minor ODS (On-Disk Structure) upgrades inline during normal
operation, avoiding the need for backup/restore cycles. Transparent to PHP.

**Driver matrix**: N/A on all three layers.

---

## Database Encryption

- **Introduced**: Firebird 3.0 (CORE-657)
- **Source**: [WhatsNew @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/WhatsNew)

**SQL**:
```sql
-- Enable encryption (requires a crypt key plugin)
ALTER DATABASE ENCRYPT WITH "crypt_key_plugin";

-- Disable encryption
ALTER DATABASE DECRYPT;
```

**PHP**:
```php
// The encryption key must be provided by a server-side plugin.
// The PHP driver connects normally - the server handles encryption.
$conn = fbird_connect('localhost:/data/encrypted.fdb', 'sysdba', 'masterkey');

// Check encryption status via monitoring:
$res = fbird_query($cxn, "SELECT MON\$CRYPT_PAGE FROM MON\$DATABASE");
// Returns the number of encrypted pages
```

**Driver matrix**: Y (SQL) on all three layers. Key management is server-side.
