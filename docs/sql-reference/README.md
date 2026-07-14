# Firebird SQL Reference for php-firebird

Comprehensive reference for SQL programmers using the **php-firebird** driver
with Firebird 3.0, 4.0, and 5.0 databases.

## Contents

| Document | Purpose |
|----------|---------|
| [Feature Cheatsheet](cheatsheet/README.md) | Every FB 3/4/5 feature mapped to driver-layer availability, with SQL+PHP code pairs |
| [SQL Syntax Reference](sql-syntax.md) | Full SQL language reference organized by category |
| [Best Practices](best-practices.md) | Full lifecycle guide for production Firebird usage through PHP |
| [Feature Inventory](feature-inventory.md) | Master cross-reference table (115 features x 3 driver layers) |

## Three driver layers

php-firebird exposes Firebird through three distinct API surfaces:

| Layer | Namespace / Prefix | Use case |
|-------|-------------------|----------|
| **Procedural** | `fbird_*` functions | Low-level, high-performance, 1:1 mapping to Firebird C API |
| **OOP** | `Firebird\*` classes | Modern PHP 8.2+ object-oriented API (`Connection`, `Statement`, `Transaction`, etc.) |
| **PDO** | `PDO` with `fbird:` DSN | Standard PDO interface for framework integration |

## Status legend (used throughout)

| Code | Meaning |
|------|---------|
| **Y (SQL)** | Works via SQL passthrough. The driver sends SQL unchanged; no native binding needed. |
| **Y (native)** | Driver has explicit API support (function, method, class, or constant). |
| **P** | Partial. Works with limitations. An issue tracks full implementation. |
| **N** | Not yet supported. An issue tracks implementation. |
| **N/A** | Server-side or engine-internal. No driver surface needed. |

## Source pinning

All upstream documentation links are pinned to the last tag of each major version:

| Version | Tag | Root |
|---------|-----|------|
| Firebird 3.0 | `R3_0_7` | [tree](https://github.com/FirebirdSQL/firebird/tree/R3_0_7) |
| Firebird 4.0 | `v4.0.7` | [tree](https://github.com/FirebirdSQL/firebird/tree/v4.0.7) |
| Firebird 5.0 | `v5.0.4` | [tree](https://github.com/FirebirdSQL/firebird/tree/v5.0.4) |

Links remain stable even as the Firebird `master` branch evolves.

## Related documents

- [Driver Parity Matrix](../parity-matrix.md) - cross-driver capability comparison (mysqli, pgsql, sqlite3, oci8, interbase)
- [EXAMPLES.md](../EXAMPLES.md) - end-to-end PHP usage examples
- [OOP API](../oop-api.md) - `Firebird\*` class reference
- [PDO Driver](../pdo-driver.md) - `pdo_fbird` DSN and attribute reference
