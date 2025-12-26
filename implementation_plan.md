# Implementation Plan: PHP Firebird Extension Fuzzer Infrastructure

[Overview]
Create a robust, industry-standard fuzzing infrastructure for the PHP Firebird extension using a dedicated `fuzz/` directory structure.

This implementation addresses the root cause issue where `tests/*.php` files are gitignored, preventing fuzzer scripts from being tracked in version control. The solution follows 2025 best practices for PHP extension security testing:

1. **Dedicated `fuzz/` directory** - Bypasses the `tests/*.php` gitignore rule and provides clean separation between deterministic phpt tests and stochastic fuzz tests.
2. **SARIF report format** - Industry-standard JSON schema (v2.1.0) for security findings, enabling direct integration with GitHub/GitLab code scanning dashboards.
3. **Pre-seeded corpus** - Accelerates coverage discovery by providing initial inputs that exercise all major fbird_* APIs.
4. **Modular architecture** - Separates fuzzing logic (harness), reporting (SARIF generator), and execution (runner script) for maintainability.

The fuzzer targets memory safety issues (buffer overflows, use-after-free, memory leaks) and undefined behavior through integration with the existing ASan-enabled Docker container (`php83-asan`).

[Types]
PHP classes and data structures for the fuzzing framework.

```php
// fuzz/src/SarifReport.php
class SarifReport {
    private string $schema = 'https://json.schemastore.org/sarif-2.1.0.json';
    private string $version = '2.1.0';
    private array $runs = [];
    
    public function addRun(string $toolName, string $toolVersion): int;
    public function addResult(int $runIndex, array $result): void;
    public function toJson(): string;
}

// Result structure for SARIF
interface SarifResult {
    string $ruleId;        // e.g., "FUZZ001"
    string $level;         // "error", "warning", "note"
    string $message;       // Human-readable description
    ?array $locations;     // File/line info if available
    ?array $stacks;        // Stack trace for crashes
    ?string $fingerprint;  // Unique hash for deduplication
}

// fuzz/src/FuzzHarness.php
class FuzzHarness {
    private array $operations = [];
    private array $state = [
        'connections' => [],
        'transactions' => [],
        'statements' => [],
        'blobs' => [],
    ];
    
    public function registerOperation(string $name, callable $fn, float $weight): void;
    public function execute(int $iterations): FuzzResult;
}

// fuzz/src/FuzzResult.php
class FuzzResult {
    public int $iterations;
    public int $passed;
    public int $failed;
    public array $errors;        // Captured error messages
    public array $coverage;      // API function coverage
    public float $duration;
}
```

[Files]
Create a new `fuzz/` directory structure following 2025 PHP extension fuzzing best practices.

**New files to create:**
- `fuzz/run.php` - Main entry point; parses args, runs harness, outputs SARIF
- `fuzz/src/SarifReport.php` - SARIF 2.1.0 report generator class
- `fuzz/src/FuzzHarness.php` - Core fuzzing logic with weighted random operations
- `fuzz/src/FuzzResult.php` - Result data structure
- `fuzz/src/Operations/ConnectionOps.php` - fbird_connect, fbird_pconnect, fbird_close operations
- `fuzz/src/Operations/TransactionOps.php` - fbird_trans, fbird_commit, fbird_rollback operations
- `fuzz/src/Operations/QueryOps.php` - fbird_query, fbird_prepare, fbird_execute operations
- `fuzz/src/Operations/BlobOps.php` - fbird_blob_create, fbird_blob_add, fbird_blob_get operations
- `fuzz/corpus/seed_connect.php` - Seed: connection patterns
- `fuzz/corpus/seed_transaction.php` - Seed: transaction patterns
- `fuzz/corpus/seed_blob.php` - Seed: BLOB edge cases
- `fuzz/corpus/seed_boundary.php` - Seed: INT64/NUMERIC boundary values
- `fuzz/README.md` - Documentation for fuzzing infrastructure
- `fuzz/crashes/.gitkeep` - Directory for crash artifacts (gitignored contents)

**Existing files to modify:**
- `scripts/fuzz_asan.sh` - Update to use `fuzz/run.php` instead of `tests/fuzzer.php`
- `scripts/qa.sh` - Update fuzz mode to reference new location
- `.gitignore` - Add `fuzz/crashes/*` and `!fuzz/crashes/.gitkeep`

**Files to delete:**
- `tests/fuzzer.php` - Empty placeholder (0 bytes)
- `docker/fuzz_report.json` - Old report location (will be generated in `fuzz/reports/`)

[Functions]
Core functions in the fuzzing framework.

**fuzz/run.php (main entry point):**
```php
function main(array $argv): int;
function parseArgs(array $argv): array;
function loadCorpus(string $corpusDir): array;
function outputSarif(SarifReport $report, string $outputPath): void;
```

**fuzz/src/SarifReport.php:**
```php
public function __construct(string $toolName = 'php-firebird-fuzzer', string $toolVersion = '1.0.0');
public function addRun(string $toolName, string $toolVersion): int;
public function addResult(int $runIndex, string $ruleId, string $level, string $message, ?array $location = null, ?array $stack = null): void;
public function setInvocation(int $runIndex, bool $success, ?string $exitCode = null): void;
public function toJson(int $options = JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES): string;
public function toArray(): array;
```

**fuzz/src/FuzzHarness.php:**
```php
public function __construct(string $dsn, string $user, string $password);
public function registerOperation(string $name, callable $fn, float $weight = 1.0): void;
public function loadDefaultOperations(): void;
public function execute(int $iterations, ?callable $progressCallback = null): FuzzResult;
protected function selectOperation(): string;
protected function executeOperation(string $name): array;
protected function captureError(): ?array;
```

**fuzz/src/FuzzResult.php:**
```php
public function __construct();
public function recordSuccess(string $operation): void;
public function recordFailure(string $operation, array $error): void;
public function getCoverage(): array;
public function toArray(): array;
```

**Operation classes (Operations/*.php):**
```php
// Each operation class provides static methods returning closures
class ConnectionOps {
    public static function connect(FuzzHarness $h): Closure;
    public static function pconnect(FuzzHarness $h): Closure;
    public static function close(FuzzHarness $h): Closure;
    public static function forceNew(FuzzHarness $h): Closure;
}

class TransactionOps {
    public static function begin(FuzzHarness $h): Closure;
    public static function commit(FuzzHarness $h): Closure;
    public static function rollback(FuzzHarness $h): Closure;
    public static function commitRetaining(FuzzHarness $h): Closure;
    public static function savepoint(FuzzHarness $h): Closure;
}

class QueryOps {
    public static function simpleQuery(FuzzHarness $h): Closure;
    public static function prepareExecute(FuzzHarness $h): Closure;
    public static function parameterizedInsert(FuzzHarness $h): Closure;
    public static function fetchAll(FuzzHarness $h): Closure;
}

class BlobOps {
    public static function createBlob(FuzzHarness $h): Closure;
    public static function streamBlob(FuzzHarness $h): Closure;
    public static function largeBlob(FuzzHarness $h): Closure;
    public static function postCommitAccess(FuzzHarness $h): Closure;  // Known edge case
}
```

[Classes]
Object-oriented design for the fuzzing framework.

**New classes:**

1. **SarifReport** (`fuzz/src/SarifReport.php`)
   - Generates SARIF 2.1.0 compliant JSON reports
   - Methods: `addRun()`, `addResult()`, `setInvocation()`, `toJson()`
   - No inheritance

2. **FuzzHarness** (`fuzz/src/FuzzHarness.php`)
   - Core fuzzer engine with weighted random operation selection
   - Maintains state of active connections, transactions, statements, blobs
   - Methods: `registerOperation()`, `execute()`, `loadDefaultOperations()`
   - No inheritance

3. **FuzzResult** (`fuzz/src/FuzzResult.php`)
   - Data class for fuzzing results
   - Tracks iterations, pass/fail counts, errors, coverage
   - Methods: `recordSuccess()`, `recordFailure()`, `getCoverage()`, `toArray()`
   - No inheritance

4. **ConnectionOps** (`fuzz/src/Operations/ConnectionOps.php`)
   - Static factory methods for connection-related operations
   - No inheritance

5. **TransactionOps** (`fuzz/src/Operations/TransactionOps.php`)
   - Static factory methods for transaction-related operations
   - No inheritance

6. **QueryOps** (`fuzz/src/Operations/QueryOps.php`)
   - Static factory methods for query-related operations
   - No inheritance

7. **BlobOps** (`fuzz/src/Operations/BlobOps.php`)
   - Static factory methods for BLOB-related operations
   - Includes known edge cases from FUZZING_2025.md research
   - No inheritance

**No existing classes are modified.**

[Dependencies]
No new external dependencies required.

The fuzzing framework uses:
- PHP 8.3+ built-in functions
- The firebird extension itself (being tested)
- Standard PHP JSON functions for SARIF output
- No Composer packages needed (keeps it lightweight for container environments)

The existing Docker infrastructure (`php83-asan` container) already provides:
- PHP compiled with AddressSanitizer
- Firebird client libraries
- All necessary build tools

[Testing]
Validation strategy for the fuzzing infrastructure.

**Manual validation:**
1. Run `php fuzz/run.php --iterations=10 --output=test.sarif` and verify SARIF output validates against schema
2. Run full suite (`./scripts/fuzz_asan.sh 100`) and confirm no false positives
3. Verify SARIF can be uploaded to GitHub code scanning (if available)

**Integration with existing tests:**
- The fuzzer is NOT a phpt test; it's a separate QA tool
- Existing phpt tests in `tests/` remain unchanged
- The `scripts/qa.sh --mode=fuzz` triggers the fuzzer

**Validation checklist:**
- [ ] SARIF output validates against https://json.schemastore.org/sarif-2.1.0.json
- [ ] Exit code 0 when no crashes, non-zero when crashes detected
- [ ] ASan errors are captured and reported in SARIF format
- [ ] All fbird_* functions are covered by at least one operation
- [ ] Corpus seeds execute without errors on clean database

[Implementation Order]
Sequential implementation steps with minimal dependencies.

1. **Create directory structure** - `fuzz/`, `fuzz/src/`, `fuzz/src/Operations/`, `fuzz/corpus/`, `fuzz/crashes/`

2. **Implement SarifReport class** - Core SARIF 2.1.0 generator (no dependencies)

3. **Implement FuzzResult class** - Simple data class (no dependencies)

4. **Implement FuzzHarness class** - Core engine (depends on FuzzResult)

5. **Implement Operation classes** - ConnectionOps, TransactionOps, QueryOps, BlobOps (depend on FuzzHarness)

6. **Create seed corpus** - Pre-generated PHP scripts exercising edge cases

7. **Implement run.php entry point** - Ties everything together (depends on all above)

8. **Update scripts/fuzz_asan.sh** - Point to new fuzz/run.php

9. **Update scripts/qa.sh** - Update fuzz mode path

10. **Update .gitignore** - Add fuzz/crashes/* pattern

11. **Delete obsolete files** - Remove tests/fuzzer.php, docker/fuzz_report.json

12. **Create fuzz/README.md** - Document usage and architecture

13. **Validate** - Run fuzzer, verify SARIF output, test in ASan container
