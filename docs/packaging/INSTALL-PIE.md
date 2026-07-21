# Installing php-firebird via PIE (PHP Installer for Extensions)

## What is PIE?

[PIE](https://github.com/php/pie) (PHP Installer for Extensions) is the modern replacement
for PECL. It uses Composer/Packagist for package discovery and builds extensions from
source on the target machine.

## Packages

| Package | Packagist URL | Provides |
|---------|---------------|----------|
| `satwareag/php-firebird` | https://packagist.org/packages/satwareag/php-firebird | Procedural `fbird_*` API + OOP `Firebird\*` classes |
| `satwareag/pdo-fbird` | https://packagist.org/packages/satwareag/pdo-fbird | PDO driver (`pdo_fbird`) - depends on `ext-firebird` |

## Prerequisites

- PHP 8.2, 8.3, 8.4, or 8.5 (CLI + dev headers)
- `pie` binary ([install](https://github.com/php/pie/blob/1.5.x/docs/usage.md))
- Firebird client library (if not bundling)
- Build tools: `gcc`, `g++`, `make`, `autoconf`

### Installing PIE

```bash
# Via Composer
composer global require php/pie

# Or download the phar directly
curl -sSLo pie https://github.com/php/pie/releases/latest/download/pie.phar
chmod +x pie && sudo mv pie /usr/local/bin/
```

## Installation

### 1. Install the main extension (procedural + OOP API)

```bash
pie install satwareag/php-firebird
```

This compiles `firebird.so` and installs it to your PHP extension directory.

### 2. Install the PDO driver (separate package)

```bash
pie install satwareag/pdo-fbird
```

This compiles `pdo_fbird.so` and installs it to your PHP extension directory.

### 3. Enable in php.ini

```ini
extension=firebird
extension=pdo_fbird
```

### 4. Verify

```bash
php -m | grep firebird
php -m | grep pdo_fbird
```

## Connecting to a database

### Procedural API

```php
<?php
$conn = fbird_connect('localhost:/path/to/database.fdb', 'SYSDBA', 'masterkey');
$result = fbird_query($conn, 'SELECT 1 FROM RDB$DATABASE');
$row = fbird_fetch_assoc($result);
var_dump($row);
fbird_close($conn);
```

### OOP API

```php
<?php
$db = Firebird\Database::connect('localhost:/path/to/database.fdb', 'SYSDBA', 'masterkey');
$result = $db->query('SELECT 1 FROM RDB$DATABASE');
foreach ($result as $row) {
    var_dump($row);
}
```

### PDO

```php
<?php
$pdo = new PDO('fbird:dbname=localhost:/path/to/database.fdb', 'SYSDBA', 'masterkey');
$stmt = $pdo->query('SELECT 1 FROM RDB$DATABASE');
var_dump($stmt->fetch());
```

## Firebird client library

The extension requires a Firebird client library. Options:

1. **Install from apt** (Debian/Ubuntu): `apt install firebird-dev`
2. **Download from GitHub**: [Firebird SQL releases](https://github.com/FirebirdSQL/firebird/releases)
   - FB 5.0 recommended (enables DECFLOAT, INT128, time zone types, batch DML)
   - FB 3.0/4.0 also supported

If `configure` can't find the Firebird client, specify the path:

```bash
pie install satwareag/php-firebird --with-firebird=/opt/firebird
```

## Version-specific notes

- **PHP 8.2**: DecFloat objects require PHP 8.3+ (tests skip on 8.2)
- **Firebird 3.0**: DECFLOAT, INT128, TIME WITH TIME ZONE, and batch DML are
  unavailable (FB 4.0+ features). All other functionality works.
- **Firebird 4.0+**: Full feature set including DECFLOAT, INT128, time zones, batch DML

## Alternatives

If you prefer pre-built binary packages, use the APT repository instead:
see [INSTALL-DEBIAN.md](INSTALL-DEBIAN.md).

## Troubleshooting

### `pie install` fails with "composer.json not found"

The Packagist package may be stale. Trigger a manual update:
1. Go to https://packagist.org/packages/satwareag/php-firebird
2. Click "Update" (or "Force Update")

### Extension loads but SIGSEGV on shutdown

This is a Firebird client version mismatch. The extension was compiled against
FB 5.0 headers but loaded with FB 3.0 client library. Ensure the client library
version matches the version used during `pie install`.

### `pdo_fbird` not found after installing `satwareag/pdo-fbird`

The PDO driver is a separate package. You must install BOTH:
```bash
pie install satwareag/php-firebird    # main extension
pie install satwareag/pdo-fbird       # PDO driver (depends on ext-firebird)
```

## See also

- [INSTALL-DEBIAN.md](INSTALL-DEBIAN.md) - APT repository installation
- [PIE documentation](https://github.com/php/pie/blob/1.5.x/docs/usage.md)
- [Packagist packages](https://packagist.org/?query=satwareag%20firebird)
