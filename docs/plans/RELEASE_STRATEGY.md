# PHP Firebird Extension - Release Strategy (2025)

> **Status**: COMPLETED 2026-03-30 — Release infrastructure operational for v10.0.1.
> **Note (2026-04-02)**: Version examples throughout this document use v7.x for historical reasons.
> Current release series is v10.6.x. The distribution patterns, naming conventions, and CI workflows
> described here are still valid; substitute v10.x version strings where v7.x appears.

## Overview

This document defines the distribution and release strategy for php-firebird,
aligned with 2025 best practices for PHP extensions and open-source projects.

**Document Version**: 1.0.0  
**Last Updated**: 2025-12-31  
**Status**: Approved

---

## Table of Contents

1. [Distribution Tiers](#1-distribution-tiers)
2. [User Personas](#2-user-personas)
3. [Linux Distribution Strategy](#3-linux-distribution-strategy)
4. [Windows Distribution Strategy](#4-windows-distribution-strategy)
5. [GitHub Releases](#5-github-releases)
6. [Version Matrix](#6-version-matrix)
7. [Auto-Update Strategies](#7-auto-update-strategies)
8. [CI/CD Automation](#8-cicd-automation)
9. [Security & Signing](#9-security--signing)
10. [Timeline & Milestones](#10-timeline--milestones)

---

## 1. Distribution Tiers

### 1.1 Must-Have (MVP for v7.0.0)

| Channel | Description | Priority |
|---------|-------------|----------|
| **GitHub Releases** | Precompiled binaries with checksums | P0 |
| **Source Distribution** | Tagged releases with build instructions | P0 |
| **manylinux Bundles** | Self-contained Linux x86_64 packages | P0 |
| **Windows DLLs** | Precompiled for PHP 8.2-8.4 (NTS/TS) | P0 |
| **SHA256 Checksums** | Every binary artifact | P0 |
| **Installation Docs** | Clear per-platform guides | P0 |

### 1.2 Should-Have (v7.0.x - v7.1.0)

| Channel | Description | Priority |
|---------|-------------|----------|
| **PECL Registration** | `pecl install firebird` | P1 |
| **GPG Signatures** | Signed releases for verification | P1 |
| **Linux ARM64** | manylinux aarch64 builds | P1 |
| **PHP 8.5 Support** | Day-1 support when stable | P1 |
| **SBOM** | CycloneDX software bill of materials | P1 |

### 1.3 Nice-to-Have (v7.2.0+)

| Channel | Description | Priority |
|---------|-------------|----------|
| **PIE Support** | PHP Installer for Extensions (emerging) | P2 |
| **Chocolatey Package** | Windows package manager | P2 |
| **Homebrew Formula** | macOS package manager | P2 |
| **Distro Packages** | Native .deb/.rpm packages | P2 |
| **Container Images** | php-firebird Docker images | P2 |

---

## 2. User Personas

### 2.1 Windows Server Admin (Corporate)

**Profile**: System administrator managing Windows Server 2022 with IIS/PHP
for enterprise applications. Needs reliable, documented installation process.

**Pain Points**:
- Manual DLL management
- Version matching complexity (PHP version, TS/NTS, architecture, Firebird version)
- No native package manager support for PHP extensions

**Solution**:
1. Clear naming convention: `php_firebird-{ver}-php{php}-{ts|nts}-{arch}-fb{fb}.dll`
2. Detection script to identify correct DLL variant
3. Step-by-step installation guide with verification
4. Bundled Firebird client DLL

### 2.2 Linux Developer (Arch/Rolling)

**Profile**: Developer on Arch Linux or similar rolling-release distro wanting
fast installation for development. Comfortable with CLI, expects modern tooling.

**Pain Points**:
- AUR packages often outdated
- PECL compilation requires dev dependencies
- Wants fast iteration, not compiling from source

**Solution**:
1. manylinux precompiled bundles (single tar.gz, extract and go)
2. `$ORIGIN` rpath patching for zero-dependency installation
3. Optional one-liner install script
4. Clear verification commands

### 2.3 Debian Server Admin (Production)

**Profile**: Operations engineer managing Debian/Ubuntu LTS servers in production.
Prioritizes stability, security, and centralized package management.

**Pain Points**:
- Wants apt integration for updates
- Needs to audit all installed software
- Security-focused, wants signed packages

**Solution**:
1. **Phase 1**: GitHub Releases with SHA256/GPG signatures
2. **Phase 2**: PECL registration for `pecl install firebird`
3. **Phase 3**: Consider PPA or native .deb packages (v7.2.0+)
4. Documented manual update procedure with checksums

---

## 3. Linux Distribution Strategy

### 3.1 manylinux Bundles (Primary)

**Target**: All Linux distributions with glibc 2.28+ (2018+)

**Build System**: `docker/manylinux/Dockerfile` using `manylinux_2_28_x86_64`

**Compatibility Matrix**:

| Distribution | Minimum Version | glibc |
|--------------|-----------------|-------|
| Ubuntu | 18.10 / 20.04 LTS | 2.28+ |
| Debian | 10 (Buster) | 2.28 |
| RHEL/CentOS/Alma/Rocky | 8 | 2.28 |
| Fedora | 29 | 2.28 |
| openSUSE Leap | 15.1 | 2.28 |
| Arch Linux | Rolling | 2.28+ |

**Bundle Contents**:
```
php-firebird-7.0.0-php84-nts-linux-x86_64/
├── firebird.so          # Extension (rpath: $ORIGIN/lib)
├── lib/
│   ├── libfbclient.so.2 # Firebird client
│   ├── libicuuc.so.70   # ICU (Unicode)
│   ├── libicudata.so.70
│   ├── libicui18n.so.70
│   ├── libtommath.so.1  # Crypto
│   ├── libtomcrypt.so.1
│   └── libre2.so.10     # Regex
├── README.md
├── LICENSE
└── DEPRECATION.md
```

**Installation Methods**:

#### Method 1: System Extension Directory (Recommended for Production)
```bash
# Detect PHP extension directory
EXTDIR=$(php -r 'echo ini_get("extension_dir");')
PHP_VER=$(php -r 'echo PHP_MAJOR_VERSION.".".PHP_MINOR_VERSION;')

# Download and extract
curl -LO https://github.com/satwareAG/php-firebird/releases/download/v7.0.0/php-firebird-7.0.0-php${PHP_VER//./}-nts-linux-x86_64.tar.gz
sudo tar -xzf php-firebird-*.tar.gz -C "$EXTDIR" --strip-components=1

# Enable extension
echo "extension=firebird.so" | sudo tee /etc/php/${PHP_VER}/mods-available/firebird.ini
sudo phpenmod firebird

# Verify
php -m | grep firebird
```

#### Method 2: Local Development (No sudo required)
```bash
# Extract to local directory
mkdir -p ~/php-firebird
tar -xzf php-firebird-*.tar.gz -C ~/php-firebird --strip-components=1

# Use directly (rpath handles library loading)
php -d "extension=$HOME/php-firebird/firebird.so" script.php
```

#### Method 3: One-Liner Install Script (Development)
```bash
curl -sSL https://raw.githubusercontent.com/satwareAG/php-firebird/main/scripts/install.sh | bash
```

### 3.2 PECL (Phase 2)

**Timeline**: v7.0.x or v7.1.0

**Benefits**:
- Standard PHP extension installation method
- Automatic compilation for target system
- Integration with system package managers (php-pear)

**Requirements**:
- `package.xml` conforming to PEAR standards
- Registered PECL account
- Build tested on multiple PHP versions

**Installation** (once registered):
```bash
pecl install firebird
```

### 3.3 Source Compilation

**For**: Advanced users, unsupported platforms, custom builds

```bash
git clone https://github.com/satwareAG/php-firebird.git
cd php-firebird
phpize
./configure --with-firebird=/path/to/firebird
make -j$(nproc)
sudo make install
```

---

## 4. Windows Distribution Strategy

### 4.1 DLL Distribution (Primary)

**Challenge**: Windows has no standard PHP extension package manager. Chocolatey
and winget only handle PHP core installation, not extensions.

**Naming Convention**:
```
php_firebird-{version}-php{php_version}-{ts|nts}-{arch}-fb{fb_version}.dll

Examples:
php_firebird-7.0.0-php8.4-ts-x64-fb5.0.dll   # PHP 8.4 Thread Safe, Firebird 5.0
php_firebird-7.0.0-php8.3-nts-x64-fb5.0.dll  # PHP 8.3 Non-Thread Safe
```

**Build Matrix** (v7.0.0):

| PHP Version | Thread Safety | Architecture | Firebird Client |
|-------------|---------------|--------------|-----------------|
| 8.2 | NTS, TS | x64 | 5.0 |
| 8.3 | NTS, TS | x64 | 5.0 |
| 8.4 | NTS, TS | x64 | 5.0 |

**Note**: Firebird 5.x client is backward compatible with FB 2.5-4.x servers
via wire protocol negotiation. Single client version simplifies distribution.

### 4.2 Installation Guide

**Step 1: Determine Your Configuration**

```cmd
REM Check PHP version
php -v

REM Check thread safety (Thread Safe = Yes → use ts)
php -i | findstr "Thread Safety"

REM Check architecture (x64 or x86)
php -i | findstr "Architecture"
```

**Step 2: Download Correct DLL**

Download from [GitHub Releases](https://github.com/satwareAG/php-firebird/releases)

**Step 3: Install Extension**

```cmd
REM Rename to standard name
ren php_firebird-7.0.0-php8.4-nts-x64-fb5.0.dll php_firebird.dll

REM Copy to extension directory (typically C:\php\ext)
copy php_firebird.dll C:\php\ext\

REM Edit php.ini and add:
REM extension=php_firebird
```

**Step 4: Install Firebird Client**

```cmd
REM Download Firebird Windows ZIP kit (not installer)
REM Extract fbclient.dll to PHP root directory (where php.exe is)
copy fbclient.dll C:\php\
```

**Step 5: Verify**

```cmd
php -m | findstr firebird
php -ri firebird
```

### 4.3 Detection Script (PowerShell)

```powershell
# detect-php-firebird-dll.ps1
# Outputs the correct DLL filename for your PHP installation

$phpInfo = php -i 2>$null
$phpVersion = (php -r "echo PHP_MAJOR_VERSION.'.'.PHP_MINOR_VERSION;")
$threadSafe = if ($phpInfo -match "Thread Safety => enabled") { "ts" } else { "nts" }
$arch = if ($phpInfo -match "Architecture => x64") { "x64" } else { "x86" }

$dllName = "php_firebird-7.0.0-php$phpVersion-$threadSafe-$arch-fb5.0.dll"
Write-Host "Download: $dllName"
Write-Host "URL: https://github.com/satwareAG/php-firebird/releases/download/v7.0.0/$dllName"
```

### 4.4 Future: Chocolatey Package (v7.2.0+)

**Consideration**: Create a Chocolatey package that:
1. Detects PHP installation and version
2. Downloads correct DLL variant
3. Installs to extension directory
4. Updates php.ini

**Complexity**: High (PHP detection, multiple variants, ini management)

---

## 5. GitHub Releases

### 5.1 Release Structure

```
v7.0.0/
├── Source code (zip)
├── Source code (tar.gz)
│
├── # Linux manylinux bundles
├── php-firebird-7.0.0-php81-nts-linux-x86_64.tar.gz
├── php-firebird-7.0.0-php81-nts-linux-x86_64.tar.gz.sha256
├── php-firebird-7.0.0-php82-nts-linux-x86_64.tar.gz
├── php-firebird-7.0.0-php82-nts-linux-x86_64.tar.gz.sha256
├── php-firebird-7.0.0-php83-nts-linux-x86_64.tar.gz
├── php-firebird-7.0.0-php83-nts-linux-x86_64.tar.gz.sha256
├── php-firebird-7.0.0-php84-nts-linux-x86_64.tar.gz
├── php-firebird-7.0.0-php84-nts-linux-x86_64.tar.gz.sha256
├── php-firebird-7.0.0-php85-nts-linux-x86_64.tar.gz
├── php-firebird-7.0.0-php85-nts-linux-x86_64.tar.gz.sha256
│
├── # Windows DLLs
├── php_firebird-7.0.0-php8.2-nts-x64-fb5.0.dll
├── php_firebird-7.0.0-php8.2-ts-x64-fb5.0.dll
├── php_firebird-7.0.0-php8.3-nts-x64-fb5.0.dll
├── php_firebird-7.0.0-php8.3-ts-x64-fb5.0.dll
├── php_firebird-7.0.0-php8.4-nts-x64-fb5.0.dll
├── php_firebird-7.0.0-php8.4-ts-x64-fb5.0.dll
│
├── # Checksums
├── SHA256SUMS.txt          # All checksums in one file
├── SHA256SUMS.txt.asc      # GPG signature (Phase 2)
│
└── # Documentation
    └── CHANGELOG.md
```

### 5.2 Release Notes Template

```markdown
## php-firebird v7.0.0

### Highlights
- Modern Firebird 5.x client bundled (backward compatible with FB 2.5-4.x)
- PHP 8.1-8.5 support
- Self-contained manylinux bundles (no system dependencies)

### Downloads

#### Linux (x86_64, glibc 2.28+)
| PHP | Download | SHA256 |
|-----|----------|--------|
| 8.2 | [tar.gz](url) | `abc123...` |
| 8.3 | [tar.gz](url) | `def456...` |
| 8.4 | [tar.gz](url) | `ghi789...` |
| 8.5 | [tar.gz](url) | `jkl012...` |

#### Windows (x64)
| PHP | Thread Safe | Download |
|-----|-------------|----------|
| 8.2 | NTS | [DLL](url) |
| 8.2 | TS | [DLL](url) |
| 8.3 | NTS | [DLL](url) |
| 8.3 | TS | [DLL](url) |
| 8.4 | NTS | [DLL](url) |
| 8.4 | TS | [DLL](url) |

### Verification
```bash
sha256sum -c SHA256SUMS.txt
```

### Changelog
See [CHANGELOG.md](CHANGELOG.md)

### Deprecation Notices
- ⚠️ PHP 8.1 support deprecated (EOL Nov 2025)
- ⚠️ Firebird 2.5 server support deprecated (EOL Sep 2020)
```

---

## 6. Version Matrix

### 6.1 PHP Versions

| PHP Version | php-firebird 7.0.x | Status | EOL |
|-------------|-------------------|--------|-----|
| 8.1 | ⚠️ Deprecated | Security fixes only | Nov 2025 |
| 8.2 | ✅ Supported | Active | Dec 2026 |
| 8.3 | ✅ Supported | Active | Nov 2027 |
| 8.4 | ✅ Supported | Active | Nov 2028 |
| 8.5 | ✅ Supported | Development | Nov 2029 |

### 6.2 Firebird Server Compatibility

| Server Version | Wire Protocol | Status |
|----------------|---------------|--------|
| 5.0 | 18-19 | ✅ Full support |
| 4.0 | 16-17 | ✅ Full support |
| 3.0 | 13-15 | ✅ Full support |
| 2.5 | 12 | ⚠️ Deprecated |

**Note**: Bundled Firebird 5.x client automatically negotiates down to server's
maximum supported protocol version.

### 6.3 Platform Support

| Platform | Architecture | Build | Status |
|----------|--------------|-------|--------|
| Linux | x86_64 | manylinux_2_28 | ✅ Primary |
| Linux | aarch64 | manylinux_2_28 | 🔜 v7.1.0 |
| Windows | x64 | VS2022 | ✅ Primary |
| Windows | x86 | VS2022 | ❌ Dropped |
| macOS | x86_64 | Homebrew | 🔜 v7.2.0 |
| macOS | arm64 | Homebrew | 🔜 v7.2.0 |

---

## 7. Auto-Update Strategies

### 7.1 Linux

**Challenge**: No centralized update mechanism for PHP extensions.

**Options by Persona**:

#### Developer (Arch/Rolling)
```bash
# Manual: Check GitHub releases
curl -s https://api.github.com/repos/satwareAG/php-firebird/releases/latest | jq -r '.tag_name'

# Or use gh CLI
gh release list -R satwareAG/php-firebird --limit 1
```

#### Production (Debian/Ubuntu)
```bash
# Cron job to check for updates (notification only)
*/6 * * * * /usr/local/bin/check-firebird-update.sh

# check-firebird-update.sh
#!/bin/bash
CURRENT="7.0.0"
LATEST=$(curl -s https://api.github.com/repos/satwareAG/php-firebird/releases/latest | jq -r '.tag_name' | tr -d 'v')
if [ "$CURRENT" != "$LATEST" ]; then
    echo "php-firebird update available: $CURRENT -> $LATEST" | mail -s "Update Available" admin@example.com
fi
```

#### PECL (Once Registered)
```bash
# Check for updates
pecl list-upgrades

# Upgrade
pecl upgrade firebird
```

### 7.2 Windows

**Strategy**: Manual with notification

1. **Notification**: Subscribe to GitHub releases (Watch → Releases only)
2. **Manual Update**:
   - Download new DLL
   - Stop web server
   - Replace DLL
   - Start web server

**Future**: PowerShell update script
```powershell
# update-php-firebird.ps1 (conceptual)
$latest = Invoke-RestMethod "https://api.github.com/repos/satwareAG/php-firebird/releases/latest"
# Download, verify checksum, replace DLL
```

---

## 8. CI/CD Automation

### 8.1 GitHub Actions Workflow

```yaml
# .github/workflows/release.yml
name: Release

on:
  push:
    tags:
      - 'v*'

jobs:
  # Linux manylinux builds
  linux-build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      
      - name: Build manylinux bundles
        run: |
          docker build -t php-firebird-manylinux -f docker/manylinux/Dockerfile .
          docker run --rm -v $(pwd)/dist:/dist php-firebird-manylinux
      
      - name: Generate checksums
        run: |
          cd dist
          sha256sum *.tar.gz > SHA256SUMS.txt
      
      - uses: actions/upload-artifact@v4
        with:
          name: linux-bundles
          path: dist/

  # Windows builds
  windows-build:
    runs-on: windows-latest
    strategy:
      matrix:
        php: ['8.2', '8.3', '8.4']
        ts: ['nts', 'ts']
    steps:
      - uses: actions/checkout@v4
      
      - name: Setup PHP
        uses: shivammathur/setup-php@v2
        with:
          php-version: ${{ matrix.php }}
          extensions: none
          
      - name: Build extension
        run: |
          # Windows build script
          scripts/windows/php-fb-build.bat
      
      - uses: actions/upload-artifact@v4
        with:
          name: windows-${{ matrix.php }}-${{ matrix.ts }}
          path: '*.dll'

  # Create GitHub Release
  release:
    needs: [linux-build, windows-build]
    runs-on: ubuntu-latest
    steps:
      - uses: actions/download-artifact@v4
        with:
          path: artifacts/
      
      - name: Consolidate artifacts
        run: |
          mkdir -p release
          find artifacts -type f \( -name "*.tar.gz" -o -name "*.dll" \) -exec cp {} release/ \;
          cd release
          sha256sum * > SHA256SUMS.txt
      
      - name: Create Release
        uses: softprops/action-gh-release@v2
        with:
          files: release/*
          generate_release_notes: true
          draft: true  # Manual review before publishing
```

### 8.2 Release Checklist

```markdown
## Release Checklist for v7.x.x

### Pre-Release
- [ ] All tests passing on CI
- [ ] CHANGELOG.md updated
- [ ] Version bumped in php_firebird.h
- [ ] Documentation updated
- [ ] Deprecation notices added (if applicable)

### Build Verification
- [ ] Linux bundles built for all PHP versions
- [ ] Windows DLLs built for all variants
- [ ] All checksums generated
- [ ] Manual smoke test on Linux
- [ ] Manual smoke test on Windows

### Release
- [ ] Create annotated tag: `git tag -a v7.x.x -m "Release v7.x.x"`
- [ ] Push tag: `git push origin v7.x.x`
- [ ] Verify GitHub Actions workflow completes
- [ ] Review draft release
- [ ] Publish release

### Post-Release
- [ ] Announce on project discussions/wiki
- [ ] Update PECL package (if registered)
- [ ] Monitor issues for critical bugs
```

---

## 9. Security & Signing

### 9.1 Checksums (Required)

Every binary artifact MUST have an accompanying SHA256 checksum.

```bash
# Generate
sha256sum php-firebird-*.tar.gz > SHA256SUMS.txt

# Verify
sha256sum -c SHA256SUMS.txt
```

### 9.2 GPG Signatures (Phase 2)

**Key Management**:
- Generate dedicated release signing key
- Publish public key on GitHub and keyservers
- Document verification process

```bash
# Sign checksums file
gpg --armor --detach-sign SHA256SUMS.txt

# Verify
gpg --verify SHA256SUMS.txt.asc SHA256SUMS.txt
```

### 9.3 SBOM (Phase 2)

Generate CycloneDX SBOM for supply chain transparency:

```bash
# Using Trivy
trivy sbom --format cyclonedx --output sbom.json .
```

---

## 10. Timeline & Milestones

### 10.1 v7.0.0 (Q1 2025)

**Must-Have**:
- [x] manylinux_2_28 build system
- [x] PHP 8.1-8.5 support
- [ ] Windows DLL builds
- [ ] GitHub Release automation
- [ ] SHA256 checksums
- [ ] Installation documentation

### 10.2 v7.0.x Patch Releases

**Should-Have**:
- [ ] GPG signatures
- [ ] PECL registration
- [ ] Linux ARM64 builds

### 10.3 v7.1.0 (Q3 2025)

**Changes**:
- Remove PHP 8.1 support (EOL Nov 2025)
- Remove Firebird 2.5 server support (deprecated)
- Add PHP 8.5 stable support

### 10.4 v7.2.0 (Q1 2026)

**Nice-to-Have**:
- macOS support (Homebrew)
- PIE support (if mature)
- Consider native distro packages

---

## Appendix A: One-Liner Install Script

```bash
#!/bin/bash
# install.sh - Quick installer for php-firebird on Linux

set -euo pipefail

VERSION="${1:-latest}"
PHP_VER=$(php -r 'echo PHP_MAJOR_VERSION.PHP_MINOR_VERSION;')
ARCH=$(uname -m)

if [ "$ARCH" != "x86_64" ]; then
    echo "Error: Only x86_64 architecture is currently supported"
    exit 1
fi

# Resolve latest version
if [ "$VERSION" = "latest" ]; then
    VERSION=$(curl -s https://api.github.com/repos/satwareAG/php-firebird/releases/latest | grep -oP '"tag_name": "\Kv[^"]+')
fi

BUNDLE="php-firebird-${VERSION#v}-php${PHP_VER}-nts-linux-x86_64"
URL="https://github.com/satwareAG/php-firebird/releases/download/${VERSION}/${BUNDLE}.tar.gz"

echo "Installing php-firebird ${VERSION} for PHP ${PHP_VER}..."

# Download and verify
curl -LO "$URL"
curl -LO "${URL}.sha256"
sha256sum -c "${BUNDLE}.tar.gz.sha256"

# Extract
mkdir -p ~/php-firebird
tar -xzf "${BUNDLE}.tar.gz" -C ~/php-firebird --strip-components=1
rm -f "${BUNDLE}.tar.gz" "${BUNDLE}.tar.gz.sha256"

echo ""
echo "Installed to ~/php-firebird/"
echo ""
echo "Test with:"
echo "  php -d 'extension=\$HOME/php-firebird/firebird.so' -m | grep firebird"
echo ""
echo "For system-wide installation, run:"
echo "  sudo cp ~/php-firebird/firebird.so \$(php -r 'echo ini_get(\"extension_dir\");')/"
echo "  sudo cp -r ~/php-firebird/lib/* /usr/local/lib/"
echo "  echo 'extension=firebird.so' | sudo tee /etc/php/${PHP_VER:0:1}.${PHP_VER:1}/mods-available/firebird.ini"
echo "  sudo phpenmod firebird"
```

---

## Appendix B: References

- [PECL Package Guidelines](https://pecl.php.net/package-guidelines.php)
- [manylinux Policy](https://github.com/pypa/manylinux)
- [Firebird Wire Protocol](https://github.com/FirebirdSQL/firebird/blob/master/doc/README.wire_protocol)
- [PHP Extension Distribution Best Practices](https://wiki.php.net/rfc)
- [PIE - PHP Installer for Extensions](https://github.com/php/pie)
