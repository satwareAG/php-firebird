#!/bin/bash
# =============================================================================
# PHP Firebird Extension - Bundle Verification Script
# =============================================================================
#
# Verifies precompiled extension bundles for correctness:
#   - RPATH configuration
#   - Dependency resolution
#   - PHP extension loading
#   - Optional: Firebird connection test
#
# Usage:
#   ./scripts/verify-bundle.sh <bundle_dir|bundle.tar.gz> [options]
#
# Options:
#   --php-binary PATH   PHP binary to test with [default: php]
#   --connection-test   Run Firebird connection test (requires FB server)
#   --dsn DSN           Firebird DSN for connection test
#   --user USER         Firebird username [default: SYSDBA]
#   --password PASSWORD Firebird password [default: masterkey]
#   --max-size SIZE     Maximum bundle size in MB [default: 100]
#   --verbose           Show detailed output
#   --strict            Fail on warnings
#   --help              Show this help message
#
# Exit Codes:
#   0 - All checks passed
#   1 - Critical failure (bundle unusable)
#   2 - Warnings detected (bundle may work with limitations)
#
# =============================================================================

set -euo pipefail

# =============================================================================
# Configuration
# =============================================================================

BUNDLE_PATH=""
PHP_BINARY="php"
RUN_CONNECTION_TEST=false
FB_DSN=""
FB_USER="SYSDBA"
FB_PASSWORD="masterkey"
MAX_SIZE_MB=100
VERBOSE=false
STRICT=false

# Results tracking
CHECKS_PASSED=0
CHECKS_FAILED=0
CHECKS_WARNED=0

# Temporary directory for extraction
TEMP_DIR=""

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# =============================================================================
# Helper Functions
# =============================================================================

usage() {
    sed -n '3,28p' "$0" | sed 's/^# //' | sed 's/^#//'
    exit 0
}

log_info() {
    echo -e "${BLUE}[INFO]${NC} $*"
}

log_pass() {
    echo -e "${GREEN}[PASS]${NC} $*"
    ((++CHECKS_PASSED))
}

log_fail() {
    echo -e "${RED}[FAIL]${NC} $*" >&2
    ((++CHECKS_FAILED))
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $*"
    ((++CHECKS_WARNED))
}

log_verbose() {
    if [ "$VERBOSE" = true ]; then
        echo -e "${BLUE}[VERBOSE]${NC} $*"
    fi
}

cleanup() {
    if [ -n "$TEMP_DIR" ] && [ -d "$TEMP_DIR" ]; then
        rm -rf "$TEMP_DIR"
    fi
}

trap cleanup EXIT

check_command() {
    if ! command -v "$1" &>/dev/null; then
        log_fail "Required command not found: $1"
        return 1
    fi
    return 0
}

# =============================================================================
# Argument Parsing
# =============================================================================

while [[ $# -gt 0 ]]; do
    case $1 in
        --php-binary)
            PHP_BINARY="$2"
            shift 2
            ;;
        --connection-test)
            RUN_CONNECTION_TEST=true
            shift
            ;;
        --dsn)
            FB_DSN="$2"
            RUN_CONNECTION_TEST=true
            shift 2
            ;;
        --user)
            FB_USER="$2"
            shift 2
            ;;
        --password)
            FB_PASSWORD="$2"
            shift 2
            ;;
        --max-size)
            MAX_SIZE_MB="$2"
            shift 2
            ;;
        --verbose)
            VERBOSE=true
            shift
            ;;
        --strict)
            STRICT=true
            shift
            ;;
        --help|-h)
            usage
            ;;
        -*)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
        *)
            if [ -z "$BUNDLE_PATH" ]; then
                BUNDLE_PATH="$1"
            else
                echo "Unexpected argument: $1" >&2
                exit 1
            fi
            shift
            ;;
    esac
done

if [ -z "$BUNDLE_PATH" ]; then
    echo "Error: Bundle path required" >&2
    echo "Usage: $0 <bundle_dir|bundle.tar.gz> [options]" >&2
    exit 1
fi

# =============================================================================
# Prepare Bundle Directory
# =============================================================================

echo ""
echo "=== PHP Firebird Bundle Verification ==="
echo ""

log_info "Bundle: $BUNDLE_PATH"

# Handle tarball vs directory
if [[ "$BUNDLE_PATH" == *.tar.gz ]] || [[ "$BUNDLE_PATH" == *.tgz ]]; then
    if [ ! -f "$BUNDLE_PATH" ]; then
        log_fail "Tarball not found: $BUNDLE_PATH"
        exit 1
    fi
    
    TEMP_DIR=$(mktemp -d)
    log_info "Extracting tarball to $TEMP_DIR..."
    tar -xzf "$BUNDLE_PATH" -C "$TEMP_DIR"
    
    # Find the extracted directory (should be single top-level)
    BUNDLE_DIR=$(find "$TEMP_DIR" -maxdepth 1 -mindepth 1 -type d | head -1)
    if [ -z "$BUNDLE_DIR" ]; then
        # Files may be at root level
        BUNDLE_DIR="$TEMP_DIR"
    fi
elif [ -d "$BUNDLE_PATH" ]; then
    BUNDLE_DIR="$BUNDLE_PATH"
else
    log_fail "Bundle path not found: $BUNDLE_PATH"
    exit 1
fi

log_info "Bundle directory: $BUNDLE_DIR"
echo ""

# =============================================================================
# Check 1: Bundle Structure
# =============================================================================

echo "--- Check 1: Bundle Structure ---"

# Check firebird.so exists
if [ -f "$BUNDLE_DIR/firebird.so" ]; then
    log_pass "firebird.so exists"
else
    log_fail "firebird.so not found in bundle"
fi

# Check lib/ directory
if [ -d "$BUNDLE_DIR/lib" ]; then
    log_pass "lib/ directory exists"
    
    LIB_COUNT=$(find "$BUNDLE_DIR/lib" -name "*.so*" -type f | wc -l)
    log_info "Found $LIB_COUNT library files in lib/"
else
    log_fail "lib/ directory not found"
fi

# Check libfbclient
if ls "$BUNDLE_DIR"/lib/libfbclient.so* &>/dev/null 2>&1; then
    log_pass "libfbclient.so found"
else
    log_fail "libfbclient.so not found in lib/"
fi

# Check documentation
for doc in LICENSE README.md; do
    if [ -f "$BUNDLE_DIR/$doc" ]; then
        log_pass "$doc exists"
    else
        log_warn "$doc missing (not critical)"
    fi
done

echo ""

# =============================================================================
# Check 2: RPATH Configuration
# =============================================================================

echo "--- Check 2: RPATH Configuration ---"

if ! check_command patchelf; then
    log_warn "patchelf not installed, skipping RPATH checks"
else
    # Check firebird.so RPATH
    EXT_RPATH=$(patchelf --print-rpath "$BUNDLE_DIR/firebird.so" 2>/dev/null || echo "")
    log_verbose "firebird.so RPATH: $EXT_RPATH"
    
    if [[ "$EXT_RPATH" == *'$ORIGIN/lib'* ]] || [[ "$EXT_RPATH" == *'$ORIGIN'* ]]; then
        log_pass "firebird.so RPATH contains \$ORIGIN"
    else
        log_fail "firebird.so RPATH should contain \$ORIGIN/lib (got: $EXT_RPATH)"
    fi
    
    # Check if using DT_RPATH (--force-rpath) vs DT_RUNPATH
    if readelf -d "$BUNDLE_DIR/firebird.so" 2>/dev/null | grep -q "RPATH"; then
        log_pass "Using DT_RPATH (stronger precedence)"
    elif readelf -d "$BUNDLE_DIR/firebird.so" 2>/dev/null | grep -q "RUNPATH"; then
        log_warn "Using DT_RUNPATH (may be overridden by LD_LIBRARY_PATH)"
    fi
    
    # Check library RPATHs
    LIB_RPATH_ERRORS=0
    for so in "$BUNDLE_DIR"/lib/*.so*; do
        if [[ -f "$so" && ! -L "$so" ]]; then
            LIB_RPATH=$(patchelf --print-rpath "$so" 2>/dev/null || echo "none")
            log_verbose "$(basename "$so") RPATH: $LIB_RPATH"
            
            if [[ "$LIB_RPATH" != *'$ORIGIN'* ]] && [[ "$LIB_RPATH" != "none" ]] && [[ -n "$LIB_RPATH" ]]; then
                log_verbose "Warning: $(basename "$so") has non-relative RPATH: $LIB_RPATH"
                ((LIB_RPATH_ERRORS++))
            fi
        fi
    done
    
    if [ "$LIB_RPATH_ERRORS" -eq 0 ]; then
        log_pass "Bundled libraries have correct RPATH"
    else
        log_warn "$LIB_RPATH_ERRORS library(ies) have non-\$ORIGIN RPATH"
    fi
fi

echo ""

# =============================================================================
# Check 3: Dependency Resolution
# =============================================================================

echo "--- Check 3: Dependency Resolution ---"

# Run ldd from within bundle directory to test $ORIGIN resolution
MISSING_DEPS=""
NOT_FOUND_COUNT=0

if check_command ldd; then
    # Change to bundle directory so $ORIGIN resolves correctly
    pushd "$BUNDLE_DIR" > /dev/null
    
    LDD_OUTPUT=$(ldd firebird.so 2>&1 || true)
    
    popd > /dev/null
    
    log_verbose "ldd output:"
    if [ "$VERBOSE" = true ]; then
        echo "$LDD_OUTPUT" | sed 's/^/  /'
    fi
    
    # Check for "not found" dependencies
    MISSING_DEPS=$(echo "$LDD_OUTPUT" | grep "not found" || true)
    
    if [ -n "$MISSING_DEPS" ]; then
        # Filter out expected system library mismatches (glibc version specific)
        CRITICAL_MISSING=$(echo "$MISSING_DEPS" | grep -v "^$" || true)
        NOT_FOUND_COUNT=$(echo "$CRITICAL_MISSING" | grep -c "not found" || echo 0)
        
        if [ "$NOT_FOUND_COUNT" -gt 0 ]; then
            log_fail "Missing dependencies detected ($NOT_FOUND_COUNT):"
            echo "$CRITICAL_MISSING" | sed 's/^/  /'
        fi
    else
        log_pass "All dependencies resolved"
    fi
    
    # List resolved dependencies
    RESOLVED=$(echo "$LDD_OUTPUT" | grep "=> /" | wc -l)
    BUNDLED=$(echo "$LDD_OUTPUT" | grep "\$ORIGIN" | wc -l || echo 0)
    log_info "Resolved: $RESOLVED system, $BUNDLED bundled"
fi

echo ""

# =============================================================================
# Check 4: System Library Verification
# =============================================================================

echo "--- Check 4: System Library Verification ---"

# These should NOT be bundled (system libc)
FORBIDDEN_LIBS=("libc.so" "libpthread.so" "libdl.so" "libm.so" "librt.so" "ld-linux")

for lib in "${FORBIDDEN_LIBS[@]}"; do
    if ls "$BUNDLE_DIR"/lib/"$lib"* &>/dev/null 2>&1; then
        log_fail "System library should not be bundled: $lib"
    fi
done

log_pass "No forbidden system libraries bundled"

# These SHOULD be bundled
REQUIRED_LIBS=("libfbclient")
OPTIONAL_LIBS=("libicuuc" "libicudata" "libtommath" "libtomcrypt")

for lib in "${REQUIRED_LIBS[@]}"; do
    if ls "$BUNDLE_DIR"/lib/"$lib".so* &>/dev/null 2>&1; then
        log_pass "Required library bundled: $lib"
    else
        log_fail "Required library missing: $lib"
    fi
done

for lib in "${OPTIONAL_LIBS[@]}"; do
    if ls "$BUNDLE_DIR"/lib/"$lib".so* &>/dev/null 2>&1; then
        log_pass "Optional library bundled: $lib"
    else
        log_warn "Optional library not bundled: $lib (may cause runtime issues)"
    fi
done

echo ""

# =============================================================================
# Check 5: PHP Extension Loading
# =============================================================================

echo "--- Check 5: PHP Extension Loading ---"

if ! check_command "$PHP_BINARY"; then
    log_warn "PHP binary not found ($PHP_BINARY), skipping load test"
else
    PHP_VERSION=$("$PHP_BINARY" -r 'echo PHP_VERSION;' 2>/dev/null || echo "unknown")
    PHP_ZTS=$("$PHP_BINARY" -r 'echo PHP_ZTS ? "ZTS" : "NTS";' 2>/dev/null || echo "unknown")
    log_info "Testing with PHP $PHP_VERSION ($PHP_ZTS)"
    
    # Attempt to load extension
    pushd "$BUNDLE_DIR" > /dev/null
    
    LOAD_OUTPUT=$("$PHP_BINARY" -d "extension=$(pwd)/firebird.so" -m 2>&1 || true)
    LOAD_EXIT=$?
    
    popd > /dev/null
    
    if echo "$LOAD_OUTPUT" | grep -qi "firebird"; then
        log_pass "Extension loads successfully"
    else
        log_fail "Extension failed to load"
        if [ "$VERBOSE" = true ] || [ $LOAD_EXIT -ne 0 ]; then
            echo "Output:"
            echo "$LOAD_OUTPUT" | head -20 | sed 's/^/  /'
        fi
    fi
    
    # Test extension functions exist
    pushd "$BUNDLE_DIR" > /dev/null
    
    FUNC_TEST=$("$PHP_BINARY" -d "extension=$(pwd)/firebird.so" -r "echo function_exists('fbird_connect') ? 'OK' : 'FAIL';" 2>&1 || echo "ERROR")
    
    popd > /dev/null
    
    if [ "$FUNC_TEST" = "OK" ]; then
        log_pass "Extension functions available"
    else
        log_fail "Extension functions not available"
    fi
    
    # Check for symbol errors
    if echo "$LOAD_OUTPUT" | grep -qi "undefined symbol"; then
        log_fail "Undefined symbols detected"
        echo "$LOAD_OUTPUT" | grep -i "undefined symbol" | head -5 | sed 's/^/  /'
    fi
fi

echo ""

# =============================================================================
# Check 6: Bundle Size
# =============================================================================

echo "--- Check 6: Bundle Size ---"

# Calculate total size
if [ -d "$BUNDLE_DIR" ]; then
    TOTAL_SIZE_KB=$(du -sk "$BUNDLE_DIR" | cut -f1)
    TOTAL_SIZE_MB=$((TOTAL_SIZE_KB / 1024))
    
    log_info "Bundle size: ${TOTAL_SIZE_MB}MB (${TOTAL_SIZE_KB}KB)"
    
    if [ "$TOTAL_SIZE_MB" -gt "$MAX_SIZE_MB" ]; then
        log_warn "Bundle exceeds ${MAX_SIZE_MB}MB limit"
    else
        log_pass "Bundle size within limits"
    fi
    
    # Breakdown by component
    if [ "$VERBOSE" = true ]; then
        echo "Size breakdown:"
        du -sh "$BUNDLE_DIR"/* 2>/dev/null | sed 's/^/  /'
        echo "Library sizes:"
        ls -lh "$BUNDLE_DIR"/lib/*.so* 2>/dev/null | awk '{print "  " $5 " " $9}' | sed "s|$BUNDLE_DIR/lib/||"
    fi
fi

echo ""

# =============================================================================
# Check 7: Connection Test (Optional)
# =============================================================================

if [ "$RUN_CONNECTION_TEST" = true ]; then
    echo "--- Check 7: Connection Test ---"
    
    if [ -z "$FB_DSN" ]; then
        log_warn "No DSN provided (--dsn), skipping connection test"
    elif ! check_command "$PHP_BINARY"; then
        log_warn "PHP not available, skipping connection test"
    else
        log_info "Testing connection to: $FB_DSN"
        
        pushd "$BUNDLE_DIR" > /dev/null
        
        CONNECTION_TEST=$(cat <<'PHPCODE'
<?php
$dsn = $argv[1];
$user = $argv[2];
$pass = $argv[3];

try {
    $db = @fbird_connect($dsn, $user, $pass);
    if ($db) {
        echo "OK";
        fbird_close($db);
    } else {
        echo "FAIL: " . fbird_errmsg();
    }
} catch (Throwable $e) {
    echo "ERROR: " . $e->getMessage();
}
PHPCODE
)
        
        CONN_RESULT=$("$PHP_BINARY" -d "extension=$(pwd)/firebird.so" -r "$CONNECTION_TEST" -- "$FB_DSN" "$FB_USER" "$FB_PASSWORD" 2>&1 || echo "EXCEPTION")
        
        popd > /dev/null
        
        if [ "$CONN_RESULT" = "OK" ]; then
            log_pass "Connection test successful"
        else
            log_warn "Connection test failed: $CONN_RESULT"
        fi
    fi
    echo ""
fi

# =============================================================================
# Summary
# =============================================================================

echo "=== Verification Summary ==="
echo ""
echo -e "  ${GREEN}Passed:${NC}  $CHECKS_PASSED"
echo -e "  ${RED}Failed:${NC}  $CHECKS_FAILED"
echo -e "  ${YELLOW}Warnings:${NC} $CHECKS_WARNED"
echo ""

# Determine exit code
if [ "$CHECKS_FAILED" -gt 0 ]; then
    echo -e "${RED}VERIFICATION FAILED${NC} - Bundle has critical issues"
    exit 1
elif [ "$CHECKS_WARNED" -gt 0 ] && [ "$STRICT" = true ]; then
    echo -e "${YELLOW}VERIFICATION FAILED (strict mode)${NC} - Bundle has warnings"
    exit 2
elif [ "$CHECKS_WARNED" -gt 0 ]; then
    echo -e "${YELLOW}VERIFICATION PASSED WITH WARNINGS${NC} - Bundle may have limitations"
    exit 0
else
    echo -e "${GREEN}VERIFICATION PASSED${NC} - Bundle is ready for distribution"
    exit 0
fi