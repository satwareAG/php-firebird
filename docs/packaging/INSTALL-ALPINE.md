# Installing php-firebird on Alpine Linux via APK

## Prerequisites

- Alpine Linux 3.21 or later
- PHP 8.4 installed via `apk add php84-cli`
- `doas` or `sudo` access for installation (root user can skip)

## Staging vs Production

| Environment | URL | Status |
|---|---|---|
| **Staging** | `packages.auc.de/alpine/` | Live (for testing) |
| **Production** | `packages.satware.com/alpine/` | Coming soon |

This document uses the **staging** repository. When production is available, replace `packages.auc.de` with `packages.satware.com`.

## Quick start

```bash
# 1. Download the satware RSA public key for APK verification
doas wget -O /etc/apk/keys/satware-apk.rsa.pub \
  https://packages.auc.de/keys/apk-rsa-public.pem

# 2. Add the satware APK repository
echo "https://packages.auc.de/alpine/v3.21/main" \
  | doas tee -a /etc/apk/repositories

# 3. Update package index
doas apk update

# 4. Install the extension
doas apk add php84-firebird

# 5. Verify
php84 -m | grep firebird
php84 -m | grep pdo_fbird
```

## Available packages

| Package | PHP version | Provides |
|---|---|---|
| `php84-firebird` | 8.4 | `firebird.so` + `pdo_fbird.so` + OO API classes + bundled FB5 client |

The package:
- **Bundles Firebird 5.0 client** (libfbclient) - built from source for musl libc
- **Enables FB4+ features** (DECFLOAT, INT128, TIME/TIMESTAMP WITH TIME ZONE, batch DML, `pdo_fbird`)
- **Provides/replaces**: `php84-interbase` (legacy extension migration)

## Architecture support

| Architecture | Status |
|---|---|
| `x86_64` | Supported |
| `aarch64` | Planned (Phase 3, pending Firebird CMake `-msse4` fix) |
| `armv7l` | Planned (Phase 3) |

## Musl libc note

Alpine Linux uses **musl libc**, not glibc. The official Firebird client tarball is glibc-linked and cannot be used on Alpine. The `php84-firebird` package builds `libfbclient` from source (FirebirdSQL/firebird v5.0.4) against musl, ensuring full compatibility.

This means:
- No `firebird-dev` apk package needed
- No RPATH needed (Alpine convention: standard linker paths only)
- ICU libraries come from the system `icu-libs` package (not bundled)

## Migration from legacy interbase

If you previously used a legacy Firebird extension:

```bash
# Remove legacy package (if installed)
doas apk del php84-interbase 2>/dev/null || true

# Install modern package
doas apk add php84-firebird

# Verify migration
php84 -m | grep firebird   # should show 'firebird'
```

## What's bundled

The package includes:

- `firebird.so` - main extension (procedural `fbird_*` + OOP `Firebird\*` classes)
- `pdo_fbird.so` - PDO driver for Firebird
- `Firebird\*.php` - OOP API class files (Connection, Database, TBuilder, etc.)
- `libfbclient.so` - Firebird 5.0 client library (musl-built from source)

INI files are installed to `/etc/php84/conf.d/`:
- `20_firebird.ini` - loads `firebird.so`
- `30_pdo_fbird.ini` - loads `pdo_fbird.so`

## Uninstallation

```bash
doas apk del php84-firebird

# Remove repository
doas sed -i '/packages.auc.de\/alpine/d' /etc/apk/repositories
doas rm /etc/apk/keys/satware-apk.rsa.pub
doas apk update
```

## Alternative installation methods

- **PIE (Packagist)**: `pie install satwareag/php-firebird` (source build)
- **GitHub Releases**: See [INSTALL-GITHUB.md](INSTALL-GITHUB.md) for `wget + apk add` from releases
- **Composer (stubs only)**: `composer require satwareag/php-firebird-stubs` (for IDE/PHPStan, not the extension)

## Troubleshooting

### Extension not loading after install

```bash
# Check if INI files exist
ls /etc/php84/conf.d/20_firebird.ini
ls /etc/php84/conf.d/30_pdo_fbird.ini

# Check PHP
php84 -m | grep firebird
```

### "libfbclient.so: cannot open shared object file"

The bundled library is in `/usr/lib/`. Verify:

```bash
ls /usr/lib/libfbclient.so*
ldd $(php-config84 --extension-dir)/firebird.so | grep fbclient
```

### DECFLOAT not available

DECFLOAT requires FB4+ client (bundled). If you see strings instead of `Firebird\DecFloat` objects:

```bash
# Verify FB5 client is loaded
php84 -r 'echo fb_server_supports("DECFLOAT") ? "YES" : "NO";'
```

### APK signature verification fails

Verify the RSA key was installed correctly:

```bash
ls -la /etc/apk/keys/satware-apk.rsa.pub
```

If missing, re-download:

```bash
doas wget -O /etc/apk/keys/satware-apk.rsa.pub \
  https://packages.auc.de/keys/apk-rsa-public.pem
doas apk update
```
