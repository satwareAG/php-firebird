# Implementation Plan: Split firebird.c (Issue #57)

[Overview]
Split the 3668-line `firebird.c` into five focused compilation units (each ≤800 lines) while keeping the trimmed `firebird.c` ≤1250 lines, satisfying the v7.0.0 milestone target of ≤1500 lines.

`firebird.c` is a monolithic 3668-line file containing: error-handling PHP functions, connection management internals, transaction management, batch operations (FB 4.0+), INI configuration, module lifecycle hooks (MINIT/MSHUTDOWN/MINFO), the `zend_function_entry` table, and utility functions (`gen_id`, limbo transactions). The file has grown beyond maintainability. The existing codebase already follows a per-concern split pattern (`fbird_service.c`, `fbird_blobs.c`, `fbird_events.c`, etc.), so this refactoring aligns with the existing architecture.

**Key constraint**: This is a *source-level* refactoring only — no public API changes, no behaviour changes, zero test regressions. Every function that moves must be callable by the same callers after the move, using the same `#include` chain.

**Dependency graph** (existing headers already handle most cross-file symbols):
- `php_fbird_includes.h` already declares: `extern int le_link, le_plink, le_trans, le_query, le_batch`, `void _php_fbird_error(void)`, `void _php_fbird_module_error(...)`, `void _php_fbird_get_link_trans(...)`, all typedef structs
- `php_firebird.h` already declares all `PHP_FUNCTION(...)` prototypes

New headers are needed **only** for the 4 static resource destructor callbacks passed to `zend_register_list_destructors_ex` in `PHP_MINIT_FUNCTION` (which stays in `firebird.c`).

---

[Types]
No new types; all structs (`fbird_db_link`, `fbird_transaction`, `fbird_query`, `fbird_batch`, etc.) remain in `php_fbird_includes.h`.

All type definitions, enums (`php_fbird_option`), and module globals (`ZEND_BEGIN_MODULE_GLOBALS(fbird)`) remain in `php_fbird_includes.h` unchanged.

---

[Files]
Five source files are created/modified; three new headers are required.

### New `.c` files (extract from `firebird.c`)

| New file | firebird.c source lines | Est. lines | Content |
|----------|------------------------|------------|---------|
| `fbird_error.c` | 588–795 | ~210 | `fbird_errmsg`, `fbird_errcode`, `fbird_sqlstate`, `fbird_escape_string`, `fbird_set_exception_mode`, `fbird_get_exception_mode`, `fbird_get_client_version/major/minor`, `_php_fbird_error`, `_php_fbird_module_error` |
| `fbird_connection.c` | 797–1005, 1451–1966 | ~730 | `_php_fbird_get_link_trans`, `_php_fbird_commit_link`, `php_fbird_commit_link_rsrc`, `_php_fbird_close_link`, `_php_fbird_close_plink`, `_php_fbird_connect`, `_php_fbird_validate_link_resource`, `_php_fbird_adopt_new_default_link`, `_php_fbird_close_resource`, `fbird_connect`, `fbird_pconnect`, `fbird_close`, `fbird_drop_db` |
| `fbird_transaction.c` | 1006–1051, 1967–2793 | ~875 | `_php_fbird_free_trans`, `fbird_trans_start`, `fbird_savepoint`, `fbird_rollback_savepoint`, `fbird_release_savepoint`, `fbird_trans_info`, `fbird_connection_info`, `fbird_trans`, `fbird_commit`, `fbird_rollback`, `fbird_commit_ret`, `fbird_rollback_ret` |
| `fbird_batch.c` | 1052–1099, 3053–3668 | ~665 | `_php_fbird_free_batch`, `fbird_batch_create`, `fbird_batch_add`, `fbird_batch_execute`, `fbird_batch_cancel`, `fbird_batch_add_blob`, `fbird_batch_register_blob` |

### New `.h` header files

| New header | Declares |
|------------|---------|
| `php_fbird_connection.h` | `_php_fbird_commit_link(fbird_db_link*)`, `php_fbird_commit_link_rsrc(zend_resource*)`, `_php_fbird_close_link(zend_resource*)`, `_php_fbird_close_plink(zend_resource*)` |
| `php_fbird_transaction.h` | `_php_fbird_free_trans(zend_resource*)` |
| `php_fbird_batch.h` | (conditional `#if FB_API_VER >= 40`) `_php_fbird_free_batch(zend_resource*)` |

### Modified files

| File | Change |
|------|--------|
| `firebird.c` | Remove lines 588–1005, 1451–2793, 3053–3668; add `#include` for 3 new headers; keep: license+includes+arginfo+function_entry_table (~537 lines), INI/GINIT (1100–1258), MINIT/MSHUTDOWN/RSHUTDOWN/MINFO (1259–1449), gen_id+limbo (2795–3052) |
| `config.m4` | Append `fbird_error.c fbird_connection.c fbird_transaction.c fbird_batch.c` to `PHP_NEW_EXTENSION` source list (line 100) |

**Each new `.c` file includes at minimum:**
```c
#include "php_fbird_includes.h"      /* types, globals, resource IDs */
#include "php_firebird.h"             /* PHP_FUNCTION declarations */
#include "php_fbird_<module>.h"       /* own header if needed */
/* + any additional module-specific headers (e.g. fbird_datetime.h) */
```

`fbird_transaction.c` also needs `#include "php_fbird_connection.h"` (calls `_php_fbird_commit_link`).
`fbird_error.c` does NOT need a new header (symbols already in `php_fbird_includes.h`).

---

[Functions]
Functions are moved verbatim — no signature changes, no logic changes.

### Functions moving to `fbird_error.c`

| Function | Current location | Change |
|----------|-----------------|--------|
| `PHP_FUNCTION(fbird_errmsg)` | firebird.c:588 | Move — no signature change |
| `PHP_FUNCTION(fbird_get_client_version)` | firebird.c:601 | Move |
| `PHP_FUNCTION(fbird_get_client_major_version)` | firebird.c:606 | Move |
| `PHP_FUNCTION(fbird_get_client_minor_version)` | firebird.c:611 | Move |
| `PHP_FUNCTION(fbird_errcode)` | firebird.c:616 | Move |
| `PHP_FUNCTION(fbird_sqlstate)` | firebird.c:628 | Move |
| `PHP_FUNCTION(fbird_escape_string)` | firebird.c:654 | Move |
| `PHP_FUNCTION(fbird_set_exception_mode)` | firebird.c:696 | Move |
| `PHP_FUNCTION(fbird_get_exception_mode)` | firebird.c:714 | Move |
| `void _php_fbird_error(void)` | firebird.c:744 | Move; declaration stays in php_fbird_includes.h |
| `void _php_fbird_module_error(...)` | firebird.c:770 | Move; declaration stays in php_fbird_includes.h |

### Functions moving to `fbird_connection.c`

| Function | Current location | Change |
|----------|-----------------|--------|
| `void _php_fbird_get_link_trans(...)` | firebird.c:797 | Move; declaration stays in php_fbird_includes.h |
| `static void _php_fbird_commit_link(fbird_db_link*)` | firebird.c:822 | Move; change from `static` → non-static; declare in php_fbird_connection.h |
| `static void php_fbird_commit_link_rsrc(zend_resource*)` | firebird.c:882 | Move; change from `static` → non-static; declare in php_fbird_connection.h |
| `static void _php_fbird_close_link(zend_resource*)` | firebird.c:889 | Move; change from `static` → non-static; declare in php_fbird_connection.h |
| `static void _php_fbird_close_plink(zend_resource*)` | firebird.c:948 | Move; change from `static` → non-static; declare in php_fbird_connection.h |
| `static void _php_fbird_connect(...)` | firebird.c:1451 | Move; remains static within fbird_connection.c |
| `static int _php_fbird_validate_link_resource(...)` | firebird.c:1618 | Move; remains static |
| `static void _php_fbird_adopt_new_default_link(...)` | firebird.c:1649 | Move; remains static |
| `static void _php_fbird_close_resource(...)` | firebird.c:1662 | Move; remains static |
| `PHP_FUNCTION(fbird_connect)` | firebird.c:1609 | Move |
| `PHP_FUNCTION(fbird_pconnect)` | firebird.c:1614 | Move |
| `PHP_FUNCTION(fbird_close)` | firebird.c:1674 | Move |
| `PHP_FUNCTION(fbird_drop_db)` | firebird.c:1727 | Move |

### Functions moving to `fbird_transaction.c`

| Function | Current location | Change |
|----------|-----------------|--------|
| `static void _php_fbird_free_trans(zend_resource*)` | firebird.c:1006 | Move; change from `static` → non-static; declare in php_fbird_transaction.h |
| `PHP_FUNCTION(fbird_trans_start)` | firebird.c:1967 | Move |
| `PHP_FUNCTION(fbird_savepoint)` | firebird.c:2151 | Move |
| `PHP_FUNCTION(fbird_rollback_savepoint)` | firebird.c:2156 | Move |
| `PHP_FUNCTION(fbird_release_savepoint)` | firebird.c:2161 | Move |
| `PHP_FUNCTION(fbird_trans_info)` | firebird.c:2166 | Move |
| `PHP_FUNCTION(fbird_connection_info)` | firebird.c:2262 | Move |
| `PHP_FUNCTION(fbird_trans)` | firebird.c:2384 | Move |
| `PHP_FUNCTION(fbird_commit)` | firebird.c:2744 | Move |
| `PHP_FUNCTION(fbird_rollback)` | firebird.c:2749 | Move |
| `PHP_FUNCTION(fbird_commit_ret)` | firebird.c:2754 | Move |
| `PHP_FUNCTION(fbird_rollback_ret)` | firebird.c:2759 | Move |

### Functions moving to `fbird_batch.c`

| Function | Current location | Change |
|----------|-----------------|--------|
| `static void _php_fbird_free_batch(zend_resource*)` (FB_API_VER >= 40) | firebird.c:1052 | Move; change from `static` → non-static; declare in php_fbird_batch.h |
| `PHP_FUNCTION(fbird_batch_create)` | firebird.c:3053 | Move |
| `PHP_FUNCTION(fbird_batch_add)` | firebird.c:3132 | Move |
| `PHP_FUNCTION(fbird_batch_execute)` | firebird.c:3519 | Move |
| `PHP_FUNCTION(fbird_batch_cancel)` | firebird.c:3569 | Move |
| `PHP_FUNCTION(fbird_batch_add_blob)` | firebird.c:3597 | Move |
| `PHP_FUNCTION(fbird_batch_register_blob)` | firebird.c:3629 | Move |

### Functions staying in `firebird.c`

`PHP_MINIT_FUNCTION(fbird)`, `PHP_MSHUTDOWN_FUNCTION(fbird)`, `PHP_RSHUTDOWN_FUNCTION(fbird)`, `PHP_MINFO_FUNCTION(fbird)`, `static PHP_GINIT_FUNCTION(fbird)`, `static PHP_INI_DISP(php_fbird_password_displayer_cb)`, `static PHP_INI_DISP(php_fbird_trans_displayer)`, `PHP_FUNCTION(fbird_gen_id)`, `PHP_FUNCTION(fbird_get_limbo_transactions)`, `PHP_FUNCTION(fbird_reconnect_transaction)`, `_fbird_res_type_name`, all arginfo (`ZEND_BEGIN_ARG_INFO`) declarations, `static const zend_function_entry firebird_functions[]`.

---

[Classes]
No class changes; `Firebird\Exception` registration stays in `PHP_MINIT_FUNCTION` in `firebird.c`.

---

[Dependencies]
No new external library dependencies; no changes to Firebird client linkage.

Internal compilation dependencies change:
- `fbird_transaction.c` gains a compile-time dependency on `php_fbird_connection.h` (to call `_php_fbird_commit_link`)
- `firebird.c` gains `#include "php_fbird_connection.h"`, `#include "php_fbird_transaction.h"`, `#include "php_fbird_batch.h"` (for destructor callbacks in `PHP_MINIT_FUNCTION`)
- `config.m4` PHP_NEW_EXTENSION source list gains 4 new `.c` files

---

[Testing]
Full regression testing via the existing Docker-based `.phpt` test suite; no new test files needed for this refactoring.

```bash
# Full test run (no regressions expected)
docker compose run --rm php83-dev make test

# Build verification (catches linking errors)
docker compose run --rm php83-dev phpize && ./configure --with-firebird && make -j$(nproc)

# Coverage smoke-check (verifies gcov instrumentation still works)
docker compose run --rm php83-dev /ext/scripts/coverage.sh
```

Verification criteria (all must pass):
1. `make` exits 0 (no compiler errors or warnings)
2. All existing `.phpt` tests pass (zero new failures)
3. `fbird_errmsg()`, `fbird_connect()`, `fbird_trans()`, `fbird_batch_create()` callable from PHP (extension loads correctly)

---

[Implementation Order]
Implement in 7 atomic steps, each independently compilable and committable.

1. **Create `php_fbird_connection.h`** — declare 4 destructor callbacks that move out of firebird.c
2. **Create `php_fbird_transaction.h`** — declare `_php_fbird_free_trans`
3. **Create `php_fbird_batch.h`** — declare `_php_fbird_free_batch` (conditional on FB_API_VER >= 40)
4. **Create `fbird_error.c`** — extract lines 588–795 from firebird.c; add `#include "php_fbird_includes.h"` + `#include "php_firebird.h"` at top
5. **Create `fbird_connection.c`** — extract lines 797–1005 + 1451–1966; de-static 4 callbacks; add includes
6. **Create `fbird_transaction.c`** — extract lines 1006–1051 + 1967–2793; de-static `_php_fbird_free_trans`; add `#include "php_fbird_connection.h"`
7. **Create `fbird_batch.c`** — extract lines 1052–1099 + 3053–3668; de-static `_php_fbird_free_batch`
8. **Modify `firebird.c`** — delete extracted line ranges; add `#include` for 3 new headers; verify remaining ~1200 lines compile
9. **Modify `config.m4`** — add 4 new `.c` files to `PHP_NEW_EXTENSION` source list (line 100)
10. **Build + test** — `docker compose run --rm php83-dev make test` → zero failures
11. **Commit + PR** — branch `refactor/split-firebird-c`, target `satware-main`, closes #57
