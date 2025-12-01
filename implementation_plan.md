# Implementation Plan

[Overview]
Add code quality workflow to GitHub Actions and update local act testing to ensure parity between local and CI validation.

This implementation addresses a gap identified in the CI pipeline: static analysis (clang-tidy, cppcheck) currently only runs locally via `scripts/host/qa_local.sh` but not in GitHub Actions. By adding a dedicated `code-quality.yml` workflow and updating the act testing script, developers will have consistent code quality validation both locally and in CI.

Key changes:
1. Create new `.github/workflows/code-quality.yml` workflow for static analysis
2. Update `scripts/host/test_with_act.sh` to support running the new code-quality job
3. Update documentation to reflect current job names and new workflow

[Types]
No new types or data structures are required for this implementation.

This is a CI/CD and scripting task that involves YAML workflow files, Bash scripts, and Markdown documentation. No application-level types are affected.

[Files]
Files to be created and modified for GitHub Actions code quality integration.

**New files to be created:**
- `.github/workflows/code-quality.yml` - GitHub Actions workflow for static analysis (clang-tidy, cppcheck)

**Existing files to be modified:**
- `scripts/host/test_with_act.sh` - Add support for `--quality` flag to run code-quality job
- `docs/development/LOCAL_GITHUB_ACTIONS_TESTING.md` - Update outdated job names and add code-quality documentation

**Files to remain unchanged:**
- `.github/workflows/main.yml` - Already correct with `linux-matrix-build` job
- `.github/workflows/coverage.yml` - Already correct for code coverage
- `scripts/host/qa_local.sh` - Reference implementation for static analysis steps
- `scripts/container/analysis/clang_tidy.sh` - Container-based clang-tidy (reference)
- `scripts/container/analysis/cppcheck.sh` - Container-based cppcheck (reference)

[Functions]
Shell functions to be added and modified.

**New functions in `scripts/host/test_with_act.sh`:**
- `run_code_quality()` - Execute code-quality workflow via act
- Updated `usage()` - Add documentation for new `--quality` flag

**Modified functions in `scripts/host/test_with_act.sh`:**
- `run_act_matrix()` - No changes (already correct)
- Main argument parsing block - Add `--quality` flag handling

[Classes]
No classes are involved in this implementation.

This is purely a CI/CD and scripting task with no object-oriented code changes.

[Dependencies]
System dependencies required for the code-quality workflow.

**GitHub Actions runner dependencies (installed in workflow):**
- `bear` - Build wrapper for generating compile_commands.json
- `clang-tools` / `clang-tidy` - Static analysis for C/C++
- `cppcheck` - Static analysis for C/C++
- `libxml2-utils` - For XML parsing (xmllint)
- Standard PHP build dependencies (already in main.yml)

**Local testing dependencies:**
- `act` - GitHub Actions local runner (already documented as required)
- Docker - Container runtime (already required)

**No new package.json or composer.json dependencies required.**

[Testing]
Validation approach for the implementation.

**Manual testing steps:**
1. Run `scripts/host/test_with_act.sh --quality` to verify code-quality workflow runs locally
2. Run `scripts/host/test_with_act.sh --php 8.4 --fb 5.0` to verify matrix builds still work
3. Push branch to GitHub and verify code-quality workflow triggers and passes
4. Verify clang-tidy and cppcheck output matches local `qa_local.sh` output

**Validation criteria:**
- Code-quality workflow completes successfully on Ubuntu runner
- Static analysis detects same issues as local `qa_local.sh`
- Exit codes properly propagate (failure blocks PR merge)
- Documentation accurately describes all commands and options

**No automated tests to add** - This is infrastructure/CI configuration.

[Implementation Order]
Ordered steps to implement changes with minimal risk.

1. **Create `.github/workflows/code-quality.yml`**
   - Model after `scripts/container/analysis/clang_tidy.sh` and `cppcheck.sh`
   - Use PHP 8.4 / Firebird 5.0 as fixed matrix (single configuration)
   - Include bear for compilation database generation
   - Add clang-tidy and cppcheck steps

2. **Test code-quality.yml locally with act**
   - Run `act -W .github/workflows/code-quality.yml -j code-quality --rm`
   - Verify successful completion
   - Debug any issues before updating scripts

3. **Update `scripts/host/test_with_act.sh`**
   - Add `--quality` flag to run code-quality job
   - Add `run_code_quality()` function
   - Update usage documentation

4. **Update `docs/development/LOCAL_GITHUB_ACTIONS_TESTING.md`**
   - Replace `linux-comprehensive-build` with `linux-matrix-build`
   - Add section for code-quality workflow
   - Update quick commands section

5. **Final validation**
   - Run `scripts/host/test_with_act.sh --quality`
   - Run `scripts/host/test_with_act.sh --php 8.3 --fb 4.0`
   - Push to GitHub and verify workflows run correctly
