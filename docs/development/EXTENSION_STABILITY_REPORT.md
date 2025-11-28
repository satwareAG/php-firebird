# PHP Firebird Extension Stability & Modernization Report

**Date:** 2025-11-28
**To:** PHP Firebird Extension Development Team
**From:** Doctrine Firebird Driver Team
**Subject:** Technical Analysis of Extension Stability Issues, Resource Handling, and PHP 8+ Compatibility

## 1. Executive Summary

During the development and stabilizing of the Doctrine Firebird Driver (targeting Firebird 3.0/4.0/5.0 on PHP 8.1+), we encountered several critical stability issues and non-standard behaviors in the `php-firebird` extension. These issues primarily revolve around **resource lifecycle management**, **transaction commit behavior**, and **PHP 8+ resource object compatibility**.

While we have implemented mitigations in the driver (e.g., strictly managing object destruction order to prevent Segmentation Faults), the underlying extension behaviors present significant risks for any PHP application using Firebird.

This report details our findings, reproduction cases, and recommendations for stabilizing the extension.

## 2. Critical Findings

### 2.1. Segmentation Faults (Exit 139) on Resource Cleanup
**Severity:** Critical
**Status:** Mitigated in Driver, Unresolved in Extension

We observed persistent Segmentation Faults during the destruction of `Statement` and `Result` objects, specifically when dealing with BLOBs. The crash occurs when:
1. A Result resource is freed (via `fbird_free_result` or GC).
2. The associated Statement resource is freed or the Transaction is committed explicitly.
3. If the destruction order is not strictly controlled (Result then Statement), or if a fetch operation is attempted on a cursor invalidated by `fbird_commit_ret`, the extension triggers a Segfault rather than a catchable error.

**Analysis:**
The extension likely attempts to access memory structures (e.g., BLOB handles or transaction context) that have already been freed or invalidated, without sufficient null-checks. This is exacerbated by PHP's Garbage Collector which does not guarantee destruction order of cyclic or unrelated objects.

### 2.2. PHP 8+ Resource Handling: `var_export` returns `NULL`
**Severity:** High (Debugging/Logging Barrier)
**Status:** Confirmed

In PHP 8.1+, passing a valid Firebird resource (`interbase query` or `interbase result`) to `var_export()` results in `NULL`, whereas `var_dump()` correctly identifies it as `resource(id) of type ...`.

**Reproduction:**
```php
$res = fbird_execute($fmt);
var_dump($res); // Output: resource(13) of type (Firebird/InterBase query)
var_export($res); // Output: NULL
```

**Impact:**
This breaks logging libraries, debugging tools (including PHPUnit's variable exporter/comparators), and Doctrine's internal logging which rely on `var_export` to inspect variables.

**Recommendation:**
The extension must properly implement resource handlers compliant with PHP 8's resource object pattern or ensure standard resource structures are populated correctly.

### 2.3. `fbird_commit_ret` Invalidates Active Cursors
**Severity:** Medium (Logical behavior)
**Status:** Confirmed functionality

The function `fbird_commit_ret` (Commit Retaining) commits the transaction context but, crucially, **closes all open cursors** associated with that transaction context.

**Impact on Drivers:**
This makes implementing "Auto-Commit" behavior impossible for SELECT statements that return a cursor. If a driver calls `fbird_execute` (starting a transaction) and then immediately `fbird_commit_ret` (to simulate auto-commit), the returned result resource becomes unusable for fetching.

**Workaround in Driver:**
The driver must detect if a query yields a result set (is SELECT) and **skip/delay auto-commit** until the result set is fully fetched or explicitly closed by the user. This adds significant complexity to managing transaction state.

**Recommendation:**
If possible, expose `IBASE_COMMIT_RETAIN` behavior that preserves cursors (if supported by Firebird engine via `WITH HOLD` or similar), or clearly document this limitation as it deviates from how other drivers (MySQL/PG) handle auto-commit (where cursors often remain valid or are buffered).

## 3. Detailed Reproduction Steps

### Segfault Trigger Pattern
Through extensive testing, we identified that accessing fetched BLOB data after the underlying result/statement might be in an unstable state triggers crashes.

**Scenario:**
1. `fbird_prepare` -> `fbird_execute` (INSERT with BLOB).
2. `fbird_prepare` -> `fbird_execute` (SELECT BLOB).
3. `fbird_fetch_row` (Fetch BLOB).
4. `fbird_commit` (Invalidating cursors).
5. `fbird_free_result` vs `fbird_free_query` race condition during PHP shutdown.

### Resource Export Failure
**Script:** `debug_resource_export.php`
```php
<?php
$conn = fbird_connect($dsn, $user, $pass);
$trans = fbird_trans($conn);
$query = fbird_prepare($trans, "SELECT 1 FROM RDB\$DATABASE");
$res = fbird_execute($query);

echo "Dump: "; var_dump($res);     // valid
echo "Export: "; var_export($res); // NULL -> BUG
```

## 4. Recommendations for Extension Developers

1.  **Fix `var_export` Support:** Ensure resource types registered by the extension have proper handlers for export, or migrate to `Zend\Resource` objects (PHP 8.1+ standard) which handle this natively.
    
2.  **Harden Memory Safety:** Audit `fbird_free_result`, `fbird_free_query`, and `fbird_blob_*` functions. Ensure that freeing a parent resource (Transaction/Connection) safely marks child resources (Statements/Results/Blobs) as invalid, preventing use-after-free segfaults.
    
3.  **Transaction API Docs/Enhancement:** Clarify the expected behavior of `commit_ret` regarding cursors. If Firebird supports "Commit Retaining Cursors", expose that flag.

4.  **Modernize Tests:** Add regression tests specifically for PHP 8 object destruction order and resource exportability.

## 5. Conclusion

The Doctrine driver team has implemented workarounds for stability, but the `php-firebird` extension requires C-level maintenance to be fully reliable in modern PHP environments. We are ready to assist with testing and validation of any fixes.

---
**Prepared by:** Doctrine Firebird Driver Maintainers
