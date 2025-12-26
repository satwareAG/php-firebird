# Alternative Driver Analysis: mlazdans/firebird-php

**Analysis Date**: 2025-12-19
**Repository**: https://github.com/mlazdans/firebird-php
**Last Commit**: December 9, 2025 (3ff6f8a - "Cleanup")
**Status**: Active development (330 commits, 3 stars, 0 forks)

## Executive Summary

The `mlazdans/firebird-php` project is a **complete rewrite** of the PHP Firebird extension using a pure object-oriented API design. Unlike the procedural `fbird_*()` functions in `satwareAG/php-firebird`, it exposes a modern namespaced OO interface (`\FireBird\*`). Both projects use C/C++ for implementation (NOT PHP FFI).

### Key Differentiators

| Aspect | satwareAG/php-firebird | mlazdans/firebird-php |
|--------|------------------------|----------------------|
| **API Style** | Procedural (`fbird_*()`) | Pure OO (`\FireBird\*`) |
| **PHP Versions** | 8.1 - 8.5 | 8.4+ only |
| **Firebird Versions** | 2.5 - 5.0 | 5.0.x focused |
| **Implementation** | C with C++ OO wrappers | C++/C mixed |
| **Array Support** | ✅ Full | ❌ Explicitly not supported |
| **Test Count** | 109 tests | Unknown (merged some from php-firebird) |
| **Maturity** | Production-ready fork | Active development, API unstable |

---

## 1. Implementation Approach Analysis

### Language Breakdown (mlazdans/firebird-php)

```
C++:   44.3%
C:     30.0%
PHP:   23.2% (stubs, tests, examples)
Batch: 1.8%  (Windows build scripts)
```

### Architecture Comparison

**satwareAG/php-firebird**:
- Traditional PHP extension structure (inherited from `ext/interbase`)
- Procedural C functions exposed as `fbird_*()` and `ibase_*()` aliases
- Internal C++ RAII wrappers in `src/cpp/` for Firebird OO API
- Resource types with automatic cleanup (safe_to_destroy patterns)

**mlazdans/firebird-php**:
- Complete rewrite from scratch
- PHP classes map directly to C++ objects
- No procedural functions at all
- PHP 8 attribute-based argument info generation (`@generate-class-entries`)
- Namespace isolation (`\FireBird\*`)

---

## 2. API Design Comparison

### Connection & Database

**satwareAG/php-firebird (Procedural)**:
```php
$db = fbird_connect($host . ':' . $database, $user, $pass);
$trans = fbird_trans(FBIRD_READ, $db);
$result = fbird_query($trans, "SELECT * FROM TEST");
while ($row = fbird_fetch_assoc($result)) {
    print_r($row);
}
fbird_commit($trans);
fbird_close($db);
```

**mlazdans/firebird-php (Object-Oriented)**:
```php
$args = new \FireBird\Connect_Args;
$args->database = "localhost:/opt/db/test.fdb";
$args->user_name = "sysdba";
$args->password = "masterkey";

$db = \FireBird\Database::connect($args);
$t = $db->start_transaction();
$q = $t->query("SELECT * FROM TEST_TABLE");
while ($r = $q->fetch_object(\FireBird\FETCH_BLOB_TEXT)) {
    print_r($r);
}
$t->commit();
$db->disconnect();
```

### Transaction Building

**satwareAG/php-firebird**:
```php
$trans = fbird_trans(
    FBIRD_READ | FBIRD_COMMITTED | FBIRD_WAIT,
    $db
);
```

**mlazdans/firebird-php (Fluent Builder Pattern)**:
```php
$tb = new \FireBird\TBuilder;
$tb->read_only()
   ->isolation_read_committed_read_consistency()
   ->wait(10);  // 10 second lock timeout

$trans = $db->start_transaction($tb);
```

### BLOB Handling

**satwareAG/php-firebird**:
```php
$blob_id = fbird_blob_create($db);
fbird_blob_add($blob_id, $data);
fbird_blob_close($blob_id);
fbird_query($db, "INSERT INTO t (blob_col) VALUES (?)", $blob_id);
```

**mlazdans/firebird-php**:
```php
$blob = $trans->create_blob();
$blob->put($data);
$blob->close();
$id = $blob->id();  // Returns Blob_Id object
// Use $id in queries
```

---

## 3. Feature Coverage Comparison

### Function Implementation Status (mlazdans)

| Function | mlazdans Status | satwareAG Status |
|----------|----------------|------------------|
| `ibase_connect` | ✅ OO | ✅ Procedural |
| `ibase_pconnect` | ❓ Uncertain | ✅ Full |
| `ibase_query` | ✅ OO | ✅ Procedural |
| `ibase_prepare` | ✅ OO | ✅ Procedural |
| `ibase_execute` | ✅ OO | ✅ Procedural |
| `ibase_fetch_*` | ✅ OO | ✅ Procedural |
| `ibase_trans` | ✅ OO (TBuilder) | ✅ Procedural |
| `ibase_commit` | ✅ OO | ✅ Procedural |
| `ibase_rollback` | ✅ OO | ✅ Procedural |
| `ibase_blob_*` | ✅ OO (Blob class) | ✅ Procedural |
| `ibase_field_info` | ✅ OO | ✅ Procedural |
| `ibase_param_info` | ✅ OO | ✅ Procedural |
| `ibase_affected_rows` | ✅ OO (Statement property) | ✅ Procedural |
| `ibase_num_fields` | ✅ OO (Statement property) | ✅ Procedural |
| `ibase_service_attach` | ❌ C++ refactoring needed | ✅ Full |
| `ibase_backup` | ❌ C++ refactoring needed | ✅ Full |
| `ibase_restore` | ❌ C++ refactoring needed | ✅ Full |
| `ibase_add_user` | ❌ C++ refactoring needed | ✅ Full |
| `ibase_modify_user` | ❌ C++ refactoring needed | ✅ Full |
| `ibase_delete_user` | ❌ C++ refactoring needed | ✅ Full |
| `ibase_server_info` | ❌ C++ refactoring needed | ✅ Full |
| `ibase_set_event_handler` | ❌ In progress | ✅ Full |
| `ibase_wait_event` | ❌ In progress | ✅ Full |
| `ibase_free_event_handler` | ❌ C++ refactoring needed | ✅ Full |
| **ARRAY SUPPORT** | ❌ **Not planned** | ✅ Full |
| `ibase_gen_id` | 🚫 "Can be done in PHP" | ✅ Procedural |
| `ibase_blob_echo` | 🚫 "Can be done in PHP" | ✅ Procedural |
| `ibase_blob_import` | 🚫 "Can be done in PHP" | ✅ Procedural |
| `ibase_db_info` | 🚫 "Not worth it" | ✅ Procedural |

### Notable Differences

1. **Array Support**: mlazdans explicitly refuses to support Firebird arrays, citing Firebird documentation recommending against array usage. satwareAG has full array support.

2. **Service API**: mlazdans has partial Service implementation (users, server info), but backup/restore/shutdown not yet implemented.

3. **Events**: mlazdans has basic event structure but Event::consume() and handlers are temporarily disabled.

4. **Multi-Transaction (2PC)**: mlazdans has Multi_Transaction class structure but "TEMP DISABLED".

---

## 4. Class Structure (mlazdans/firebird-php)

```
\FireBird\
├── Database         # Main entry point (connect/create)
├── Transaction      # Transaction management
├── Statement        # Prepared statements / queries
├── Blob             # Binary large object handling
├── Blob_Id          # BLOB identifier value object
├── TBuilder         # Fluent transaction parameter builder
├── Service          # Service API (partial)
├── Connect_Args     # Connection arguments DTO
├── Create_Args      # Database creation arguments DTO
├── Service_Connect_Args  # Service connection arguments DTO
├── Var_Info         # Field/parameter information
├── Db_Info          # Database information
├── Server_Info      # Server version info
├── Server_Db_Info   # Server database stats
├── User_Info        # User management DTO
├── Fb_Error         # Error structure
└── Fb_Exception     # Exception with SQLSTATE
```

### Exception Handling

**mlazdans uses exception-first approach**:
```php
class Fb_Exception extends \Exception {
    public readonly string $sqlstate;     // SQLSTATE code
    public readonly array $errors;        // Multiple error chain
    public readonly string $file_ext;     // PHP file where error occurred
    public readonly int $line_ext;        // PHP line number
}
```

**satwareAG uses configurable approach**:
```php
// Enable via INI
ini_set('ibase.enable_exceptions', true);
// OR check return values + fbird_errmsg() / fbird_errcode()
```

---

## 5. Open Issues Analysis

### mlazdans/firebird-php Open Issues: **1**

| Issue | Title | Status | Description |
|-------|-------|--------|-------------|
| #1 | Rename project and add to Firebird website | Open | Naming/branding discussion |

**No bug reports or technical issues currently open.**

### satwareAG/php-firebird Status

All 13 tracked upstream issues are resolved (100% resolution rate). See `docs/UPSTREAM_FIXED_ISSUES_INSPECTION.md`.

---

## 6. Unique Features Worth Adopting

### From mlazdans/firebird-php to satwareAG

1. **TBuilder Fluent Interface**
   - Elegant transaction parameter configuration
   - Strongly typed isolation levels
   - Could be implemented as optional OO wrapper

2. **Blob_Id Value Object**
   - Type-safe BLOB identifier handling
   - `__toString()` for backward compatibility
   - Static `from_str()` factory method

3. **Db_Info Comprehensive Structure**
   - Rich database metadata (60+ properties)
   - Includes metrics: reads, writes, fetches, marks
   - ODS version, page size, buffer count
   - Connection flags, crypto state, replica mode

4. **Fb_Exception with SQLSTATE**
   - Standard SQLSTATE codes for error classification
   - Multiple error chaining in `$errors` array
   - Source location tracking (`file_ext`, `line_ext`)

5. **Fetch Flags as Constants**
   - `FETCH_BLOB_TEXT` - Return BLOBs as strings
   - `FETCH_UNIXTIME` - Return dates as timestamps
   - `FETCH_DATE_OBJ` - Return as DateTime objects

### Not Adopting

1. **Pure OO API** - Would break backward compatibility
2. **No Array Support** - Our users need array support
3. **PHP 8.4+ Only** - Too restrictive for production environments

---

## 7. Technical Insights

### Connection Arguments Class

mlazdans uses typed DTOs for connection:
```php
class Connect_Args {
    public string $database;      // host:/path/db.fdb format
    public string $user_name;
    public string $password;
    public string $role_name;
    public string $charset;
    public int $num_buffers;
    public int $timeout;
}
```

This pattern could improve PHP 8.0+ attribute support in satwareAG.

### Transaction ID Exposure

mlazdans exposes transaction ID directly:
```php
$trans->id;  // Virtual property, read-only
```

satwareAG could add `fbird_trans_id($trans)` for debugging/logging.

### Limbo Transaction Handling

mlazdans has explicit limbo transaction support:
```php
$ids = $db->get_limbo_transactions(100);  // Get up to 100 limbo trans IDs
$trans = $db->reconnect_transaction($id); // Reconnect to limbo
$trans->commit();  // Or rollback
```

This is better exposed than satwareAG's approach.

---

## 8. Performance Considerations

### BLOB Seek Support

mlazdans implements BLOB seeking (for streamed blobs):
```php
$blob->seek($offset, \FireBird\BLOB_SEEK_START);
$blob->seek($offset, \FireBird\BLOB_SEEK_CURRENT);
$blob->seek($offset, \FireBird\BLOB_SEEK_END);
```

satwareAG could add `fbird_blob_seek()` for large BLOB navigation.

### IBatch Exploration (TODO in mlazdans)

mlazdans notes potential for IBatch API:
- `IStatement::createBatch`
- `IAttachment::createBatch`
- Reduces network round trips for bulk operations

This is worth researching for satwareAG's future development.

---

## 9. Conclusion

### Strengths of mlazdans/firebird-php

1. Modern OO design with clear separation of concerns
2. Excellent type safety with PHP 8.4 features
3. Fluent builder pattern for transactions
4. Clean exception-based error handling
5. Comprehensive database info structure

### Weaknesses of mlazdans/firebird-php

1. No backward compatibility (procedural API)
2. Missing critical features (backup/restore, events, arrays)
3. PHP 8.4+ only limits adoption
4. Service API incomplete
5. API marked as unstable

### Recommendation for satwareAG

**Do not replace or merge** with mlazdans/firebird-php. Instead:

1. **Adopt patterns selectively**:
   - Add `fbird_trans_id()` function
   - Add BLOB seek support (`fbird_blob_seek()`)
   - Expose rich Db_Info structure
   - Consider TBuilder-style OO wrapper (optional)

2. **Maintain compatibility**:
   - Keep procedural API as primary
   - Support PHP 8.1+ for production environments
   - Continue array support

3. **Consider OO wrapper layer**:
   - Create optional `\FireBird\*` namespace wrapper around procedural functions
   - Users who want OO can use wrapper
   - Procedural users unaffected

4. **Research IBatch**:
   - Investigate Firebird's IBatch API for bulk operations
   - Could provide significant performance improvements

---

## Appendix: Source Files Comparison

### mlazdans/firebird-php File Structure

```
firebird-php/
├── blob.cpp                 # Blob class implementation
├── database.cpp             # Database class (connect/create)
├── transaction.cpp          # Transaction class
├── statement.cpp            # Statement class (queries)
├── service.cpp              # Service API
├── tbuilder.cpp             # Transaction builder
├── event.c                  # Event handling (partial)
├── multi_transaction.c      # 2PC support (disabled)
├── firebird_php.cpp         # PHP extension entry point
├── firebird_utils.cpp       # Utility functions
├── firebird.stub.php        # PHP stubs for IDE support
├── fbp/                     # Helper PHP classes
├── examples/                # Usage examples
├── tests/                   # Test suite
└── win32/                   # Windows build scripts
```

### satwareAG/php-firebird File Structure

```
php-firebird/
├── firebird.c               # Main extension entry
├── fbird_*.c                # Feature modules (blobs, events, etc.)
├── src/cpp/                 # C++ RAII wrappers
├── src/Firebird/            # PHP helper classes
├── tests/                   # Comprehensive test suite (109 tests)
├── docs/                    # Documentation
└── docker/                  # Development environment
```

---

**Document Version**: 1.0
**Author**: Jane Alesi (AI)
**Purpose**: Competitive analysis for satwareAG/php-firebird development strategy
