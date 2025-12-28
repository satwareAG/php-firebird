# Implementation Plan: Precompiled PHP Extension Distribution

[Overview]
Implement automated building and distribution of precompiled PHP Firebird extensions with bundled Firebird client libraries using manylinux_2_28 and GitHub Actions.

The php-firebird extension requires the Firebird client library (libfbclient) at runtime. Currently, users must install Firebird client libraries system-wide. This implementation provides precompiled extension packages with bundled libraries using `$ORIGIN` rpath, enabling drop-in deployment without system-wide dependencies.

**Strategy (Hybrid Distribution Model):**
1. **Source Distribution** - PECL/GitHub (existing)
2. **Precompiled Binaries** - GitHub Releases (new)
3. **Container Images** - Docker Hub (future)

**Build Matrix (10 packages per release):**
| PHP Version | Status | NTS | ZTS |
|-------------|--------|-----|-----|
| 8.1 | ⚠️ DEPRECATED | ✓ | ✓ |
| 8.2 | Active | ✓ | ✓ |
| 8.3 | Active | ✓ | ✓ |
| 8.4 | Active | ✓ | ✓ |
| 8.5 | Current | ✓ | ✓ |

**Technical Approach:**
- **Base Image**: `quay.io/pypa/manylinux_2_28_x86_64` (AlmaLinux 8, glibc 2.28)
- **Library Bundling**: patchelf with `--force-rpath --set-rpath '$ORIGIN/lib'`
- **Firebird Client**: Bundle FB 5.x client (backward compatible with FB 2.5-5.0 servers)

**Research Completed:**
- DeepWiki: pypa/manylinux - build infrastructure and CI patterns
- DeepWiki: shivammathur/setup-php - PHP version matrix strategies
- DeepWiki: NixOS/patchelf - ELF RPATH manipulation best practices
- Research: `docs/research/PRECOMPILED_EXTENSION_STRATEGY.md`

[Types]
No type definitions required - this is a build/CI infrastructure implementation.

The implementation involves shell scripts, Dockerfiles, and GitHub Actions YAML. No C/PHP code types are modified.

[Files]
Create new build infrastructure files and a GitHub Actions release workflow.

**New Files to Create:**

1. `.github/workflows/release-precompiled.yml`
   - GitHub Actions workflow for automated precompiled builds
   - Triggers on release creation and manual dispatch
   - Matrix strategy for PHP 8.1-8.4 × NTS/ZTS
   - Uploads artifacts to GitHub Release

2. `scripts/verify-bundle.sh`
   - Bundle verification script
   - Checks RPATH correctness with patchelf
   - Verifies dependency resolution with ldd
   - Tests PHP extension loading

3. `scripts/install-php-versions.sh`
   - Script to install multiple PHP versions in manylinux container
   - Uses shivammathur/php-builder binaries
   - Configures NTS and ZTS variants

4. `build/manylinux/docker-compose.yml`
   - Local development compose file
   - Mounts source directory for iterative builds
   - Environment variables for PHP version selection

**Existing Files to Modify:**

1. `build/manylinux/Dockerfile` (already created, needs refinement)
   - Improve PHP version installation reliability
   - Add version switching mechanism
   - Optimize layer caching

2. `scripts/build-precompiled.sh` (already created, needs enhancement)
   - Add automatic transitive dependency detection
   - Improve library search paths
   - Add bundle size reporting
   - Add hash verification for reproducibility

3. `README.md` (already updated)
   - Add GitHub Releases download instructions
   - Document precompiled package installation

4. `CHANGELOG.md`
   - Document v7.0.0 precompiled distribution feature

[Functions]
No C/PHP functions modified - shell script functions only.

**Shell Script Functions (build-precompiled.sh):**

1. `bundle_library()` - NEW
   - Recursively finds and bundles a library and its dependencies
   - Uses ldd to trace transitive deps
   - Filters out system libraries (glibc, libpthread, etc.)

2. `detect_php_variant()` - NEW
   - Detects if current PHP is NTS or ZTS
   - Returns appropriate build flags

3. `generate_checksums()` - NEW
   - Creates SHA256 checksums for release artifacts
   - Enables verification of downloaded packages

[Classes]
No class modifications - build infrastructure only.

[Dependencies]
Build environment dependencies installed in Dockerfile.

**Dockerfile Dependencies (AlmaLinux 8/manylinux_2_28):**
- `patchelf` - ELF binary modification
- `php-devel` - PHP development headers (per version)
- `libicu-devel` - ICU libraries
- `libtommath-devel` - BigNum library
- `libtomcrypt-devel` - Cryptography library
- `re2-devel` - Regex library (Firebird 4.0+)

**GitHub Actions Dependencies:**
- `quay.io/pypa/manylinux_2_28_x86_64` - Build container
- `softprops/action-gh-release@v1` - Release asset upload
- `actions/upload-artifact@v4` - Artifact storage

**No changes to extension runtime dependencies.**

[Testing]
Verify builds locally with Docker before CI deployment.

**Local Testing:**
```bash
# Build test container
docker build -t php-firebird-builder build/manylinux/

# Test single build (PHP 8.4 NTS)
docker run --rm -v $(pwd):/src php-firebird-builder 8.4 nts x86_64

# Verify bundle
./scripts/verify-bundle.sh dist/php-firebird-7.0.0-php84-nts-linux-x86_64/
```

**CI Testing Matrix:**
| Test | PHP Versions | Distributions |
|------|--------------|---------------|
| Build | 8.1, 8.2, 8.3, 8.4 | manylinux_2_28 |
| Load | 8.4 | Ubuntu 20.04, Ubuntu 22.04, Debian 11 |
| Connect | 8.4 | Against FB 3.0 and FB 5.0 servers |

**Verification Criteria:**
1. `patchelf --print-rpath firebird.so` shows `$ORIGIN/lib`
2. `ldd firebird.so` (from bundle dir) shows no "not found"
3. `php -d "extension=./firebird.so" -m | grep firebird` succeeds
4. `php -d "extension=./firebird.so" -r "echo fbird_connect(...);"` succeeds
5. Bundle size < 50MB compressed

[Implementation Order]
Phased implementation: refinement, CI workflow, testing, documentation.

**Phase 1: Enhance Build Scripts (2 hours)**
1. Improve `scripts/build-precompiled.sh`:
   - Add `bundle_library()` with ldd-based dependency tracing
   - Add system library whitelist (glibc, libpthread, etc.)
   - Add automatic ICU version detection
   - Add `--dry-run` mode for testing
   - Add checksums generation

2. Create `scripts/verify-bundle.sh`:
   - RPATH verification
   - Dependency resolution check
   - PHP load test
   - Connection test (optional, requires FB server)

3. Create `scripts/install-php-versions.sh`:
   - PHP version installation from shivammathur/php-builder
   - NTS/ZTS variant handling
   - Version switching helper

**Phase 2: Refine Docker Build Environment (1 hour)**
4. Update `build/manylinux/Dockerfile`:
   - Use multi-stage build for smaller final image
   - Pre-install Firebird SDK with version flexibility
   - Add PHP version management
   - Optimize layer caching

5. Create `build/manylinux/docker-compose.yml`:
   - Volume mounts for local development
   - Environment variable configuration
   - Cache optimization

**Phase 3: GitHub Actions Workflow (2 hours)**
6. Create `.github/workflows/release-precompiled.yml`:
   - Trigger on release creation
   - Matrix strategy: PHP 8.1-8.4 × NTS/ZTS
   - Build in manylinux container
   - Upload to GitHub Release
   - Generate checksums

7. Create `.github/workflows/test-precompiled.yml`:
   - Test built packages on multiple distributions
   - Verify load and basic functionality
   - Run after release-precompiled completes

**Phase 4: Documentation and Testing (1 hour)**
8. Update `CHANGELOG.md`:
   - Document precompiled distribution feature

9. Test full release flow:
   - Create draft release
   - Trigger build workflow
   - Verify uploads
   - Test downloads on fresh systems

**Phase 5: Optional Enhancements (future)**
10. Add aarch64 (ARM64) support
11. Add Docker Hub images with preloaded extension
12. Add PHP 8.5 support when available
