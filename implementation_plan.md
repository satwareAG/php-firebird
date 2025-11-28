# Implementation Plan - Code Quality, Coverage, and Release Preparation

[Overview]
Perform a comprehensive code quality and test coverage audit of the PHP Firebird extension, propose and implement fixes, and verify the build pipeline locally using `act` to ensure readiness for an intermediate release.

We will analyze the codebase using `cppcheck` and `clang-tidy`, measure test coverage with `lcov`, and assess file complexity. Based on these findings, we will address critical quality issues. Finally, we will validate the changes by running the GitHub Actions workflow locally using `act`.

[Types]
No new PHP types or classes are explicitly planned at this stage. C-level struct modifications (e.g., `ibase_blob`) may occur if defect remediation requires it, but the primary focus is cleanup and verification.

[Files]
- **Existing file modification**: `ibase_*.c` files (based on static analysis findings) to fix memory leaks, logic errors, or complexity issues.
- **Existing file modification**: `tests/*.phpt` (based on coverage gaps) to improve test coverage.
- **New file creation**: `docs/development/INTERMEDIATE_RELEASE_REPORT.md` to document findings, fixes, and release readiness.
- **Configuration updates**: `.github/workflows/*.yml` (only if local `act` testing reveals pipeline issues).

[Functions]
- **Refactored**: Functions identified with high cyclomatic complexity or security warnings by `cppcheck`/`clang-tidy` will be refactored for safety and readability.
- **No public API changes** are intended unless a critical bug requires it (intermediate release focus is stability).

[Classes]
No architectural changes to PHP-exposed classes (`Firebird\Query`, `Firebird\Connection`, etc.) are planned in this phase.

[Dependencies]
- no new dependencies.
- Tools required: `act` (for local Github Actions), `lcov` (coverage), `cppcheck`, `clang-tidy`.

[Implementation Order]
1.  **Static Analysis**: Run `cppcheck` and `clang-tidy` on the codebase. Document and fix critical issues.
2.  **Complexity Analysis**: Identify high-complexity files/functions. Propose refactoring if risks justify it.
3.  **Test Coverage**: Run tests with coverage enabled (`scripts/container/coverage.sh` or manual). Identify major gaps. Add tests for uncovered critical paths.
4.  **Local Pipeline Verification**: Run `act` to simulate the GitHub Actions workflow locally. Fix any pipeline-specific failures.
5.  **Documentation**: Compile `INTERMEDIATE_RELEASE_REPORT.md`.

task_progress Items:
- [ ] Step 1: Run Static Analysis (cppcheck/clang-tidy) and fix critical issues
- [ ] Step 2: Run Coverage Analysis (lcov) and identify gaps
- [ ] Step 3: Add regression tests to improve coverage on critical paths
- [ ] Step 4: Verify pipeline locally using `act`
- [ ] Step 5: Generate Release Report
