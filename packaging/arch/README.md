# php-firebird - Arch/Manjaro packaging

Local pacman-tracked package that builds and installs the
`firebird` (procedural `fbird_*`) and `pdo_fbird` (PDO `fbird:`) PHP extensions
from the upstream source tag.

## Why a PKGBUILD

Manually copying `.so` files into `/usr/lib/php/modules/` leaves them
**untracked by pacman**: no clean upgrade, no uninstall trail, and `.bak`
residue accumulates across rebuilds. A PKGBUILD installs to the same canonical
locations but pacman owns every file, so:

- `pacman -Syu` will not silently drop or overwrite them.
- `pacman -R php-firebird` removes them cleanly.
- Rebuilds are a one-line `makepkg -si`.

## Layout installed by the package

| File | Path |
|------|------|
| `firebird.so`     | `/usr/lib/php/modules/firebird.so`     |
| `pdo_fbird.so`    | `/usr/lib/php/modules/pdo_fbird.so`    |
| loader ini        | `/etc/php/conf.d/firebird.ini`         |
| loader ini (PDO)  | `/etc/php/conf.d/pdo_fbird.ini`        |

Both `.so` link against the system `libfbclient` and `libtommath` (declared in
`depends`). No rpath, no bundled libraries - identical approach to the rest of
the Arch `php-*` extension set.

## Build & install (first time)

```bash
cd packaging/arch
makepkg -si
```

`makepkg -si` builds in `$srcdir`, then installs via `sudo pacman -U`.
After install verify:

```bash
php -m | grep -iE '^(firebird|pdo_fbird)$'
php -i | grep 'Firebird extension version'   # -> 13.2.6
pacman -Ql php-firebird                       # files pacman now owns
```

## Rebuild after a new release

```bash
# 1. Bump pkgver
sed -i 's/^pkgver=.*/pkgver=13.2.7/' PKGBUILD

# 2. Refresh the source checksum (one of):
updpkgsums                   # from pacman-contrib, fetches + rewrites sha256sums
# or manually:
makepkg -g >> PKGBUILD       # print new checksums, then edit to replace the old

# 3. Build and reinstall
makepkg -si
```

## Rebuild after a PHP upgrade (module API change)

PHP extensions are ABI-pinned to the Zend Module API. When `pacman -Syu`
upgrades `php` across a minor boundary (e.g. 8.5 -> 8.6) the module API changes
and the extension stops loading with `PHP Warning: Module compiled with module
API mismatch`. Rebuild against the new headers:

```bash
cd packaging/arch
makepkg -si
```

If `php` jumped a major version, also confirm the upstream tag still supports it
(see the repo's CI matrix in `.github/workflows/`).

## Sanity checks after install

```bash
# module API must match between PHP and the extension
php-config --phpapi                                 # e.g. 20250925
php -i | grep 'PHP Extension Build'                 # API20250925,NTS
readelf -d /usr/lib/php/modules/firebird.so | grep -iE 'rpath|runpath' || echo 'no rpath (correct)'
ldd /usr/lib/php/modules/firebird.so | grep -iE 'fbclient|tommath|not found'
```

`ldd` should resolve `libfbclient.so.2` to `/usr/lib/libfbclient.so.2` and
`libtommath.so.1` to `/usr/lib/libtommath.so.1`, with no `not found` lines.

## Build prerequisites

Provided by `depends` / `makedepends` in the PKGBUILD, so `makepkg -si` pulls
them automatically:

- `php` (provides `phpize`, `php-config`, headers)
- `libfbclient` (provides `fb_config`, `/usr/include/firebird/`, `libfbclient.so`)
- `libtommath`
- `autoconf`, `automake`, `libtool` (for `phpize`)
