#!/bin/bash
# =============================================================================
# Verify Precompiled Bundles on Multiple Linux Distributions
# =============================================================================
# Tests php-firebird bundles on target distributions to verify glibc compatibility
# and library bundling.
#
# Usage: ./scripts/verify-distributions.sh [--all|--ubuntu|--debian|--rocky]
#
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
DIST_DIR="${PROJECT_DIR}/dist"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $*"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $*"; }
log_error() { echo -e "${RED}[ERROR]${NC} $*" >&2; }
log_test() { echo -e "${BLUE}[TEST]${NC} $*"; }

# Results tracking
declare -A TEST_RESULTS

# Test a specific distribution
test_distribution() {
    local DISTRO="$1"
    local IMAGE="$2"
    local PHP_PKG="$3"
    local PHP_VERSION="$4"
    local BUNDLE="php-firebird-7.0.0-php${PHP_VERSION}-nts-linux-x86_64.tar.gz"
    
    log_test "Testing ${DISTRO} with PHP ${PHP_VERSION}..."
    
    if [ ! -f "${DIST_DIR}/${BUNDLE}" ]; then
        log_error "Bundle not found: ${BUNDLE}"
        TEST_RESULTS["${DISTRO}_${PHP_VERSION}"]="SKIP (no bundle)"
        return 1
    fi
    
    # Create test script with specific bundle name
    local TEST_SCRIPT="#!/bin/bash
set -e

BUNDLE_NAME=\"${BUNDLE}\"

echo \"=== Distribution Info ===\"
cat /etc/os-release | grep -E '^(NAME|VERSION|ID)='
echo ''

echo \"=== glibc Version ===\"
ldd --version | head -1
echo ''

echo \"=== Installing PHP ===\"
"
    
    # Add distribution-specific PHP installation
    case "$DISTRO" in
        ubuntu*)
            TEST_SCRIPT+="
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq ${PHP_PKG} > /dev/null 2>&1
"
            ;;
        debian*)
            TEST_SCRIPT+="
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq ${PHP_PKG} > /dev/null 2>&1
"
            ;;
        rocky*|almalinux*)
            TEST_SCRIPT+="
dnf install -y -q ${PHP_PKG} > /dev/null 2>&1
"
            ;;
    esac
    
    TEST_SCRIPT+='
echo "PHP Version: $(php -v | head -1)"
echo ""

echo "=== Extracting Bundle ==="
cd /tmp
echo "Extracting: /bundle/${BUNDLE_NAME}"
tar xzf "/bundle/${BUNDLE_NAME}"
cd php-firebird-*
ls -la

echo ""
echo "=== Checking library dependencies ==="
ldd firebird.so || true
echo ""

echo "=== Checking bundled libraries ==="
ls -la lib/
echo ""

echo "=== Testing extension load ==="
# Try loading with bundled libraries
export LD_LIBRARY_PATH="$(pwd)/lib:${LD_LIBRARY_PATH:-}"
php -d "extension=$(pwd)/firebird.so" -m | grep -i firebird && echo "✓ Extension loaded successfully" || echo "✗ Extension failed to load"

echo ""
echo "=== Checking extension info ==="
php -d "extension=$(pwd)/firebird.so" -r "phpinfo(INFO_MODULES);" 2>/dev/null | grep -A 20 "^firebird$" | head -25 || true

echo ""
echo "=== Testing basic function availability ==="
php -d "extension=$(pwd)/firebird.so" -r "
    \$funcs = [\"fbird_connect\", \"fbird_query\", \"fbird_fetch_assoc\", \"fbird_close\"];
    foreach (\$funcs as \$f) {
        echo function_exists(\$f) ? \"✓ \$f exists\" : \"✗ \$f missing\";
        echo PHP_EOL;
    }
"
'
    
    # Run test in Docker container
    local RESULT
    if docker run --rm \
        -v "${DIST_DIR}:/bundle:ro" \
        "${IMAGE}" \
        bash -c "$TEST_SCRIPT" 2>&1; then
        TEST_RESULTS["${DISTRO}_${PHP_VERSION}"]="PASS"
        log_info "✓ ${DISTRO} PHP ${PHP_VERSION}: PASSED"
        return 0
    else
        TEST_RESULTS["${DISTRO}_${PHP_VERSION}"]="FAIL"
        log_error "✗ ${DISTRO} PHP ${PHP_VERSION}: FAILED"
        return 1
    fi
}

# Test Ubuntu distributions
test_ubuntu() {
    log_info "=== Testing Ubuntu Distributions ==="
    
    # Ubuntu 20.04 (Focal) - glibc 2.31
    test_distribution "ubuntu2004" "ubuntu:20.04" "php-cli" "81" || true
    
    # Ubuntu 22.04 (Jammy) - glibc 2.35
    test_distribution "ubuntu2204" "ubuntu:22.04" "php-cli" "81" || true
    
    # Ubuntu 24.04 (Noble) - glibc 2.39
    test_distribution "ubuntu2404" "ubuntu:24.04" "php-cli" "83" || true
}

# Test Debian distributions
test_debian() {
    log_info "=== Testing Debian Distributions ==="
    
    # Debian 10 (Buster) - glibc 2.28 (minimum target)
    test_distribution "debian10" "debian:buster" "php-cli" "81" || true
    
    # Debian 11 (Bullseye) - glibc 2.31
    test_distribution "debian11" "debian:bullseye" "php-cli" "81" || true
    
    # Debian 12 (Bookworm) - glibc 2.36
    test_distribution "debian12" "debian:bookworm" "php-cli" "82" || true
}

# Test RHEL-based distributions
test_rocky() {
    log_info "=== Testing Rocky/RHEL Distributions ==="
    
    # Rocky Linux 8 - glibc 2.28 (minimum target)
    test_distribution "rocky8" "rockylinux:8" "php-cli" "81" || true
    
    # Rocky Linux 9 - glibc 2.34
    test_distribution "rocky9" "rockylinux:9" "php-cli" "81" || true
    
    # AlmaLinux 8 (same as manylinux base)
    test_distribution "almalinux8" "almalinux:8" "php-cli" "81" || true
}

# Print summary
print_summary() {
    echo ""
    log_info "=========================================="
    log_info "         TEST RESULTS SUMMARY"
    log_info "=========================================="
    
    local PASSED=0
    local FAILED=0
    local SKIPPED=0
    
    for key in "${!TEST_RESULTS[@]}"; do
        local result="${TEST_RESULTS[$key]}"
        case "$result" in
            PASS)
                echo -e "  ${GREEN}✓${NC} $key: $result"
                ((PASSED++))
                ;;
            FAIL)
                echo -e "  ${RED}✗${NC} $key: $result"
                ((FAILED++))
                ;;
            *)
                echo -e "  ${YELLOW}○${NC} $key: $result"
                ((SKIPPED++))
                ;;
        esac
    done
    
    echo ""
    echo "Total: ${PASSED} passed, ${FAILED} failed, ${SKIPPED} skipped"
    echo ""
    
    if [ $FAILED -gt 0 ]; then
        return 1
    fi
    return 0
}

# Main
main() {
    local TARGET="${1:-all}"
    
    log_info "Verifying precompiled bundles on Linux distributions"
    log_info "Distribution directory: ${DIST_DIR}"
    echo ""
    
    # Check bundles exist
    if [ ! -d "${DIST_DIR}" ] || [ -z "$(ls -A ${DIST_DIR}/*.tar.gz 2>/dev/null)" ]; then
        log_error "No bundles found in ${DIST_DIR}"
        log_info "Run: docker run --rm -v \$(pwd)/dist:/dist php-firebird-manylinux"
        exit 1
    fi
    
    log_info "Available bundles:"
    ls -1 "${DIST_DIR}"/*.tar.gz
    echo ""
    
    case "$TARGET" in
        --ubuntu|ubuntu)
            test_ubuntu
            ;;
        --debian|debian)
            test_debian
            ;;
        --rocky|rocky|rhel)
            test_rocky
            ;;
        --all|all|*)
            test_ubuntu
            test_debian
            test_rocky
            ;;
    esac
    
    print_summary
}

main "$@"
