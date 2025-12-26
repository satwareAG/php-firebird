# Implementation Plan: Windows DLL Build Workflow for php-firebird

[Overview]
Implement automated Windows DLL builds for php-firebird using the official php-windows-builder GitHub Actions with multi-version Firebird SDK support.

This implementation addresses GitHub Issue #24 by creating a complete CI/CD pipeline that builds Windows DLLs for PHP 8.1-8.4 across multiple Firebird client versions (3.0, 4.0, 5.0). The workflow follows best practices from large open source PHP extensions like xdebug and uses the official php/php-windows-builder infrastructure. DLLs will be automatically attached to GitHub releases, enabling Windows users to easily install the extension with their PHP installations.

**Key Challenges Addressed:**
1. Firebird client libraries are not included in PHP Windows SDK deps - requires custom SDK download step
2. Multiple Firebird versions require matrix-based builds with version-specific SDK setup
3. DLL naming must follow established conventions for user clarity
4. Release artifacts need automated attachment to GitHub releases

**Best Practices Incorporated:**
- Matrix strategy for PHP version × architecture × thread safety × Firebird version
- Immutable releases support (tag-triggered builds)
- Checksum generation for release integrity
- Fail-fast disabled for broader compatibility testing
- Artifact caching for Firebird SDK downloads

[Types]
No new types required - this is a CI/CD workflow implementation.

The workflow uses GitHub Actions matrix types:
- `php-version`: string (8.1, 8.2, 8.3, 8.4)
- `arch`: string (x64, x86)
- `ts`: string (nts, ts)
- `firebird-version`: string (3.0, 4.0, 5.0)

[Files]
Create 3 new workflow files and 1 supporting script.

**New Files to Create:**

1. **`.github/workflows/windows.yml`** (Primary workflow)
   - Main Windows DLL build workflow
   - Matrix: PHP 8.1-8.4 × x64/x86 × NTS/TS × FB 3.0/4.0/5.0
   - Uses php-windows-builder actions
   - Downloads Firebird SDK before build
   - Produces DLLs as artifacts

2. **`.github/workflows/release-windows.yml`** (Release workflow)
   - Triggered on release creation
   - Builds all DLL variants
   - Attaches DLLs to GitHub release with checksums
   - Generates SHA256SUMS.txt for verification

3. **`.github/scripts/download-firebird-sdk.ps1`** (PowerShell helper)
   - Downloads Firebird Windows SDK from GitHub releases
   - Extracts to build directory
   - Sets environment variables for php-windows-builder
   - Supports FB 3.0.12, 4.0.6, 5.0.3 versions

4. **`docs/WINDOWS_INSTALLATION.md`** (Documentation)
   - Instructions for downloading correct DLL variant
   - php.ini configuration
   - Firebird client runtime requirements
   - Troubleshooting guide

**Existing Files to Modify:**

5. **`composer.json`** (Minor update)
   - Ensure `"type": "php-ext"` is present (already correct)
   - Add `"php": ">=8.1"` constraint if not present

6. **`README.md`** (Documentation update)
   - Add Windows installation section
   - Link to releases page for DLL downloads
   - Reference WINDOWS_INSTALLATION.md

[Functions]
No code functions - PowerShell script functions only.

**PowerShell Script Functions (download-firebird-sdk.ps1):**

1. **`Get-FirebirdSDK`**
   - Parameters: `$Version` (3.0|4.0|5.0), `$Architecture` (x64|x86)
   - Downloads Firebird Windows installer from GitHub releases
   - Returns path to extracted SDK directory

2. **`Extract-FirebirdSDK`**
   - Parameters: `$InstallerPath`, `$DestinationPath`
   - Extracts SDK components (headers, libs) from installer
   - Uses 7-zip or Windows built-in extraction

3. **`Set-FirebirdBuildEnvironment`**
   - Parameters: `$SDKPath`
   - Sets CFLAGS, LDFLAGS, PATH for php-windows-builder
   - Exports `FIREBIRD_HOME` environment variable

**Workflow Job Functions:**

4. **`get-windows-matrix`** (GitHub Actions job)
   - Generates complete build matrix
   - Combines: php-version × arch × ts × firebird-version
   - Excludes incompatible combinations (PHP 8.1 + FB 5.0 unsupported)

5. **`build-windows`** (GitHub Actions job)
   - Downloads Firebird SDK
   - Invokes php-windows-builder/extension action
   - Uploads DLL artifact with descriptive name

6. **`release-artifacts`** (GitHub Actions job)
   - Downloads all build artifacts
   - Generates checksums
   - Uploads to GitHub release

[Classes]
No classes - this is infrastructure code.

[Dependencies]
External dependencies for GitHub Actions workflow.

**GitHub Actions Dependencies:**

1. **`php/php-windows-builder/extension-matrix@v1`**
   - Generates build matrix from composer.json
   - Official PHP project action

2. **`php/php-windows-builder/extension@v1`**
   - Compiles PHP extension on Windows
   - Uses Windows SDK and Visual Studio

3. **`php/php-windows-builder/release@v1`**
   - Uploads artifacts to GitHub releases
   - Handles draft/published releases

4. **`actions/checkout@v4`**
   - Standard checkout action

5. **`actions/upload-artifact@v4`**
   - Upload build artifacts between jobs

6. **`actions/download-artifact@v4`**
   - Download artifacts for release job

**Firebird SDK Dependencies (Downloaded at Build Time):**

| Version | Download URL | Components |
|---------|--------------|------------|
| 3.0.12  | `github.com/FirebirdSQL/firebird/releases/download/v3.0.12/Firebird-3.0.12.33787-0.amd64.tar.gz` | fbclient_ms.lib, ibase.h |
| 4.0.6   | `github.com/FirebirdSQL/firebird/releases/download/v4.0.6/Firebird-4.0.6.3602-0-x64.zip` | fbclient_ms.lib, ibase.h, firebird/Interface.h |
| 5.0.3   | `github.com/FirebirdSQL/firebird/releases/download/v5.0.3/Firebird-5.0.3.1683-0-windows-x64.exe` | fbclient_ms.lib, ibase.h, firebird/Interface.h |

[Testing]
Verify workflow functions correctly before first release.

**Manual Verification Steps:**

1. **Workflow Syntax Validation**
   - Run `gh workflow view windows.yml` to verify syntax
   - Check for YAML parsing errors in GitHub Actions tab

2. **Matrix Generation Test**
   - Trigger workflow on push to feature branch
   - Verify all expected matrix combinations appear
   - Check exclusions work correctly

3. **Single Build Test**
   - Run with limited matrix (PHP 8.4, x64, NTS, FB 5.0)
   - Verify Firebird SDK download succeeds
   - Confirm DLL is produced as artifact

4. **Full Matrix Test**
   - Run complete matrix on feature branch
   - Verify all builds complete or fail appropriately
   - Document any failing combinations

5. **Release Workflow Test**
   - Create draft release on feature branch
   - Verify DLLs are attached to release
   - Verify checksums are correct

**DLL Verification Tests (Manual on Windows):**

6. **Extension Loading Test**
   ```powershell
   php -d extension=php_firebird.dll -m | Select-String firebird
   ```

7. **Connection Test** (requires Firebird server)
   ```php
   php -d extension=php_firebird.dll -r "var_dump(fbird_connect('localhost:test.fdb'));"
   ```

**CI/CD Integration Tests:**

8. **Artifact Naming Convention**
   - Verify DLLs follow: `php_firebird-{ext-ver}-{php-ver}-{ts|nts}-{arch}-fb{fb-ver}.dll`

9. **Checksum Verification**
   - Verify SHA256SUMS.txt matches all DLL files

[Implementation Order]
Sequential implementation to minimize risk and enable incremental validation.

1. **Step 1: Create PowerShell SDK download script**
   - File: `.github/scripts/download-firebird-sdk.ps1`
   - Test locally on Windows or with PowerShell Core
   - Verify downloads for all 3 Firebird versions
   - Verify extraction and environment setup

2. **Step 2: Create basic Windows build workflow**
   - File: `.github/workflows/windows.yml`
   - Start with single PHP version (8.4), single FB version (5.0)
   - Verify php-windows-builder integration works
   - Test Firebird SDK injection into build

3. **Step 3: Expand to full matrix**
   - Add PHP 8.1, 8.2, 8.3 to matrix
   - Add Firebird 3.0, 4.0 to matrix
   - Add x86 architecture
   - Add TS (thread-safe) builds
   - Document any incompatible combinations

4. **Step 4: Create release workflow**
   - File: `.github/workflows/release-windows.yml`
   - Trigger on release creation
   - Collect all artifacts from build workflow
   - Generate checksums
   - Upload to release

5. **Step 5: Create Windows installation documentation**
   - File: `docs/WINDOWS_INSTALLATION.md`
   - DLL selection guide (PHP version, TS/NTS, architecture, FB version)
   - php.ini configuration
   - Firebird client runtime requirements
   - Troubleshooting

6. **Step 6: Update README with Windows section**
   - Add Windows installation overview
   - Link to releases page
   - Link to detailed documentation

7. **Step 7: Test release process**
   - Create draft release (tag: v7.0.0-rc.8-test)
   - Verify DLLs are built and attached
   - Verify checksums
   - Delete test release

8. **Step 8: Update GitHub Issue #24**
   - Document completion
   - Reference release with Windows DLLs
   - Close issue

---

## Appendix: Workflow YAML Templates

### A1: Main Windows Build Workflow

```yaml
name: Windows DLL Build

on:
  push:
    branches: [master, main]
  pull_request:
  workflow_dispatch:
    inputs:
      php-versions:
        description: 'PHP versions (comma-separated)'
        default: '8.4'
      firebird-versions:
        description: 'Firebird versions (comma-separated)'
        default: '5.0'

permissions:
  contents: read

env:
  FIREBIRD_SDK_CACHE_KEY: firebird-sdk-v1

jobs:
  get-matrix:
    runs-on: ubuntu-latest
    outputs:
      matrix: ${{ steps.set-matrix.outputs.matrix }}
    steps:
      - uses: actions/checkout@v4
      - id: set-matrix
        run: |
          # Generate matrix JSON
          # PHP: 8.1, 8.2, 8.3, 8.4
          # Arch: x64 (x86 optional - FB SDK availability)
          # TS: nts, ts
          # FB: 3.0, 4.0, 5.0
          matrix='{"include":['
          for php in "8.1" "8.2" "8.3" "8.4"; do
            for ts in "nts" "ts"; do
              for fb in "3.0" "4.0" "5.0"; do
                matrix+='{"php-version":"'$php'","arch":"x64","ts":"'$ts'","firebird-version":"'$fb'","os":"windows-2022"},'
              done
            done
          done
          matrix="${matrix%,}]}"
          echo "matrix=$matrix" >> $GITHUB_OUTPUT

  build:
    needs: get-matrix
    runs-on: ${{ matrix.os }}
    strategy:
      fail-fast: false
      matrix: ${{ fromJson(needs.get-matrix.outputs.matrix) }}
    
    steps:
      - uses: actions/checkout@v4
      
      - name: Cache Firebird SDK
        uses: actions/cache@v4
        id: cache-fb-sdk
        with:
          path: C:\firebird-sdk
          key: ${{ env.FIREBIRD_SDK_CACHE_KEY }}-${{ matrix.firebird-version }}-${{ matrix.arch }}
      
      - name: Download Firebird SDK
        if: steps.cache-fb-sdk.outputs.cache-hit != 'true'
        shell: pwsh
        run: |
          .\.github\scripts\download-firebird-sdk.ps1 `
            -Version "${{ matrix.firebird-version }}" `
            -Architecture "${{ matrix.arch }}" `
            -DestinationPath "C:\firebird-sdk"
      
      - name: Set Firebird Environment
        shell: pwsh
        run: |
          $sdkPath = "C:\firebird-sdk"
          echo "FIREBIRD_HOME=$sdkPath" >> $env:GITHUB_ENV
          echo "$sdkPath\bin" >> $env:GITHUB_PATH
          echo "CFLAGS=-I$sdkPath\include" >> $env:GITHUB_ENV
          echo "LDFLAGS=-L$sdkPath\lib" >> $env:GITHUB_ENV
      
      - name: Build Extension
        uses: php/php-windows-builder/extension@v1
        with:
          php-version: ${{ matrix.php-version }}
          arch: ${{ matrix.arch }}
          ts: ${{ matrix.ts }}
          args: --with-firebird=${{ env.FIREBIRD_HOME }}
      
      - name: Rename DLL with metadata
        shell: pwsh
        run: |
          $dllName = "php_firebird-${{ github.ref_name }}-${{ matrix.php-version }}-${{ matrix.ts }}-${{ matrix.arch }}-fb${{ matrix.firebird-version }}.dll"
          # Find and rename the built DLL
          Get-ChildItem -Path artifacts -Filter "*.dll" | 
            Rename-Item -NewName $dllName
      
      - name: Upload Artifact
        uses: actions/upload-artifact@v4
        with:
          name: php-firebird-${{ matrix.php-version }}-${{ matrix.ts }}-${{ matrix.arch }}-fb${{ matrix.firebird-version }}
          path: artifacts/*.dll
```

### A2: Release Workflow

```yaml
name: Release Windows DLLs

on:
  release:
    types: [created]
  workflow_dispatch:
    inputs:
      tag:
        description: 'Release tag to build for'
        required: true

permissions:
  contents: write

jobs:
  # ... (matrix generation and build jobs similar to main workflow)
  
  release:
    needs: build
    runs-on: ubuntu-latest
    steps:
      - name: Download all artifacts
        uses: actions/download-artifact@v4
        with:
          path: release-artifacts
          merge-multiple: true
      
      - name: Generate checksums
        run: |
          cd release-artifacts
          sha256sum *.dll > SHA256SUMS.txt
      
      - name: Upload to Release
        uses: php/php-windows-builder/release@v1
        with:
          release: ${{ github.event.release.tag_name || inputs.tag }}
          token: ${{ secrets.GITHUB_TOKEN }}
```

### A3: PowerShell SDK Download Script

```powershell
# .github/scripts/download-firebird-sdk.ps1
param(
    [Parameter(Mandatory=$true)]
    [ValidateSet("3.0", "4.0", "5.0")]
    [string]$Version,
    
    [Parameter(Mandatory=$true)]
    [ValidateSet("x64", "x86")]
    [string]$Architecture,
    
    [Parameter(Mandatory=$true)]
    [string]$DestinationPath
)

$ErrorActionPreference = "Stop"

# Define download URLs for each version
$downloads = @{
    "3.0" = @{
        "x64" = "https://github.com/FirebirdSQL/firebird/releases/download/v3.0.12/Firebird-3.0.12.33787-0-x64.zip"
        "x86" = "https://github.com/FirebirdSQL/firebird/releases/download/v3.0.12/Firebird-3.0.12.33787-0-win32.zip"
    }
    "4.0" = @{
        "x64" = "https://github.com/FirebirdSQL/firebird/releases/download/v4.0.6/Firebird-4.0.6.3602-0-x64.zip"
        "x86" = $null  # FB 4.0 x86 not available
    }
    "5.0" = @{
        "x64" = "https://github.com/FirebirdSQL/firebird/releases/download/v5.0.3/Firebird-5.0.3.1683-0-windows-x64.exe"
        "x86" = $null  # FB 5.0 x86 not available
    }
}

$url = $downloads[$Version][$Architecture]
if (-not $url) {
    throw "Firebird $Version is not available for $Architecture"
}

# Create destination directory
New-Item -ItemType Directory -Force -Path $DestinationPath | Out-Null

$tempFile = Join-Path $env:TEMP "firebird-sdk-$Version-$Architecture.download"

Write-Host "Downloading Firebird $Version SDK ($Architecture)..."
Invoke-WebRequest -Uri $url -OutFile $tempFile -UseBasicParsing

# Extract based on file type
if ($url.EndsWith(".exe")) {
    # Inno Setup installer - use 7-zip to extract
    Write-Host "Extracting from Inno Setup installer..."
    & 7z x $tempFile -o"$DestinationPath\extracted" -y
    
    # Copy SDK files to standard locations
    Copy-Item "$DestinationPath\extracted\sdk\*" -Destination $DestinationPath -Recurse -Force
    Copy-Item "$DestinationPath\extracted\fbclient.dll" -Destination "$DestinationPath\bin\" -Force
} else {
    # ZIP file
    Write-Host "Extracting ZIP archive..."
    Expand-Archive -Path $tempFile -DestinationPath "$DestinationPath\extracted" -Force
    
    # Find and copy SDK structure
    $extractedDir = Get-ChildItem "$DestinationPath\extracted" -Directory | Select-Object -First 1
    Copy-Item "$extractedDir\*" -Destination $DestinationPath -Recurse -Force
}

# Clean up
Remove-Item $tempFile -Force
Remove-Item "$DestinationPath\extracted" -Recurse -Force -ErrorAction SilentlyContinue

# Verify required files exist
$requiredFiles = @(
    "include\ibase.h",
    "lib\fbclient_ms.lib"
)

foreach ($file in $requiredFiles) {
    $path = Join-Path $DestinationPath $file
    if (-not (Test-Path $path)) {
        throw "Required file not found: $path"
    }
}

Write-Host "Firebird $Version SDK installed successfully to $DestinationPath"
Write-Host "Contents:"
Get-ChildItem $DestinationPath -Recurse | ForEach-Object { Write-Host "  $($_.FullName)" }
```

---

## Appendix: DLL Naming Convention

**Format:** `php_firebird-{ext-ver}-php{php-ver}-{ts}-{arch}-fb{fb-ver}.dll`

**Examples:**
- `php_firebird-7.0.0-php8.4-nts-x64-fb5.0.dll`
- `php_firebird-7.0.0-php8.3-ts-x64-fb4.0.dll`
- `php_firebird-7.0.0-php8.2-nts-x86-fb3.0.dll`

**Matrix Output (24 DLLs per release):**

| PHP | TS/NTS | Arch | Firebird | DLL Name |
|-----|--------|------|----------|----------|
| 8.1 | NTS | x64 | 3.0 | php_firebird-7.0.0-php8.1-nts-x64-fb3.0.dll |
| 8.1 | NTS | x64 | 4.0 | php_firebird-7.0.0-php8.1-nts-x64-fb4.0.dll |
| 8.1 | NTS | x64 | 5.0 | php_firebird-7.0.0-php8.1-nts-x64-fb5.0.dll |
| 8.1 | TS | x64 | 3.0 | php_firebird-7.0.0-php8.1-ts-x64-fb3.0.dll |
| ... | ... | ... | ... | ... |
| 8.4 | TS | x64 | 5.0 | php_firebird-7.0.0-php8.4-ts-x64-fb5.0.dll |

---

## Appendix: GitHub Issue #24 Reference

**Issue URL:** https://github.com/satwareAG/php-firebird/issues/24
**Title:** Build Windows DLLs with GitHub Actions using php-windows-builder
**Status:** OPEN

This implementation plan directly addresses all action items from the issue.
