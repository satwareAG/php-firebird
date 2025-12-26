# PHP Firebird Extension Fuzzer

This directory contains the fuzzing infrastructure for the PHP Firebird extension. It is designed to detect memory safety issues, undefined behavior, and logic bugs using a coverage-guided, grammar-aware approach.

## Architecture

The fuzzer uses a modular architecture:

- **`run.php`**: Main entry point. Parses arguments, initializes the harness, and outputs SARIF reports.
- **`src/FuzzHarness.php`**: Core engine. Manages state (connections, transactions, etc.) and executes weighted random operations.
- **`src/Operations/`**: Operation classes defining valid API sequences.
- **`src/SarifReport.php`**: Generates industry-standard SARIF 2.1.0 reports for integration with security tools.
- **`corpus/`**: Seed scripts that exercise specific edge cases (BLOBs, boundaries, etc.).

## Usage

### Running via Docker (Recommended)

The easiest way to run the fuzzer is using the provided helper script, which runs inside the ASan-enabled container:

```bash
./scripts/fuzz_asan.sh [iterations]
```

Example:
```bash
./scripts/fuzz_asan.sh 1000
```

### Running Manually

If you have a local PHP build with the Firebird extension enabled:

```bash
php fuzz/run.php --iterations=1000 --output=report.sarif --dsn=localhost:/path/to/db.fdb
```

## Configuration

The fuzzer accepts the following arguments:

- `--iterations=N`: Number of operations to execute (default: 1000)
- `--output=FILE`: Path to save SARIF report (default: fuzz_report.sarif)
- `--dsn=DSN`: Firebird connection string

## Output

The fuzzer generates a SARIF 2.1.0 JSON report containing:
- Execution status (pass/fail)
- List of detected errors with stack traces
- Coverage metrics (implicit via operation counts)

This format can be uploaded to GitHub Code Scanning or viewed in SARIF viewers.

## Adding New Operations

To add new fuzzing operations:

1. Create a new method in the appropriate `src/Operations/` class (or create a new class).
2. The method should return a `Closure` that takes `FuzzHarness $h` as context.
3. Register the operation in `FuzzHarness::loadDefaultOperations()` with an appropriate weight.

Example:

```php
// src/Operations/MyOps.php
class MyOps {
    public static function myOp(FuzzHarness $h): Closure {
        return function() use ($h) {
            // Implementation
        };
    }
}

// src/FuzzHarness.php
$this->registerOperation('my_op', MyOps::myOp($this), 5.0);
