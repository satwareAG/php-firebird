# Migration Guide: v10.x to v11.0

> v11.0 is a **breaking change** release. All `fbird_*` procedural functions that
> previously returned PHP `resource` handles now return typed `Firebird\*` objects.

## PHP Version Requirement

v11.0 requires **PHP 8.2+**. PHP 8.1 reached end-of-life in November 2024.

## What Changed

| Function(s) | v10 return type | v11 return type |
|-------------|-----------------|-----------------|
| `fbird_connect()`, `fbird_pconnect()`, `fbird_create_database()` | `resource\|false` | `Firebird\Connection\|false` |
| `fbird_trans()`, `fbird_trans_start()` | `resource\|false` | `Firebird\Transaction\|false` |
| `fbird_query()`, `fbird_execute()` | `resource\|false` | `Firebird\ResultSet\|false` |
| `fbird_blob_create()`, `fbird_blob_open()`, `fbird_blob_create_seekable()`, `fbird_blob_open_seekable()` | `resource\|false` | `Firebird\Blob\|false` |
| `fbird_set_event_handler()`, `fbird_wait_event()` | `resource\|false` | `Firebird\Event\|false` |
| `fbird_service_attach()` | `resource\|false` | `Firebird\Service\|false` |

All consuming functions (`fbird_commit()`, `fbird_rollback()`, `fbird_close()`,
`fbird_query()`, `fbird_fetch_row()`, etc.) accept **both** the new objects and
legacy `resource` handles via dual-accept bridges. This eases gradual migration.

## Breaking Change: `is_resource()` Checks

The most common breaking change is code that tests connection or result handles
with `is_resource()`. Objects are not resources, so these checks will now return
`false` even for valid handles.

```php
// v10 - BREAKS in v11
$conn = fbird_connect($dsn, $user, $pass);
if (is_resource($conn)) {
    // ...
}

// v11 - correct
$conn = fbird_connect($dsn, $user, $pass);
if ($conn instanceof \Firebird\Connection) {
    // ...
}

// v11 - works for both old and new code
$conn = fbird_connect($dsn, $user, $pass);
if ($conn !== false) {
    // simplest pattern - just check for false
}
```

## Migration by Function Family

### Connection Functions

```php
// v10
$dbh = fbird_connect('localhost:/var/db/test.fdb', 'SYSDBA', 'masterkey');
if (!is_resource($dbh)) {
    die(fbird_errmsg());
}
fbird_close($dbh);

// v11
$dbh = fbird_connect('localhost:/var/db/test.fdb', 'SYSDBA', 'masterkey');
if ($dbh === false) {
    die(fbird_errmsg());
}
fbird_close($dbh); // still works - dual-accept
```

### Transaction Functions

```php
// v10
$trans = fbird_trans(IBASE_DEFAULT, $dbh);
if (!is_resource($trans)) {
    die('Transaction failed');
}

// v11
$trans = fbird_trans(IBASE_DEFAULT, $dbh);
if ($trans === false) {
    die('Transaction failed');
}
// $trans is now Firebird\Transaction
```

### Query and Result Functions

```php
// v10
$result = fbird_query($dbh, 'SELECT * FROM users');
if (!is_resource($result)) {
    die(fbird_errmsg());
}
while ($row = fbird_fetch_assoc($result)) {
    // ...
}
fbird_free_result($result);

// v11
$result = fbird_query($dbh, 'SELECT * FROM users');
if ($result === false) {
    die(fbird_errmsg());
}
while ($row = fbird_fetch_assoc($result)) {  // dual-accept
    // ...
}
fbird_free_result($result);  // dual-accept
```

### Blob Functions

```php
// v10
$blob = fbird_blob_create($dbh);
if (!is_resource($blob)) {
    die('Blob creation failed');
}

// v11
$blob = fbird_blob_create($dbh);
if ($blob === false) {
    die('Blob creation failed');
}
// $blob is now Firebird\Blob
```

### Event Functions

```php
// v10
$event = fbird_set_event_handler($dbh, 'my_callback', 'MY_EVENT');
if (!is_resource($event)) {
    die('Event handler failed');
}

// v11
$event = fbird_set_event_handler($dbh, 'my_callback', 'MY_EVENT');
if ($event === false) {
    die('Event handler failed');
}
// $event is now Firebird\Event
```

### Service Functions

```php
// v10
$svc = fbird_service_attach('localhost', 'SYSDBA', 'masterkey');
if (!is_resource($svc)) {
    die('Service attach failed');
}

// v11
$svc = fbird_service_attach('localhost', 'SYSDBA', 'masterkey');
if ($svc === false) {
    die('Service attach failed');
}
// $svc is now Firebird\Service
```

## Checking Object Types

| v10 pattern | v11 equivalent |
|-------------|----------------|
| `is_resource($dbh)` | `$dbh instanceof \Firebird\Connection` |
| `is_resource($trans)` | `$trans instanceof \Firebird\Transaction` |
| `is_resource($result)` | `$result instanceof \Firebird\ResultSet` |
| `is_resource($blob)` | `$blob instanceof \Firebird\Blob` |
| `is_resource($event)` | `$event instanceof \Firebird\Event` |
| `is_resource($svc)` | `$svc instanceof \Firebird\Service` |

## OOP API (Unchanged)

The `Firebird\Connection`, `Firebird\Database`, `Firebird\Transaction` OOP wrapper
classes introduced in v10.0 are unchanged. They continue to work as before.

## Dual-Accept Transition Period

All consuming functions accept both legacy `resource` handles and new `Firebird\*`
objects. This means you can migrate incrementally:

1. Update `is_resource()` checks to `!== false` or `instanceof` checks
2. Update PHPDoc `@param`/`@return` annotations
3. Update static analysis stubs (add `satwareag/php-firebird-stubs` as dev dependency)

## Static Analysis

Install the stubs package for PHPStan/Psalm type coverage:

```bash
composer require --dev satwareag/php-firebird-stubs
```

Add to `phpstan.neon`:

```yaml
parameters:
    stubFiles:
        - vendor/satwareag/php-firebird-stubs/firebird-stubs.php
        - vendor/satwareag/php-firebird-stubs/firebird-classes.php
```

## Summary

The minimum migration effort is:
1. Replace `is_resource($handle)` with `$handle !== false` throughout your codebase
2. Ensure PHP 8.2+ is available

No other code changes are required for basic functionality - all consuming functions
accept both old resources and new objects through dual-accept bridges.
