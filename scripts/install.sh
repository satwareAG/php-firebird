#!/bin/bash
# =============================================================================
# PHP Firebird Extension - Quick Installer for Linux
# =============================================================================
#
# Downloads and installs the php-firebird extension from GitHub Releases.
# Self-contained manylinux bundles with bundled Firebird client libraries.
#
# Usage:
#   curl -sSL https://raw.githubusercontent.com/satwareAG/php-firebird/main/scripts/install.sh | bash
#   curl -sSL .../install.sh | bash -s -- v7.0.0           # Specific version
#   curl -sSL .../install.sh | bash -s -- --system         # System-wide install
#   ./install.sh [VERSION] [--system] [--verify-only]
#
# Options:
#   VERSION       Specific version (e.g., v7.0.0) or "latest" [default: latest]
#   --system      Install system-wide (requires sudo)
#   --verify-only Only verify existing installation
#   --help        Show this help message
#
# Requirements:
#   - Linux x86_64 with glibc 2.28+ (Ubuntu 18.10+, Debian 10+, RHEL 8+)
#   - PHP 8.1+ CLI installed
#   - curl and tar
#
# =============================================================================

set -euo pipefail

# =============================================================================
# Configuration
# =============================================================================

REPO="satwareAG/php-firebird"
VERSION="${1:-latest}"
SYSTEM_INSTALL=false
VERIFY_ONLY=false
INSTALL_DIR="${HOME}/php-firebird"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# =============================================================================
# Helper Functions
# =============================================================================

log_info() {
    echo -e "${GREEN}>>>${NC} $*"
}

log_warn() {
    echo -e "${YELLOW}WARNING:${NC} $*"
}

log_error() {
    echo -e "${RED}ERROR:${NC} $*" >&2
}

show_help() {
    sed -n '3,22p' "$0" | sed 's/^# //' | sed 's/^#//'
    exit 0
}

check_command() {
    if ! command -v "$1" &>/dev/null; then
        log_error "Required command not found: $1"
        exit 1
    fi
}

# =============================================================================
# Argument Parsing
# =============================================================================

while [[ $# -gt 0 ]]; do
    case $1 in
        --system)
            SYSTEM_INSTALL=true
            shift
            ;;
        --verify-only)
            VERIFY_ONLY=true
            shift
            ;;
        --help|-h)
            show_help
            ;;
        -*)
            log_error "Unknown option: $1"
            exit 1
            ;;
        *)
            VERSION="$1"
            shift
            ;;
    esac
done

# =============================================================================
# Pre-flight Checks
# =============================================================================

log_info "PHP Firebird Extension Installer"
echo ""

# Check architecture
ARCH=$(uname -m)
if [ "$ARCH" != "x86_64" ]; then
    log_error "Unsupported architecture: $ARCH"
    log_error "Only x86_64 (amd64) is currently supported"
    exit 1
fi

# Check glibc version
GLIBC_VERSION=$(ldd --version 2>&1 | head -1 | grep -oP '\d+\.\d+$' || echo "0.0")
GLIBC_MAJOR=$(echo "$GLIBC_VERSION" | cut -d. -f1)
GLIBC_MINOR=$(echo "$GLIBC_VERSION" | cut -d. -f2)

if [ "$GLIBC_MAJOR" -lt 2 ] || { [ "$GLIBC_MAJOR" -eq 2 ] && [ "$GLIBC_MINOR" -lt 28 ]; }; then
    log_error "glibc $GLIBC_VERSION is too old. Minimum required: 2.28"
    log_error "Supported distributions: Ubuntu 18.10+, Debian 10+, RHEL 8+, Fedora 29+"
    exit 1
fi

# Check required commands
check_command php
check_command curl
check_command tar

# Get PHP version
PHP_MAJOR=$(php -r 'echo PHP_MAJOR_VERSION;')
PHP_MINOR=$(php -r 'echo PHP_MINOR_VERSION;')
PHP_VER="${PHP_MAJOR}${PHP_MINOR}"
PHP_VER_DOT="${PHP_MAJOR}.${PHP_MINOR}"

# Validate PHP version
if [ "$PHP_MAJOR" -lt 8 ] || { [ "$PHP_MAJOR" -eq 8 ] && [ "$PHP_MINOR" -lt 1 ]; }; then
    log_error "PHP $PHP_VER_DOT is not supported. Minimum required: 8.1"
    exit 1
fi

# Check thread safety
PHP_ZTS=$(php -r 'echo PHP_ZTS ? "zts" : "nts";')

log_info "Detected Configuration:"
echo "  PHP Version: $PHP_VER_DOT ($PHP_ZTS)"
echo "  Architecture: $ARCH"
echo "  glibc: $GLIBC_VERSION"
echo ""

# =============================================================================
# Verify Only Mode
# =============================================================================

if [ "$VERIFY_ONLY" = true ]; then
    log_info "Verifying existing installation..."
    
    if php -m 2>/dev/null | grep -q "^firebird$"; then
        EXT_VER=$(php -r 'echo phpversion("firebird");' 2>/dev/null || echo "unknown")
        log_info "✓ firebird extension is loaded (version: $EXT_VER)"
        php -ri firebird 2>/dev/null | head -10
        exit 0
    else
        log_error "✗ firebird extension is NOT loaded"
        exit 1
    fi
fi

# =============================================================================
# Resolve Version
# =============================================================================

log_info "Resolving version..."

if [ "$VERSION" = "latest" ]; then
    VERSION=$(curl -sS "https://api.github.com/repos/${REPO}/releases/latest" | \
        grep -oP '"tag_name": "\K[^"]+' || echo "")
    
    if [ -z "$VERSION" ]; then
        log_error "Failed to fetch latest version from GitHub"
        exit 1
    fi
fi

# Ensure version starts with 'v'
if [[ ! "$VERSION" =~ ^v ]]; then
    VERSION="v${VERSION}"
fi

VERSION_NUM="${VERSION#v}"
log_info "Version: $VERSION"

# =============================================================================
# Build Download URL
# =============================================================================

# Bundle naming: php-firebird-{ver}-php{XX}-nts-linux-x86_64.tar.gz
BUNDLE_NAME="php-firebird-${VERSION_NUM}-php${PHP_VER}-${PHP_ZTS}-linux-${ARCH}"
TARBALL="${BUNDLE_NAME}.tar.gz"
CHECKSUM="${TARBALL}.sha256"

BASE_URL="https://github.com/${REPO}/releases/download/${VERSION}"
TARBALL_URL="${BASE_URL}/${TARBALL}"
CHECKSUM_URL="${BASE_URL}/${CHECKSUM}"

log_info "Download URL: $TARBALL_URL"

# =============================================================================
# Download and Verify
# =============================================================================

TEMP_DIR=$(mktemp -d)
trap "rm -rf $TEMP_DIR" EXIT

cd "$TEMP_DIR"

log_info "Downloading extension bundle..."
if ! curl -fSL -o "$TARBALL" "$TARBALL_URL"; then
    log_error "Failed to download $TARBALL"
    log_error "Check that version $VERSION exists and has a build for PHP $PHP_VER_DOT"
    exit 1
fi

log_info "Downloading checksum..."
if curl -fSL -o "$CHECKSUM" "$CHECKSUM_URL" 2>/dev/null; then
    log_info "Verifying checksum..."
    if sha256sum -c "$CHECKSUM"; then
        log_info "✓ Checksum verified"
    else
        log_error "Checksum verification failed!"
        exit 1
    fi
else
    log_warn "Checksum file not available, skipping verification"
fi

# =============================================================================
# Install
# =============================================================================

if [ "$SYSTEM_INSTALL" = true ]; then
    # System-wide installation
    log_info "Installing system-wide (requires sudo)..."
    
    EXT_DIR=$(php -r 'echo ini_get("extension_dir");')
    
    # Extract to temp location
    tar -xzf "$TARBALL"
    
    # Copy extension
    sudo cp "${BUNDLE_NAME}/firebird.so" "$EXT_DIR/"
    
    # Copy libraries
    if [ -d "${BUNDLE_NAME}/lib" ]; then
        # Try extension dir parent/lib first, then /usr/local/lib
        LIB_DIR=$(dirname "$EXT_DIR")/lib
        if [ -d "$LIB_DIR" ]; then
            sudo cp -a "${BUNDLE_NAME}/lib/"* "$LIB_DIR/"
        else
            sudo cp -a "${BUNDLE_NAME}/lib/"* /usr/local/lib/
            sudo ldconfig
        fi
    fi
    
    # Enable extension
    INI_DIR="/etc/php/${PHP_VER_DOT}/mods-available"
    if [ -d "$INI_DIR" ]; then
        echo "extension=firebird.so" | sudo tee "$INI_DIR/firebird.ini" > /dev/null
        if command -v phpenmod &>/dev/null; then
            sudo phpenmod firebird
        fi
    else
        # Fallback: add to main php.ini
        PHP_INI=$(php -r 'echo php_ini_loaded_file();')
        if [ -n "$PHP_INI" ] && [ -f "$PHP_INI" ]; then
            if ! grep -q "extension=firebird" "$PHP_INI"; then
                echo "extension=firebird.so" | sudo tee -a "$PHP_INI" > /dev/null
            fi
        fi
    fi
    
    log_info "✓ Installed to $EXT_DIR"
else
    # Local installation (no sudo)
    log_info "Installing to $INSTALL_DIR..."
    
    rm -rf "$INSTALL_DIR"
    mkdir -p "$INSTALL_DIR"
    
    tar -xzf "$TARBALL" -C "$INSTALL_DIR" --strip-components=1
    
    log_info "✓ Installed to $INSTALL_DIR"
fi

# =============================================================================
# Verify Installation
# =============================================================================

echo ""
log_info "Verifying installation..."

if [ "$SYSTEM_INSTALL" = true ]; then
    if php -m | grep -q "^firebird$"; then
        EXT_VER=$(php -r 'echo phpversion("firebird");')
        log_info "✓ Extension loaded successfully (version: $EXT_VER)"
    else
        log_warn "Extension installed but not loaded. You may need to restart PHP-FPM or Apache."
    fi
else
    # Test local installation
    if php -d "extension=${INSTALL_DIR}/firebird.so" -m 2>/dev/null | grep -q "^firebird$"; then
        EXT_VER=$(php -d "extension=${INSTALL_DIR}/firebird.so" -r 'echo phpversion("firebird");')
        log_info "✓ Extension loaded successfully (version: $EXT_VER)"
    else
        log_error "Failed to load extension"
        exit 1
    fi
fi

# =============================================================================
# Usage Instructions
# =============================================================================

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

if [ "$SYSTEM_INSTALL" = true ]; then
    echo -e "${GREEN}Installation complete!${NC}"
    echo ""
    echo "The extension is now available system-wide."
    echo ""
    echo "Verify with:"
    echo "  php -m | grep firebird"
    echo "  php -ri firebird"
else
    echo -e "${GREEN}Installation complete!${NC}"
    echo ""
    echo "The extension is installed to: $INSTALL_DIR"
    echo ""
    echo "To use in scripts:"
    echo "  php -d \"extension=${INSTALL_DIR}/firebird.so\" your_script.php"
    echo ""
    echo "To make permanent, add to php.ini:"
    echo "  extension=${INSTALL_DIR}/firebird.so"
    echo ""
    echo "For system-wide installation, run:"
    echo "  $0 --system"
fi

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
