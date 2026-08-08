# Installing php-firebird on RHEL/AlmaLinux/Fedora via DNF/YUM

## Prerequisites

- AlmaLinux 8/9, Rocky Linux 8/9, RHEL 8/9, or Fedora 41
- PHP 8.2 or 8.3 installed via system packages (`dnf install php-cli`)
- `sudo` access for installation

## Staging vs Production

| Environment | URL | Status |
|---|---|---|
| **Staging** | `packages.auc.de/rpm/` | Live (for testing) |
| **Production** | `packages.satware.com/rpm/` | Coming soon |

This document uses the **staging** repository. When production is available, replace `packages.auc.de` with `packages.satware.com` and use the production GPG key.

## Quick start

```bash
# 1. Import the satware GPG public key (staging)
sudo rpm --import https://packages.auc.de/keys/staging-gpg.pub.asc

# 2. Add the satware YUM repository
sudo tee /etc/yum.repos.d/satware-php-firebird.repo << 'EOF'
[satware-php-firebird]
name=satware PHP Firebird Extension
baseurl=https://packages.auc.de/rpm/el$releasever/x86_64/
enabled=1
gpgcheck=1
gpgkey=https://packages.auc.de/keys/staging-gpg.pub.asc
EOF

# 3. Install the extension
sudo dnf install php-firebird

# 4. Verify
php -m | grep firebird
php -m | grep pdo_fbird
```

## Available packages

| Package | PHP version | Provides |
|---|---|---|
| `php-firebird` | 8.2 (EL8/EL9) / 8.3 (Fedora 41) | `firebird.so` + `pdo_fbird.so` + OO API classes + bundled FB5 client |

Each package:
- **Bundles Firebird 5.0 client** (libfbclient) - no need to install `firebird-devel` from dnf
- **Enables FB4+ features** (DECFLOAT, INT128, TIME/TIMESTAMP WITH TIME ZONE, batch DML, `pdo_fbird`)
- **Obsoletes**: `php-interbase` (legacy FirebirdSQL/php-firebird migration)
- **Provides**: `php-firebird`, `php-interbase` (virtual packages)

## Distro-specific PHP versions

The RPM is built against the distro-default PHP:

| Distro | Default PHP | Package |
|---|---|---|
| AlmaLinux 8 / Rocky 8 / RHEL 8 | PHP 8.2 | `php-firebird` (el8) |
| AlmaLinux 9 / Rocky 9 / RHEL 9 | PHP 8.2 | `php-firebird` (el9) |
| Fedora 41 | PHP 8.3 | `php-firebird` (fc41) |

For PHP 8.4+ on EL8/EL9, use the [Remi repository](https://blog.remirepo.net/) to enable a newer PHP stream, then install `php-firebird` from our repo.

## Architecture support

| Architecture | Status |
|---|---|
| `x86_64` | Supported |
| `aarch64` | Planned (Phase 3) |
| `armv7l` | Planned (Phase 3) |

## Migration from legacy `php-interbase`

If you previously used the legacy `FirebirdSQL/php-firebird` v3.0.0 (installed as `interbase`):

```bash
# Remove legacy package
sudo dnf remove php-interbase

# Install modern package
sudo dnf install php-firebird

# Verify migration
php -m | grep firebird   # should show 'firebird' (not 'interbase')
```

## What's bundled

The package includes:

- `firebird.so` - main extension (procedural `fbird_*` + OOP `Firebird\*` classes)
- `pdo_fbird.so` - PDO driver for Firebird
- `Firebird\*.php` - OOP API class files (Connection, Database, TBuilder, etc.)
- `libfbclient.so.5` - Firebird 5.0 client library (bundled, not from dnf)
- `libtomcrypt.so` - crypto library (libfbclient dependency)
- `libtommath.so` - math library (libfbclient dependency)

The extension uses **RPATH** (`$ORIGIN/../../php-firebird`) to find the bundled libraries, so no system `firebird-devel` package is needed.

## Uninstallation

```bash
sudo dnf remove php-firebird
sudo rm /etc/yum.repos.d/satware-php-firebird.repo
```

## Alternative installation methods

- **PIE (Packagist)**: `pie install satwareag/php-firebird` (source build)
- **GitHub Releases**: See [INSTALL-GITHUB.md](INSTALL-GITHUB.md) for `wget + dnf install` from releases
- **Composer (stubs only)**: `composer require satwareag/php-firebird-stubs` (for IDE/PHPStan, not the extension)

## Troubleshooting

### Extension not loading after install

```bash
# Check if INI file exists
ls /etc/php.d/20-firebird.ini

# Check PHP
php -m | grep firebird
```

### "libfbclient.so: cannot open shared object file"

The bundled libraries should resolve via RPATH. If not:

```bash
# Check RPATH
readelf -d $(php-config --extension-dir)/firebird.so | grep RPATH

# Should show: $ORIGIN/../../php-firebird

# Check bundled libs (x86_64 uses /usr/lib64, aarch64 uses /usr/lib)
ls /usr/lib64/php-firebird/libfbclient*
```

### DECFLOAT not available

DECFLOAT requires FB4+ client (bundled). If you see strings instead of `Firebird\DecFloat` objects:

```bash
# Verify FB5 client is bundled (not system FB3)
php -r 'echo fb_server_supports("DECFLOAT") ? "YES" : "NO";'
```

If `NO`, the system `firebird-devel` (FB3) may be shadowing the bundled FB5 client. Remove it:

```bash
sudo dnf remove firebird-devel
```

### GPG key verification fails

Verify the imported key fingerprint matches the staging key:

```bash
rpm -qa gpg-pubkey --qf '%{NAME}-%{VERSION}-%{RELEASE}\t%{SUMMARY}\n' | grep satware
```

Staging GPG fingerprint: `B600F3717315AD4262AB81A95AB7B54BBBC7FED8`
