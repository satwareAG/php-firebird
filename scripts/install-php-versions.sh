#!/bin/bash
# =============================================================================
# PHP Firebird Extension - PHP Version Installer for manylinux_2_28
# =============================================================================
#
# Installs multiple PHP versions in the manylinux_2_28 build environment.
# Uses prebuilt PHP binaries from shivammathur/php-builder-source.
#
# Usage:
#   ./scripts/install-php-versions.sh [options] [version...]
#
# Arguments:
#   version...          PHP versions to install (8.1, 8.2, 8.3, 8.4, 8.5)
#                       Default: 8.2 8.3 8.4
#
# Options:
#   --nts               Install NTS (Non-Thread Safe) variant only
#   --zts               Install ZTS (Zend Thread Safe) variant only
#   --both              Install both NTS and ZTS variants (default)
#   --prefix PATH       Installation prefix [default: /opt/php]
#   --switch VERSION    Switch active PHP to VERSION after install
#   --list              List installed PHP versions
#   --help              Show this help message
#
# Environment:
#   PHP_MIRROR          Mirror for PHP source downloads
#   PHP_PREFIX          Installation prefix (overrides --prefix)
#
# Examples:
#   ./scripts/install-php-versions.sh 8.4              # Install PHP 8.4 (NTS+ZTS)
#   ./scripts/install-php-versions.sh --nts 8.3 8.4    # Install 8.3 and 8.4 NTS
#   ./scripts/install-php-versions.sh --switch 8.4    # Switch to PHP 8.4
#   ./scripts/install-php-versions.sh --list           # Show installed versions
#
# =============================================================================

set -euo pipefail

# =============================================================================
# Configuration
# =============================================================================

PHP_VERSIONS=()
INSTALL_NTS=true
INSTALL_ZTS=true
PHP_PREFIX="${PHP_PREFIX:-/opt/php}"
SWITCH_VERSION=""
LIST_ONLY=false

# Version mappings (version -> full release)
declare -A PHP_RELEASES=(
    ["8.1"]="8.1.32"
    ["8.2"]="8.2.28"
    ["8.3"]="8.3.16"
    ["8.4"]="8.4.4"
    ["8.5"]="8.5.0alpha1"
)

# PHP source download URLs
PHP_MUSEUM="https://museum.php.net/php8"
PHP_DOWNLOADS="https://www.php.net/distributions"
PHP_GITHUB="https://github.com/php/php-src/archive/refs/tags"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Build parallelism
NPROC=$(nproc 2>/dev/null || echo 4)

# =============================================================================
# Helper Functions
# =============================================================================

usage() {
    sed -n '3,32p' "$0" | sed 's/^# //' | sed 's/^#//'
    exit 0
}

log_info() {
    echo -e "${BLUE}[INFO]${NC} $*"
}

log_success() {
    echo -e "${GREEN}[OK]${NC} $*"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $*"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $*" >&2
}

check_command() {
    if ! command -v "$1" &>/dev/null; then
        log_error "Required command not found: $1"
        return 1
    fi
    return 0
}

get_full_version() {
    local short="$1"
    echo "${PHP_RELEASES[$short]:-$short}"
}

get_short_version() {
    local full="$1"
    echo "${full%.*}"
}

# =============================================================================
# Argument Parsing
# =============================================================================

while [[ $# -gt 0 ]]; do
    case $1 in
        --nts)
            INSTALL_NTS=true
            INSTALL_ZTS=false
            shift
            ;;
        --zts)
            INSTALL_NTS=false
            INSTALL_ZTS=true
            shift
            ;;
        --both)
            INSTALL_NTS=true
            INSTALL_ZTS=true
            shift
            ;;
        --prefix)
            PHP_PREFIX="$2"
            shift 2
            ;;
        --switch)
            SWITCH_VERSION="$2"
            shift 2
            ;;
        --list)
            LIST_ONLY=true
            shift
            ;;
        --help|-h)
            usage
            ;;
        -*)
            log_error "Unknown option: $1"
            exit 1
            ;;
        *)
            PHP_VERSIONS+=("$1")
            shift
            ;;
    esac
done

# Default versions if none specified
if [ ${#PHP_VERSIONS[@]} -eq 0 ] && [ "$LIST_ONLY" = false ] && [ -z "$SWITCH_VERSION" ]; then
    PHP_VERSIONS=("8.2" "8.3" "8.4")
fi

# =============================================================================
# List Installed Versions
# =============================================================================

list_installed() {
    echo ""
    echo "=== Installed PHP Versions ==="
    echo ""
    
    if [ ! -d "$PHP_PREFIX" ]; then
        log_warn "No PHP installations found at $PHP_PREFIX"
        return
    fi
    
    for dir in "$PHP_PREFIX"/*/; do
        if [ -d "$dir" ]; then
            local version_dir=$(basename "$dir")
            local php_bin="${dir}bin/php"
            
            if [ -x "$php_bin" ]; then
                local full_version=$("$php_bin" -r 'echo PHP_VERSION;' 2>/dev/null || echo "unknown")
                local zts=$("$php_bin" -r 'echo PHP_ZTS ? "ZTS" : "NTS";' 2>/dev/null || echo "?")
                local api=$("$php_bin" -r 'echo PHP_API_VERSION;' 2>/dev/null || echo "?")
                printf "  %-12s  %-8s  %-6s  API: %s\n" "$version_dir" "$full_version" "($zts)" "$api"
            fi
        fi
    done
    
    echo ""
    
    # Show current active PHP
    if command -v php &>/dev/null; then
        local active_version=$(php -r 'echo PHP_VERSION;' 2>/dev/null || echo "unknown")
        local active_zts=$(php -r 'echo PHP_ZTS ? "ZTS" : "NTS";' 2>/dev/null || echo "?")
        local active_path=$(which php 2>/dev/null || echo "unknown")
        echo "Active: PHP $active_version ($active_zts)"
        echo "Path: $active_path"
    else
        echo "Active: None (php not in PATH)"
    fi
    
    echo ""
}

if [ "$LIST_ONLY" = true ]; then
    list_installed
    exit 0
fi

# =============================================================================
# Switch PHP Version
# =============================================================================

switch_php() {
    local version="$1"
    local variant="${2:-nts}"
    
    local php_dir="${PHP_PREFIX}/${version}-${variant}"
    
    if [ ! -d "$php_dir" ]; then
        # Try without variant suffix
        php_dir="${PHP_PREFIX}/${version}"
    fi
    
    if [ ! -d "$php_dir" ]; then
        log_error "PHP $version ($variant) not found at $php_dir"
        log_info "Run: $0 $version --${variant}"
        return 1
    fi
    
    log_info "Switching to PHP $version ($variant)..."
    
    # Update alternatives (if available)
    if command -v update-alternatives &>/dev/null; then
        update-alternatives --install /usr/bin/php php "${php_dir}/bin/php" 100 2>/dev/null || true
        update-alternatives --install /usr/bin/phpize phpize "${php_dir}/bin/phpize" 100 2>/dev/null || true
        update-alternatives --install /usr/bin/php-config php-config "${php_dir}/bin/php-config" 100 2>/dev/null || true
    fi
    
    # Create symlinks in /usr/local/bin
    if [ -d /usr/local/bin ]; then
        ln -sf "${php_dir}/bin/php" /usr/local/bin/php 2>/dev/null || true
        ln -sf "${php_dir}/bin/phpize" /usr/local/bin/phpize 2>/dev/null || true
        ln -sf "${php_dir}/bin/php-config" /usr/local/bin/php-config 2>/dev/null || true
    fi
    
    # Verify
    if php -v &>/dev/null; then
        log_success "Active PHP: $(php -r 'echo PHP_VERSION . " (" . (PHP_ZTS ? "ZTS" : "NTS") . ")";')"
    else
        log_warn "PHP switch completed but php not in PATH"
        log_info "Add to PATH: export PATH=${php_dir}/bin:\$PATH"
    fi
}

if [ -n "$SWITCH_VERSION" ]; then
    # Determine variant based on flags
    local_variant="nts"
    if [ "$INSTALL_ZTS" = true ] && [ "$INSTALL_NTS" = false ]; then
        local_variant="zts"
    fi
    switch_php "$SWITCH_VERSION" "$local_variant"
    exit 0
fi

# =============================================================================
# Install Dependencies
# =============================================================================

install_dependencies() {
    log_info "Installing build dependencies..."
    
    # Detect package manager
    if command -v dnf &>/dev/null; then
        dnf install -y \
            gcc gcc-c++ make autoconf automake bison re2c \
            libxml2-devel sqlite-devel \
            openssl-devel libcurl-devel \
            libpng-devel libjpeg-devel freetype-devel \
            zlib-devel bzip2-devel readline-devel \
            libxslt-devel libzip-devel \
            oniguruma-devel 2>/dev/null || true
    elif command -v yum &>/dev/null; then
        yum install -y \
            gcc gcc-c++ make autoconf automake bison re2c \
            libxml2-devel sqlite-devel \
            openssl-devel libcurl-devel \
            libpng-devel libjpeg-devel freetype-devel \
            zlib-devel bzip2-devel readline-devel \
            libxslt-devel libzip-devel \
            oniguruma-devel 2>/dev/null || true
    elif command -v apt-get &>/dev/null; then
        apt-get update
        apt-get install -y \
            build-essential autoconf bison re2c \
            libxml2-dev libsqlite3-dev \
            libssl-dev libcurl4-openssl-dev \
            libpng-dev libjpeg-dev libfreetype6-dev \
            zlib1g-dev libbz2-dev libreadline-dev \
            libxslt1-dev libzip-dev \
            libonig-dev 2>/dev/null || true
    else
        log_warn "Unknown package manager - dependencies may be missing"
    fi
}

# =============================================================================
# Download PHP Source
# =============================================================================

download_php_source() {
    local version="$1"
    local full_version=$(get_full_version "$version")
    local tarball="php-${full_version}.tar.xz"
    local url=""
    
    # Try different download sources
    local urls=(
        "${PHP_DOWNLOADS}/${tarball}"
        "${PHP_MUSEUM}/${tarball}"
        "${PHP_GITHUB}/php-${full_version}.tar.gz"
    )
    
    for url in "${urls[@]}"; do
        log_info "Trying: $url"
        if curl -fsSL --head "$url" &>/dev/null; then
            curl -fsSL "$url" -o "/tmp/${tarball}" && return 0
        fi
    done
    
    # Try .tar.gz instead of .tar.xz
    tarball="php-${full_version}.tar.gz"
    for url in "${urls[@]}"; do
        url="${url%.xz}.gz"
        log_info "Trying: $url"
        if curl -fsSL --head "$url" &>/dev/null; then
            curl -fsSL "$url" -o "/tmp/${tarball}" && return 0
        fi
    done
    
    log_error "Failed to download PHP $full_version"
    return 1
}

# =============================================================================
# Build PHP
# =============================================================================

build_php() {
    local version="$1"
    local variant="$2"  # nts or zts
    
    local full_version=$(get_full_version "$version")
    local install_dir="${PHP_PREFIX}/${version}-${variant}"
    
    log_info "Building PHP $full_version ($variant)..."
    
    # Skip if already installed
    if [ -x "${install_dir}/bin/php" ]; then
        local installed_version=$("${install_dir}/bin/php" -r 'echo PHP_VERSION;' 2>/dev/null || echo "")
        if [ "$installed_version" = "$full_version" ]; then
            log_success "PHP $full_version ($variant) already installed"
            return 0
        fi
    fi
    
    # Download source
    download_php_source "$version" || return 1
    
    # Extract
    local src_dir="/tmp/php-${full_version}"
    rm -rf "$src_dir"
    
    local tarball="/tmp/php-${full_version}.tar.xz"
    [ ! -f "$tarball" ] && tarball="/tmp/php-${full_version}.tar.gz"
    
    tar -xf "$tarball" -C /tmp
    
    # Handle different archive structures
    if [ ! -d "$src_dir" ]; then
        # GitHub archive has different structure
        mv /tmp/php-src-* "$src_dir" 2>/dev/null || true
    fi
    
    if [ ! -d "$src_dir" ]; then
        log_error "Failed to extract PHP source"
        return 1
    fi
    
    cd "$src_dir"
    
    # Configure options
    local configure_opts=(
        "--prefix=${install_dir}"
        "--enable-cli"
        "--disable-cgi"
        "--disable-phpdbg"
        "--without-pear"
        "--enable-shared"
        "--enable-mbstring"
        "--enable-intl"
        "--enable-opcache"
        "--with-openssl"
        "--with-zlib"
        "--with-curl"
    )
    
    # ZTS-specific options
    if [ "$variant" = "zts" ]; then
        configure_opts+=("--enable-zts")
    fi
    
    # Generate configure script
    if [ ! -f configure ]; then
        log_info "Running buildconf..."
        ./buildconf --force
    fi
    
    # Configure
    log_info "Configuring PHP $full_version ($variant)..."
    ./configure "${configure_opts[@]}" || {
        log_error "Configure failed"
        return 1
    }
    
    # Build
    log_info "Compiling PHP $full_version ($variant)..."
    make -j"$NPROC" || {
        log_error "Build failed"
        return 1
    }
    
    # Install
    log_info "Installing PHP $full_version ($variant) to $install_dir..."
    make install || {
        log_error "Installation failed"
        return 1
    }
    
    # Cleanup
    cd /
    rm -rf "$src_dir" "$tarball"
    
    # Verify installation
    if "${install_dir}/bin/php" -v &>/dev/null; then
        local installed=$("${install_dir}/bin/php" -r 'echo PHP_VERSION . " (" . (PHP_ZTS ? "ZTS" : "NTS") . ")";')
        log_success "Installed: PHP $installed"
    else
        log_error "Installation verification failed"
        return 1
    fi
    
    return 0
}

# =============================================================================
# Main Installation
# =============================================================================

echo ""
echo "=== PHP Version Installer for manylinux ==="
echo ""
echo "Versions to install: ${PHP_VERSIONS[*]}"
echo "Variants: $([ "$INSTALL_NTS" = true ] && echo "NTS ")$([ "$INSTALL_ZTS" = true ] && echo "ZTS")"
echo "Prefix: $PHP_PREFIX"
echo ""

# Check required tools
check_command curl || exit 1
check_command make || exit 1
check_command tar || exit 1

# Install dependencies
install_dependencies

# Create prefix directory
mkdir -p "$PHP_PREFIX"

# Track success/failure
INSTALLED=()
FAILED=()

# Build each version and variant
for version in "${PHP_VERSIONS[@]}"; do
    if [ "$INSTALL_NTS" = true ]; then
        if build_php "$version" "nts"; then
            INSTALLED+=("${version}-nts")
        else
            FAILED+=("${version}-nts")
        fi
    fi
    
    if [ "$INSTALL_ZTS" = true ]; then
        if build_php "$version" "zts"; then
            INSTALLED+=("${version}-zts")
        else
            FAILED+=("${version}-zts")
        fi
    fi
done

# Summary
echo ""
echo "=== Installation Summary ==="
echo ""

if [ ${#INSTALLED[@]} -gt 0 ]; then
    log_success "Successfully installed: ${INSTALLED[*]}"
fi

if [ ${#FAILED[@]} -gt 0 ]; then
    log_error "Failed to install: ${FAILED[*]}"
fi

# Switch to first installed version
if [ ${#INSTALLED[@]} -gt 0 ]; then
    first_installed="${INSTALLED[0]}"
    switch_php "${first_installed%-*}" "${first_installed##*-}"
fi

echo ""
echo "To switch PHP versions:"
echo "  $0 --switch VERSION"
echo ""
echo "Or set PATH manually:"
echo "  export PATH=${PHP_PREFIX}/VERSION-VARIANT/bin:\$PATH"
echo ""

# Exit with error if any failed
[ ${#FAILED[@]} -gt 0 ] && exit 1
exit 0
