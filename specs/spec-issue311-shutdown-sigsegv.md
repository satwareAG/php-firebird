# Spec: Issue #311 — SIGSEGV during module shutdown with persistent connections

## Problem

PHP crashes with SIGSEGV (exit code 139) during module shutdown when a
persistent connection (`fbird_pconnect`) was used during the request.

The `!FBG(in_mshutdown)` guard in `_php_fbird_close_plink` is ineffective
because `in_mshutdown` is set in `PHP_MSHUTDOWN_FUNCTION`, which runs
AFTER `zend_destroy_rsrc_list(&EG(persistent_list))` — the function that
calls `_php_fbird_close_plink` via `plist_entry_destructor`.

## Root Cause

PHP shutdown order (Zend/zend.c):
1. `shutdown_executor()` → `zend_shutdown_executor_values()` → sets `EG_FLAGS_IN_RESOURCE_SHUTDOWN`, destroys `EG(regular_list)`
2. `zend_destroy_rsrc_list(&EG(persistent_list))` → calls `plist_entry_destructor` → `_php_fbird_close_plink`
3. `zend_destroy_modules()` → `PHP_MSHUTDOWN_FUNCTION` → sets `in_mshutdown = 1` (TOO LATE)

At step 2, `in_mshutdown` is still 0, so the guard allows
`zend_hash_str_del(&EG(regular_list), ...)` to execute on freed memory → SIGSEGV.

## Fix

Replace `!FBG(in_mshutdown)` with `!(EG(flags) & EG_FLAGS_IN_RESOURCE_SHUTDOWN)`
at 3 sites in `fbird_connection.c`:

| # | Line | Function | Context |
|---|------|----------|---------|
| 1 | 261 | `_php_fbird_close_plink` | `zend_hash_str_del(&EG(regular_list))` + remove `zend_hash_str_del(&EG(persistent_list))` |
| 2 | 99 | `_php_fbird_commit_link` | Error reporting after commit |
| 3 | 111 | `_php_fbird_commit_link` | Error reporting after rollback |

`EG_FLAGS_IN_RESOURCE_SHUTDOWN` is set at the START of
`zend_shutdown_executor_values()`, BEFORE `EG(regular_list)` is destroyed,
and stays set through module shutdown. Available on PHP 8.2-8.5.

## Acceptance Criteria

- `fbird_pconnect` followed by clean exit does not SIGSEGV
- `run-tests.php` exit code is 0 (not 139)
- No regression in non-persistent connection shutdown
- All 12-container test matrix passes
