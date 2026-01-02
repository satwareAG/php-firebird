# Implementation Plan: Fix SIGSEGV during MSHUTDOWN (Issues #50, #51)

[Overview]
Fix SIGSEGV (exit code 139) occurring during PHP shutdown when persistent connections are cleaned up.

The root cause is `_php_fbird_close_plink()` accessing `EG(regular_list)` and `EG(persistent_list)` during MSHUTDOWN when these executor globals may already be destroyed or in an inconsistent state. Persistent resource destructors are called during or after MSHUTDOWN, making EG() access unsafe.

The fix adds an `in_mshutdown` flag to module globals that is set at the start of MSHUTDOWN, preventing EG() access during final cleanup.

**PHP Lifecycle Context:**
1. RSHUTDOWN - Request shutdown (per-request cleanup)
2. Resource destructors for regular resources
3. MSHUTDOWN - Module shutdown (one-time cleanup)
4. Persistent resource destructors (called during or after MSHUTDOWN) ← SIGSEGV here

[Types]
No new types required. Single boolean flag added to existing module globals structure.

**Modified struct: `zend_fbird_globals`** (in php_fbird_includes.h)
- Add field: `zend_bool in_mshutdown;` - Flag indicating module shutdown in progress (0=false, 1=true)

[Files]
Two files require modification to implement the fix.

**Modified files:**
1. `php_fbird_includes.h` - Add `in_mshutdown` field to module globals structure
2. `firebird.c` - Set flag in MSHUTDOWN, check flag in persistent link destructor, initialize in GINIT

**No new files to create.**

[Functions]
Three existing functions require modification.

**Modified functions:**

1. `PHP_MSHUTDOWN_FUNCTION(fbird)` in `firebird.c`
   - Current location: Line ~1260
   - Change: Add `IBG(in_mshutdown) = 1;` as **first statement** before any other code
   - Rationale: Must be set before persistent resource destructors are called

2. `_php_fbird_close_plink()` in `firebird.c`
   - Current location: Line 983
   - Change: Wrap `zend_hash_str_del()` calls (lines 1010-1011) with `if (!IBG(in_mshutdown))` check
   - Rationale: Skip EG() access during MSHUTDOWN to prevent SIGSEGV

3. `PHP_GINIT_FUNCTION(fbird)` in `firebird.c`
   - Current location: Find with `grep -n "PHP_GINIT_FUNCTION"`
   - Change: Add `fbird_globals->in_mshutdown = 0;` initialization
   - Rationale: Ensure clean state on module load

[Classes]
No class modifications required. This is a C extension with no OOP structures affected.

[Dependencies]
No new dependencies required. Uses existing Zend Engine types (`zend_bool`).

[Testing]
Verification through existing test suite and doctrine-firebird-driver integration.

**Testing approach:**
1. Run full php-firebird test matrix: `./scripts/test_matrix.sh`
2. Verify exit code 0 (not 139) on all PHP versions
3. Run doctrine-firebird-driver PHPUnit suite which uses persistent connections
4. Monitor for any memory leaks with Valgrind: `./scripts/run-valgrind.sh`

**Expected outcome:**
- All php81-dev through php85-fb5-dev containers pass
- No SIGSEGV (exit 139) during PHP shutdown
- Clean process termination after test completion

[Implementation Order]
Sequential implementation to minimize risk and ensure proper testing at each step.

1. **Add `in_mshutdown` field to module globals** (php_fbird_includes.h)
   - Single line addition after `exception_mode` field
   - No functional change until flag is used

2. **Initialize flag in PHP_GINIT_FUNCTION** (firebird.c)
   - Set `fbird_globals->in_mshutdown = 0;`
   - Ensures clean state

3. **Set flag in PHP_MSHUTDOWN_FUNCTION** (firebird.c)
   - Add `IBG(in_mshutdown) = 1;` as first statement
   - Must precede UNREGISTER_INI_ENTRIES

4. **Guard EG() access in _php_fbird_close_plink** (firebird.c)
   - Wrap zend_hash_str_del calls with `if (!IBG(in_mshutdown))`
   - Add FBDEBUG log for skipped cleanup

5. **Test with test_matrix.sh**
   - Verify all containers pass
   - Check for exit code 139

6. **Commit and push changes**
   - Conventional commit: `fix(shutdown): prevent EG() access during MSHUTDOWN`
   - Reference issues #50, #51

---

## Code Changes Detail

### php_fbird_includes.h (line ~92)
```c
// After existing line:
int exception_mode;             /* Exception mode: 0=SILENT (default), 1=THROW */
// Add:
zend_bool in_mshutdown;         /* Flag: true during MSHUTDOWN to prevent EG() access */
```

### firebird.c - PHP_GINIT_FUNCTION
```c
// Add to initialization section:
fbird_globals->in_mshutdown = 0;
```

### firebird.c - PHP_MSHUTDOWN_FUNCTION (line ~1260)
```c
PHP_MSHUTDOWN_FUNCTION(fbird)
{
    /* Set flag FIRST to prevent EG() access in persistent resource destructors (Issue #50, #51) */
    IBG(in_mshutdown) = 1;
    
#ifndef PHP_WIN32
    // ... existing code ...
```

### firebird.c - _php_fbird_close_plink (line ~1008)
```c
/* Remove cache entries from both regular and persistent lists (Issue #35).
 * Persistent connections are cached in EG(persistent_list) with hash key.
 * Skip during MSHUTDOWN when EG() globals may be destroyed (Issue #50, #51). */
if (!IBG(in_mshutdown) && 
    (link->hash_key[0] != '\0' || memcmp(link->hash_key, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16) != 0)) {
    zend_hash_str_del(&EG(regular_list), link->hash_key, sizeof(link->hash_key) - 1);
    zend_hash_str_del(&EG(persistent_list), link->hash_key, sizeof(link->hash_key) - 1);
    FBDEBUG("Removed cache entries for persistent link");
} else if (IBG(in_mshutdown)) {
    FBDEBUG("Skipping EG() access during MSHUTDOWN (Issue #50, #51)");
}