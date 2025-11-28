# PHP Firebird Extension (Modernized)

A high-performance PHP extension providing native connectivity to Firebird and InterBase databases. This modernized version targets PHP 8.1+ with C++17 standards and comprehensive development tooling.

## Features

- **Native Performance**: Direct fbclient library integration
- **Full Firebird Support**: Compatible with Firebird 2.5, 3.0, 4.0, 5.0+
- **Modern PHP**: Optimized for PHP 8.1+ with typed properties and attributes
- **Memory Safety**: Built with AddressSanitizer and comprehensive static analysis
- **Cross-Platform**: Linux, Windows, macOS support

## Requirements

### System Requirements
- **PHP**: 8.1+ with development headers
- **C++ Compiler**: GCC 7+ or Clang 5+ (C++17 support)
- **Firebird**: Client libraries (fbclient) and headers (ibase.h)
- **Build Tools**: autotools, make, pkg-config

### Supported Platforms
- Linux (Ubuntu 20.04+, CentOS 8+, openSUSE 15.3+)
- Windows 10/11 (Visual Studio 2019+)
- macOS 10.15+ (Xcode 11+)

## Quick Start

### Docker Development (Recommended)
```bash
# Clone and setup
git clone https://github.com/FirebirdSQL/php-firebird.git
cd php-firebird

# Start development environment
cd docker/
docker-compose up -d php81-dev

# Build extension
docker exec php-firebird-dev-php81-dev-1 /ext/scripts/container/build.sh

# Run tests
docker exec php-firebird-dev-php81-dev-1 /ext/scripts/container/test.sh
```

### Native Installation

#### Linux (Ubuntu/Debian)
```bash
# Install dependencies (PHP 8.1+)
sudo apt-get update
sudo apt-get install php8.1-dev firebird-dev firebird3.0-server

# Build extension
git clone https://github.com/FirebirdSQL/php-firebird.git
cd php-firebird
phpize
CPPFLAGS=-I/usr/include/firebird ./configure --with-interbase
make all test

# Install
sudo make install
echo "extension=interbase.so" | sudo tee -a /etc/php/8.1/mods-available/interbase.ini
sudo phpenmod interbase
```

#### Linux (openSUSE)
```bash
# Install dependencies (PHP 8.1+)
sudo zypper install php8-devel libfbclient2 libfbclient-devel

# Build extension
git clone https://github.com/FirebirdSQL/php-firebird.git
cd php-firebird
phpize
CPPFLAGS=-I/usr/include/firebird ./configure --with-interbase
make all test

# Install
sudo make install
echo "extension=interbase.so" | sudo tee -a /etc/php8/conf.d/interbase.ini
```

#### Windows
```batch
REM Prerequisites: Visual Studio 2019+ with C++ tools, Git for Windows

REM Download PHP SDK
git clone https://github.com/Microsoft/php-sdk-binary-tools.git c:\php-sdk
cd c:\php-sdk

REM Prepare build environment (x64)
phpsdk-vs16-x64.bat

REM Setup build tree for PHP 8.1+
phpsdk_buildtree php81
git clone https://github.com/php/php-src.git
cd php-src
git checkout PHP-8.1

REM Get dependencies
phpsdk_deps --update --branch 8.1

REM Download extension source
mkdir ..\pecl
git clone https://github.com/FirebirdSQL/php-firebird.git ..\pecl\interbase

REM Build (adjust Firebird path as needed)
buildconf --force
configure --disable-all --enable-cli --with-interbase="shared,C:\Program Files\Firebird\4_0"
nmake
```

### macOS (Homebrew)
```bash
# Install dependencies
brew install php@8.1 firebird

# Build extension
git clone https://github.com/FirebirdSQL/php-firebird.git
cd php-firebird
phpize
./configure --with-interbase=$(brew --prefix firebird)
make all test

# Install
sudo make install
echo "extension=interbase.so" >> $(php --ini | grep "Scan for" | cut -d: -f2 | tr -d ' ')/interbase.ini
```

## Development Setup

### Prerequisites Verification
```bash
# Verify PHP 8.1+ with development headers
php -v  # Should show 8.1 or higher
php-config --version  # Should show 8.1 or higher

# Verify Firebird client
pkg-config --exists fbclient && echo "Firebird client found"

# Verify C++17 compiler
g++ --version  # GCC 7+ required
clang++ --version  # Clang 5+ required
```

### Build with Development Tools
```bash
# Build with debugging symbols and sanitizers
export CXXFLAGS="-g -O0 -fsanitize=address,undefined -std=c++17"
export CFLAGS="-g -O0 -fsanitize=address"
phpize
./configure --with-interbase --enable-debug
make clean && make

# Run tests with memory checking
make test
```

### Static Analysis Integration
```bash
# Install analysis tools
# Ubuntu/Debian: sudo apt-get install clang-tidy cppcheck valgrind
# macOS: brew install clang-tidy cppcheck valgrind
# openSUSE: sudo zypper install clang-tools cppcheck valgrind

# Run static analysis
clang-tidy *.cpp *.h --checks='*,-fuchsia-*' -- -I$(php-config --include-dir)
cppcheck --enable=all --std=c++17 *.cpp *.h

# Memory analysis (Linux)
valgrind --tool=memcheck --track-origins=yes php -dextension=./modules/interbase.so -r "echo 'Extension loaded';"
```

## Usage Example

```php
<?php
// Connect to Firebird database
$db = ibase_connect('/path/to/database.fdb', 'username', 'password');

// Modern PHP 8.1+ with null coalescing and match expressions
$result = ibase_query($db, 'SELECT * FROM users WHERE active = ?', 1) 
    ?? throw new Exception('Query failed');

// Fetch data with modern patterns
$users = [];
while ($row = ibase_fetch_assoc($result)) {
    $users[] = $row;
}

// Clean up
ibase_free_result($result);
ibase_close($db);
?>
```

## Configuration

### Runtime Configuration
```ini
; php.ini settings
extension=interbase.so

; Optional: Connection defaults
ibase.default_charset = UTF8
ibase.default_user = SYSDBA
ibase.dateformat = %Y-%m-%d %H:%M:%S
ibase.timeformat = %H:%M:%S
```

### Environment Variables
```bash
# Firebird client library path (if not in standard location)
export LD_LIBRARY_PATH=/opt/firebird/lib:$LD_LIBRARY_PATH

# Windows: Ensure fbclient.dll is in PATH
set PATH=%PATH%;C:\Program Files\Firebird\4_0\bin
```

## Testing

### Running Tests
```bash
# All tests
make test

# Specific tests
php run-tests.php tests/ibase_connect_001.phpt

# With specific Firebird version
FB_VERSION=4.0 make test
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
phpize8.1  # Ubuntu/Debian versioned phpize
./configure --with-php-config=/usr/bin/php-config8.1
```

**Windows Visual Studio version:**
- Use Visual Studio 2019+ (vs16) for PHP 8.1+
- Ensure Windows SDK 10.0.20348.0+ is installed
- For compatibility, use same compiler as your PHP build

### Performance Optimization

**Connection Pooling:**
```php
<?php
// Use persistent connections for better performance
$db = ibase_pconnect('/path/to/database.fdb', 'user', 'pass');
?>
```

**Prepared Statements:**
```php
<?php
// Optimize repeated queries
$stmt = ibase_prepare($db, 'SELECT * FROM users WHERE id = ?');
for ($i = 1; $i <= 1000; $i++) {
    $result = ibase_execute($stmt, $i);
    // Process result
    ibase_free_result($result);
}
ibase_free_query($stmt);
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

## Version Compatibility

### Current Version: 6.1.1-RC2

**Supported PHP Versions:**
- PHP 8.1 (minimum required)
- PHP 8.2 (fully supported)
- PHP 8.3 (fully supported)
- PHP 8.4 (planned support)

**Supported Firebird Versions:**
- Firebird 2.5 (legacy support)
- Firebird 3.0 (full support)
- Firebird 4.0 (full support)
- Firebird 5.0+ (planned)

**Dropped Support:**
- ❌ PHP 7.x (legacy, security issues)
- ❌ PHP 5.x (legacy, no longer maintained)

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

- **Source Repository**: https://github.com/FirebirdSQL/php-firebird
- **Firebird Documentation**: https://firebirdsql.org/en/documentation/
- **PHP Extensions Guide**: https://www.php.net/manual/en/internals2.php
- **Issue Tracker**: https://github.com/FirebirdSQL/php-firebird/issues

---
