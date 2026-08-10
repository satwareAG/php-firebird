# PHP Firebird Extension (Modernized)

[![CI](https://github.com/satwareAG/php-firebird/actions/workflows/ci.yml/badge.svg)](https://github.com/satwareAG/php-firebird/actions/workflows/ci.yml)
[![License: PHP-3.01](https://img.shields.io/badge/License-PHP--3.01-blue.svg)](LICENSE)
[![PHP 8.2+](https://img.shields.io/badge/PHP-8.2%2B-8892BF.svg)](https://www.php.net/)
[![Version](https://img.shields.io/badge/version-13.2.1-blue.svg)](CHANGELOG.md)
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/satwareAG/php-firebird)

A high-performance PHP extension providing native connectivity to Firebird databases. This modernized version targets PHP 8.2+ with C++17 standards and comprehensive development tooling.

> **⚠️ Breaking Changes in v7.0**: This version uses `fbird_*` function names (not `ibase_*`) and builds as `firebird.so` (not `interbase.so`). See the [Migration Guide](#migration-guide) for upgrade instructions.

## Features

- **Native Performance**: Direct fbclient library integration
- **Full Firebird Support**: Connects to Firebird 3.0, 4.0, 5.0+ servers (tested against full 12-target PHP/Firebird matrix)
- **Modern C++ OO API**: Uses Firebird 3.0+ Object-Oriented API with RAII wrappers — zero legacy `isc_*` calls
- **Modern PHP**: Optimized for PHP 8.2+ with typed properties and attributes
- **Layer 1 — `fbird_*` procedural API**: Full-featured function-based interface for Firebird-specific features
- **Layer 2 — `Firebird\*` OOP classes**: Native C-registered PHP classes (`Exception`, `DatabaseException`, `TransactionException`, `Connection`, `Transaction`, `Statement`, `ResultSet`, `Blob`, `Service`, `Event`, `BatchHandle`, `DecFloat`) with Firebird-specific features
- **Layer 3 — `pdo_fbird` PDO driver**: Separate `pdo_fbird.so` extension with `fbird:` DSN prefix, compatible with PDO without colliding with PHP's bundled `pdo_firebird`. Load after `firebird.so`.
- **Exception Mode API**: PDO-style exception handling with runtime switchable error modes (SILENT/THROW)
- **Memory Safety**: Built with AddressSanitizer and comprehensive static analysis
- **Cross-Platform**: Linux, Windows, macOS support
- **Clean API**: `fbird_*` function prefix (no legacy InterBase naming)
- **LTO Optimized**: Link-Time Optimization enabled by default for GCC 8+ (auto-disabled for PHP < 8.3 and sanitizers)

## API Layers

This extension provides three complementary API layers:

| Feature | `PDO_Firebird` (bundled) | `pdo_fbird` (Layer 3) | `Firebird\*` (Layer 2) | `fbird_*` (Layer 1) |
|---------|--------------------------|----------------------|------------------------|---------------------|
| **API Style** | PDO (agnostic) | PDO (agnostic) | OOP (Firebird-specific) | Procedural |
| **DSN prefix** | `firebird:` | `fbird:` | N/A | N/A |
| **Events Support** | ❌ No | ✅ Yes (Attributes) | ✅ Yes | ✅ Yes |
| **Service API** | ❌ No | ✅ Yes (Attributes) | ✅ Yes | ✅ Full |
| **Array Fields** | ❌ No | ✅ Yes (Read/Write) | ❌ No | ✅ Yes |
| **BLOB Streaming** | ✅ Via LOB | ✅ Via LOB | ✅ Native | ✅ Native + Streams |
| **Prepared Statements** | ✅ Yes | ✅ Yes | ✅ Yes | ✅ Yes |
| **Transaction Control** | ✅ Basic | ✅ Full (Isolation) | ✅ Advanced | ✅ Advanced (TPB) |
| **Named Cursors** | ❌ No | ✅ Yes (Scrollable) | ❌ No | ✅ Yes |
| **Generator/Sequence** | Via SQL only | ✅ `lastInsertId()` | Via SQL only | ✅ `fbird_gen_id()` |
| **Modern OO API** | ❌ Legacy C API | ✅ FB 3.0+ OO API | ✅ FB 3.0+ OO API | ✅ FB 3.0+ OO API |
| **Exception Mode** | ✅ Always | ✅ Always | ✅ Always | ✅ Optional (v9+) |

**When to use `pdo_fbird` (Layer 3):**
- Building database-agnostic applications that need PDO compatibility
- Migrating from `PDO_Firebird` without changing application code structure
- Simple CRUD operations via PDO interface

**When to use `Firebird\*` classes (Layer 2):**
- OOP-style code with Firebird-specific features (events, service API)
- Modern PHP applications preferring class-based APIs

**When to use `fbird_*` functions (Layer 1):**
- Firebird array field support required
- Advanced transaction control (savepoints, table locking, TPB)
- Performance-critical applications
- Full service API (backup, restore, user management)

## Requirements

### System Requirements
- **PHP**: 8.2+ with development headers
- **C++ Compiler**: GCC 7+ or Clang 5+ (C++17 support)
- **Firebird**: 3.0+ client libraries (fbclient) and headers
- **Build Tools**: autotools, make, pkg-config

> **⚠️ Firebird 3.0+ Client Library Required**: This extension requires the Firebird 3.0+ client library (fbclient) for building, as it uses the modern OO API (FB_API_VER >= 30). Supported server versions: 3.0, 4.0, 5.0+.

### Supported Platforms
- Linux x86_64 and ARM64/aarch64 (Ubuntu 20.04+, Debian 11+, Rocky/AlmaLinux 8+, openSUSE 15.3+)
- Linux musl/Alpine (Alpine 3.18+, x86_64)
- Windows 10/11 x64 (Visual Studio 2019+)
- macOS arm64 (Apple Silicon, macOS 11+)

> **Note**: glibc 2.28+ required for glibc precompiled binaries; musl 1.2+ for Alpine binaries. PHP 8.2+ is required.

#### Precompiled Binary Matrix (v12.0.0+)

| Platform | Architectures | PHP Versions | Variants | Bundles |
|----------|--------------|--------------|----------|---------|
| **Linux (glibc)** | x86_64, aarch64 | 8.2-8.5 | NTS, ZTS | 16 |
| **Linux (musl)** | x86_64 | 8.2-8.5 | NTS, ZTS | 8 |
| **macOS** | arm64 | 8.2-8.5 | NTS, ZTS | 8 |
| **Windows** | x64 | 8.2-8.5 | NTS, TS | 8 |

Download from the [Releases Page](https://github.com/satwareAG/php-firebird/releases).

## Quick Start

### Docker Development (Recommended)
```bash
# Clone and setup
git clone https://github.com/satwareAG/php-firebird.git
cd php-firebird

# Start development environment
cd docker/
docker-compose up -d php83-dev

# Build extension
docker exec php-firebird-dev-php83-dev-1 /ext/scripts/build.sh

# Run tests
docker exec php-firebird-dev-php83-dev-1 /ext/scripts/test.sh
```

### Native Installation

#### Linux (Ubuntu/Debian)
```bash
# Install dependencies (PHP 8.2+)
sudo apt-get update
sudo apt-get install php8.3-dev firebird-dev firebird3.0-server

# Build extension
git clone https://github.com/satwareAG/php-firebird.git
cd php-firebird
phpize
CPPFLAGS=-I/usr/include/firebird ./configure --with-firebird
make all test

# Install
sudo make install
echo "extension=firebird.so" | sudo tee /etc/php/8.3/mods-available/firebird.ini
sudo phpenmod firebird
```

#### Linux (openSUSE)
```bash
# Install dependencies (PHP 8.2+)
sudo zypper install php8-devel libfbclient2 libfbclient-devel

# Build extension
git clone https://github.com/satwareAG/php-firebird.git
cd php-firebird
phpize
CPPFLAGS=-I/usr/include/firebird ./configure --with-firebird
make all test

# Install
sudo make install
echo "extension=firebird.so" | sudo tee /etc/php8/conf.d/firebird.ini
```

#### Linux / macOS (Pre-compiled Bundles)
Pre-compiled bundles include `firebird.so` with bundled Firebird client libraries (`$ORIGIN/lib/` RPATH).

1. Go to the [Releases Page](https://github.com/satwareAG/php-firebird/releases).
2. Download the bundle matching your platform, PHP version, and thread safety (NTS/ZTS).
3. Extract: `tar -xzf php-firebird-*.tar.gz`
4. Copy `firebird.so` and `lib/` to your PHP extension directory.
5. Add `extension=firebird.so` to your `php.ini`.

#### Windows (Pre-compiled DLLs)
Pre-compiled Windows DLLs for PHP 8.2-8.5 (x64, NTS/TS).

1. Go to the [Releases Page](https://github.com/satwareAG/php-firebird/releases).
2. Download the `.zip` matching your PHP version and thread safety.
3. See the [Windows Installation Guide](docs/WINDOWS_INSTALLATION.md) for detailed instructions.

#### Windows (Manual Build)
```batch
REM Prerequisites: Visual Studio 2019+ with C++ tools, Git for Windows

REM Download PHP SDK
git clone https://github.com/Microsoft/php-sdk-binary-tools.git c:\php-sdk
cd c:\php-sdk

REM Prepare build environment (x64)
phpsdk-vs16-x64.bat

REM Setup build tree for PHP 8.3+
phpsdk_buildtree php83
git clone https://github.com/php/php-src.git
cd php-src
git checkout PHP-8.3

REM Get dependencies
phpsdk_deps --update --branch 8.3

REM Download extension source
mkdir ..\pecl
git clone https://github.com/satwareAG/php-firebird.git ..\pecl\firebird

REM Build (adjust Firebird path as needed)
buildconf --force
configure --disable-all --enable-cli --with-firebird="shared,C:\Program Files\Firebird\5_0"
nmake
```

### macOS (Homebrew)
```bash
# Install dependencies
brew install php@8.3 firebird

# Build extension
git clone https://github.com/satwareAG/php-firebird.git
cd php-firebird
phpize
./configure --with-firebird=$(brew --prefix firebird)
make all test

# Install
sudo make install
echo "extension=firebird.so" >> $(php --ini | grep "Scan for" | cut -d: -f2 | tr -d ' ')/firebird.ini
```

## Usage Example

```php
<?php
// Connect to Firebird database
$db = fbird_connect('/path/to/database.fdb', 'SYSDBA', 'masterkey');

if (!$db) {
    throw new Exception('Connection failed: ' . fbird_errmsg());
}

// Execute a query with parameters
$result = fbird_query($db, 'SELECT * FROM users WHERE active = ?', 1);

if (!$result) {
    throw new Exception('Query failed: ' . fbird_errmsg());
}

// Fetch data
$users = [];
while ($row = fbird_fetch_assoc($result)) {
    $users[] = $row;
}

// Clean up
fbird_free_result($result);
fbird_close($db);

print_r($users);
?>
```

### Exception Handling

The extension provides PDO-style exception handling with runtime switchable error modes:

```php
<?php
// Enable exception mode (PDO-style error handling)
fbird_set_exception_mode(FBIRD_EXCEPTION_MODE_THROW);

try {
    $db = fbird_connect('/path/to/database.fdb', 'SYSDBA', 'masterkey');
    $result = fbird_query($db, 'SELECT * FROM invalid_table');
} catch (Firebird\Exception $e) {
    echo "Error: " . $e->getMessage() . "\n";
    echo "SQLSTATE: " . $e->getSqlState() . "\n";  // e.g., "42000"
    echo "Error code: " . $e->getCode() . "\n";
}

// Switch back to silent mode (traditional PHP warnings)
fbird_set_exception_mode(FBIRD_EXCEPTION_MODE_SILENT);

// Traditional error handling with warnings
$db = fbird_connect('/path/to/database.fdb', 'SYSDBA', 'masterkey');
if (!$db) {
    echo "Connection failed: " . fbird_errmsg() . "\n";
}
?>
```

**Exception Mode API:**
- `fbird_set_exception_mode(int $mode)` - Set error handling mode (SILENT or THROW)
- `fbird_get_exception_mode()` - Get current error handling mode
- `FBIRD_EXCEPTION_MODE_SILENT` - Traditional PHP warnings (default, backward compatible)
- `FBIRD_EXCEPTION_MODE_THROW` - Throw `Firebird\Exception` on errors (PDO-style)

**Common SQLSTATE Codes:**
- `23000` - Integrity constraint violation
- `42000` - Syntax error or access rule violation
- `HY000` - General error

**Doctrine DBAL Integration:** Exception mode is required for Doctrine DBAL 4.x compatibility. Enable THROW mode at application bootstrap:

```php
<?php
// Early in your bootstrap (before any database operations)
fbird_set_exception_mode(FBIRD_EXCEPTION_MODE_THROW);
?>
```

### Transactions Example

```php
<?php
$db = fbird_connect('/path/to/database.fdb', 'SYSDBA', 'masterkey');

// Start a transaction
$trans = fbird_trans($db);

try {
    fbird_query($trans, "INSERT INTO users (name) VALUES (?)", 'Alice');
    fbird_query($trans, "INSERT INTO logs (message) VALUES (?)", 'User Alice created');
    
    // Commit if all operations succeed
    fbird_commit($trans);
    echo "Transaction committed successfully\n";
} catch (Exception $e) {
    // Rollback on error
    fbird_rollback($trans);
    echo "Transaction rolled back: " . $e->getMessage() . "\n";
}

fbird_close($db);
?>
```

### Advanced Transactions with TPB (Transaction Parameter Block)

The extension provides comprehensive transaction control via the Transaction Parameter Block (TPB):

```php
<?php
$db = fbird_connect('/path/to/database.fdb', 'SYSDBA', 'masterkey');

// Simple: Use flag-first API for common configurations
// Read-only transaction with SNAPSHOT isolation (CONCURRENCY)
$readTrans = fbird_trans(FBIRD_READ | FBIRD_CONCURRENCY, $db);

// Read-write with READ COMMITTED + record versioning + wait
$writeTrans = fbird_trans(FBIRD_WRITE | FBIRD_COMMITTED | FBIRD_REC_VERSION | FBIRD_WAIT, $db);

// Advanced: Use array options for full TPB control
$options = [
    'access_mode' => FBIRD_WRITE,
    'isolation' => FBIRD_COMMITTED,
    'lock_resolution' => FBIRD_WAIT,
    'lock_timeout' => 10,  // 10 second timeout
    'tables' => [
        'USERS' => FBIRD_LOCK_PROTECTED | FBIRD_LOCK_WRITE,
        'LOGS' => FBIRD_LOCK_SHARED | FBIRD_LOCK_READ
    ]
];
$trans = fbird_trans_start($db, $options);

// Query using the transaction
fbird_query($trans, "UPDATE users SET last_login = CURRENT_TIMESTAMP WHERE id = ?", 123);

// Inspect transaction state
$info = fbird_trans_info($trans);
print_r($info);
// Output: ['id' => 12345, 'state' => 'ACTIVE', 'isolation' => 'READ_COMMITTED', ...]

fbird_commit($trans);
fbird_close($db);
?>
```

**Transaction Flags (combinable with `|`):**

| Flag | Description |
|------|-------------|
| `FBIRD_READ` | Read-only access mode |
| `FBIRD_WRITE` | Read-write access mode (default) |
| `FBIRD_CONCURRENCY` | SNAPSHOT isolation (repeatable read) |
| `FBIRD_COMMITTED` | READ COMMITTED isolation |
| `FBIRD_CONSISTENCY` | SERIALIZABLE isolation (table-level locks) |
| `FBIRD_REC_VERSION` | Read latest committed version (with COMMITTED) |
| `FBIRD_REC_NO_VERSION` | Read only version at transaction start |
| `FBIRD_WAIT` | Wait for locked records |
| `FBIRD_NOWAIT` | Fail immediately on lock conflict |
| `FBIRD_LOCK_TIMEOUT` | Enable lock timeout (set value via array API) |
| `FBIRD_READ_CONSISTENCY` | Firebird 4.0+ read consistency mode |

**Table Reservation Flags (for `tables` array option):**

| Flag | Description |
|------|-------------|
| `FBIRD_LOCK_SHARED` | Shared lock (other transactions can read) |
| `FBIRD_LOCK_PROTECTED` | Protected lock (other transactions blocked) |
| `FBIRD_LOCK_EXCLUSIVE` | Exclusive lock (no other access) |
| `FBIRD_LOCK_READ` | Lock for reading |
| `FBIRD_LOCK_WRITE` | Lock for writing |

### Prepared Statements Example

```php
<?php
$db = fbird_connect('/path/to/database.fdb', 'SYSDBA', 'masterkey');

// Prepare a statement for repeated execution
$stmt = fbird_prepare($db, 'SELECT * FROM users WHERE id = ?');

for ($i = 1; $i <= 10; $i++) {
    $result = fbird_execute($stmt, $i);
    if ($row = fbird_fetch_assoc($result)) {
        echo "User $i: " . $row['NAME'] . "\n";
    }
    fbird_free_result($result);
}

fbird_free_query($stmt);
fbird_close($db);
?>
```

### BLOB Handling Example

```php
<?php
$db = fbird_connect('/path/to/database.fdb', 'SYSDBA', 'masterkey');

// Create a BLOB
$blob = fbird_blob_create($db);
fbird_blob_add($blob, "This is the content of the BLOB field.");
$blob_id = fbird_blob_close($blob);

// Insert the BLOB
fbird_query($db, "INSERT INTO documents (content) VALUES (?)", $blob_id);

// Read a BLOB
$result = fbird_query($db, "SELECT content FROM documents WHERE id = 1");
$row = fbird_fetch_assoc($result);
$content = fbird_blob_info($db, $row['CONTENT']);

echo "BLOB length: " . $content['length'] . " bytes\n";

fbird_free_result($result);
fbird_close($db);
?>
```

## Configuration

### Runtime Configuration
```ini
; php.ini settings
extension=firebird.so

; PDO Firebird driver (optional, load AFTER firebird.so)
extension=pdo_fbird.so

; Output format defaults (for displaying date/time values)
fbird.timestampformat = "%Y-%m-%d %H:%M:%S"
fbird.dateformat = "%Y-%m-%d"
fbird.timeformat = "%H:%M:%S"
```

> **⚠️ Breaking Change (v7.0)**: INI settings have been renamed from `ibase.*` to `fbird.*`. Update your php.ini configuration accordingly.
> **⚠️ Breaking Change (v12.0)**: `pdo_fbird` is now a separate extension. Add `extension=pdo_fbird.so` to your `php.ini` (after `firebird.so`) to use the `fbird:` PDO DSN.

### Supported Input Date/Time Formats

When binding date/time parameters, the extension auto-detects the following formats:

**Date Formats (all cross-platform compatible):**
| Format | Example | Description |
|--------|---------|-------------|
| ISO 8601 | `2025-12-17` | `YYYY-MM-DD` (recommended) |
| European | `17.12.2025` | `DD.MM.YYYY` |
| US | `12/17/2025` | `MM/DD/YYYY` |

**Time Formats:**
| Format | Example | Description |
|--------|---------|-------------|
| Basic | `14:30:00` | `HH:MM:SS` |
| With fractions | `14:30:00.1234` | Up to microsecond precision |
| With offset | `14:30:00+01:00` | Timezone offset (Firebird 4.0+) |
| With named zone | `14:30:00 Europe/Berlin` | Named timezone (Firebird 4.0+) |

**Timestamp Formats:**
Combine any date format with any time format, separated by space or `T`:
- `2025-12-17 14:30:00`
- `2025-12-17T14:30:00.1234+01:00`
- `17.12.2025 14:30:00 America/New_York`

**Timezone Support (Firebird 4.0+):**
- Offset format: `+HH:MM`, `-HH:MM`, `+HHMM`, `-HHMM`
- Named zones: IANA timezone names (e.g., `America/New_York`, `Europe/Berlin`)
- UTC shorthand: `Z`

### Environment Variables
```bash
# Firebird client library path (if not in standard location)
export LD_LIBRARY_PATH=/opt/firebird/lib:$LD_LIBRARY_PATH

# Windows: Ensure fbclient.dll is in PATH
set PATH=%PATH%;C:\Program Files\Firebird\5_0\bin
```

## Migration Guide

### Migrating from `ext/interbase` or Legacy `php-firebird`

This version uses the modern `fbird_*` function prefix instead of `ibase_*`. Here's how to migrate:

#### Function Name Changes

| Old Name (ibase_*) | New Name (fbird_*) |
|-------------------|--------------------|
| `ibase_connect()` | `fbird_connect()` |
| `ibase_pconnect()` | `fbird_pconnect()` |
| `ibase_close()` | `fbird_close()` |
| `ibase_query()` | `fbird_query()` |
| `ibase_fetch_assoc()` | `fbird_fetch_assoc()` |
| `ibase_fetch_object()` | `fbird_fetch_object()` |
| `ibase_fetch_row()` | `fbird_fetch_row()` |
| `ibase_free_result()` | `fbird_free_result()` |
| `ibase_prepare()` | `fbird_prepare()` |
| `ibase_execute()` | `fbird_execute()` |
| `ibase_free_query()` | `fbird_free_query()` |
| `ibase_trans()` | `fbird_trans()` |
| `ibase_commit()` | `fbird_commit()` |
| `ibase_commit_ret()` | `fbird_commit_ret()` |
| `ibase_rollback()` | `fbird_rollback()` |
| `ibase_rollback_ret()` | `fbird_rollback_ret()` |
| `ibase_blob_create()` | `fbird_blob_create()` |
| `ibase_blob_open()` | `fbird_blob_open()` |
| `ibase_blob_add()` | `fbird_blob_add()` |
| `ibase_blob_get()` | `fbird_blob_get()` |
| `ibase_blob_close()` | `fbird_blob_close()` |
| `ibase_blob_cancel()` | `fbird_blob_cancel()` |
| `ibase_blob_info()` | `fbird_blob_info()` |
| `ibase_blob_echo()` | `fbird_blob_echo()` |
| `ibase_blob_import()` | `fbird_blob_import()` |
| `ibase_errmsg()` | `fbird_errmsg()` |
| `ibase_errcode()` | `fbird_errcode()` |
| `ibase_affected_rows()` | `fbird_affected_rows()` |
| `ibase_num_fields()` | `fbird_num_fields()` |
| `ibase_num_params()` | `fbird_num_params()` |
| `ibase_field_info()` | `fbird_field_info()` |
| `ibase_param_info()` | `fbird_param_info()` |
| `ibase_name_result()` | `fbird_name_result()` |
| `ibase_drop_db()` | `fbird_drop_db()` |
| `ibase_add_user()` | `fbird_add_user()` |
| `ibase_modify_user()` | `fbird_modify_user()` |
| `ibase_delete_user()` | `fbird_delete_user()` |
| `ibase_service_attach()` | `fbird_service_attach()` |
| `ibase_service_detach()` | `fbird_service_detach()` |
| `ibase_backup()` | `fbird_backup()` |
| `ibase_restore()` | `fbird_restore()` |
| `ibase_maintain_db()` | `fbird_maintain_db()` |
| `ibase_db_info()` | `fbird_db_info()` |
| `ibase_server_info()` | `fbird_server_info()` |
| `ibase_set_event_handler()` | `fbird_set_event_handler()` |
| `ibase_free_event_handler()` | `fbird_free_event_handler()` |
| `ibase_wait_event()` | `fbird_wait_event()` |

#### Quick Migration Script

For simple projects, you can use search-and-replace:

```bash
# Linux/macOS: Replace all ibase_ with fbird_ in PHP files
find . -name "*.php" -exec sed -i 's/ibase_/fbird_/g' {} +

# macOS (BSD sed requires backup extension)
find . -name "*.php" -exec sed -i '' 's/ibase_/fbird_/g' {} +
```

#### Extension Loading Changes

```ini
; OLD (remove this)
extension=interbase.so

; NEW (add this)
extension=firebird.so
extension=pdo_fbird.so   ; optional: only if using PDO with fbird: DSN (v12.0+)
```

#### Build Flag Changes

```bash
# OLD
./configure --with-interbase=/opt/firebird

# NEW
./configure --with-firebird=/opt/firebird
```

#### Resource Type Changes

Resource types have also been renamed for clarity:

| Old Type | New Type |
|----------|----------|
| `Firebird/InterBase link` | `Firebird link` |
| `Firebird/InterBase transaction` | `Firebird transaction` |
| `Firebird/InterBase result` | `Firebird result` |
| `Firebird/InterBase query` | `Firebird query` |
| `Firebird/InterBase blob` | `Firebird blob` |

### Why the Change?

This extension was originally forked from PHP's `ext/interbase`. The rename to `fbird_*` functions:

1. **Reflects modern reality**: InterBase is essentially defunct; Firebird is the active successor
2. **Clear differentiation**: Makes it obvious this is a new, maintained driver
3. **Clean break**: Encourages users to review and test their code during migration
4. **Future-proof**: No confusion with legacy or potentially conflicting code

## Testing

### Running Tests

`make test` only loads `firebird.so` (the main extension). To test PDO
functionality, use `run-tests.php` with explicit extension loading:

```bash
# Build both extensions first
phpize && ./configure && make
cd pdo_fbird && phpize && ./configure && make && cd ..

# All tests (loads both firebird.so AND pdo_fbird.so)
php run-tests.php \
  -d extension=modules/firebird.so \
  -d extension=pdo_fbird/modules/pdo_fbird.so \
  -p $(which php) \
  tests/

# Specific test file
php run-tests.php \
  -d extension=modules/firebird.so \
  -d extension=pdo_fbird/modules/pdo_fbird.so \
  -p $(which php) \
  tests/fbird_connect_001.phpt
```

The CI runs 389 `.phpt` tests across 12 containers (PHP 8.2-8.5 x FB 3.0/4.0/5.0).

### Test Environment Setup
```bash
# Create test database (requires SYSDBA access)
isql -user SYSDBA -password masterkey
CREATE DATABASE '/tmp/test.fdb';
EXIT;

# Run extension tests
export TEST_DB_PATH=/tmp/test.fdb
export TEST_DB_USER=SYSDBA  
export TEST_DB_PASS=masterkey
php run-tests.php -d extension=modules/firebird.so tests/
```

## Troubleshooting

### Common Build Issues

**Missing fbclient library:**
```bash
# Verify library presence
pkg-config --exists fbclient || echo "Install firebird-dev package"

# Manual library path
export PKG_CONFIG_PATH=/usr/lib/pkgconfig:/opt/firebird/lib/pkgconfig
```

**PHP version conflicts:**
```bash
# Use specific PHP version
phpize8.3  # Ubuntu/Debian versioned phpize
./configure --with-php-config=/usr/bin/php-config8.3
```

**Windows Visual Studio version:**
- Use Visual Studio 2019+ (vs16) for PHP 8.2-8.3
- Use Visual Studio 2022 (vs17) for PHP 8.4+
- Ensure Windows SDK 10.0.20348.0+ is installed
- For compatibility, use same compiler as your PHP build

### Connection Behavior

**Connection Reuse (Default Behavior):**

By design, `fbird_connect()` reuses existing connections when called with identical parameters. This matches PostgreSQL's `pg_connect()` behavior and provides efficiency benefits for typical use cases:

```php
<?php
// Same parameters = same connection (by design)
$conn1 = fbird_connect('/path/to/db.fdb', 'SYSDBA', 'masterkey');
$conn2 = fbird_connect('/path/to/db.fdb', 'SYSDBA', 'masterkey');

// $conn1 and $conn2 reference the SAME connection resource
var_dump($conn1 === $conn2);  // bool(true)
?>
```

**When You Need Separate Connections:**

If you need independent connections (e.g., for parallel transactions, different isolation levels, or connection-specific state), use one of these approaches:

```php
<?php
// Approach 1: Use different parameters (charset, role, etc.)
$conn1 = fbird_connect($db, $user, $pass, 'UTF8');
$conn2 = fbird_connect($db, $user, $pass, 'ISO8859_1');  // Different charset = new connection

// Approach 2: Use different roles
$admin = fbird_connect($db, $user, $pass, null, null, null, 'ADMIN');
$reader = fbird_connect($db, $user, $pass, null, null, null, 'READER');

// Approach 3: Mix persistent and non-persistent
$conn1 = fbird_connect($db, $user, $pass);
$conn2 = fbird_pconnect($db, $user, $pass);  // Different pool
?>
```

**Force New Connection (v7.0+):**

Use the `FBIRD_CONNECT_FORCE_NEW` flag (value: 2) to bypass connection reuse and create a new database connection:

```php
<?php
// Force a new connection even with identical parameters
$conn1 = fbird_connect($db, $user, $pass);
$conn2 = fbird_connect($db, $user, $pass, '', 0, 0, '', FBIRD_CONNECT_FORCE_NEW);

// $conn1 and $conn2 are now DIFFERENT connections
var_dump($conn1 === $conn2);  // bool(false)
?>
```

This flag mirrors PostgreSQL's `PGSQL_CONNECT_FORCE_NEW` behavior and is useful for:
- Parallel operations within the same request
- Testing scenarios requiring isolated connections
- Connection pool implementations

### Performance Optimization

**Connection Pooling:**
```php
<?php
// Use persistent connections for better performance
$db = fbird_pconnect('/path/to/database.fdb', 'SYSDBA', 'masterkey');
?>
```

**Prepared Statements:**
```php
<?php
// Optimize repeated queries
$stmt = fbird_prepare($db, 'SELECT * FROM users WHERE id = ?');
for ($i = 1; $i <= 1000; $i++) {
    $result = fbird_execute($stmt, $i);
    // Process result
    fbird_free_result($result);
}
fbird_free_query($stmt);
?>
```

## Development

### Contributing
See [CONTRIBUTING.md](CONTRIBUTING.md) for detailed development guidelines including:
- Modern C++17 development standards
- Static analysis tool integration
- Cross-platform testing procedures
- Docker development environment

### Development Tools
- **Static Analysis**: clang-tidy, Cppcheck
- **Memory Safety**: AddressSanitizer, ThreadSanitizer (ZTS), Valgrind
- **Debugging**: GDB/LLDB with PHP symbols
- **IDE Support**: CLion, VS Code, Visual Studio

### Architecture
- **Language**: C (main extension) + C++ (utilities) with C++17 standard
- **API**: Zend Extension API with modern PHP 8.2+ features
- **Thread Safety**: Support for both ZTS and NTS builds
- **Memory Model**: RAII principles with automatic resource cleanup

## IDE and Static Analysis Support

For IDE autocompletion and static analysis tools (PHPStan, Psalm), install the stubs package:

```bash
composer require --dev satwareag/php-firebird-stubs
```

This provides:
- **IDE Autocompletion**: Full function signatures and parameter hints for PhpStorm, VSCode (Intelephense), etc.
- **PHPStan/Psalm Support**: Type stubs for static analysis of code using `fbird_*` functions
- **Inline Documentation**: PHPDoc comments describing each function and constant

### PHPStan Configuration

The stubs are automatically loaded via Composer. For explicit configuration in `phpstan.neon`:

```neon
parameters:
    scanFiles:
        - vendor/satwareag/php-firebird-stubs/firebird-stubs.php
        - vendor/satwareag/php-firebird-stubs/firebird-classes.php
```

### Psalm Configuration

Add to your `psalm.xml`:

```xml
<stubs>
    <file name="vendor/satwareag/php-firebird-stubs/firebird-stubs.php"/>
    <file name="vendor/satwareag/php-firebird-stubs/firebird-classes.php"/>
</stubs>
```

## Function Reference

### Connection Functions
- `fbird_connect()` - Open a connection to a database
- `fbird_pconnect()` - Open a persistent connection
- `fbird_close()` - Close a database connection

### Query Functions
- `fbird_query()` - Execute a query (link or transaction as first arg)
- `fbird_query_params_tx($link, $trans, $sql, ?$params)` - Execute with explicit link + transaction + params (Doctrine DBAL integration)
- `fbird_prepare()` - Prepare a query for later execution
- `fbird_execute()` - Execute a prepared query
- `fbird_execute_statement()` - Execute a prepared statement resource
- `fbird_execute_query()` - Execute SQL string with optional parameters
- `fbird_execute_auto()` - Auto-commit execution helper
- `fbird_free_query()` - Free memory allocated by a prepared query
- `fbird_free_result()` - Free a result set

### Fetch Functions
- `fbird_fetch_row()` - Fetch a row as enumerated array
- `fbird_fetch_assoc()` - Fetch a row as associative array
- `fbird_fetch_object()` - Fetch a row as object

### Transaction Functions
- `fbird_trans()` - Begin a transaction (flag-first API: `fbird_trans(FLAGS, $db)`)
- `fbird_trans_start()` - Begin a transaction with array options (full TPB control)
- `fbird_commit()` - Commit a transaction
- `fbird_commit_ret()` - Commit and retain transaction context
- `fbird_rollback()` - Roll back a transaction
- `fbird_rollback_ret()` - Rollback and retain transaction context
- `fbird_trans_info()` - Return transaction information (ID, state, isolation)

### Savepoint Functions
- `fbird_savepoint()` - Create a named savepoint
- `fbird_rollback_savepoint()` - Rollback to a named savepoint
- `fbird_release_savepoint()` - Release a named savepoint

### Transaction Constants
- **Access Modes**: `FBIRD_READ`, `FBIRD_WRITE`
- **Isolation Levels**: `FBIRD_CONCURRENCY`, `FBIRD_COMMITTED`, `FBIRD_CONSISTENCY`
- **Record Versioning**: `FBIRD_REC_VERSION`, `FBIRD_REC_NO_VERSION`
- **Lock Resolution**: `FBIRD_WAIT`, `FBIRD_NOWAIT`, `FBIRD_LOCK_TIMEOUT`
- **Table Reservation**: `FBIRD_LOCK_SHARED`, `FBIRD_LOCK_PROTECTED`, `FBIRD_LOCK_EXCLUSIVE`, `FBIRD_LOCK_READ`, `FBIRD_LOCK_WRITE`
- **Firebird 4.0+**: `FBIRD_READ_CONSISTENCY`

### BLOB Functions
- `fbird_blob_create()` - Create blob for adding data
- `fbird_blob_open()` - Open blob for retrieving data
- `fbird_blob_add()` - Add data to blob
- `fbird_blob_get()` - Get data from blob
- `fbird_blob_close()` - Close blob
- `fbird_blob_cancel()` - Cancel blob
- `fbird_blob_info()` - Return blob information
- `fbird_blob_echo()` - Output blob contents
- `fbird_blob_import()` - Create blob from file

### Information Functions
- `fbird_errmsg()` - Return error message
- `fbird_errcode()` - Return error code
- `fbird_affected_rows()` - Return affected rows
- `fbird_num_fields()` - Get number of fields
- `fbird_num_params()` - Get number of parameters
- `fbird_field_info()` - Get field information
- `fbird_param_info()` - Get parameter information
- `fbird_name_result()` - Assign name to result set

### Database Administration
- `fbird_drop_db()` - Drop a database

### Service Functions
- `fbird_service_attach()` - Connect to service manager
- `fbird_service_detach()` - Disconnect from service manager
- `fbird_backup()` - Initiate backup task
- `fbird_restore()` - Initiate restore task
- `fbird_maintain_db()` - Execute maintenance command
- `fbird_db_info()` - Get database information
- `fbird_server_info()` - Get server information

### User Management
- `fbird_add_user()` - Add a user
- `fbird_modify_user()` - Modify user information
- `fbird_delete_user()` - Delete a user

### Event Functions
- `fbird_set_event_handler()` - Register event handler (returns `Firebird\Event`)
- `fbird_free_event_handler()` - Free event handler
- `fbird_wait_event()` - Wait for event
- `fbird_poll_event()` - Poll for events with optional timeout

**OOP Event API (v12.0+):**
```php
$event = fbird_set_event_handler($conn, $callback, 'MY_EVENT');
$event->wait(5.0);    // Block up to 5 seconds
$event->getName();    // Get event name
$event->getCount();   // Get callback invocation count
$event->cancel();     // Cancel pending wait
```

### Batch Functions (Firebird 4.0+)
- `fbird_batch_create()` - Create batch from prepared statement for bulk INSERT
- `fbird_batch_add()` - Add row with automatic type conversion
- `fbird_batch_add_blob()` - Create inline BLOB, returns "HHHHHHHH:LLLL" ID
- `fbird_batch_register_blob()` - Register existing BLOB for batch use
- `fbird_batch_get_blob_alignment()` - Get inline BLOB alignment requirement in bytes
- `fbird_batch_append_blob_data()` - Append data chunk to in-progress batch BLOB
- `fbird_batch_add_blob_stream()` - Add BLOB data via IBatch addBlobStream protocol
- `fbird_batch_set_default_bpb()` - Set default BLOB Property Block for all batch BLOBs
- `fbird_batch_execute()` - Execute batch (returns total_processed, success_count, error_count)
- `fbird_batch_cancel()` - Cancel without executing

### PHP Event Polling Wrappers (src/Firebird/)

For advanced timeout and non-blocking event handling, PHP wrapper classes are provided:

```php
<?php
use Firebird\EventPoller;

$conn = fbird_connect('/path/to/database.fdb', 'SYSDBA', 'masterkey');
$event = fbird_set_event_handler($conn, function($name) {
    echo "Event received: $name\n";
    return true;  // Continue listening
}, 'MY_EVENT');

// Create poller with auto-detected strategy
$poller = EventPoller::create($event);

// Or specify strategy explicitly
$poller = EventPoller::create($event, 'process');  // Most reliable
$poller->setConnectionDetails('/path/to/database.fdb', 'SYSDBA', 'masterkey', ['MY_EVENT'], $callback);

// Poll with 5 second timeout
$result = $poller->poll(5000);

if ($result === FBIRD_EVENT_TIMEOUT) {
    echo "Timeout - no events\n";
}

$poller->free();
fbird_close($conn);
```

**Available Strategies:**
| Strategy | Constructor | Min Timeout | Requirements |
|----------|-------------|-------------|--------------|
| `process` | `ProcessEventPoller` | 10ms | `proc_open()` (most reliable) |
| `pcntl` | `PcntlEventPoller` | 1000ms | pcntl extension (Unix only) |
| `fiber` | `FiberEventPoller` | 1ms | amphp/amp ^3.0 |
| `auto` | Auto-detect | Varies | Best available |

See [docs/oop-api.md](docs/oop-api.md) for the `Firebird\Event` class reference.

## Version Compatibility

### Current Version: 13.2.1 (Stable Release)

**Supported PHP Versions:**
- PHP 8.2 (fully supported, minimum)
- PHP 8.3 (fully supported)
- PHP 8.4 (fully supported)
- PHP 8.5 (fully supported)

> **ℹ️ PHP 8.1**: PHP 8.1 reached end-of-life on November 25, 2025 and is no longer supported as of v7.2.0. Please upgrade to PHP 8.2 or newer.

**Firebird Client Library (for building):**
- Firebird 3.0+ client library required (uses OO API)
- FB 5.0 client recommended (connects to all server versions)

**Firebird Server Connectivity:**
| Client Library | Server Versions Supported | Notes |
|----------------|---------------------------|-------|
| FB 3.0 client | FB 3.0 | Legacy |
| FB 4.0 client | FB 3.0, 4.0 | Stable |
| FB 5.0 client | FB 3.0, 4.0, 5.0+ | **Recommended** |

> **ℹ️ Firebird 2.5 Server**: Firebird 2.5 reached EOL in September 2020 and is no longer supported as of v7.2.0. Please migrate to Firebird 3.0+.

### Precompiled Binaries (v12.0.0+)

Starting with v12.0.0, we provide precompiled bundles across Linux (glibc + musl), macOS (arm64), and Windows. Both `firebird.so` and `pdo_fbird.so` are included:

```bash
# Download from GitHub Releases (example: Linux x86_64 glibc)
wget https://github.com/satwareAG/php-firebird/releases/download/v13.2.0/php-firebird-13.2.0-php84-nts-linux-x86_64.tar.gz

# Extract to PHP extension directory
EXTDIR=$(php -r 'echo ini_get("extension_dir");')
sudo tar -xzf php-firebird-13.2.0-php84-nts-linux-x86_64.tar.gz -C "$EXTDIR" --strip-components=1

# Enable and verify (both extensions required)
echo "extension=firebird.so" | sudo tee /etc/php/8.4/mods-available/firebird.ini
echo "extension=pdo_fbird.so" | sudo tee /etc/php/8.4/mods-available/pdo_fbird.ini
sudo phpenmod firebird pdo_fbird
php -m | grep -E 'firebird|pdo_fbird'
```

**Package Compatibility:**
- **glibc 2.28+** required (Ubuntu 18.10+, Debian 11+, RHEL 8+)
- **PHP 8.2+** required - use third-party repos if your distribution has an older default
- **No system Firebird installation needed** - client libraries bundled via `$ORIGIN` rpath
- **Connects to any Firebird server** (3.0-5.0) using bundled FB 5.x client

**Installing PHP 8.2+ on Older Distributions:**

If your distribution ships with PHP < 8.2, use these third-party repositories:

```bash
# Ubuntu 20.04 / Debian 11 (ondrej/php PPA)
sudo apt-get install -y software-properties-common
sudo add-apt-repository -y ppa:ondrej/php  # Ubuntu
# For Debian: https://packages.sury.org/php/README.txt
sudo apt-get update
sudo apt-get install -y php8.4-cli php8.4-common

# Rocky Linux 8/9 / AlmaLinux 8/9 (remi repo)
sudo dnf install -y https://rpms.remirepo.net/enterprise/remi-release-$(rpm -E %rhel).rpm
sudo dnf module reset php
sudo dnf module enable php:remi-8.4
sudo dnf install -y php php-cli
```

After installing PHP 8.2+, install the precompiled extension bundle matching your PHP version.

**Dropped Support:**
- ❌ PHP 7.x (legacy, security issues)
- ❌ PHP 8.0 (legacy, no longer maintained)
- ❌ PHP 8.1 (EOL Nov 2025, dropped in v7.2.0)
- ❌ Firebird 2.5 client library (requires OO API from FB 3.0+ client)
- ❌ `ibase_*` function aliases (use `fbird_*` instead)
- ❌ `interbase.so` extension name (use `firebird.so`)
- ❌ Integrated `pdo_fbird` in `firebird.so` (v12.0: load `pdo_fbird.so` separately)
- ❌ `IB`/`ib_`/`ibase` internal naming (v12.0: all renamed to `FB`/`fb_`/`fbird`)

## Security

> **⚠️ SQL INJECTION WARNING**: Never concatenate user input directly into SQL queries. Always use parameterized queries.

### SQL Injection Prevention

**✅ SAFE - Always use parameterized queries:**
```php
<?php
// Prepared statements - RECOMMENDED
$stmt = fbird_prepare($db, "SELECT * FROM users WHERE username = ? AND active = ?");
$result = fbird_execute($stmt, $username, 1);

// Inline parameters
$result = fbird_query($db, "SELECT * FROM accounts WHERE id = ?", $accountId);
```

**❌ UNSAFE - Never do this:**
```php
<?php
// SQL Injection vulnerable - NEVER concatenate user input!
$result = fbird_query($db, "SELECT * FROM users WHERE username = '$username'");

// Even with addslashes() - NOT safe for Firebird!
$result = fbird_query($db, "SELECT * FROM users WHERE username = '" . addslashes($username) . "'");
```

### Escaping Functions

For cases where parameterized queries cannot be used (dynamic SQL generation), use `fbird_escape_string()`:

```php
<?php
// Escape single quotes (' → '') for safe SQL string literals
$safe = fbird_escape_string($userInput);
$sql = "SELECT * FROM users WHERE notes LIKE '%" . $safe . "%'";
```

> **Note**: `fbird_escape_string()` is a **secondary defense**. Parameterized queries are always preferred.

### Security Features

- **Parameterized Queries**: Primary defense against SQL injection
- **Input Validation**: Comprehensive parameter checking
- **Memory Safety**: AddressSanitizer integration for development
- **Resource Management**: Automatic cleanup prevents leaks
- **String Escaping**: `fbird_escape_string()` for dynamic SQL edge cases

For comprehensive security guidance, see [docs/SECURITY.md](docs/SECURITY.md).

### Reporting Security Issues

For security-related issues, please email the maintainers directly rather than creating public issues.

## License

This extension is licensed under the PHP License v3.01. See [LICENSE](LICENSE) for details.

## Links

- **Source Repository**: https://github.com/satwareAG/php-firebird
- **Firebird Documentation**: https://firebirdsql.org/en/documentation/
- **PHP Extensions Guide**: https://www.php.net/manual/en/internals2.php
- **Issue Tracker**: https://github.com/satwareAG/php-firebird/issues

---
