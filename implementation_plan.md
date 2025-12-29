# Implementation Plan: Issue #39 - Windows Extension-Matrix

[Overview]
Adopt php-windows-builder/extension-matrix action for automatic PHP version detection and build matrix generation.

This change replaces the manual bash matrix generation in `.github/workflows/windows.yml` with the official `php/php-windows-builder/extension-matrix@v1` action. The action automatically reads PHP version constraints from `composer.json` and generates the appropriate build matrix, reducing maintenance burden when new PHP versions are released.

**Current approach**: Manual shell script generates a hardcoded matrix for PHP 8.1, 8.2, 8.3, 8.4.
**New approach**: `extension-matrix` reads `"php": ">=8.1"` from composer.json and generates matrix automatically.

[Types]
No type changes required - this is a CI/CD workflow update only.

[Files]
Modify one workflow file.

**Files to Modify:**
1. `.github/workflows/windows.yml`
   - Replace `get-matrix` job bash script with `extension-matrix` action
   - Update `build` job to use generated matrix including `os` field
   - Keep Firebird SDK caching and environment setup
   - Simplify the workflow while preserving release upload functionality

**Files to Create:**
None.

**Files to Delete:**
None.

[Functions]
No function modifications - this is workflow configuration.

[Classes]
No class modifications - this is workflow configuration.

[Dependencies]
No new package dependencies.

**GitHub Action Dependency:**
- `php/php-windows-builder/extension-matrix@v1` - Already available, used by extension@v1 build action

[Testing]
Validation through CI/CD workflow execution.

**Pre-push Validation:**
- Review generated workflow YAML for syntax errors
- Verify matrix structure compatibility with existing build job

**Post-push Validation:**
- Monitor GitHub Actions workflow execution
- Verify all PHP version/arch/ts combinations build successfully
- Compare artifact output with previous workflow runs

[Implementation Order]
Single-step implementation of workflow update.

1. **Update windows.yml workflow**
   - Replace `get-matrix` job shell script with `extension-matrix` action
   - Add `php-version-list` input to explicitly list: `8.1, 8.2, 8.3, 8.4`
   - Set `arch-list: x64` (x86 not supported due to Firebird SDK limitation)
   - Set `ts-list: nts, ts`
   - Update build job to use `${{ matrix.os }}` from generated matrix
   - Keep Firebird SDK setup steps unchanged

2. **Commit and push changes**
   - Use conventional commit: `ci(windows): adopt extension-matrix for automatic build matrix`
   - Reference Issue #39

3. **Verify workflow execution**
   - Check GitHub Actions for successful execution
   - Verify all builds complete

4. **Close Issue #39**
   - Add completion comment with changes summary
