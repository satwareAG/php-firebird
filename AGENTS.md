# AGENTS.md

## About php-firebird

**php-firebird** is a modernized PHP extension providing native connectivity to Firebird databases (3.0, 4.0, 5.0+). It is a high-performance replacement for the legacy `ext/interbase`, written in C/C++17 using the modern Firebird OO API.

The project follows **Spec-Driven Development (SDD)** and targets high-quality API standards for modern PHP (8.2+).

---

## API Architecture

The extension provides three distinct layers:

| Layer | Type | Namespace / Prefix | Purpose |
|-------|------|-------------------|---------|
| **Layer 1** | Procedural | `fbird_*` | Low-level, high-performance C API functions |
| **Layer 2** | OOP | `Firebird\*` | Native PHP classes (`Connection`, `Transaction`, etc.) |
| **Layer 3** | PDO | `pdo_fbird` | Separate extension providing `fbird:` DSN prefix |

---

## Integration Standards

### SDD/TDD Workflow
1. **Spec**: Update `specs/` before changing code.
2. **Test**: Write `.phpt` (native) or `phpunit` (OOP) tests first.
3. **Pillars**: Memory safety (ASAN), type safety (C++17/PHP 8.2+).

### IPADP Conformance
This project targets **L3 conformance** (Discovery).
- **Metadata**: `specs/metadata.json`
- **Linkages**: Relates to `doctrine-firebird-driver` (downstream).

---

## Technical Context for Agents

- **Build System**: `phpize` + `configure`.
- **Primary Source**: `firebird.c` (Zend API entry point).
- **Modern Logic**: `firebird_utils.cpp` / `fbird_classes.c`.
- **PDO Driver**: `pdo_fbird/`.

Agents MUST maintain consistency between the three layers when introducing new features.
