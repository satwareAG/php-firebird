# Examples & Recipes

Practical examples for the php-firebird extension, highlighting **unique features** not found in other PHP database extensions.

---

## Table of Contents

- [Basic Operations](#basic-operations)
- [Unique Features](#unique-features-)
  - [Transaction-Aware Queries](#1-transaction-aware-queries)
  - [Multi-Database Transactions](#2-multi-database-transactions)
  - [INI Credential Fallback](#3-ini-credential-fallback)
  - [Connection Reuse & Force New](#4-connection-reuse--force-new)
- [Advanced Topics](#advanced-topics)
- [Migration Recipes](#migration-recipes)

---

## Basic Operations

### Connecting to a Database

```php
<?php
// Local database
$db = fbird_connect('/var/firebird/data/employee.fdb', 'SYSDBA', 'masterkey');

// Remote database
$db = fbird_connect('server:/path/to/database.fdb', 'SYSDBA', 'masterkey');

// With charset
$db = fbird_connect($database, 'SYSDBA', 'masterkey', 'UTF8');

// Check connection
if (!$db) {
    die('Connection failed: ' . fbird_errmsg());
}

fbird_close($db);
?>
```

### Executing Queries

```php
<?php
$db = fbird_connect($database, $user, $password);

// Simple query
$result = fbird_query($db, "SELECT * FROM employees WHERE department = 'Sales'");

// Fetch results
while ($row = fbird_fetch_assoc($result)) {
    echo $row['FIRST_NAME'] . ' ' . $row['LAST_NAME'] . "\n";
}

// Parameterized query (prevents SQL injection)
$result = fbird_query($db, 
    "SELECT * FROM employees WHERE salary > ? AND department = ?",
    50000, 'Engineering'
);

fbird_free_result($result);
fbird_close($db);
?>
```

### Prepared Statements

```php
<?php
$db = fbird_connect($database, $user, $password);

// Prepare once
$stmt = fbird_prepare($db, "INSERT INTO logs (message, created_at) VALUES (?, CURRENT_TIMESTAMP)");

// Execute multiple times
$messages = ['User logged in', 'Action performed', 'User logged out'];
foreach ($messages as $msg) {
    fbird_execute($stmt, $msg);
}

fbird_free_query($stmt);
fbird_close($db);
?>
```

### Basic Transaction

```php
<?php
$db = fbird_connect($database, $user, $password);

// Start transaction
$trans = fbird_trans($db);

try {
    fbird_query($trans, "UPDATE accounts SET balance = balance - 100 WHERE id = 1");
    fbird_query($trans, "UPDATE accounts SET balance = balance + 100 WHERE id = 2");
    
    fbird_commit($trans);
    echo "Transfer successful\n";
} catch (Exception $e) {
    fbird_rollback($trans);
    echo "Transfer failed: " . $e->getMessage() . "\n";
}

fbird_close($db);
?>
```

---

## Unique Features ⭐

These features are **unique to php-firebird** and not found in mysqli, pgsql, or PDO.

### 1. Transaction-Aware Queries

**Problem**: In other extensions, queries always use the connection's implicit transaction.

**Solution**: php-firebird allows passing a transaction handle directly to `fbird_query()`.

```php
<?php
// mysqli: query always uses connection's transaction context
mysqli_query($conn, $sql);  // No explicit transaction control

// pgsql: same limitation
pg_query($conn, $sql);  // No explicit transaction control

// php-firebird: query can use EXPLICIT transaction
$db = fbird_connect($database, $user, $password);

// Create specific transactions with different isolation levels
$trans_read = fbird_trans(FBIRD_READ | FBIRD_COMMITTED, $db);
$trans_write = fbird_trans(FBIRD_WRITE | FBIRD_CONSISTENCY, $db);

// Queries explicitly use the desired transaction
$read_result = fbird_query($trans_read, "SELECT * FROM inventory");
$write_result = fbird_query($trans_write, "UPDATE inventory SET qty = qty - 1 WHERE id = ?", $id);

// Independent commit/rollback
fbird_commit($trans_read);   // Commits read transaction
fbird_commit($trans_write);  // Commits write transaction separately

fbird_close($db);
?>
```

**Use Cases**:
- Multiple concurrent transactions with different isolation levels
- Long-running read transaction while short-lived writes occur
- Explicit control over transaction boundaries within same connection

### 2. Multi-Database Transactions

**Problem**: Most extensions only support transactions within a single database.

**Solution**: php-firebird can span a single transaction across multiple databases.

```php
<?php
// UNIQUE FEATURE: Single transaction spanning TWO databases!
$db1 = fbird_connect('server:/db/accounting.fdb', 'SYSDBA', 'masterkey');
$db2 = fbird_connect('server:/db/inventory.fdb', 'SYSDBA', 'masterkey');

// Single transaction with BOTH databases
// Note: Parameters are (flags, db, flags, db, ...)
$trans = fbird_trans(
    FBIRD_WRITE, $db1,   // Write access to accounting
    FBIRD_WRITE, $db2    // Write access to inventory
);

try {
    // Update accounting database
    fbird_query($trans, "UPDATE accounts SET balance = balance - 500 WHERE id = ?", $customerId);
    
    // Update inventory database (SAME transaction!)
    fbird_query($trans, "UPDATE products SET stock = stock - 1 WHERE id = ?", $productId);
    
    // ATOMIC COMMIT - both databases or neither!
    fbird_commit($trans);
    echo "Purchase completed (both databases updated atomically)\n";
    
} catch (Exception $e) {
    // ATOMIC ROLLBACK - undoes changes in BOTH databases
    fbird_rollback($trans);
    echo "Purchase failed: " . $e->getMessage() . "\n";
}

fbird_close($db1);
fbird_close($db2);
?>
```

**⚠️ Parameter Order Warning**:
```php
// CORRECT: flags, db, flags, db, ...
$trans = fbird_trans(FBIRD_WRITE, $db1, FBIRD_WRITE, $db2);

// WRONG: db, flags, db, flags, ...
$trans = fbird_trans($db1, FBIRD_WRITE, $db2, FBIRD_WRITE);  // Error!
```

**Use Cases**:
- Distributed transactions across multiple Firebird servers
- Data synchronization between databases
- Cross-database integrity constraints

### 3. INI Credential Fallback

**Problem**: Hardcoding credentials in every `fbird_connect()` call is insecure and inflexible.

**Solution**: Configure defaults in `php.ini`, omit from code.

```ini
; php.ini
fbird.default_user = "SYSDBA"
fbird.default_password = "masterkey"
fbird.default_charset = "UTF8"
```

```php
<?php
// Credentials automatically loaded from php.ini
$db = fbird_connect('/var/firebird/data/employee.fdb');

// Explicitly provided credentials override INI
$db = fbird_connect($database, 'other_user', 'other_pass');

// Also works with service manager
$service = fbird_service_attach('localhost');  // Uses INI credentials
?>
```

**Also Works With**:
```php
// Service manager
fbird_service_attach($host);  // Uses fbird.default_user/password

// Persistent connections
fbird_pconnect($database);    // Uses fbird.default_user/password
```

### 4. Connection Reuse & Force New

**Default Behavior**: php-firebird reuses connections (efficient).

**Escape Hatch**: `FBIRD_CONNECT_FORCE_NEW` creates fresh connection.

```php
<?php
// Default: connection REUSE (same credentials = same connection)
$db1 = fbird_connect($dsn, 'SYSDBA', 'masterkey');
$db2 = fbird_connect($dsn, 'SYSDBA', 'masterkey');

// $db1 and $db2 are THE SAME connection!
var_dump($db1 === $db2);  // bool(true)

// Force NEW connection (matches PostgreSQL PGSQL_CONNECT_FORCE_NEW)
$db3 = fbird_connect($dsn, 'SYSDBA', 'masterkey', null, 0, 0, null, FBIRD_CONNECT_FORCE_NEW);

// $db3 is a DIFFERENT connection
var_dump($db1 === $db3);  // bool(false)
?>
```

**Use Cases**:
- Testing connection isolation
- Different session settings per connection
- Connection pooling reset scenarios

---

## Advanced Topics

### Transaction Isolation Levels

```php
<?php
$db = fbird_connect($database, $user, $password);

// SNAPSHOT isolation (CONCURRENCY) - default Firebird behavior
$trans = fbird_trans(FBIRD_CONCURRENCY, $db);

// SERIALIZABLE isolation (CONSISTENCY) - strictest
$trans = fbird_trans(FBIRD_CONSISTENCY, $db);

// READ COMMITTED with record versions
$trans = fbird_trans(FBIRD_COMMITTED | FBIRD_REC_VERSION, $db);

// READ COMMITTED without record versions (blocks on uncommitted)
$trans = fbird_trans(FBIRD_COMMITTED | FBIRD_REC_NO_VERSION, $db);

// With lock timeout (WAIT mode, 5 second timeout)
$trans = fbird_trans(FBIRD_WRITE | FBIRD_WAIT | FBIRD_LOCK_TIMEOUT, 5, $db);
?>
```

### Savepoints

```php
<?php
$db = fbird_connect($database, $user, $password);
$trans = fbird_trans($db);

fbird_query($trans, "INSERT INTO users (name) VALUES ('Alice')");

// Create savepoint
fbird_savepoint($trans, 'sp1');

fbird_query($trans, "INSERT INTO users (name) VALUES ('Bob')");

// Create another savepoint
fbird_savepoint($trans, 'sp2');

fbird_query($trans, "INSERT INTO users (name) VALUES ('Charlie')");

// Rollback to sp2 (keeps Alice and Bob)
fbird_rollback_savepoint($trans, 'sp2');

// Or rollback to sp1 (keeps only Alice)
// fbird_rollback_savepoint($trans, 'sp1');

fbird_commit($trans);
fbird_close($db);
?>
```

### BLOB Handling

```php
<?php
$db = fbird_connect($database, $user, $password);

// Write BLOB
$blob = fbird_blob_create($db);
fbird_blob_add($blob, "Large text content here...");
fbird_blob_add($blob, "More content...");
$blob_id = fbird_blob_close($blob);

fbird_query($db, "INSERT INTO documents (content) VALUES (?)", $blob_id);

// Read BLOB
$result = fbird_query($db, "SELECT content FROM documents WHERE id = 1");
$row = fbird_fetch_assoc($result, FBIRD_FETCH_BLOBS);
echo $row['CONTENT'];  // Full blob content

fbird_close($db);
?>
```

### Event Handling

```php
<?php
$db = fbird_connect($database, $user, $password);

// Register event handler
$event = fbird_set_event_handler($db, function($event_name, $count) {
    echo "Event '$event_name' fired $count time(s)\n";
}, 'new_order', 'order_shipped');

// Poll for events (non-blocking)
while (true) {
    $result = fbird_poll_event($event, 1000);  // 1 second timeout
    
    if ($result === FBIRD_EVENT_TIMEOUT) {
        echo "No events, doing other work...\n";
    }
    
    // Break condition
    if (check_shutdown_signal()) break;
}

fbird_free_event_handler($event);
fbird_close($db);
?>
```

### Service Manager

```php
<?php
// Connect to service manager
$service = fbird_service_attach('localhost');  // Uses INI credentials

// Get server version
$version = fbird_server_info($service, FBIRD_SVC_SERVER_VERSION);
echo "Server: $version\n";

// Database backup
fbird_backup($service, '/var/firebird/data/employee.fdb', '/backup/employee.fbk', FBIRD_BKP_METADATA_ONLY, true);

// Database restore
fbird_restore($service, '/backup/employee.fbk', '/var/firebird/data/employee_copy.fdb', FBIRD_RES_CREATE);

// Get user list
$users = fbird_server_info($service, FBIRD_SVC_GET_USERS);
print_r($users);

fbird_service_detach($service);
?>
```

---

## Migration Recipes

### From ibase_* to fbird_*

```php
<?php
// OLD (deprecated but still works)
$db = ibase_connect($database, $user, $password);
$result = ibase_query($db, "SELECT * FROM users");
while ($row = ibase_fetch_assoc($result)) {
    print_r($row);
}
ibase_close($db);

// NEW (recommended)
$db = fbird_connect($database, $user, $password);
$result = fbird_query($db, "SELECT * FROM users");
while ($row = fbird_fetch_assoc($result)) {
    print_r($row);
}
fbird_close($db);
?>
```

### Updating Constants

```php
<?php
// OLD (will NOT work - no BC aliases for constants)
$trans = fbird_trans(IBASE_READ | IBASE_COMMITTED, $db);

// NEW (required)  
$trans = fbird_trans(FBIRD_READ | FBIRD_COMMITTED, $db);
?>
```

### Migration Script Helper

```bash
#!/bin/bash
# migrate_firebird.sh - Find and report legacy usage

echo "=== Checking for ibase_* function calls ==="
grep -rn "ibase_" --include="*.php" .

echo ""
echo "=== Checking for IBASE_* constants ==="
grep -rn "IBASE_[A-Z]" --include="*.php" .

echo ""
echo "=== Checking for INI directives ==="
grep -rn "ibase\." --include="*.ini" --include="*.php" .
```

---

## Complete Working Examples

### Order Processing System

```php
<?php
/**
 * Complete example: Order processing with multi-database transactions
 */

$db_orders = fbird_connect('server:/db/orders.fdb', 'SYSDBA', 'masterkey');
$db_inventory = fbird_connect('server:/db/inventory.fdb', 'SYSDBA', 'masterkey');

function process_order($customer_id, $product_id, $quantity) {
    global $db_orders, $db_inventory;
    
    // Atomic transaction across BOTH databases
    $trans = fbird_trans(
        FBIRD_WRITE | FBIRD_COMMITTED, $db_orders,
        FBIRD_WRITE | FBIRD_COMMITTED, $db_inventory
    );
    
    try {
        // Check inventory (in inventory database)
        $result = fbird_query($trans, 
            "SELECT stock, price FROM products WHERE id = ?", 
            $product_id
        );
        $product = fbird_fetch_assoc($result);
        
        if (!$product || $product['STOCK'] < $quantity) {
            throw new Exception("Insufficient stock");
        }
        
        $total = $product['PRICE'] * $quantity;
        
        // Create order (in orders database)
        fbird_query($trans,
            "INSERT INTO orders (customer_id, product_id, quantity, total, created_at) 
             VALUES (?, ?, ?, ?, CURRENT_TIMESTAMP)",
            $customer_id, $product_id, $quantity, $total
        );
        
        // Reduce inventory (in inventory database)
        fbird_query($trans,
            "UPDATE products SET stock = stock - ? WHERE id = ?",
            $quantity, $product_id
        );
        
        // ATOMIC COMMIT - both databases updated or neither
        fbird_commit($trans);
        return ['success' => true, 'total' => $total];
        
    } catch (Exception $e) {
        fbird_rollback($trans);
        return ['success' => false, 'error' => $e->getMessage()];
    }
}

// Usage
$result = process_order(customer_id: 42, product_id: 100, quantity: 2);
if ($result['success']) {
    echo "Order placed! Total: $" . number_format($result['total'], 2);
} else {
    echo "Order failed: " . $result['error'];
}

fbird_close($db_orders);
fbird_close($db_inventory);
?>
```

---

## See Also

- [API Reference](API_REFERENCE.md) - Complete function documentation
- [CHANGELOG](../CHANGELOG.md) - Version history
- [Documentation Standards](DOCUMENTATION_STANDARDS.md) - Contributing guidelines
