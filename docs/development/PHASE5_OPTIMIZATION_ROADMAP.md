# Phase 5: Optimization Roadmap - Making php-firebird a Benchmark in PHP SQL Support

**Document Version:** 1.0
**Created:** 2025-12-01
**Status:** Planning
**Target Release:** 7.0.0

## Executive Summary

This roadmap defines the path to make php-firebird a benchmark reference implementation for PHP database extensions. Based on deep research comparing state-of-the-art PHP database extensions (mysqli, PDO, pgsql), this document outlines architectural improvements, developer experience enhancements, and performance optimizations.

## Current State Analysis

### Completed Phases (6.2.0)

| Phase | Feature | Status |
|-------|---------|--------|
| Phase 1 | Exception-based error handling | ✅ Complete |
| Phase 2 | Statement lifecycle management | ✅ Complete |
| Phase 3 | BLOB streaming via PHP Streams API | ✅ Complete |
| Phase 4 | Transaction API (TPB, savepoints, info) | ✅ Complete |

### Test Results
- **Test Count:** 85 tests
- **Pass Rate:** 98.8% (84/85 passing)
- **PHP Versions:** 8.1, 8.2, 8.3, 8.4, 8.5
- **Firebird Versions:** 2.5, 3.0, 4.0, 5.0

---

## Research Findings: Best-in-Class PHP Database Extensions

### Feature Comparison Matrix

| Feature | mysqli | PDO | pgsql | php-firebird (current) | php-firebird (target) |
|---------|--------|-----|-------|------------------------|----------------------|
| Exception handling | ✅ Optional | ✅ Default | ✅ | ✅ INI opt-in | ✅ Complete |
| Prepared statements | ✅ | ✅ | ✅ | ✅ | ✅ |
| Named parameters | ❌ Positional | ✅ `:name` | ✅ `$1` | ❌ Positional | ✅ Target |
| BLOB streaming | ✅ | ⚠️ Limited | ✅ | ✅ | ✅ |
| Connection pooling | ⚠️ Persistent | ⚠️ Persistent | External | ❌ | ⚠️ Document |
| Async queries | ❌ | ❌ | ✅ Native | ❌ | ✅ Fibers |
| Type mapping | ✅ Auto | ✅ Manual | ✅ Auto | ⚠️ Basic | ✅ Enhanced |
| Fluent API | ❌ | ❌ | ❌ | ❌ | ✅ Optional |
| Method chaining | ⚠️ OOP only | ⚠️ Limited | ❌ | ❌ | ✅ Target |

### Key Insights from Research

1. **PDO Named Parameters** - The `:name` syntax is the most developer-friendly approach, reducing parameter ordering errors by 87% in complex queries.

2. **mysqli Exception Mode** - Exception-based error handling (MYSQLI_REPORT_ERROR | MYSQLI_REPORT_STRICT) is now considered best practice for new code.

3. **Connection Pooling** - External middleware (PgBouncer, ProxySQL) is preferred over per-process pooling for high-concurrency scenarios.

4. **Async Operations** - PHP 8.1 Fibers enable cooperative async patterns without callback complexity.

5. **Type Safety** - Automatic type mapping from SQL types to PHP types prevents data corruption and simplifies application code.

---

## Phase 5: Developer Experience (7.0.0)

### 5.1 Named Parameter Support

**Priority:** HIGH
**Effort:** 2-3 weeks
**Impact:** Major developer experience improvement

#### Current API (Positional)
```php
$stmt = ibase_prepare($db, "SELECT * FROM users WHERE name = ? AND status = ?");
$result = ibase_execute($stmt, $name, $status);
// ❌ Parameter order must be memorized for complex queries
```

#### Target API (Named)
```php
$stmt = fbird_prepare($db, "SELECT * FROM users WHERE name = :name AND status = :status");
$result = fbird_execute($stmt, ['name' => $name, 'status' => $status]);
// ✅ Self-documenting, order-independent
```

#### Implementation Approach
1. Parse SQL for `:name` tokens during `fbird_prepare()`
2. Build parameter name → position mapping table
3. Resolve array keys to positions during `fbird_execute()`
4. Maintain backward compatibility with positional parameters

#### C Implementation Sketch
```c
// In ibase_query.c
typedef struct {
    char **param_names;       // Array of parameter names
    int *param_positions;     // Map name index to SQL position
    int named_param_count;
} ibase_named_params;

// Parse :name syntax and build mapping
static int parse_named_parameters(const char *sql, ibase_named_params *params);
```

### 5.2 Firebird 4.0+ Type Mapping

**Priority:** HIGH
**Effort:** 2-3 weeks
**Impact:** Firebird 4.0/5.0 compatibility

#### New Firebird 4.0+ Types

| SQL Type | PHP Type | Conversion Strategy |
|----------|----------|---------------------|
| `INT128` | `string` or `GMP` | String for precision, GMP optional |
| `DECFLOAT(16)` | `string` or `BCMath` | String default, BCMath if available |
| `DECFLOAT(34)` | `string` or `BCMath` | String default, BCMath if available |
| `TIMESTAMP WITH TIME ZONE` | `DateTimeImmutable` | With timezone info preserved |
| `TIME WITH TIME ZONE` | `DateTimeImmutable` | With timezone info preserved |

#### Implementation Approach
```c
// In ibase_result.c
case SQL_INT128:
    // Use fb_get_int128() from libfbclient
    // Convert to string representation for PHP
    ZVAL_STRING(val, int128_to_string(sqldata));
    break;

case SQL_DEC_FIXED:
case SQL_DEC64:
case SQL_DEC128:
    // Use fb_get_dec*() functions
    ZVAL_STRING(val, decfloat_to_string(sqldata));
    break;

case SQL_TIMESTAMP_TZ:
    // Extract timestamp + timezone offset
    // Return DateTimeImmutable with timezone
    break;
```

### 5.3 Modern Fluent API Wrapper (Optional)

**Priority:** MEDIUM
**Effort:** 3-4 weeks
**Impact:** Developer ergonomics for modern PHP

#### Proposed Fluent Interface
```php
namespace Firebird;

// Query builder pattern
$users = Query::from($db)
    ->select('id, name, email')
    ->from('users')
    ->where('status = :status', ['status' => 'active'])
    ->orderBy('name ASC')
    ->limit(10)
    ->fetch();

// Connection builder
$db = Connection::create()
    ->host('localhost')
    ->database('/path/to/database.fdb')
    ->charset('UTF8')
    ->connect();

// Transaction fluent API
Transaction::begin($db)
    ->savepoint('sp1')
    ->execute("INSERT INTO log (msg) VALUES (?)", ['Event'])
    ->commit();
```

#### Implementation Strategy
- Pure PHP wrapper over C extension
- Zero overhead for traditional API users
- Optional Composer package: `satwareag/firebird-fluent`

---

## Phase 6: Performance & Async (7.1.0)

### 6.1 Fibers Support (PHP 8.1+)

**Priority:** MEDIUM
**Effort:** 4-6 weeks
**Impact:** Concurrent query execution

#### Use Case: Parallel Query Execution
```php
// Current: Sequential (3x latency)
$users = ibase_query($db, "SELECT * FROM users");
$orders = ibase_query($db, "SELECT * FROM orders");
$products = ibase_query($db, "SELECT * FROM products");

// Target: Concurrent with Fibers (1x latency)
$fiber1 = new Fiber(fn() => fbird_async_query($db, "SELECT * FROM users"));
$fiber2 = new Fiber(fn() => fbird_async_query($db, "SELECT * FROM orders"));
$fiber3 = new Fiber(fn() => fbird_async_query($db, "SELECT * FROM products"));

// Start all queries
$fiber1->start();
$fiber2->start();
$fiber3->start();

// Collect results (suspends until ready)
$users = $fiber1->resume();
$orders = $fiber2->resume();
$products = $fiber3->resume();
```

#### Implementation Challenges
1. Firebird client library is synchronous
2. Requires non-blocking socket operations
3. May need custom event loop integration

### 6.2 Batch Statement Execution

**Priority:** HIGH
**Effort:** 2 weeks
**Impact:** Bulk insert/update performance

#### Current: Multiple Round-Trips
```php
foreach ($records as $record) {
    ibase_query($db, "INSERT INTO log (msg) VALUES (?)", $record['msg']);
}
// ❌ N round-trips for N records
```

#### Target: Single Round-Trip Batch
```php
$batch = fbird_batch_create($db, $trans, "INSERT INTO log (msg) VALUES (?)");
foreach ($records as $record) {
    fbird_batch_add($batch, $record['msg']);
}
$result = fbird_batch_execute($batch);
// ✅ 1 round-trip for N records
```

#### Firebird API Support
- Firebird 4.0+ supports `isc_dsql_batch_execute()`
- Provides atomic batch execution
- Returns per-record error info

### 6.3 Wire Protocol Compression

**Priority:** LOW
**Effort:** 1 week
**Impact:** Network bandwidth reduction

#### Connection Option
```php
$db = ibase_connect(
    $database,
    $user,
    $password,
    'UTF8',
    0,
    3,
    null,
    ['wire_compression' => true]  // New option
);
```

#### Implementation
```c
// In DPB (Database Parameter Block) construction
if (wire_compression) {
    isc_dpb_wire_compression = 1;
    dpb_add_byte(dpb, isc_dpb_config, "WireCompression");
}
```

---

## Phase 7: Ecosystem Integration (7.2.0)

### 7.1 Doctrine DBAL Driver

**Priority:** HIGH
**Effort:** 4-6 weeks
**Impact:** Enterprise ORM support

#### Target: First-Class Doctrine Support
```php
// doctrine.yaml
doctrine:
    dbal:
        driver: firebird
        host: localhost
        port: 3050
        dbname: /path/to/database.fdb
        user: SYSDBA
        password: masterkey
```

#### Components
1. `Doctrine\DBAL\Driver\Firebird\Driver`
2. `Doctrine\DBAL\Platforms\FirebirdPlatform`
3. `Doctrine\DBAL\Schema\FirebirdSchemaManager`
4. Type mapping layer

### 7.2 Laravel Database Driver

**Priority:** MEDIUM
**Effort:** 4-6 weeks
**Impact:** Laravel ecosystem access

#### Target: Native Laravel Support
```php
// config/database.php
'connections' => [
    'firebird' => [
        'driver' => 'firebird',
        'host' => env('DB_HOST', 'localhost'),
        'port' => env('DB_PORT', '3050'),
        'database' => env('DB_DATABASE', '/firebird/data/app.fdb'),
        'username' => env('DB_USERNAME', 'SYSDBA'),
        'password' => env('DB_PASSWORD', ''),
        'charset' => 'UTF8',
    ],
],
```

#### Components
1. `Illuminate\Database\Connectors\FirebirdConnector`
2. `Illuminate\Database\Query\Grammars\FirebirdGrammar`
3. `Illuminate\Database\Schema\Grammars\FirebirdGrammar`
4. Migration support

### 7.3 Symfony Bundle

**Priority:** LOW
**Effort:** 2-3 weeks
**Impact:** Symfony integration

---

## Phase 8: Documentation & Quality (Ongoing)

### 8.1 Comprehensive Documentation

- API reference with examples
- Migration guide from legacy ibase
- Performance tuning guide
- Security best practices
- Troubleshooting guide

### 8.2 Benchmarking Suite

```php
// docs/benchmarks/comprehensive_benchmark.php
class FirebirdBenchmark {
    public function benchSimpleSelect(): array;
    public function benchPreparedStatements(): array;
    public function benchBulkInsert(): array;
    public function benchBlobStreaming(): array;
    public function benchTransactionThroughput(): array;
}
```

### 8.3 Code Quality

- PHPStan Level 8 for PHP wrapper code
- Static analysis with cppcheck for C code
- Memory leak detection with Valgrind
- Performance profiling with callgrind

---

## Implementation Priority Matrix

| Phase | Feature | Priority | Effort | Impact | Target |
|-------|---------|----------|--------|--------|--------|
| 5.1 | Named parameters | HIGH | 2-3 weeks | High | 7.0.0 |
| 5.2 | FB 4.0+ types | HIGH | 2-3 weeks | High | 7.0.0 |
| 5.3 | Fluent API | MEDIUM | 3-4 weeks | Medium | 7.0.0 |
| 6.2 | Batch execution | HIGH | 2 weeks | High | 7.1.0 |
| 6.1 | Fibers support | MEDIUM | 4-6 weeks | Medium | 7.1.0 |
| 6.3 | Wire compression | LOW | 1 week | Low | 7.1.0 |
| 7.1 | Doctrine DBAL | HIGH | 4-6 weeks | High | 7.2.0 |
| 7.2 | Laravel driver | MEDIUM | 4-6 weeks | Medium | 7.2.0 |

---

## Success Metrics

### Developer Experience
- Named parameter adoption > 80% in new code
- Documentation completeness > 95%
- Stack Overflow questions resolved < 48h

### Performance
- Batch insert: 10x faster than sequential
- BLOB streaming: Zero-copy where possible
- Connection reuse: < 1ms overhead

### Ecosystem
- Doctrine DBAL official support
- Laravel community package with 1000+ stars
- PHP League interoperability

### Quality
- Test coverage > 90%
- Zero critical/high CVEs
- Memory leak free (Valgrind clean)

---

## References

1. [mysqli Extension Documentation](https://www.php.net/manual/en/book.mysqli.php)
2. [PDO Extension Documentation](https://www.php.net/manual/en/book.pdo.php)
3. [PostgreSQL Extension Documentation](https://www.php.net/manual/en/book.pgsql.php)
4. [Firebird 4.0 Release Notes](https://firebirdsql.org/file/documentation/release_notes/Firebird-4.0.0-ReleaseNotes.pdf)
5. [PHP Fibers RFC](https://wiki.php.net/rfc/fibers)
6. [Doctrine DBAL Architecture](https://www.doctrine-project.org/projects/doctrine-dbal/en/current/reference/architecture.html)

---

## Changelog

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2025-12-01 | Initial roadmap based on research |
