---
description: >-
  Ideal PHP 8.5 local development setup for php-firebird C extension development
  on Manjaro/Arch Linux — extension selection, performance tooling, Xdebug
  configuration, and gcov/lcov C-level coverage.
tags:
  - php
  - php-extension
  - firebird
  - xdebug
  - opcache
  - development
  - manjaro
  - arch-linux
last_updated: '2026-03-04'
---

# PHP 8.5 Local Dev Setup — php-firebird Extension Development

**Context**: PHP 8.5.3 (NTS, CLI) on Manjaro Linux (Arch-based).
**Use case**: C extension developer for `satwareAG/php-firebird` — databases are
SQLite and Firebird **only** (no MySQL, no PostgreSQL).

---

## Current State (2026-03-04)

### System

| Item | Value |
|------|-------|
| PHP | 8.5.3 (NTS, CLI) — built 2026-02-12 |
| Firebird client | `/usr/lib/libfbclient.so.5.0.3` |
| php-firebird | `7.1.0-rc.1-1-g6424966` at `/usr/lib/php/modules/firebird.so` |
| Xdebug | 3.5.1 |
| Config dir | `/etc/php/conf.d/` |

### Changes Applied

| Action | File | Reason |
|--------|------|--------|
| ✅ Disabled | `pdo_mysql.ini` → `.disabled` | No MySQL needed |
| ✅ Disabled | `pdo_pgsql.ini` → `.disabled` | No PostgreSQL needed |
| ⏳ Pending | `xdebug.ini` update | Mode separation |
| ⏳ Pending | `apcu.ini` enable | User cache for testing |
| ⏳ Pending | `opcache-cli.ini` create | CLI OPcache for benchmarks |

---

## Recommended Extension Set

### Keep (essential)

| Extension | Why |
|-----------|-----|
| `firebird` | Primary DB — procedural `fbird_*` API |
| `pdo_sqlite` + `sqlite3` | Lightweight DB for unit tests |
| `PDO` | Base PDO layer |
| `bcmath` | Numeric precision in tests |
| `curl` | HTTP in integration tests |
| `dom`, `xml`, `xmlreader`, `xmlwriter`, `SimpleXML` | XML processing |
| `gd` | Image tests (if needed) |
| `iconv`, `intl`, `mbstring` | String/encoding |
| `json` | Serialization |
| `openssl` | Crypto |
| `pcntl` | Process control (async tests) |
| `posix` | POSIX functions |
| `sodium` | Modern crypto |
| `zip`, `zlib` | Compression |
| `Xdebug` | Debug + coverage |
| `Zend OPcache` | Always compiled in PHP 8.5 |

### Disable (not needed for this use case)

| Extension | File | Reason |
|-----------|------|--------|
| `pdo_mysql` | `pdo_mysql.ini` | No MySQL |
| `pdo_pgsql` | `pdo_pgsql.ini` | No PostgreSQL |
| `mysqlnd` | Built-in, cannot disable separately | Loaded by pdo_mysql — gone after disabling |

### Enable (currently disabled)

| Extension | File | Action |
|-----------|------|--------|
| `apcu` | `apcu.ini` | Uncomment `extension=apcu.so` |

### Consider Adding

| Extension | Install | Use case |
|-----------|---------|----------|
| `pdo_firebird` | Built into PHP — needs `--with-pdo-firebird` at compile time | PDO interface to Firebird (complement to procedural `fbird_*`) |
| `igbinary` | `pecl install igbinary` | Faster serialization for cache testing |

**Note on `pdo_firebird`**: The Arch PHP package may not include it. Check:
`php -m | grep -i pdo_firebird`. If missing, it requires recompiling PHP with
`--with-pdo-firebird=/usr`. For the `php-firebird` extension itself (procedural
`fbird_*` API), `pdo_firebird` is not required.

**Note on `parallel`**: Requires ZTS (thread-safe) PHP build (`--enable-maintainer-zts`).
The current NTS build cannot use it. Not recommended unless extension specifically
targets multithreaded PHP.

---

## Xdebug Configuration

### Daily Development (`/etc/php/conf.d/xdebug.ini`)

```ini
; Xdebug 3.x — php-firebird C extension development
; Mode: debug + coverage for daily dev work
zend_extension=xdebug.so

; Active modes: step-debugger + code coverage
; Override per-run: XDEBUG_MODE=off php bench.php
xdebug.mode=debug,coverage

; Step debugger — IDE listens on 9003 (PhpStorm/VS Code default)
xdebug.start_with_request=default
xdebug.client_host=127.0.0.1
xdebug.client_port=9003
xdebug.idekey=PHPSTORM

; ⚠️  NEVER benchmark with Xdebug enabled — 2-5x overhead minimum
; Disable: XDEBUG_MODE=off php scripts/bench.php
```

### Profiling (separate file `/etc/php/conf.d/xdebug-profile.ini.disabled`)

```ini
; Activate: sudo mv xdebug-profile.ini.disabled xdebug-profile.ini
; Or per-run: XDEBUG_MODE=profile php script.php
zend_extension=xdebug.so
xdebug.mode=profile
xdebug.start_with_request=trigger
xdebug.use_compression=false
xdebug.profiler_output_name=cachegrind.out.%p.%t
xdebug.output_dir=/tmp/xdebug-profiles
```

Analyze with: `kcachegrind /tmp/xdebug-profiles/cachegrind.out.*`

### Mode Reference

| Mode | Use | Overhead |
|------|-----|----------|
| `debug` | Step debugging, breakpoints | Low when IDE not connected |
| `coverage` | PHP line coverage for `.phpt` tests | Medium |
| `profile` | Cachegrind output for KCacheGrind | High — trigger only |
| `trace` | Full call trace to file | Very high |
| `develop` | Enhanced `var_dump()` | Minimal |
| `gcstats` | GC statistics | Low |
| `off` | Completely disabled | Zero |

**Key rule**: Use `XDEBUG_MODE=off php ...` for all benchmarks and performance
measurements. Xdebug adds 2-5x overhead that completely invalidates timing results.

---

## OPcache CLI Configuration

PHP 8.5 ships OPcache as a **required built-in** (cannot be removed). For CLI,
it defaults to `Off`. Enable for benchmarking to match production behavior:

**Create `/etc/php/conf.d/opcache-cli.ini`**:

```ini
; OPcache for CLI — enables production-like caching for benchmarks
; PHP 8.5: OPcache is mandatory (compiled in), this just enables it for CLI
[opcache]
opcache.enable_cli=1
opcache.memory_consumption=128
opcache.interned_strings_buffer=16
opcache.max_accelerated_files=10000
opcache.validate_timestamps=1
opcache.revalidate_freq=0
; For read-only filesystem benchmarks:
;opcache.file_cache_read_only=1
```

**Verify**: `php -r "var_dump(opcache_get_status()['opcache_enabled']);"`

---

## APCu Configuration

Enable in `/etc/php/conf.d/apcu.ini`:

```ini
extension=apcu.so
apc.enabled=1
apc.shm_size=64M
apc.ttl=3600
; CLI: must explicitly enable (disabled by default for CLI)
apc.enable_cli=1
```

**Use case**: Testing extension code that interacts with user-space caching,
or benchmarking serialization paths.

---

## php.ini Tuning for CLI Dev

Current `/etc/php/conf.d/memory.ini` sets `memory_limit = -1` (unlimited) — acceptable
for CLI dev. Additional recommended settings (add to `php.ini` or a new conf.d file):

```ini
; /etc/php/conf.d/dev-cli.ini
; Development-only settings for CLI extension work

; Error reporting — show everything
error_reporting = E_ALL
display_errors = On
display_startup_errors = On
log_errors = On
error_log = /tmp/php-cli-errors.log

; Execution — no timeout for long test runs
max_execution_time = 0

; Timezone — UTC for consistent test behavior
date.timezone = UTC

; PHP 8.5 new: max_memory_limit (system ceiling, cannot be overridden at runtime)
; max_memory_limit = 2G  ; uncomment if needed
```

---

## C-Level Coverage: gcov + lcov

For measuring which C code in the extension is exercised by `.phpt` tests.

### Build with Coverage Instrumentation

```bash
# In the extension source directory
make clean && phpize --clean && phpize
CFLAGS="-fprofile-arcs -ftest-coverage -O0 -g" \
LDFLAGS="-fprofile-arcs -ftest-coverage" \
./configure --with-firebird=/usr
make -j$(nproc)
```

### Run Tests and Collect Coverage

```bash
# Run tests (generates .gcda files alongside .gcno files)
make test TESTS="tests/" NO_INTERACTION=1

# Collect coverage data
lcov --capture --directory . --output-file coverage.info \
     --exclude "*/tests/*" --exclude "/usr/*"

# Generate HTML report
genhtml coverage.info --output-directory coverage_html/
xdg-open coverage_html/index.html
```

### Docker-Based Coverage (Recommended)

The project's Docker setup handles this cleanly:

```bash
docker compose run --rm php83-dev /ext/scripts/coverage.sh
```

See `scripts/coverage.sh` for the full workflow.

### gcov vs Xdebug Coverage

| Tool | Measures | Use for |
|------|----------|---------|
| `gcov` + `lcov` | C source lines in `.c` files | Extension C code coverage |
| Xdebug `coverage` mode | PHP lines in `.phpt` test files | PHP test coverage |

Both are needed for complete coverage analysis of a PHP C extension.

---

## Xdebug vs Blackfire for Extension Development

**Recommendation: Xdebug for local extension dev.**

| Criterion | Xdebug | Blackfire |
|-----------|--------|-----------|
| Local, no external service | ✅ | ❌ (SaaS) |
| Free | ✅ | ❌ (paid tiers) |
| Cachegrind output (KCacheGrind) | ✅ | ❌ |
| C-level profiling integration | ✅ (via system tools) | ❌ |
| Web UI | ❌ (use KCacheGrind) | ✅ |
| Production profiling | ❌ | ✅ |
| Overhead | High (2-5x) | Lower |

For extension development, Xdebug's local nature and cachegrind output are
decisive advantages. Use `XDEBUG_MODE=profile` with trigger-based activation
to avoid overhead on every run.

**Alternative**: [SPX (Simple Profiling eXtension)](https://github.com/NoiseByNorthwest/php-spx)
— built-in web UI, lower overhead than Xdebug, attempts to subtract profiling
overhead from measurements. Worth evaluating for 2026.

---

## Final Recommended `/etc/php/conf.d/` State

```text
/etc/php/conf.d/
├── apcu.ini                    ← ENABLE: extension=apcu.so + apc.enable_cli=1
├── bcmath.ini                  ← keep
├── dev-cli.ini                 ← CREATE: error_reporting, date.timezone, etc.
├── exif.ini                    ← keep
├── firebird.ini                ← keep (extension=firebird.so)
├── gd.ini                      ← keep
├── iconv.ini                   ← keep
├── interbase.ini.disabled      ← keep disabled (stale legacy)
├── intl.ini                    ← keep
├── memory.ini                  ← keep (memory_limit = -1)
├── opcache-cli.ini             ← CREATE: opcache.enable_cli=1
├── pdo_mysql.ini.disabled      ← ✅ DONE
├── pdo_pgsql.ini.disabled      ← ✅ DONE
├── pdo_sqlite.ini              ← keep
├── phpunit.ini                 ← keep
├── soap.ini                    ← keep
├── sodium.ini                  ← keep
├── xdebug.ini                  ← UPDATE: add client_host, client_port, idekey
├── xdebug-profile.ini.disabled ← CREATE: profiling config (disabled by default)
└── xsl.ini                     ← keep
```

---

## Next Steps

- [ ] Update `/etc/php/conf.d/xdebug.ini` with full config (client_host, port, idekey)
- [ ] Enable APCu: uncomment `extension=apcu.so` + add `apc.enable_cli=1`
- [ ] Create `/etc/php/conf.d/opcache-cli.ini`
- [ ] Create `/etc/php/conf.d/xdebug-profile.ini.disabled`
- [ ] Create `/etc/php/conf.d/dev-cli.ini` (error_reporting, date.timezone)
- [ ] Verify: `php -m | grep -v pdo_mysql | grep -v pdo_pgsql`
- [ ] Verify OPcache CLI: `php -r "var_dump(opcache_get_status()['opcache_enabled']);"`
- [ ] Investigate `pdo_firebird` availability in Arch PHP package

## References

- [Xdebug 3 Modes](https://xdebug.org/docs/all_settings#mode)
- [PHP 8.5 OPcache mandatory](https://tideways.com/profiler/blog/whats-new-in-php-8-5-in-terms-of-performance-debugging-and-operations)
- [PHP 8.5 max_memory_limit](https://php.watch/versions/8.5/max_memory_limit)
- [Arch Linux PHP Wiki](https://wiki.archlinux.org/title/PHP)
- [SPX Profiler](https://github.com/NoiseByNorthwest/php-spx)
- [gcov/lcov coverage](https://dev.to/naveenkhasyap/generating-code-coverage-report-using-gnu-gcov-lcov-59p)
- [Xdebug overhead warning](https://www.npopov.com/2012/01/19/Careful-XDebug-can-skew-your-performance-numbers.html)
