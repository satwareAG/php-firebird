# Installing php-firebird from GitHub Releases

## Overview

GitHub Releases host prebuilt `.tar.gz` bundles and, starting with v13.1.0,
native `.deb`, `.rpm`, and `.apk` packages for direct download. This is the
simplest installation method if you do not want to configure a full
APT/YUM/APK repository.

Native packages are attached to the release as **draft assets** during the
build pipeline, then published when the release is finalized. They include the
bundled Firebird 5.0 client library - no separate `firebird-dev` install needed.

> **Note**: Native `.deb`/`.rpm`/`.apk` packages are available starting with
> **v13.1.0**. Earlier releases (v13.0.x) only have `.tar.gz` bundles. See
> [INSTALL-PIE.md](INSTALL-PIE.md) for tarball bundle installation.

## Available packages (v13.1.0+)

| Format | File pattern | Distro | Arch |
|---|---|---|---|
| `.deb` | `php8.{2,3,4,5}-firebird_*_amd64.deb` | Debian 12/13, Ubuntu 22.04/24.04 | x86_64 |
| `.deb` | `php8.{2,3,4,5}-firebird_*_arm64.deb` | Debian 12/13, Ubuntu 22.04/24.04 | aarch64 |
| `.rpm` | `php-firebird-*.fc41.x86_64.rpm` | Fedora 41 | x86_64 |
| `.apk` | `php84-firebird-*.apk` | Alpine 3.21+ | x86_64 |

## Finding your package

1. Go to [releases](https://github.com/satwareAG/php-firebird/releases)
2. Find the latest release (e.g. `v13.1.0`)
3. Download the package matching your distro + PHP version + architecture

Or from the command line:

```bash
# List release assets for the latest version
gh release view --repo satwareAG/php-firebird --json assets --jq '.assets[].name'

# Download a specific .deb
gh release download --repo satwareAG/php-firebird --pattern 'php8.4-firebird*_amd64.deb'
```

## Debian/Ubuntu (.deb)

```bash
# Download the .deb for your PHP version and architecture
wget https://github.com/satwareAG/php-firebird/releases/download/v13.1.0/php8.4-firebird_13.1.0-1_amd64.deb

# Install
sudo apt install ./php8.4-firebird_13.1.0-1_amd64.deb

# Verify
php -m | grep firebird
php -m | grep pdo_fbird
php -r 'echo function_exists("fbird_connect") ? "YES" : "NO";'
```

**Notes**:
- Use `apt install ./file.deb` (not `dpkg -i`) so apt resolves dependencies
  automatically (`libtommath1`, `php8.4-common`)
- The `.deb` bundles `libfbclient.so.5`, `libtomcrypt.so`, `libtommath.so`
  under `/usr/lib/php-firebird/` with RPATH `$ORIGIN/../../php-firebird`
- INI files are installed to `/etc/php/{VERSION}/mods-available/`

## RHEL/Fedora (.rpm)

```bash
# Download the .rpm for your distro
wget https://github.com/satwareAG/php-firebird/releases/download/v13.1.0/php-firebird-13.1.0-1.fc41.x86_64.rpm

# Install
sudo dnf install ./php-firebird-13.1.0-1.fc41.x86_64.rpm

# Verify
php -m | grep firebird
php -m | grep pdo_fbird
php -r 'echo function_exists("fbird_connect") ? "YES" : "NO";'
```

**Notes**:
- The RPM is built against the distro-default PHP (Fedora 41 = PHP 8.3)
- The `.rpm` bundles `libfbclient.so.5` under `/usr/lib64/php-firebird/`
- INI files are installed to `/etc/php.d/`

## Alpine (.apk)

```bash
# Download the .apk
wget https://github.com/satwareAG/php-firebird/releases/download/v13.1.0/php84-firebird-13.1.0-r0.apk

# Install (allow untrusted since GitHub Releases are not signed with an APK key)
sudo apk add --allow-untrusted ./php84-firebird-13.1.0-r0.apk

# Verify
php -m | grep firebird
php -m | grep pdo_fbird
php -r 'echo function_exists("fbird_connect") ? "YES" : "NO";'
```

**Notes**:
- Alpine 3.21+ ships PHP 8.4 as `php84`
- The `.apk` bundles the Firebird client built from source against musl libc
- INI files are installed to `/etc/php84/conf.d/`

## Version selection

Replace the version in the URLs above with your target release:

| Release | URL tag | Notes |
|---|---|---|
| v13.1.0 | `v13.1.0` | First release with native .deb/.rpm/.apk |
| v13.0.3 | `v13.0.3` | Tarball bundles only |

Check [releases](https://github.com/satwareAG/php-firebird/releases) for the
complete list.

## What's bundled

All packages include:

- `firebird.so` - main extension (procedural `fbird_*` + OOP `Firebird\*` classes)
- `pdo_fbird.so` - PDO driver for Firebird
- `Firebird\*.php` - OOP API class files (Connection, Transaction, etc.)
- `libfbclient.so.5` - Firebird 5.0 client library (bundled, not from distro)
- `libtomcrypt.so` - crypto library (libfbclient dependency)
- `libtommath.so` - math library (libfbclient dependency)

The extension uses **RPATH** (`$ORIGIN/../../php-firebird`) to find the bundled
libraries, so no system `firebird-dev`/`firebird-devel` package is needed.

## Alternative installation methods

- **APT repository**: See [INSTALL-DEBIAN.md](INSTALL-DEBIAN.md) for `apt install` from our repo
- **YUM repository**: See [INSTALL-RPM.md](INSTALL-RPM.md) for `dnf install` from our repo
- **APK repository**: See [INSTALL-ALPINE.md](INSTALL-ALPINE.md) for `apk add` from our repo
- **PIE (Packagist)**: `pie install satwareag/php-firebird` (source build)
- **Composer (stubs only)**: `composer require satwareag/php-firebird-stubs` (for IDE/PHPStan)

## Troubleshooting

### Extension not loading after install

```bash
# Check if the .so file exists
ls $(php-config --extension-dir)/firebird.so

# Check if INI file exists
ls /etc/php/8.4/mods-available/firebird.ini  # Debian
ls /etc/php.d/20-firebird.ini                 # Fedora
ls /etc/php84/conf.d/firebird.ini             # Alpine

# Check PHP
php -m | grep firebird
```

### "libfbclient.so: cannot open shared object file"

```bash
# Check RPATH
readelf -d $(php-config --extension-dir)/firebird.so | grep RPATH
# Should show: $ORIGIN/../../php-firebird

# Check bundled libs
ls /usr/lib/php-firebird/libfbclient*    # Debian
ls /usr/lib64/php-firebird/libfbclient*  # Fedora
```

### Wrong PHP version

GitHub Release packages are built for specific PHP versions. If you get
"PHP Warning: invalid library version", make sure the package matches your
installed PHP:

```bash
php -v  # Check your PHP version
```

Download the `.deb`/`.rpm`/`.apk` matching your PHP version (8.2, 8.3, 8.4, or 8.5).
