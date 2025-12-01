# Phase 5: Deep Optimization Research - Making php-firebird a Benchmark Extension

**Status:** Research Complete | **Date:** 2025-12-01  
**Objective:** Transform php-firebird into a benchmark PHP SQL extension - easy to use, rock-stable, state of the art

## Executive Summary

Based on comprehensive research of PHP database extensions (PDO, mysqli, pgsql) and Firebird client optimization patterns, this document outlines the strategic path to make php-firebird a benchmark in PHP SQL support.

### Current State (Phase 1-4 Complete)
- ✅ 84/85 tests passing (98.8% success rate)
- ✅ Exception-based error handling (`ibase.enable_exceptions`)
- ✅ BLOB streaming with PHP stream wrappers
- ✅ Transaction API with savepoints, TPB configuration, transaction inspection
- ✅ PHP 8.1-8.5 compatibility with Firebird 2.5/3.0/4.0/5.0

### Vision: "The PDO of Firebird"
Make php-firebird as developer-friendly as PDO while leveraging Firebird's unique capabilities:
- Named parameters with `:param` syntax (like PDO)
- Fluent API for transaction configuration
- Type-safe INT128/DECFLOAT/BOOLEAN handling
- Memory-efficient streaming for large objects
- Comprehensive exception handling with context

---

## Part 1: Lessons from PHP Database Extensions

### 1.1 Memory Management Best Practices

**From PDO/mysqli/pgsql Research:**

| Pattern | Implementation for php-firebird |
|---------|--------------------------------|
| Explicit resource cleanup | Provide `fbird_free_*()` functions that immediately release server-side resources |
| Unbuffered result fetching | Add `IBASE_UNBUFFERED` flag to `fbird_query()` for large result sets |
| Statement caching | Client-side prepared statement cache to avoid re-prepare overhead |
| Connection lifecycle | Clear documentation on persistent vs per-request connections |

**Recommendation #1: Unbuffered Query Mode**
```php
// Current: All results buffered in memory
$result = fbird_query($db, "SELECT * FROM large_table");

// Proposed: Streaming/unbuffered mode for memory efficiency
$result = fbird_query($db, "SELECT * FROM large_table", FBIRD_UNBUFFERED);
while ($row = fbird_fetch_assoc($result)) {
    // Process row-by-row, minimal memory footprint
}
```

### 1.2 Connection Pooling Considerations

**Current Limitations:**
- PHP's persistent connections (`ibase_pconnect`) create per-process pools
- No true connection pooling like PgBouncer/ProxySQL

**Recommendation #2: Connection Parameter Documentation**
Document optimal connection patterns for different deployment scenarios:
- Apache mod_php: Persistent connections appropriate
- PHP-FPM: Careful with persistent connections (long pool lifetimes)
- CLI/Daemon scripts: Explicit connection management required

### 1.3 Named Parameter Syntax (Critical for Developer Experience)

**PDO's Named Parameters are a key usability feature:**
```php
// PDO style - intuitive and readable
$stmt = $pdo->prepare("INSERT INTO users (name, email) VALUES (:name, :email)");
$stmt->execute([':name' => 'John', ':email' => 'john@example.com']);
```

**Recommendation #3: Named Parameter Support**
```php
// Proposed for php-firebird (Phase 5)
$stmt = fbird_prepare($db, "INSERT INTO users (name, email) VALUES (:name, :email)");
fbird_execute($stmt, [':name' => 'John', ':email' => 'john@example.com']);

// Implementation: Parse :name syntax, convert to ? placeholders internally
// Maintain parameter map for binding
```

### 1.4 Exception Handling Patterns (Implemented)

**Current Implementation (Phase 1):**
```php
// Enable exceptions (opt-in for backward compatibility)
ini_set('ibase.enable_exceptions', 1);

try {
    $result = fbird_query($db, "SELECT * FROM invalid_table");
} catch (Firebird\Exception $e) {
    // Structured error handling
    error_log("Firebird error: " . $e->getMessage());
}
```

**Enhancement: Rich Exception Messages**
```php
// Proposed enhancement: Exception with SQL context
try {
    fbird_execute($stmt, $params);
} catch (Firebird\Exception $e) {
    // $e->getSql() - The SQL that failed
    // $e->getParams() - Sanitized parameters
    // $e->getGDSCode() - Firebird-specific error code
    // $e->getISCStatus() - Full ISC status vector
}
```

---

## Part 2: Firebird-Specific Optimizations

### 2.1 Firebird 4.0+ Type System

**New Data Types Requiring PHP Support:**

| Firebird Type | PHP Representation | Implementation |
|---------------|-------------------|----------------|
| `INT128` | `string` (arbitrary precision) | Use GMP/BCMath for arithmetic |
| `DECFLOAT(16/34)` | `string` (preserve precision) | Scientific notation support |
| `BOOLEAN` | Native `bool` | Direct mapping |
| `TIME WITH TIME ZONE` | `DateTimeImmutable` | Preserve timezone info |
| `TIMESTAMP WITH TIME ZONE` | `DateTimeImmutable` | Full precision |

**Recommendation #4: Type-Safe Fetch Options**
```php
// Proposed: Type-aware fetching
$result = fbird_query($db, "SELECT id, balance, is_active, created_at FROM accounts");

// Option A: Automatic type conversion
$row = fbird_fetch_assoc($result, FBIRD_TYPED);
// $row['id'] => int
// $row['balance'] => string (DECFLOAT preserved)
// $row['is_active'] => bool
// $row['created_at'] => DateTimeImmutable

// Option B: Explicit type map
$types = [
    'id' => FBIRD_INT,
    'balance' => FBIRD_STRING,  // Keep as string for precision
    'is_active' => FBIRD_BOOL,
    'created_at' => FBIRD_DATETIME
];
$row = fbird_fetch_typed($result, $types);
```

### 2.2 Transaction Parameter Buffer (TPB) Optimization

**Current Implementation (Phase 4):**
```php
$trans = fbird_trans_start($db, [
    'access_mode' => IBASE_READ,
    'isolation_level' => IBASE_CONCURRENCY,
    'lock_resolution' => IBASE_WAIT,
    'lock_timeout' => 10
]);
```

**Enhancement: Predefined Transaction Profiles**
```php
// Proposed: Common transaction profiles
$trans = fbird_trans_profile($db, FBIRD_TRANS_READ_COMMITTED_READ_ONLY);
$trans = fbird_trans_profile($db, FBIRD_TRANS_SERIALIZABLE_WRITE);
$trans = fbird_trans_profile($db, FBIRD_TRANS_SNAPSHOT_READ);

// Constants map to optimal TPB configurations:
// FBIRD_TRANS_READ_COMMITTED_READ_ONLY => [
//     'access_mode' => IBASE_READ,
//     'isolation_level' => IBASE_COMMITTED,
//     'lock_resolution' => IBASE_NOWAIT,
//     'record_version' => true
// ]
```

### 2.3 BLOB Streaming Enhancements

**Current Implementation (Phase 3):**
```php
$stream = fbird_blob_open_stream($db, $blob_id);
while (!feof($stream)) {
    echo fread($stream, 8192);
}
fclose($stream);
```

**Enhancement: Stream Context Options**
```php
// Proposed: Configurable stream behavior
$options = stream_context_create([
    'fbird_blob' => [
        'chunk_size' => 65536,      // 64KB chunks (default 8KB)
        'read_ahead' => true,       // Prefetch next chunk
        'compression' => 'zstd'     // Auto-decompress compressed BLOBs
    ]
]);

$stream = fbird_blob_open_stream($db, $blob_id, 'r', $options);
```

### 2.4 Wire Protocol Optimization

**Connection-Level Optimizations:**
```php
// Proposed: Connection options for Firebird 3.0+
$db = fbird_connect($database, $user, $password, [
    'wire_compression' => true,        // Enable wire compression
    'wire_crypt' => 'required',        // Encryption: disabled/enabled/required
    'auth_plugin' => 'Srp256',         // Secure Remote Password
    'process_name' => 'MyApp',         // Identify in fb_lock_print
    'client_library' => '/path/to/fbclient.so'  // Custom client lib
]);
```

---

## Part 3: Developer Experience Improvements

### 3.1 Fluent API Design

**Proposed: Query Builder Pattern**
```php
// Chainable query construction (optional wrapper)
$result = fbird($db)
    ->select('users', ['id', 'name', 'email'])
    ->where('status', '=', 'active')
    ->orderBy('name')
    ->limit(100)
    ->execute();

// Compiles to: SELECT id, name, email FROM users WHERE status = ? ORDER BY name ROWS 1 TO 100
```

### 3.2 Error Message Enhancement

**Current Error Messages:**
```
ibase_query(): Dynamic SQL Error
SQL error code = -204
Table unknown
USERS
At line 1, column 15
```

**Proposed Enhanced Error Format:**
```php
// Firebird\QueryException extends Firebird\Exception
throw new Firebird\QueryException(
    message: "Table 'USERS' does not exist",
    sql: "SELECT * FROM USERS WHERE id = ?",
    params: [123],
    gdsCode: 335544569,
    iscStatus: [-204, ...],
    hint: "Did you mean 'USER' or 'ACCOUNTS'? Use fbird_tables() to list available tables."
);
```

### 3.3 Metadata Inspection API

**Enhancement: Rich Schema Information**
```php
// Proposed: Comprehensive metadata functions
$tables = fbird_tables($db);           // List all tables
$columns = fbird_columns($db, 'USERS'); // Column info with types, nullability, defaults
$indices = fbird_indices($db, 'USERS'); // Index information
$triggers = fbird_triggers($db, 'USERS'); // Trigger definitions
$generators = fbird_generators($db);    // Sequence/generator list
```

---

## Part 4: Performance Benchmarking Methodology

### 4.1 Benchmark Suite Design

**Test Categories:**

| Category | Tests | Purpose |
|----------|-------|---------|
| **Connection** | Connect/disconnect cycles, persistent vs fresh | Connection overhead |
| **Simple Queries** | SELECT 1, single row fetch | Minimal query path |
| **Prepared Statements** | Prepare once, execute 10K times | Statement reuse efficiency |
| **Bulk Insert** | 10K/100K row inserts | Write throughput |
| **Bulk Fetch** | Fetch 100K rows | Read throughput, memory |
| **BLOB Operations** | 1MB/10MB/100MB BLOBs | Streaming efficiency |
| **Transactions** | Commit/rollback cycles | Transaction overhead |
| **Concurrent** | Parallel connections | Scalability |

### 4.2 Comparison Targets

**Benchmark Against:**
1. PDO_Firebird (if available in environment)
2. PHP mysqli (normalized for MySQL, pattern comparison)
3. PHP pgsql (normalized for PostgreSQL, pattern comparison)
4. Jaybird JDBC (Java, for feature parity reference)
5. FirebirdSql.Data.FirebirdClient (.NET, for feature parity reference)

### 4.3 Benchmark Implementation

```php
// docs/benchmarks/run_all.php already exists - extend it

class FirebirdBenchmark {
    private array $results = [];
    
    public function benchmarkPreparedStatementReuse(int $iterations = 10000): array {
        $db = fbird_connect(...);
        $stmt = fbird_prepare($db, "SELECT * FROM test WHERE id = ?");
        
        $start = hrtime(true);
        for ($i = 0; $i < $iterations; $i++) {
            fbird_execute($stmt, [$i]);
            fbird_free_result(fbird_execute($stmt, [$i]));
        }
        $elapsed = (hrtime(true) - $start) / 1e6; // milliseconds
        
        return [
            'operation' => 'prepared_statement_reuse',
            'iterations' => $iterations,
            'total_ms' => $elapsed,
            'ops_per_sec' => $iterations / ($elapsed / 1000)
        ];
    }
    
    // Additional benchmark methods...
}
```

---

## Part 5: Implementation Roadmap

### Phase 5.1: Type System Enhancement (Priority: HIGH)
- [ ] INT128 type mapping to string
- [ ] DECFLOAT(16/34) precision preservation
- [ ] BOOLEAN native mapping
- [ ] TIME/TIMESTAMP WITH TIME ZONE support
- [ ] Add `FBIRD_TYPED` fetch mode constant

### Phase 5.2: Named Parameters (Priority: HIGH)
- [ ] Parse `:name` syntax in SQL strings
- [ ] Build parameter name → position map
- [ ] Support both positional and named in same statement
- [ ] Update documentation with examples

### Phase 5.3: Connection Optimization (Priority: MEDIUM)
- [ ] Wire compression option (`WireCompression` DPB)
- [ ] Wire encryption option (`WireCrypt` DPB)
- [ ] Authentication plugin selection
- [ ] Process identification
- [ ] Connection timeout configuration

### Phase 5.4: Developer Experience (Priority: MEDIUM)
- [ ] Enhanced exception messages with SQL context
- [ ] Metadata inspection functions
- [ ] Transaction profiles (predefined configurations)
- [ ] Unbuffered query mode

### Phase 5.5: Performance & Stability (Priority: HIGH)
- [ ] Fix `blob_stream_chunked_write.phpt` test
- [ ] Comprehensive benchmark suite
- [ ] Memory leak analysis with Valgrind
- [ ] Stress testing with concurrent connections

---

## Appendix A: Competitive Analysis

### A.1 PDO Features php-firebird Should Match

| PDO Feature | php-firebird Status | Priority |
|------------|---------------------|----------|
| Named parameters (`:name`) | Not implemented | HIGH |
| `PDO::ERRMODE_EXCEPTION` | ✅ `ibase.enable_exceptions` | Done |
| `fetchColumn()` | Not implemented | MEDIUM |
| `fetchAll(PDO::FETCH_GROUP)` | Not implemented | LOW |
| `PDO::ATTR_STRINGIFY_FETCHES` | Partial | MEDIUM |
| Transaction begin/commit/rollback | ✅ Implemented | Done |
| Prepared statement reuse | ✅ Implemented | Done |

### A.2 Firebird-Unique Features (Differentiators)

| Feature | php-firebird Status | Competitive Advantage |
|---------|---------------------|----------------------|
| BLOB streaming | ✅ Implemented | Unique in PHP ecosystem |
| Transaction savepoints | ✅ Implemented | Better than mysqli |
| TPB configuration | ✅ Implemented | Full Firebird power |
| Exception-based errors | ✅ Implemented | Modern PHP patterns |
| Service API | ✅ Implemented | Database administration |
| Event notifications | ✅ Implemented | Real-time patterns |

---

## Appendix B: References

1. PHP PDO Documentation: https://www.php.net/manual/en/book.pdo.php
2. mysqli Documentation: https://www.php.net/manual/en/book.mysqli.php
3. Firebird Documentation: https://firebirdsql.org/file/documentation/
4. Jaybird JDBC Driver: https://github.com/FirebirdSQL/jaybird
5. FirebirdSql .NET Provider: https://github.com/FirebirdSQL/NETProvider
6. Firebird Wire Protocol: https://firebirdsql.org/file/documentation/papers_presentations/

---

## Conclusion

The php-firebird extension has achieved a solid foundation with Phases 1-4 complete. To become a benchmark in PHP SQL support, focus on:

1. **Developer Experience**: Named parameters, enhanced errors, metadata API
2. **Type Safety**: Full Firebird 4.0+ type system support
3. **Performance**: Benchmark suite, memory optimization, unbuffered queries
4. **Documentation**: Comprehensive examples, migration guides, best practices

The unique combination of Firebird's enterprise features (events, generators, BLOB streaming, flexible transactions) with modern PHP patterns (exceptions, type safety, fluent API) positions php-firebird as a compelling choice for PHP applications requiring a robust, feature-rich database layer.
