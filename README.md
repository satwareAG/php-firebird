# PHP Firebird Extension (Modernized)

A high-performance PHP extension providing native connectivity to Firebird databases. This modernized version targets PHP 8.1+ with C++17 standards and comprehensive development tooling.

> **⚠️ Breaking Changes in v1.0**: This version uses `fbird_*` function names (not `ibase_*`) and builds as `firebird.so` (not `interbase.so`). See the [Migration Guide](#migration-guide) for upgrade instructions.

## Features

- **Native Performance**: Direct fbclient library integration
- **Full Firebird Support**: Compatible with Firebird 3.0, 4.0, 5.0+
- **Modern C++ OO API**: Uses Firebird 3.0+ Object-Oriented API with RAII wrappers
- **Modern PHP**: Optimized for PHP 8.1+ with typed properties and attributes
- **Memory Safety**: Built with AddressSanitizer and comprehensive static analysis
- **Cross-Platform**: Linux, Windows, macOS support
- **Clean API**: `fbird_*` function prefix (no legacy InterBase naming)

## Requirements

### System Requirements
- **PHP**: 8.1+ with development headers
- **C++ Compiler**: GCC 7+ or Clang 5+ (C++17 support)
- **Firebird**: Client libraries (fbclient) and headers (ibase.h)
- **Build Tools**: autotools, make, pkg-config

### Supported Platforms
- Linux (Ubuntu 20.04+, Debian 11+, openSUSE 15.3+)
- Windows 10/11 (Visual Studio 2019+)
- macOS 10.15+ (Xcode 11+)

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
docker exec php-firebird-dev-php83-dev-1 /ext/scripts/container/build.sh

# Run tests
docker exec php-firebird-dev-php83-dev-1 /ext/scripts/container/test.sh
```

### Native Installation

#### Linux (Ubuntu/Debian)
```bash
# Install dependencies (PHP 8.1+)
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
# Install dependencies (PHP 8.1+)
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

#### Windows
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

; Connection format defaults
fbird.timestampformat = "%Y-%m-%d %H:%M:%S"
fbird.dateformat = "%Y-%m-%d"
fbird.timeformat = "%H:%M:%S"
```

> **⚠️ Breaking Change (v1.0)**: INI settings have been renamed from `ibase.*` to `fbird.*`. Update your php.ini configuration accordingly.

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
```bash
# All tests
make test

# Specific tests
php run-tests.php tests/fbird_connect_001.phpt

# With specific Firebird version
FB_VERSION=5.0 make test
```

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
make test
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
- Use Visual Studio 2019+ (vs16) for PHP 8.1-8.3
- Use Visual Studio 2022 (vs17) for PHP 8.4+
- Ensure Windows SDK 10.0.20348.0+ is installed
- For compatibility, use same compiler as your PHP build

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
- **Memory Safety**: AddressSanitizer, Valgrind
- **Debugging**: GDB/LLDB with PHP symbols
- **IDE Support**: CLion, VS Code, Visual Studio

### Architecture
- **Language**: C (main extension) + C++ (utilities) with C++17 standard
- **API**: Zend Extension API with modern PHP 8.1+ features
- **Thread Safety**: Support for both ZTS and NTS builds
- **Memory Model**: RAII principles with automatic resource cleanup

## Function Reference

### Connection Functions
- `fbird_connect()` - Open a connection to a database
- `fbird_pconnect()` - Open a persistent connection
- `fbird_close()` - Close a database connection

### Query Functions
- `fbird_query()` - Execute a query
- `fbird_prepare()` - Prepare a query for later execution
- `fbird_execute()` - Execute a prepared query
- `fbird_free_query()` - Free memory allocated by a prepared query
- `fbird_free_result()` - Free a result set

### Fetch Functions
- `fbird_fetch_row()` - Fetch a row as enumerated array
- `fbird_fetch_assoc()` - Fetch a row as associative array
- `fbird_fetch_object()` - Fetch a row as object

### Transaction Functions
- `fbird_trans()` - Begin a transaction
- `fbird_commit()` - Commit a transaction
- `fbird_commit_ret()` - Commit and retain
- `fbird_rollback()` - Roll back a transaction
- `fbird_rollback_ret()` - Rollback and retain

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
- `fbird_set_event_handler()` - Register event handler
- `fbird_free_event_handler()` - Free event handler
- `fbird_wait_event()` - Wait for event
- `fbird_poll_event()` - Poll for events with optional timeout

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

See [EVENT_TIMEOUT_RFC.md](docs/development/EVENT_TIMEOUT_RFC.md) for implementation details.

## Version Compatibility

### Current Version: 1.0.0

**Supported PHP Versions:**
- PHP 8.1 (minimum required)
- PHP 8.2 (fully supported)
- PHP 8.3 (fully supported)
- PHP 8.4 (fully supported)
- PHP 8.5 (development)

**Supported Firebird Versions:**
- Firebird 3.0 (minimum required, full support)
- Firebird 4.0 (full support)
- Firebird 5.0+ (full support)

**Dropped Support:**
- ❌ PHP 7.x (legacy, security issues)
- ❌ PHP 8.0 (legacy, no longer maintained)
- ❌ Firebird 2.5 (legacy, use OO API from FB 3.0+)
- ❌ `ibase_*` function aliases (use `fbird_*` instead)
- ❌ `interbase.so` extension name (use `firebird.so`)

## Security

### Security Features
- **Input Validation**: Comprehensive parameter checking
- **Memory Safety**: AddressSanitizer integration
- **SQL Injection Protection**: Prepared statement support
- **Resource Management**: Automatic cleanup prevents leaks

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
