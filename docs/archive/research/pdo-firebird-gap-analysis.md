# PDO Firebird Gap Analysis (Updated v9.0.0)

**Project**: satwareAG/php-firebird v9.0.0
**Compared to**: php-src PDO Firebird driver (ext/pdo_firebird)
**Date**: 2026-03-24

## A. Executive Summary

As of version 9.0.0, the `pdo_fbird` driver has **closed all major functional gaps** identified in early 2026. It is now a strict superset of the bundled `pdo_firebird` driver, offering native support for modern Firebird features (FB 4.0/5.0) and advanced integration (Service API, Events, Array fields) while using the modern C++ OO API.

## B. PDO Driver Feature Comparison Matrix

| Slot | This Project (v9.0.0) | php-src | Notes |
|------|-------------|---------|-------|
| `quoter` | **✅ implemented** | impl | Doubles quotes, wraps in single quotes |
| `begin` | **✅ isolation levels** | impl | Full support for isolation levels |
| `set_attribute` | **✅ full** | full | Added: FETCH_TABLE_NAMES, date/time format, isolation, WRITABLE_TRANSACTION |
| `last_id` | **✅ implemented** | NULL | This project: supports `lastInsertId($sequence)` |
| `check_liveness` | **✅ implemented** | impl | Uses real `fbc_ping()` roundtrip |
| `in_manually_transaction` | **✅ implemented** | impl | Tracks manual txn state |

## C. PDO Statement Feature Comparison Matrix

| Feature | This Project (v9.0.0) | php-src | Notes |
|---------|-------------|---------|-------|
| `describe_col` | **✅ table names** | impl | Prepends table name if `FETCH_TABLE_NAMES` is set |
| `set_attr` | **✅ full** | impl | Added: `CURSOR_SCROLL` |
| `next_rowset` | **✅ stubbed** | impl | FB has no multi-rowset; returns 0 |
| Named params (`:name`) | **✅ implemented** | impl | Full preprocessing support |
| Blob streaming | **✅ implemented** | impl | `PDO::PARAM_LOB` returns PHP stream |
| Scrollable cursors | **✅ implemented** | impl | ABS/REL/FIRST/LAST/PRIOR via OO API (FB 5.0+) |
| FB 4+ type coercion | **✅ native** | impl | Native support for INT128, DECFLOAT |
| Nullable params | **✅ implemented** | impl | Allows NULL binding to NOT NULL columns |

## D. Strategic Positioning

`pdo_fbird` is now the recommended driver for modern Firebird development.

| Dimension | php-src Pdo\Firebird | This Project (pdo_fbird) |
|-----------|---------------------|-------------------------|
| **Client API** | Legacy `isc_*` macros | Modern OO C++ wrappers (`fbc_*`) |
| **FB 4+ features** | Coercion only | Native INT128/DECFLOAT, `SET BIND` |
| **FB 5+ features** | No | Scrollable cursors over network |
| **Service API** | No | Full support via attributes |
| **Event Polling** | No | Full support via attributes |
| **Array Fields** | No | Full Read/Write support |
| **lastInsertId()** | No | Supported via sequence name |

## E. Conclusion

The gap analysis is considered **RESOLVED**. Future work will focus on internal modernization (v10.0.0) and performance optimizations.
