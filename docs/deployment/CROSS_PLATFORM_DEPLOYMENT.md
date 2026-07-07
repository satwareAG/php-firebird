# Cross-Platform Deployment Guide - PHP Firebird Extension

## Overview

Deployment instructions for the php-firebird extension with C++17 features across Linux, Windows, and macOS platforms. The extension requires C++17 compiler support and PHP 8.2+ minimum version.

> **Breaking Change (v7.0.0):** The extension has been renamed from `interbase` to `firebird`. All functions are now prefixed with `fbird_*` instead of `ibase_*`. Use `extension=firebird` in configuration.

## Requirements Matrix

| Platform | PHP Version | C++ Compiler | Firebird Client | Additional Requirements |
|----------|-------------|--------------|----------------|------------------------|
| **Linux** | 8.1+ | g++ 8.0+ (C++17) | libfirebird-dev | autoconf, build-essential |
| **Windows** | 8.1+ | MSVC 2017+ | Firebird 4.0+ Client | Visual Studio Build Tools |
| **macOS** | 8.1+ | clang 10.0+ | Firebird via Homebrew | Xcode Command Line Tools |

## Linux Deployment

### Ubuntu/Debian

**Install Dependencies:**
```bash
# Ubuntu 20.04/22.04/24.04
sudo apt update
sudo apt install -y software-properties-common
sudo add-apt-repository ppa:ondrej/php -y
sudo apt update
sudo apt install -y php8.3-dev build-essential libfirebird-dev autoconf

# Verify C++17 support
g++ --version  # Should be 8.0+ for C++17 support
```

**Build Extension:**
```bash
git clone https://github.com/satwareAG/php-firebird.git
cd php-firebird

phpize8.3
./configure --with-php-config=/usr/bin/php-config8.3 --with-firebird
make clean && make

# Test compilation
php8.3 -d extension=modules/firebird.so -m | grep firebird
```

**Install Extension:**
```bash
# Copy to extension directory
sudo cp modules/firebird.so $(php-config8.3 --extension-dir)/

# Enable in php.ini
echo "extension=firebird" | sudo tee -a /etc/php/8.3/cli/conf.d/20-firebird.ini
echo "extension=firebird" | sudo tee -a /etc/php/8.3/fpm/conf.d/20-firebird.ini

# Restart services
sudo systemctl restart php8.3-fpm
```

### CentOS/RHEL/Fedora

**Install Dependencies:**
```bash  
# CentOS/RHEL 8+
sudo dnf install -y epel-release
sudo dnf install -y php-devel gcc-c++ firebird-devel autoconf

# Fedora
sudo dnf install -y php-devel gcc-c++ firebird-devel autoconf
```

**Build and Install:**
```bash
phpize
./configure --with-firebird
make clean && make
sudo cp modules/firebird.so $(php-config --extension-dir)/
echo "extension=firebird" | sudo tee /etc/php.d/20-firebird.ini
```

## Windows Deployment

### Prerequisites

**Install Visual Studio Build Tools 2019+:**
```powershell
# Download and install Build Tools for Visual Studio
# https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2019

# Or via winget
winget install Microsoft.VisualStudio.2022.BuildTools
```

**Install PHP 8.2+ for Windows:**
```powershell
# Download from https://windows.php.net/download/
# Extract to C:\php\

# Add to PATH
[Environment]::SetEnvironmentVariable("PATH", "$env:PATH;C:\php", "User")
```

**Install Firebird Client:**
```powershell
# Download Firebird 5.0+ installer
Invoke-WebRequest -Uri "https://github.com/FirebirdSQL/firebird/releases/download/v5.0.3/Firebird-5.0.3.1683-ReleaseCandidate1-windows-x64.zip" -OutFile "firebird.zip"

# Extract
Expand-Archive -Path firebird.zip -DestinationPath C:\firebird
```

### Build Process

**Configure Build Environment:**
```batch
@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

set FIREBIRD_HOME=C:\firebird
set PHP_HOME=C:\php
```

**Configure and Build:**
```batch
phpize
configure.bat --with-firebird="%FIREBIRD_HOME%"
nmake clean
nmake
```

**Test and Install:**
```batch
php -d extension=Release\php_firebird.dll -m | findstr firebird

copy Release\php_firebird.dll "%PHP_HOME%\ext\"
echo extension=firebird >> "%PHP_HOME%\php.ini"
```

## macOS Deployment

### Prerequisites

**Install Xcode Command Line Tools:**
```bash
xcode-select --install
```

**Install Dependencies via Homebrew:**
```bash
# Install Homebrew if not present
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install PHP and Firebird
brew install php@8.3 firebird autoconf

# Verify C++17 support
clang --version  # Should support C++17 (clang 10.0+)
```

### Build Process

**Configure Build Environment:**
```bash
export PATH="/opt/homebrew/sbin:/opt/homebrew/bin:$PATH"
export PKG_CONFIG_PATH="/opt/homebrew/lib/pkgconfig"
```

**Build Extension:**
```bash
phpize
./configure --with-firebird --with-php-config=/opt/homebrew/bin/php-config@8.3
make clean && make

# Test compilation
/opt/homebrew/bin/php@8.3 -d extension=modules/firebird.so -m | grep firebird
```

**Install Extension:**
```bash
# Copy to extension directory
EXT_DIR=$(/opt/homebrew/bin/php-config@8.3 --extension-dir)
sudo cp modules/firebird.so "$EXT_DIR/"

# Enable in php.ini
echo "extension=firebird" >> /opt/homebrew/etc/php/8.3/php.ini

# Restart services (if using php-fpm)
brew services restart php@8.3
```

## Docker Deployment

### Multi-Version Development Environment

**Using Provided Docker Compose:**
```bash
# Start development environment
cd docker
docker-compose up -d php83-dev

# Build in specific PHP version
docker-compose exec php83-dev bash -c "cd /ext && phpize && ./configure --with-firebird && make"

# Test across versions
for version in php81-dev php82-dev php83-dev php84-dev php85-dev; do
  echo "Testing $version..."
  docker-compose exec $version bash -c "cd /ext && php -d extension=modules/firebird.so -m | grep firebird"
done
```

### Production Docker Image

**Create Dockerfile:**
```dockerfile
FROM php:8.3-fpm
RUN apt-get update && apt-get install -y \
    build-essential \
    autoconf \
    libfirebird-dev \
    && rm -rf /var/lib/apt/lists/*

COPY . /usr/src/php-firebird
WORKDIR /usr/src/php-firebird

RUN phpize && \
    ./configure --with-firebird && \
    make && \
    make install && \
    docker-php-ext-enable firebird
```

## Verification and Testing

### Extension Verification

**Basic Functionality Test:**
```php
<?php
// test_installation.php
if (!extension_loaded('firebird')) {
    die('❌ Firebird extension not loaded');
}

echo "✅ Extension loaded: " . phpversion('firebird') . "\n";

// Test client version function (safe with null parameters)
$version = fbird_client_version();
echo "✅ Client version: " . dechex($version) . "\n";

echo "🎉 Extension working correctly!\n";
```

**Run Verification:**
```bash
php test_installation.php
```

### Firebird Connection Test

**Database Connection Test:**
```php
<?php
// test_firebird_connection.php
$db = '/path/to/test.fdb';  // Adjust path
$username = 'SYSDBA';
$password = 'masterkey';

try {
    $connection = fbird_connect($db, $username, $password);
    if ($connection) {
        echo "✅ Firebird connection successful\n";
        
        // Test client version function
        $version = fbird_client_version();
        echo "✅ Client version: " . dechex($version) . "\n";
        
        fbird_close($connection);
        echo "✅ Connection closed safely\n";
    } else {
        echo "❌ Connection failed: " . fbird_errmsg() . "\n";
    }
} catch (Exception $e) {
    echo "❌ Error: " . $e->getMessage() . "\n";
}
```

## Migration from ibase_* to fbird_*

When upgrading from older versions using `ibase_*` functions:

1. **Search and replace** all function calls: `ibase_` → `fbird_`
2. **Update extension loading**: `extension=interbase` → `extension=firebird`
3. **Update extension checks**: `extension_loaded('interbase')` → `extension_loaded('firebird')`
4. **Update constants**: `IBASE_*` → `FBIRD_*`

See the [Migration Guide in README.md](../../README.md#migration-guide) for a complete function mapping table.

## Troubleshooting

### Common Issues

**Linux:**
- **Missing libfirebird-dev**: `sudo apt install libfirebird-dev`
- **PHP version mismatch**: Use correct `php-config8.x` for your PHP version
- **Permission denied**: Run with `sudo` for system-wide installation

**Windows:**
- **MSVC not found**: Install Visual Studio Build Tools 2019+
- **Firebird not found**: Verify installation and set FIREBIRD_HOME
- **Extension load failed**: Check PHP architecture matches (x64 vs x86)
- **fbclient.dll not found**: Ensure Firebird bin directory is in PATH

**macOS:**
- **Homebrew path issues**: Use full paths `/opt/homebrew/bin/php@8.3`
- **M1/M2/M3 compatibility**: Use arm64 builds, avoid Rosetta emulation
- **Permission issues**: Use `sudo` for system directories

### Performance Verification

**Run Performance Benchmark:**
```bash
# Compile and run performance validation
g++ -std=c++17 tests/performance_benchmark.cpp -o benchmark
./benchmark
```

**Expected Results:**
- std::optional operations: ~80-100 ns/call
- Input validation: <5% overhead (near 0% due to constexpr)
- Move semantics: 20-30% performance improvement over copy
- No performance regression from C++17 modernization

## CI/CD Integration

### GitHub Actions

**Cross-platform validation runs on:**
- Push to main/feature branches
- Pull requests to main
- Linux multi-PHP-version testing (8.1-8.5)
- Static analysis (clang-tidy, Cppcheck)
- Memory safety (AddressSanitizer, Valgrind)

### GitLab CI/CD

**Trigger Pipeline:**
```bash
git add .
git commit -m "feat: add new feature"
git push origin main
```

**Pipeline runs automatically:**
- Multi-version PHP builds (8.1-8.5)
- Static analysis
- Memory safety validation
- Cross-platform compilation verification

## Production Deployment Checklist

**Pre-Deployment:**
- [ ] All CI/CD pipelines passing (GitHub Actions)
- [ ] Static analysis reports clean (zero errors)
- [ ] Memory safety validation passed
- [ ] All 85 tests passing (100% pass rate)
- [ ] Cross-platform compilation verified

**Deployment:**
- [ ] Extension compiled for target platform and PHP version
- [ ] Firebird client libraries installed and accessible
- [ ] Extension enabled in php.ini configuration (`extension=firebird`)
- [ ] Web server/PHP-FPM restarted after installation
- [ ] Basic functionality verification completed

**Post-Deployment:**
- [ ] Extension loading verified: `php -m | grep firebird`
- [ ] Database connectivity tested with actual Firebird instance
- [ ] Application functionality validated with `fbird_*` functions
- [ ] Performance monitoring confirms expected behavior
- [ ] Error logs reviewed for any compatibility issues

This guide ensures reliable deployment of the php-firebird extension across all supported platforms with comprehensive validation and testing procedures.
