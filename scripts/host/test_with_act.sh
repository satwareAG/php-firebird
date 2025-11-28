#!/bin/bash
# Baby Steps™ systematic testing of all Linux GitHub Actions jobs
# Tests every matrix combination individually with proper validation

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Counters
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Results array
declare -a TEST_RESULTS

# Function to log test results
log_result() {
    local test_name="$1"
    local status="$2"
    local details="$3"

    TEST_RESULTS+=("$test_name|$status|$details")

    if [ "$status" = "PASS" ]; then
        echo -e "${GREEN}✅ PASS${NC}: $test_name"
        ((PASSED_TESTS++))
    else
        echo -e "${RED}❌ FAIL${NC}: $test_name - $details"
        ((FAILED_TESTS++))
    fi
    ((TOTAL_TESTS++))
}

# Function to run single test with timeout
run_test() {
    local test_name="$1"
    local php_version="$2"
    local build_type="$3"
    local firebird_version="$4"
    local timeout_minutes="${5:-10}"

    echo -e "\n${YELLOW}🧪 Testing${NC}: $test_name"
    echo "   Matrix: PHP $php_version + Firebird $firebird_version + $build_type"

    # Clean up any existing containers
    docker stop $(docker ps -q --filter ancestor=catthehacker/ubuntu:act-latest) 2>/dev/null || true
    docker container prune -f 2>/dev/null || true

    # Run test with timeout
    local start_time=$(date +%s)

    if timeout ${timeout_minutes}m act \
        --job linux-comprehensive-build \
        --matrix php-version:$php_version \
        --matrix build-type:$build_type \
        --matrix firebird-version:$firebird_version \
        --quiet 2>&1; then

        local end_time=$(date +%s)
        local duration=$((end_time - start_time))

        # Verify build artifacts were created
        local container_id=$(docker ps -q --filter ancestor=catthehacker/ubuntu:act-latest | head -1)

        if [ -n "$container_id" ]; then
            if docker exec "$container_id" test -f modules/interbase.so 2>/dev/null; then
                # Test extension loading
                if docker exec "$container_id" php$php_version -d extension=modules/interbase.so -m 2>/dev/null | grep -q interbase; then
                    log_result "$test_name" "PASS" "Built and loads correctly (${duration}s)"
                else
                    log_result "$test_name" "FAIL" "Extension built but doesn't load"
                fi
            else
                log_result "$test_name" "FAIL" "Extension file not created"
            fi

            # Clean up
            docker stop "$container_id" 2>/dev/null || true
        else
            log_result "$test_name" "PASS" "Workflow completed (${duration}s, no artifacts to verify)"
        fi
    else
        log_result "$test_name" "FAIL" "Workflow failed or timed out (${timeout_minutes}m)"

        # Clean up failed containers
        docker stop $(docker ps -q --filter ancestor=catthehacker/ubuntu:act-latest) 2>/dev/null || true
    fi

    # Brief pause between tests
    sleep 2
}

# Function to test cpp17 modernization job
test_cpp17_modernization() {
    echo -e "\n${YELLOW}🧪 Testing${NC}: C++17 Modernization Validation"

    # Clean up
    docker stop $(docker ps -q --filter ancestor=catthehacker/ubuntu:act-latest) 2>/dev/null || true
    docker container prune -f 2>/dev/null || true

    local start_time=$(date +%s)

    if timeout 15m act --job cpp17-modernization-validation --quiet 2>&1; then
        local end_time=$(date +%s)
        local duration=$((end_time - start_time))
        log_result "cpp17-modernization-validation" "PASS" "C++17 validation completed (${duration}s)"
    else
        log_result "cpp17-modernization-validation" "FAIL" "C++17 validation failed or timed out"
    fi

    # Clean up
    docker stop $(docker ps -q --filter ancestor=catthehacker/ubuntu:act-latest) 2>/dev/null || true
}

# Main testing function
main() {
    echo "🚀 Starting Baby Steps™ GitHub Actions Local Testing"
    echo "Testing all Linux CI jobs with act..."
    echo ""

    # Test Matrix: PHP 8.3 (4 combinations)
    echo "=== PHP 8.3 Matrix Testing ==="
    run_test "Step 1: PHP 8.3 + Firebird 3.0 + Release" "8.3" "release" "3.0" 12
    run_test "Step 2: PHP 8.3 + Firebird 4.0 + Release" "8.3" "release" "4.0" 12
    run_test "Step 3: PHP 8.3 + Firebird 3.0 + Debug" "8.3" "debug" "3.0" 12
    run_test "Step 4: PHP 8.3 + Firebird 4.0 + Debug" "8.3" "debug" "4.0" 12

    # Test Matrix: PHP 8.4 (4 combinations)
    echo -e "\n=== PHP 8.4 Matrix Testing ==="
    run_test "Step 5: PHP 8.4 + Firebird 3.0 + Release" "8.4" "release" "3.0" 12
    run_test "Step 6: PHP 8.4 + Firebird 4.0 + Release" "8.4" "release" "4.0" 12
    run_test "Step 7: PHP 8.4 + Firebird 3.0 + Debug" "8.4" "debug" "3.0" 12
    run_test "Step 8: PHP 8.4 + Firebird 4.0 + Debug" "8.4" "debug" "4.0" 12

    # Test Matrix: PHP 8.5 (4 combinations)
    echo -e "\n=== PHP 8.5 Matrix Testing ==="
    run_test "Step 9: PHP 8.5 + Firebird 3.0 + Release" "8.5" "release" "3.0" 12
    run_test "Step 10: PHP 8.5 + Firebird 4.0 + Release" "8.5" "release" "4.0" 12
    run_test "Step 11: PHP 8.5 + Firebird 3.0 + Debug" "8.5" "debug" "3.0" 12
    run_test "Step 12: PHP 8.5 + Firebird 4.0 + Debug" "8.5" "debug" "4.0" 12

    # Test C++17 Modernization
    echo -e "\n=== C++17 Modernization Testing ==="
    test_cpp17_modernization

    # Final cleanup
    echo -e "\n🧹 Cleaning up..."
    docker stop $(docker ps -q --filter ancestor=catthehacker/ubuntu:act-latest) 2>/dev/null || true
    docker container prune -f 2>/dev/null || true

    # Print summary
    echo ""
    echo "=================================================="
    echo "🏁 BABY STEPS™ TESTING COMPLETE"
    echo "=================================================="
    echo -e "${GREEN}✅ PASSED${NC}: $PASSED_TESTS/$TOTAL_TESTS tests"
    echo -e "${RED}❌ FAILED${NC}: $FAILED_TESTS/$TOTAL_TESTS tests"

    # Success percentage
    local success_rate=$((PASSED_TESTS * 100 / TOTAL_TESTS))
    echo "📊 Success Rate: $success_rate%"

    echo ""
    echo "📋 DETAILED RESULTS:"
    echo "===================="

    for result in "${TEST_RESULTS[@]}"; do
        IFS='|' read -r name status details <<< "$result"
        if [ "$status" = "PASS" ]; then
            echo -e "${GREEN}✅${NC} $name - $details"
        else
            echo -e "${RED}❌${NC} $name - $details"
        fi
    done

    echo ""

    if [ $success_rate -eq 100 ]; then
        echo -e "${GREEN}🎉 ALL TESTS PASSED! Workflows ready for production.${NC}"
        exit 0
    else
        echo -e "${RED}⚠️  Some tests failed. Review logs for debugging.${NC}"
        exit 1
    fi
}

# Run if called directly
if [ "${BASH_SOURCE[0]}" == "${0}" ]; then
    main "$@"
fi
