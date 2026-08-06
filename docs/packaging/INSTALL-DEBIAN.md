# Installing php-firebird on Debian/Ubuntu via APT

## Prerequisites

- Debian 12 (Bookworm), Debian 13 (Trixie), Ubuntu 22.04 (Jammy), or Ubuntu 24.04 (Noble)
- PHP 8.2, 8.3, 8.4, or 8.5 installed via system packages (`apt install php8.4-cli`)
- `sudo` access for installation

## Staging vs Production

| Environment | URL | GPG key | Status |
|---|---|---|---|
| **Staging** | `packages.auc.de/apt/` | Staging (`B600F371...`) | Live (for testing) |
| **Production** | `packages.satware.com/apt/` | Production (`E914FA35...`) | Coming soon |

The **staging** repository is live and fully functional. When production is available, switch by replacing the URL and GPG key. The quick start below uses staging URLs; see the production section below for the production alternative.

## Quick start (staging)

```bash
# 1. Add the satware APT repository (staging)
curl -fsSL https://packages.auc.de/keys/staging-gpg.pub.asc \
  | sudo gpg --dearmor -o /etc/apt/keyrings/satware-php-firebird.gpg
echo "deb [signed-by=/etc/apt/keyrings/satware-php-firebird.gpg] https://packages.auc.de/apt $(lsb_release -cs) main" \
  | sudo tee /etc/apt/sources.list.d/satware-php-firebird.list

# 2. Update package list
sudo apt update

# 3. Install the extension for your PHP version
sudo apt install php8.4-firebird

# 4. Verify
php -m | grep firebird
php -m | grep pdo_fbird
```

Staging GPG fingerprint: `B600F3717315AD4262AB81A95AB7B54BBBC7FED8`

## Production installation

When the production repository at `packages.satware.com` is available:

```bash
# Import production GPG key
curl -fsSL https://packages.satware.com/keys/prod-gpg.pub.asc \
  | sudo gpg --dearmor -o /etc/apt/keyrings/satware-php-firebird.gpg

# Add production APT source
echo "deb [signed-by=/etc/apt/keyrings/satware-php-firebird.gpg] https://packages.satware.com/apt $(lsb_release -cs) main" \
  | sudo tee /etc/apt/sources.list.d/satware-php-firebird.list

sudo apt update
sudo apt install php8.4-firebird
```

Production GPG fingerprint: `E914FA3545EBA11013D8875D7CA900B9AB260C65`

## Available packages

| Package | PHP version | Provides |
|---|---|---|
| `php8.2-firebird` | 8.2 | `firebird.so` + `pdo_fbird.so` + OO API classes + bundled FB5 client |
| `php8.3-firebird` | 8.3 | same |
| `php8.4-firebird` | 8.4 | same |
| `php8.5-firebird` | 8.5 | same |

Each package:
- **Bundles Firebird 5.0 client** (libfbclient) — no need to install `firebird-dev` from apt
- **Enables FB4+ features** (DECFLOAT, INT128, TIME/TIMESTAMP WITH TIME ZONE, batch DML, `pdo_fbird`)
- **Provides**: `php-firebird`, `php-pdo-fbird` (virtual packages)
- **Conflicts**: `php{VERSION}-interbase` (legacy FirebirdSQL/php-firebird migration)

## Architecture support

| Architecture | Status |
|---|---|
| `amd64` (x86_64) | ✅ Supported |
| `arm64` (aarch64) | ✅ Supported |
| `armhf` (armv7l) | Planned (Phase 3) |

## Migration from legacy `php-interbase`

If you previously used the legacy `FirebirdSQL/php-firebird` v3.0.0 (installed as `interbase`):

```bash
# Remove legacy package
sudo apt remove php8.4-interbase

# Install modern package
sudo apt install php8.4-firebird

# Verify migration
php -m | grep firebird   # should show 'firebird' (not 'interbase')
```

## What's bundled

The package includes:

- `firebird.so` — main extension (procedural `fbird_*` + OOP `Firebird\*` classes)
- `pdo_fbird.so` — PDO driver for Firebird
- `Firebird\*.php` — OOP API class files (Connection, Database, TBuilder, etc.)
- `libfbclient.so.5` — Firebird 5.0 client library (bundled, not from apt)
- `libtomcrypt.so` — crypto library (libfbclient dependency)
- `libtommath.so` — math library (libfbclient dependency)

The extension uses **RPATH** (`$ORIGIN/../../php-firebird`) to find the bundled libraries, so no system `firebird-dev` package is needed.

## Uninstallation

```bash
sudo apt remove php8.4-firebird
sudo rm /etc/apt/sources.list.d/satware-php-firebird.list
sudo apt update
```

## Alternative installation methods

- **PIE (Packagist)**: `pie install satwareag/php-firebird` (source build)
- **GitHub Releases**: Download `.deb` directly from [releases](https://github.com/satwareAG/php-firebird/releases) and `dpkg -i`
- **Composer (stubs only)**: `composer require satwareag/php-firebird-stubs` (for IDE/PHPStan, not the extension)

## Troubleshooting

### Extension not loading after install

```bash
# Check if INI file exists
ls /etc/php/8.4/mods-available/firebird.ini

# Enable the extension
sudo phpenmod firebird

# Check PHP
php -m | grep firebird
```

### "libfbclient.so.5: cannot open shared object file"

The bundled libraries should resolve via RPATH. If not:

```bash
# Check RPATH
readelf -d $(php-config --extension-dir)/firebird.so | grep RPATH

# Should show: $ORIGIN/../../php-firebird

# Check bundled libs
ls /usr/lib/php-firebird/libfbclient*
```

### DECFLOAT not available

DECFLOAT requires FB4+ client (bundled). If you see strings instead of `Firebird\DecFloat` objects:

```bash
# Verify FB5 client is bundled (not system FB3)
php -r 'echo fb_server_supports("DECFLOAT") ? "YES" : "NO";'
```

If `NO`, the system `firebird-dev` (FB3) may be shadowing the bundled FB5 client. Remove it:

```bash
sudo apt remove firebird-dev
```
