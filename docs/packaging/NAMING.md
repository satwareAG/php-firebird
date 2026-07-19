# Package Naming Convention

Canonical naming rules for php-firebird native distribution packages across all Linux distribution families and the PHP Installer for Extensions (PIE).

## Summary

| Distribution channel | Package name | Example install command |
|---|---|---|
| Debian/Ubuntu (APT) | `php{VERSION}-firebird` | `apt install php8.4-firebird` |
| RHEL/AlmaLinux/Rocky/Fedora (YUM/DNF) | `php-firebird` | `dnf install php-firebird` |
| Alpine (APK) | `php{VERSION}-pecl-firebird` | `apk add php84-pecl-firebird` |
| PIE / Packagist (procedural+OOP) | `satwareag/php-firebird` | `pie install satwareag/php-firebird` |
| PIE / Packagist (PDO driver) | `satwareag/pdo-fbird` | `pie install satwareag/pdo-fbird` |
| GitHub Releases (manual download) | `php-firebird-{ver}-php{XX}-{variant}-linux-{arch}.tar.gz` | `wget <url> && tar xzf <file>` |

## Architectures

| Architecture | Debian name | RPM name | Alpine name | PIE name |
|---|---|---|---|---|
| 64-bit x86 | `amd64` | `x86_64` | `x86_64` | `x86_64` |
| 64-bit ARM | `arm64` | `aarch64` | `aarch64` | `arm64` |
| 32-bit ARM | `armhf` | `armv7l` | `armv7` | (not supported by PIE) |
| 32-bit x86 | `i386` | `i686` | `x86` | `x86` |

## Debian/Ubuntu (APT)

**Package name**: `php{VERSION}-firebird`

### Binary packages

One binary package per supported PHP version:

| Package | PHP version | Provides |
|---|---|---|
| `php8.2-firebird` | 8.2 | `php-firebird`, `php-pdo-fbird` |
| `php8.3-firebird` | 8.3 | `php-firebird`, `php-pdo-fbird` |
| `php8.4-firebird` | 8.4 | `php-firebird`, `php-pdo-fbird` |
| `php8.5-firebird` | 8.5 | `php-firebird`, `php-pdo-fbird` |

### Control fields

```
Package: php8.4-firebird
Architecture: amd64 arm64 armhf
Depends: php8.4-common
Recommends: php8.4-pdo
Provides: php-firebird, php-pdo-fbird
Conflicts: php8.4-interbase
Replaces: php8.4-interbase
```

### File layout

```
usr/lib/php/{PHP_API_VERSION}/firebird.so
usr/lib/php/{PHP_API_VERSION}/pdo_fbird.so
usr/share/php/Firebird/*.php
usr/lib/php-firebird/libfbclient.so.5
etc/php/8.4/mods-available/20-firebird.ini
etc/php/8.4/mods-available/20-pdo_fbird.ini
```

### APT repo paths

```
packages.satware.com/apt/
├── dists/bookworm/main/binary-amd64/Packages.gz
├── dists/bookworm/main/binary-arm64/Packages.gz
├── dists/trixie/main/binary-amd64/Packages.gz
├── dists/jammy/main/binary-amd64/Packages.gz
├── dists/noble/main/binary-amd64/Packages.gz
└── pool/main/p/php8.4-firebird/php8.4-firebird_13.0.0-1_amd64.deb
```

## RHEL/AlmaLinux/Rocky/Fedora (YUM/DNF)

**Package name**: `php-firebird` (single package; multi-PHP via module streams)

### Spec file

```spec
Name:    php-firebird
Version: 13.0.0
Release: 1%{?dist}
Provides: php-firebird, php-pdo-fbird
Conflicts: php-interbase
```

### File layout

```
usr/lib64/php/modules/firebird.so
usr/lib64/php/modules/pdo_fbird.so
usr/share/php/Firebird/*.php
usr/lib/php-firebird/libfbclient.so.5
etc/php.d/20-firebird.ini
```

### YUM repo paths

```
packages.satware.com/rpm/
├── el/8/x86_64/php-firebird-13.0.0-1.el8.x86_64.rpm
├── el/8/aarch64/php-firebird-13.0.0-1.el8.aarch64.rpm
├── el/9/x86_64/...
├── fc/41/x86_64/...
└── repodata/repomd.xml
```

## Alpine (APK)

**Package name**: `php{VERSION}-pecl-firebird` (Alpine convention; `pecl-` prefix retained for consistency with existing Alpine PHP extension packages)

### APKBUILD

```sh
pkgname=php84-pecl-firebird
pkgver=13.0.0
pkgrel=0
arch="x86_64 aarch64"
depends="php84-common php84-pdo"
```

### File layout

```
usr/lib/php84/modules/firebird.so
usr/lib/php84/modules/pdo_fbird.so
usr/share/php/Firebird/*.php
usr/lib/php-firebird/libfbclient.so.5
etc/php84/conf.d/20_firebird.ini
```

### APK repo paths

```
packages.satware.com/alpine/
├── v3.21/main/x86_64/APKINDEX.tar.gz
├── v3.21/main/aarch64/APKINDEX.tar.gz
└── php84-pecl-firebird-13.0.0-r0.apk
```

## PIE / Packagist

**Package name**: `satwareag/php-firebird` (procedural + OOP API), `satwareag/pdo-fbird` (PDO driver)

### composer.json (root, firebird)

```json
{
    "name": "satwareag/php-firebird",
    "type": "php-ext",
    "php-ext": {
        "extension-name": "firebird",
        "priority": "20",
        "support-zts": true,
        "support-nts": true,
        "configure-options": [
            {
                "name": "with-firebird",
                "description": "Path to Firebird client installation",
                "needs-value": true
            }
        ],
        "download-url-method": ["pre-packaged-binary", "composer-default"]
    }
}
```

### pdo_fbird/composer.json (split package)

```json
{
    "name": "satwareag/pdo-fbird",
    "type": "php-ext",
    "php-ext": {
        "extension-name": "pdo_fbird",
        "priority": "15",
        "build-path": "pdo_fbird",
        "configure-options": [
            {
                "name": "with-pdo-fbird",
                "description": "Path to Firebird client installation",
                "needs-value": true
            }
        ],
        "download-url-method": ["pre-packaged-binary", "composer-default"]
    }
}
```

### PIE pre-packaged-binary asset naming

PIE expects release assets named:

```
php_{ExtensionName}-{Version}_php{PhpVersion}-{Arch}-{OS}-{Libc}-{Debug}-{TSMode}.{Format}
```

Examples:

| File | PHP | Arch | Libc | Variant |
|---|---|---|---|---|
| `php_firebird-13.1.0_php8.4-x86_64-linux-glibc-nts.zip` | 8.4 | x86_64 | glibc | nts |
| `php_firebird-13.1.0_php8.4-x86_64-linux-musl-nts.zip` | 8.4 | x86_64 | musl | nts |
| `php_firebird-13.1.0_php8.4-aarch64-linux-glibc-nts.zip` | 8.4 | aarch64 | glibc | nts |
| `php_firebird-13.1.0_php8.4-x86_64-linux-glibc-zts.zip` | 8.4 | x86_64 | glibc | zts |
| `php_pdo_fbird-13.1.0_php8.4-x86_64-linux-glibc-nts.zip` | 8.4 | x86_64 | glibc | nts |

## GitHub Releases (tarball bundles)

**Asset name**: `php-firebird-{ver}-php{XX}-{variant}-linux-{arch}.tar.gz`

Examples:

| File | PHP | Variant | Arch |
|---|---|---|---|
| `php-firebird-13.1.0-php84-nts-linux-x86_64.tar.gz` | 8.4 | nts | x86_64 |
| `php-firebird-13.1.0-php84-nts-linux-aarch64.tar.gz` | 8.4 | nts | aarch64 |
| `php-firebird-13.1.0-php84-zts-linux-x86_64.tar.gz` | 8.4 | zts | x86_64 |
| `php-firebird-13.1.0-php84-nts-linux-musl-x86_64.tar.gz` | 8.4 | nts | x86_64 (musl) |

## Extension name vs package name

| Concept | Value | Why |
|---|---|---|
| PHP extension name (lowercase, no `ext-` prefix) | `firebird` | Used in `php -m` output, `extension=firebird.so` in ini |
| PHP extension name (PDO) | `pdo_fbird` | Used in `php -m` output, `extension=pdo_fbird.so` in ini |
| Composer `ext-*` require name | `ext-firebird`, `ext-pdo_fbird` | Used by `composer.json` `require` blocks |
| Debian package name | `php8.4-firebird` | Debian PHP extension naming convention |
| RPM package name | `php-firebird` | RPM PHP extension naming convention |
| Alpine package name | `php84-pecl-firebird` | Alpine PHP extension naming convention (legacy `pecl-` prefix) |
| PIE/Packagist name | `satwareag/php-firebird`, `satwareag/pdo-fbird` | Composer vendor/package format |

## Bundled Firebird client

All packages bundle `libfbclient.so.5` (Firebird 5.0.x client) with RPATH `$ORIGIN/../../php-firebird`. This ensures `#if FB_API_VER >= 40` code paths (DECFLOAT, INT128, time zones, batch DML, `pdo_fbird`) are always enabled regardless of what `firebird-dev` package the distro ships. No `Depends: libfbclient-dev` or `firebird-dev` is declared.

### RPATH calculation

Package layout:
```
usr/lib/php/{PHP_API_VERSION}/firebird.so    # e.g. usr/lib/php/20240924/firebird.so
usr/lib/php-firebird/libfbclient.so.5        # Bundled FB5 client + deps
```

`$ORIGIN` resolves to `usr/lib/php/20240924/` (the directory containing `firebird.so`). The relative path to `usr/lib/php-firebird/` is `../../php-firebird`:
- `$ORIGIN/..` = `usr/lib/php/`
- `$ORIGIN/../..` = `usr/lib/`
- `$ORIGIN/../../php-firebird` = `usr/lib/php-firebird/` ✓

Verify with: `readelf -d $(php-config --extension-dir)/firebird.so | grep RPATH`

## Conflicts and migration

All packages declare `Conflicts: php-interbase` (or `php8.4-interbase` on Debian) to supersede the legacy `FirebirdSQL/php-firebird` v3.0.0 fork still used by some downstream consumers. Users migrating from legacy `interbase` to modern `firebird` should:

```bash
# Debian/Ubuntu
sudo apt remove php8.4-interbase
sudo apt install php8.4-firebird

# RHEL/Fedora
sudo dnf remove php-interbase
sudo dnf install php-firebird
```

The `Replaces: php8.4-interbase` declaration allows the new package to take over files owned by the legacy package without conflicts.
