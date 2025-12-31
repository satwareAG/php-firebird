# Implementation Plan

[Overview]
Fix the test-bundles workflow to download and test the correct PHP version bundle matching each distribution's system PHP version.

The `test-bundles` job in `.github/workflows/release-precompiled.yml` currently hardcodes downloading the PHP 8.4 NTS bundle (`pattern: '*php84-nts*'`) but then tests on distributions with different PHP versions (Ubuntu 22.04=PHP 8.1, Ubuntu 24.04=PHP 8.3, Debian 12=PHP 8.2, AlmaLinux 9=PHP 8.0). This causes the test to always skip with "System PHP differs from bundle PHP" because the extension built for one PHP version cannot be loaded by a different PHP version (API version mismatch).

The solution is to modify the matrix to include an explicit PHP version mapping for each distribution, then use that to download the correct bundle. Additionally, AlmaLinux 9 should be removed since its default PHP 8.0 is not supported (the extension requires PHP 8.1+).

[Types]
No type changes required - this is a GitHub Actions YAML workflow modification.

[Files]
Modify `.github/workflows/release-precompiled.yml` to fix the test-bundles job matrix and download logic.

**Files to modify:**

1. `.github/workflows/release-precompiled.yml` (lines 465-600 approximately)
   - Change the `matrix.distro` to include PHP version mapping
   - Update the download artifact step to use the matrix PHP version
   - Remove AlmaLinux 9 from testing (PHP 8.0 not supported)
   - Update step name to reflect dynamic PHP version

[Functions]
No function changes - YAML workflow modification only.

[Classes]
No class changes - YAML workflow modification only.

[Dependencies]
No dependency changes required.

[Testing]
Verification will occur by running the workflow on GitHub Actions after the changes are merged or via workflow_dispatch.

**Test scenarios:**
1. Ubuntu 22.04 should download `*php81-nts*` bundle and successfully load it with system PHP 8.1
2. Ubuntu 24.04 should download `*php83-nts*` bundle and successfully load it with system PHP 8.3
3. Debian 12 should download `*php82-nts*` bundle and successfully load it with system PHP 8.2
4. All tests should pass the PHP extension loading step (Step 10) instead of skipping due to version mismatch

**Local testing (optional):**
```bash
# Validate YAML syntax
npx yaml-lint .github/workflows/release-precompiled.yml

# Use act for local testing (limited - cannot test matrix expansion fully)
act -n -l | grep test-bundles
```

[Implementation Order]
Implement changes in a single commit since they are tightly coupled.

1. **Remove AlmaLinux 9 from the matrix** - PHP 8.0 is not supported by the extension
2. **Restructure matrix from simple list to include/objects** - Each entry needs both distro and php fields
3. **Update Download step name** - Change from "Download PHP 8.4 NTS Bundle" to "Download PHP ${{ matrix.php }} NTS Bundle"
4. **Update Download artifact pattern** - Change from `'*php84-nts*'` to `'*php${{ matrix.php }}-nts*'`
5. **Test the workflow** - Push changes and verify all test-bundles jobs pass

---

## Detailed Changes

### Current Matrix (Lines 471-477):
```yaml
    strategy:
      fail-fast: false
      matrix:
        distro:
          - ubuntu:22.04
          - ubuntu:24.04
          - debian:12
          - almalinux:9
```

### New Matrix:
```yaml
    strategy:
      fail-fast: false
      matrix:
        include:
          - distro: ubuntu:22.04
            php: "81"
          - distro: ubuntu:24.04
            php: "83"
          - distro: ubuntu:24.10
            php: "83"
          - distro: debian:12
            php: "82"
          - distro: fedora:41
            php: "83"
          # AlmaLinux 9 removed - default PHP 8.0 not supported (requires 8.1+)
```

### Current Download Step (Lines 480-484):
```yaml
      - name: Download PHP 8.4 NTS Bundle
        uses: actions/download-artifact@v4
        with:
          pattern: '*php84-nts*'
          path: bundle
```

### New Download Step:
```yaml
      - name: Download PHP ${{ matrix.php }} NTS Bundle
        uses: actions/download-artifact@v4
        with:
          pattern: '*php${{ matrix.php }}-nts*'
          path: bundle
```

---

## PHP Version Mapping Reference

| Distribution  | System PHP | Bundle Pattern    | Status    |
|---------------|------------|-------------------|-----------|
| Ubuntu 22.04  | 8.1        | `*php81-nts*`     | Supported |
| Ubuntu 24.04  | 8.3        | `*php83-nts*`     | Supported |
| Ubuntu 24.10  | 8.3        | `*php83-nts*`     | NEW       |
| Debian 12     | 8.2        | `*php82-nts*`     | Supported |
| Fedora 41     | 8.3        | `*php83-nts*`     | NEW       |
| AlmaLinux 9   | 8.0        | N/A               | REMOVED   |

This mapping is consistent with `scripts/verify-distributions.sh` which already uses these same PHP version mappings for local testing.