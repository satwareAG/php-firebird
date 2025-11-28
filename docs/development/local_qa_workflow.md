# Local QA Workflow & Optimization

## Overview

The `scripts/qa_local.sh` script provides a unified, optimized workflow for local development. It integrates static analysis, build verification, and robust testing into a single command, leveraging the Docker environment for consistency.

This workflow is designed to "Fail Fast" — ensuring code quality issues (static analysis) are caught before running the full test suite or dynamic analysis.

## Prerequisites

- Docker & Docker Compose
- A running PHP development container (script will start it if needed)

## Usage

```bash
./scripts/qa_local.sh [container_name] [mode]
```

### Arguments

- **container_name**: The Docker service name to run against (default: `php82-dev`).
  - Options: `php81-dev`, `php82-dev`, `php83-dev`, `php84-dev`, `php85-dev`.
- **mode**: `fast` (default) or `full`.
  - `fast`: Static Analysis + Build + Unit Tests.
  - `full`: Fast + Valgrind (Memory Leaks) + AddressSanitizer (Memory Errors).

### Examples

**Standard Iteration (Fast):**
```bash
./scripts/qa_local.sh
```
Checks code style, static analysis, compiles, and runs unit tests on PHP 8.2.

**Target Specific Version:**
```bash
./scripts/qa_local.sh php83-dev
```

**Deep Analysis (Full):**
```bash
./scripts/qa_local.sh php82-dev full
```
Runs everything in 'fast' mode, plus Valgrind and AddressSanitizer checks. Note that `full` mode takes significantly longer as ASan requires a clean rebuild.

## Workflow Steps

1. **Environment Check**: Ensures the target Docker container is running.
2. **Tool Provisioning**: Installs QA tools (`bear`, `clang-tidy`, `cppcheck`) inside the container if missing.
3. **Compilation Database**: Builds the extension using `bear` to generate `compile_commands.json` (required for accurate static analysis).
4. **Static Analysis (Fail Fast)**:
   - **Clang-Tidy**: Modern C++17 checks and best practices.
   - **Cppcheck**: Comprehensive static analysis.
   - *Fail Fast*: If any tool reports errors, the script exits immediately.
5. **Unit Tests**: Runs the standard test suite (`run-tests.php`).
6. **Dynamic Analysis** (Full Mode Only):
   - **Valgrind**: Analyzes memory usage on the standard build.
   - **AddressSanitizer (ASan)**: Rebuilds the extension with ASan instrumentation and re-runs tests to catch memory corruption.

## Optimization Notes

- **Caching**: Tools are installed once per container lifespan.
- **Build Efficiency**: The script attempts to reuse the build for both static analysis and testing. However, ASan requires a rebuild.
- **Integration**: Fits into the "Inner Loop" of development — run this before committing.

## Troubleshooting

- If tools fail to install: Ensure internet connectivity from within Docker containers.
- If `bear` fails: Ensure `make clean` was successful. The script handles this automatically.
