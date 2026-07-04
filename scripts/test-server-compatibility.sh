#!/bin/bash
# =============================================================================
# Firebird Server Backwards Compatibility Test Script
# =============================================================================
#
# Purpose: Verify that the Firebird 5.x client library can connect to and
# operate correctly with older Firebird server versions (2.5, 3.0, 4.0, 5.0).
#
# This validates the single-bundle distribution strategy documented in:
# docs/research/firebird-client-compatibility.md
#
# Usage:
#   ./scripts/test-server-compatibility.sh [server_version]
#
# Examples:
#   ./scripts/test-server-compatibility.sh        # Test all servers
#   ./scripts/test-server-compatibility.sh 30     # Test only FB 3.0
#   ./scripts/test-server-compatibility.sh 25 30  # Test FB 2.5 and 3.0
#
# Prerequisites:
#   - Docker and Docker Compose installed
#   - php-firebird extension compiled in the PHP container
#
# =============================================================================

set -euo pipefail

source "$(dirname "$0")/lib/logging.sh"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Test counters
TESTS_PASSED=0
TESTS_FAILED=0
TESTS_SKIPPED=0

# Server configurations
# Format: "service_name:host:port:description"
declare -A SERVERS=(
    ["25"]="firebird25:firebird25:3050:Firebird 2.5.x (Protocol 10-12)"
    ["30"]="firebird30:firebird30:3050:Firebird 3.0.x (Protocol 13-15)"
    ["40"]="firebird40:firebird40:3050:Firebird 4.0.x (Protocol 16-17)"
    ["50"]="firebird50:firebird50:3050:Firebird 5.0.x (Protocol 18-19)"
)

# PHP container to use (with FB 5.x client)
PHP_CONTAINER="php85-fb5-dev"

# Database credentials
DB_USER="SYSDBA"
DB_PASS="masterkey"
DB_NAME="test.fdb"

log_header() {
    echo ""
    log_info "========================================"
    log_info "$1"
    log_info "========================================"
}

# Check if Docker services are running
check_services() {
    local version=$1
    IFS=':' read -r service host port desc <<< "${SERVERS[$version]}"
    
    if ! docker compose -f "${PROJECT_ROOT}/docker/docker-compose.yml" ps --status running | grep -q "$service"; then
        log_info "Starting $service..."
        docker compose -f "${PROJECT_ROOT}/docker/docker-compose.yml" up -d "$service" 2>/dev/null || true
        sleep 5
    fi
}

# Generate PHP test script
generate_test_script() {
    local host=$1
    local port=$2
    local test_type=$3
    
    local dsn="${host}/${port}:/firebird/data/${DB_NAME}"
    
    case $test_type in
        "connection")
            cat << 'PHPEOF'
<?php
declare(strict_types=1);
error_reporting(E_ALL);

$dsn = $argv[1] ?? '';
$user = $argv[2] ?? 'SYSDBA';
$pass = $argv[3] ?? 'masterkey';

try {
    $conn = fbird_connect($dsn, $user, $pass);
    if ($conn) {
        // Get server version
        $result = fbird_query($conn, "SELECT RDB\$GET_CONTEXT('SYSTEM', 'ENGINE_VERSION') FROM RDB\$DATABASE");
        $row = fbird_fetch_row($result);
        $version = $row[0] ?? 'Unknown';
        fbird_free_result($result);
        fbird_close($conn);
        echo "SUCCESS:Connected to Firebird $version";
        exit(0);
    }
    echo "FAIL:Connection returned false";
    exit(1);
} catch (Throwable $e) {
    echo "FAIL:" . $e->getMessage();
    exit(1);
}
PHPEOF
            ;;
        
        "crud")
            cat << 'PHPEOF'
<?php
declare(strict_types=1);
error_reporting(E_ALL);

$dsn = $argv[1] ?? '';
$user = $argv[2] ?? 'SYSDBA';
$pass = $argv[3] ?? 'masterkey';

try {
    $conn = fbird_connect($dsn, $user, $pass);
    if (!$conn) {
        echo "FAIL:Connection failed";
        exit(1);
    }

    // Create test table
    @fbird_query($conn, "DROP TABLE compat_test");
    fbird_query($conn, "CREATE TABLE compat_test (id INTEGER PRIMARY KEY, name VARCHAR(100), created_at TIMESTAMP)");
    fbird_commit($conn);

    // INSERT
    $trans = fbird_trans($conn);
    fbird_query($trans, "INSERT INTO compat_test (id, name, created_at) VALUES (1, 'Test Row 1', CURRENT_TIMESTAMP)");
    fbird_query($trans, "INSERT INTO compat_test (id, name, created_at) VALUES (2, 'Test Row 2', CURRENT_TIMESTAMP)");
    fbird_commit($trans);

    // SELECT
    $result = fbird_query($conn, "SELECT COUNT(*) FROM compat_test");
    $row = fbird_fetch_row($result);
    if ($row[0] != 2) {
        echo "FAIL:Expected 2 rows, got " . $row[0];
        exit(1);
    }
    fbird_free_result($result);

    // UPDATE
    $trans = fbird_trans($conn);
    fbird_query($trans, "UPDATE compat_test SET name = 'Updated Row' WHERE id = 1");
    fbird_commit($trans);

    $result = fbird_query($conn, "SELECT name FROM compat_test WHERE id = 1");
    $row = fbird_fetch_row($result);
    if ($row[0] !== 'Updated Row') {
        echo "FAIL:Update verification failed";
        exit(1);
    }
    fbird_free_result($result);

    // DELETE
    $trans = fbird_trans($conn);
    fbird_query($trans, "DELETE FROM compat_test WHERE id = 2");
    fbird_commit($trans);

    $result = fbird_query($conn, "SELECT COUNT(*) FROM compat_test");
    $row = fbird_fetch_row($result);
    if ($row[0] != 1) {
        echo "FAIL:Delete verification failed";
        exit(1);
    }
    fbird_free_result($result);

    // Cleanup
    fbird_query($conn, "DROP TABLE compat_test");
    fbird_commit($conn);
    fbird_close($conn);

    echo "SUCCESS:CRUD operations completed";
    exit(0);
} catch (Throwable $e) {
    echo "FAIL:" . $e->getMessage();
    exit(1);
}
PHPEOF
            ;;
        
        "blob")
            cat << 'PHPEOF'
<?php
declare(strict_types=1);
error_reporting(E_ALL);

$dsn = $argv[1] ?? '';
$user = $argv[2] ?? 'SYSDBA';
$pass = $argv[3] ?? 'masterkey';

try {
    $conn = fbird_connect($dsn, $user, $pass);
    if (!$conn) {
        echo "FAIL:Connection failed";
        exit(1);
    }

    // Create test table with BLOB
    @fbird_query($conn, "DROP TABLE blob_test");
    fbird_query($conn, "CREATE TABLE blob_test (id INTEGER PRIMARY KEY, data BLOB SUB_TYPE 1)");
    fbird_commit($conn);

    // Test data (1KB of text)
    $testData = str_repeat("BLOB test data - line number ", 30) . "\n";
    $testData = substr($testData, 0, 1024);

    // Write BLOB
    $trans = fbird_trans($conn);
    $blob = fbird_blob_create($trans);
    fbird_blob_add($blob, $testData);
    $blobId = fbird_blob_close($blob);
    
    $stmt = fbird_prepare($trans, "INSERT INTO blob_test (id, data) VALUES (?, ?)");
    fbird_execute($stmt, 1, $blobId);
    fbird_commit($trans);

    // Read BLOB back
    $result = fbird_query($conn, "SELECT data FROM blob_test WHERE id = 1");
    $row = fbird_fetch_row($result);
    $blobInfo = fbird_blob_info($conn, $row[0]);
    
    $readBlob = fbird_blob_open($conn, $row[0]);
    $readData = fbird_blob_get($readBlob, $blobInfo['length']);
    fbird_blob_close($readBlob);
    fbird_free_result($result);

    if ($readData !== $testData) {
        echo "FAIL:BLOB data mismatch (wrote " . strlen($testData) . " bytes, read " . strlen($readData) . " bytes)";
        exit(1);
    }

    // Cleanup
    fbird_query($conn, "DROP TABLE blob_test");
    fbird_commit($conn);
    fbird_close($conn);

    echo "SUCCESS:BLOB operations completed (" . strlen($testData) . " bytes)";
    exit(0);
} catch (Throwable $e) {
    echo "FAIL:" . $e->getMessage();
    exit(1);
}
PHPEOF
            ;;
        
        "transaction")
            cat << 'PHPEOF'
<?php
declare(strict_types=1);
error_reporting(E_ALL);

$dsn = $argv[1] ?? '';
$user = $argv[2] ?? 'SYSDBA';
$pass = $argv[3] ?? 'masterkey';

try {
    $conn = fbird_connect($dsn, $user, $pass);
    if (!$conn) {
        echo "FAIL:Connection failed";
        exit(1);
    }

    // Create test table
    @fbird_query($conn, "DROP TABLE trans_test");
    fbird_query($conn, "CREATE TABLE trans_test (id INTEGER PRIMARY KEY, value INTEGER)");
    fbird_commit($conn);

    // Test COMMIT
    $trans1 = fbird_trans($conn);
    fbird_query($trans1, "INSERT INTO trans_test (id, value) VALUES (1, 100)");
    fbird_commit($trans1);

    $result = fbird_query($conn, "SELECT value FROM trans_test WHERE id = 1");
    $row = fbird_fetch_row($result);
    if ($row[0] != 100) {
        echo "FAIL:COMMIT verification failed";
        exit(1);
    }
    fbird_free_result($result);

    // Test ROLLBACK
    $trans2 = fbird_trans($conn);
    fbird_query($trans2, "UPDATE trans_test SET value = 999 WHERE id = 1");
    fbird_rollback($trans2);

    $result = fbird_query($conn, "SELECT value FROM trans_test WHERE id = 1");
    $row = fbird_fetch_row($result);
    if ($row[0] != 100) {
        echo "FAIL:ROLLBACK verification failed (value = " . $row[0] . ", expected 100)";
        exit(1);
    }
    fbird_free_result($result);

    // Test nested/multiple transactions
    $trans3 = fbird_trans($conn);
    $trans4 = fbird_trans($conn);
    
    fbird_query($trans3, "INSERT INTO trans_test (id, value) VALUES (2, 200)");
    fbird_query($trans4, "INSERT INTO trans_test (id, value) VALUES (3, 300)");
    
    fbird_commit($trans3);
    fbird_rollback($trans4);

    $result = fbird_query($conn, "SELECT COUNT(*) FROM trans_test");
    $row = fbird_fetch_row($result);
    if ($row[0] != 2) {
        echo "FAIL:Multi-transaction test failed (count = " . $row[0] . ", expected 2)";
        exit(1);
    }
    fbird_free_result($result);

    // Cleanup
    fbird_query($conn, "DROP TABLE trans_test");
    fbird_commit($conn);
    fbird_close($conn);

    echo "SUCCESS:Transaction operations completed";
    exit(0);
} catch (Throwable $e) {
    echo "FAIL:" . $e->getMessage();
    exit(1);
}
PHPEOF
            ;;
        
        "prepared")
            cat << 'PHPEOF'
<?php
declare(strict_types=1);
error_reporting(E_ALL);

$dsn = $argv[1] ?? '';
$user = $argv[2] ?? 'SYSDBA';
$pass = $argv[3] ?? 'masterkey';

try {
    $conn = fbird_connect($dsn, $user, $pass);
    if (!$conn) {
        echo "FAIL:Connection failed";
        exit(1);
    }

    // Create test table
    @fbird_query($conn, "DROP TABLE prepared_test");
    fbird_query($conn, "CREATE TABLE prepared_test (id INTEGER PRIMARY KEY, name VARCHAR(100), amount DECIMAL(18,2))");
    fbird_commit($conn);

    // Test prepared INSERT with different types
    $trans = fbird_trans($conn);
    $stmt = fbird_prepare($trans, "INSERT INTO prepared_test (id, name, amount) VALUES (?, ?, ?)");
    
    fbird_execute($stmt, 1, 'String Value', 123.45);
    fbird_execute($stmt, 2, 'Another String', 999.99);
    fbird_execute($stmt, 3, null, 0.01);  // NULL test
    
    fbird_free_query($stmt);
    fbird_commit($trans);

    // Test prepared SELECT
    $stmt = fbird_prepare($conn, "SELECT name, amount FROM prepared_test WHERE id = ?");
    
    $result = fbird_execute($stmt, 1);
    $row = fbird_fetch_assoc($result);
    if ($row['NAME'] !== 'String Value' || (float)$row['AMOUNT'] !== 123.45) {
        echo "FAIL:Prepared SELECT verification failed";
        exit(1);
    }
    fbird_free_result($result);
    
    // Test NULL retrieval
    $result = fbird_execute($stmt, 3);
    $row = fbird_fetch_assoc($result);
    if ($row['NAME'] !== null) {
        echo "FAIL:NULL value verification failed";
        exit(1);
    }
    fbird_free_result($result);
    fbird_free_query($stmt);

    // Cleanup
    fbird_query($conn, "DROP TABLE prepared_test");
    fbird_commit($conn);
    fbird_close($conn);

    echo "SUCCESS:Prepared statement operations completed";
    exit(0);
} catch (Throwable $e) {
    echo "FAIL:" . $e->getMessage();
    exit(1);
}
PHPEOF
            ;;
    esac
}

# Run a single test
run_test() {
    local version=$1
    local test_type=$2
    local description=$3
    
    IFS=':' read -r service host port server_desc <<< "${SERVERS[$version]}"
    local dsn="${host}/${port}:/firebird/data/${DB_NAME}"
    
    # Generate test script
    local test_script
    test_script=$(generate_test_script "$host" "$port" "$test_type")
    
    # Run test in PHP container with extension loaded
    local output
    output=$(echo "$test_script" | docker compose -f "${PROJECT_ROOT}/docker/docker-compose.yml" \
        exec -T "$PHP_CONTAINER" php -d extension=/ext/modules/firebird.so -- "$dsn" "$DB_USER" "$DB_PASS" 2>&1) || true
    
    if [[ "$output" == SUCCESS:* ]]; then
        ((TESTS_PASSED++)) || true
        log_pass "FB $version - $description: ${output#SUCCESS:}"
        return 0
    elif [[ "$output" == FAIL:* ]]; then
        ((TESTS_FAILED++)) || true
        log_fail "FB $version - $description: ${output#FAIL:}"
        return 1
    else
        ((TESTS_FAILED++)) || true
        log_fail "FB $version - $description: Unexpected output: $output"
        return 1
    fi
}

# Run all tests for a server version
test_server_version() {
    local version=$1
    IFS=':' read -r service host port desc <<< "${SERVERS[$version]}"
    
    log_header "Testing $desc"
    
    # Ensure service is running
    check_services "$version"
    
    # Wait for server to be ready
    log_info "Waiting for Firebird $version to be ready..."
    sleep 3
    
    # Run test suite
    run_test "$version" "connection" "Connection test" || true
    run_test "$version" "crud" "CRUD operations" || true
    run_test "$version" "blob" "BLOB operations" || true
    run_test "$version" "transaction" "Transaction handling" || true
    run_test "$version" "prepared" "Prepared statements" || true
}

# Print summary
print_summary() {
    log_header "Test Summary"
    
    local total=$((TESTS_PASSED + TESTS_FAILED + TESTS_SKIPPED))
    
    echo ""
    log_info "  Passed:  $TESTS_PASSED"
    log_info "  Failed:  $TESTS_FAILED"
    log_info "  Skipped: $TESTS_SKIPPED"
    log_info "  Total:   $total"
    echo ""

    if [[ $TESTS_FAILED -eq 0 ]]; then
        log_pass "✓ All tests passed!"
        echo ""
        echo "The Firebird 5.x client library successfully connects to all tested"
        echo "server versions. The single-bundle distribution strategy is validated."
    else
        log_fail "✗ Some tests failed."
        echo ""
        echo "Review the failures above. Note that some failures may be expected"
        echo "due to feature differences between Firebird versions."
    fi
    echo ""
}

# Ensure PHP container is running with extension loaded
ensure_php_container() {
    log_info "Ensuring PHP container ($PHP_CONTAINER) is running..."
    
    if ! docker compose -f "${PROJECT_ROOT}/docker/docker-compose.yml" ps --status running | grep -q "$PHP_CONTAINER"; then
        docker compose -f "${PROJECT_ROOT}/docker/docker-compose.yml" up -d "$PHP_CONTAINER"
        sleep 5
    fi
    
    # Check if extension needs to be built
    if [[ ! -f "${PROJECT_ROOT}/modules/firebird.so" ]]; then
        log_info "Building php-firebird extension..."
        docker compose -f "${PROJECT_ROOT}/docker/docker-compose.yml" \
            exec -T "$PHP_CONTAINER" bash -c "cd /ext && phpize && ./configure --with-firebird=/opt/firebird && make -j\$(nproc)" 2>&1 || {
            ((TESTS_FAILED++)) || true
        log_fail "Failed to build extension"
            exit 1
        }
    fi
    
    # Verify extension can be loaded
    log_info "Verifying php-firebird extension..."
    local ext_check
    ext_check=$(docker compose -f "${PROJECT_ROOT}/docker/docker-compose.yml" \
        exec -T "$PHP_CONTAINER" php -d extension=/ext/modules/firebird.so -m 2>&1 | grep -i firebird || echo "")
    
    if [[ -z "$ext_check" ]]; then
            ((TESTS_FAILED++)) || true
        log_fail "php-firebird extension cannot be loaded in $PHP_CONTAINER"
        echo ""
        echo "Please build the extension first:"
        echo "  docker compose -f docker/docker-compose.yml exec $PHP_CONTAINER bash"
        echo "  phpize && ./configure --with-firebird=/opt/firebird && make"
        exit 1
    fi
    
    ((TESTS_PASSED++)) || true
    log_pass "Extension loaded: $ext_check"
}

# Main execution
main() {
    log_header "Firebird Server Backwards Compatibility Tests"
    echo ""
    echo "Testing Firebird 5.x client library against multiple server versions."
    echo "Reference: docs/research/firebird-client-compatibility.md"
    echo ""
    
    # Ensure PHP container is ready
    ensure_php_container
    
    # Determine which servers to test
    local versions_to_test=()
    if [[ $# -eq 0 ]]; then
        # Test all servers
        versions_to_test=("25" "30" "40" "50")
    else
        # Test specified servers
        versions_to_test=("$@")
    fi
    
    # Validate server versions
    for version in "${versions_to_test[@]}"; do
        if [[ -z "${SERVERS[$version]:-}" ]]; then
            echo "Error: Unknown server version '$version'"
            echo "Valid versions: ${!SERVERS[*]}"
            exit 1
        fi
    done
    
    # Run tests
    for version in "${versions_to_test[@]}"; do
        test_server_version "$version"
    done
    
    # Print summary
    print_summary
    
    # Exit with appropriate code
    if [[ $TESTS_FAILED -gt 0 ]]; then
        exit 1
    fi
    exit 0
}

# Run main function
main "$@"