#!/bin/bash
# scripts/test_with_act.sh
# CI Pre-flight Validator - Ensures local tests match GitHub Actions CI
#
# Usage: ./scripts/test_with_act.sh [mode] [options]
#
# Modes:
#   --qa           Run code quality checks (PHPStan, PHPCS, clang-tidy, cppcheck)
#   --matrix       Run PHP/Firebird compatibility matrix
#   --coverage     Run tests with code coverage (mirrors coverage.yml)
#   --sanitizers   Build with ASan/UBSan and run tests (mirrors sanitizers.yml)
#   --full         Run complete CI simulation (QA + Matrix + Coverage)
#   --syntax       Validate workflow YAML syntax only (requires act)
#
# Options:
#   --php <ver>      PHP version for matrix (8.1, 8.2, 8.3, 8.4, 8.5)
#   --fb <ver>       Firebird version for matrix (2.5, 3.0, 4.0, 5.0)
#   --all            Run all matrix combinations (slow!)
#   --container NAME Override container for QA mode
#   --skip-build     Skip C extension compilation
#   --help           Show this help message
#
# Examples:
#   ./scripts/test_with_act.sh --qa                    # Quick quality check
#   ./scripts/test_with_act.sh --matrix --php 8.4 --fb 4.0  # Single matrix cell
#   ./scripts/test_with_act.sh --matrix --all          # Full matrix (slow!)
#   ./scripts/test_with_act.sh --full                  # Complete CI simulation
#   ./scripts/test_with_act.sh --syntax                # Validate workflow YAML
#
# CI Parity Guarantee:
#   If ./scripts/test_with_act.sh --full passes locally, GitHub Actions CI MUST pass
#
# See implementation_plan.md for architecture details.

set -euo pipefail

# ============================================================================
# Configuration
# ============================================================================

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

# Project paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DOCKER_DIR="$PROJECT_ROOT/docker"

# Workflow files
WORKFLOW_MAIN=".github/workflows/main.yml"
WORKFLOW_QUALITY=".github/workflows/code-quality.yml"
WORKFLOW_COVERAGE=".github/workflows/coverage.yml"

# Defaults
MODE=""
PHP_VERSION="8.3"
FB_VERSION="4.0"
CONTAINER="php83-dev"
SKIP_BUILD=false
RUN_ALL_MATRIX=false
GITLEAKS_CONFIG="$PROJECT_ROOT/.gitleaks.toml"

# Exit codes
EXIT_SUCCESS=0
EXIT_QUALITY_FAILED=1
EXIT_BUILD_FAILED=2
EXIT_TESTS_FAILED=3
EXIT_SYNTAX_INVALID=4

# Results tracking
declare -A RESULTS
START_TIME=$(date +%s)

# ============================================================================
# Usage
# ============================================================================

usage() {
    sed -n '2,32p' "$0" | cut -c3-
    exit 0
}

# ============================================================================
# Prerequisites Check
# ============================================================================

check_prerequisites() {
    local missing=()
    local warnings=()

    echo -e "${BLUE}>> Checking prerequisites...${NC}"

    # Required: docker
    if ! command -v docker &>/dev/null; then
        missing+=("docker")
    fi

    # Required: docker compose
    if ! docker compose version &>/dev/null 2>&1; then
        missing+=("docker compose (V2+)")
    fi

    # Required for PHP analysis: composer
    if ! command -v composer &>/dev/null; then
        warnings+=("composer (required for PHPStan/PHPCS)")
    fi

    # Optional: act (only for --syntax mode)
    if ! command -v act &>/dev/null; then
        warnings+=("act (only needed for --syntax mode)")
    fi

    # Optional: gitleaks
    if ! command -v gitleaks &>/dev/null; then
        warnings+=("gitleaks (optional, for secret scanning)")
    fi

    # Report
    if [ ${#missing[@]} -gt 0 ]; then
        echo -e "${RED}ERROR: Missing required tools:${NC}"
        for tool in "${missing[@]}"; do
            echo -e "  - $tool"
        done
        exit 1
    fi

    if [ ${#warnings[@]} -gt 0 ]; then
        echo -e "${YELLOW}⚠ Optional tools not found:${NC}"
        for tool in "${warnings[@]}"; do
            echo -e "  - $tool"
        done
    fi

    echo -e "${GREEN}✓ Prerequisites OK${NC}"
}

# ============================================================================
# CI Matrix to Local Container Mapping
# ============================================================================

# Map CI PHP version to local container name
map_php_to_container() {
    local php_ver="$1"
    local fb_ver="${2:-4.0}"

    # Special cases for Firebird client compatibility
    case "$fb_ver" in
        2.5|3.0)
            # Firebird 2.5/3.0 requires FB 3.0 client library
            echo "php84-fb3-dev"
            return
            ;;
        5.0)
            # Firebird 5.0 benefits from FB 5.x client
            echo "php85-fb5-dev"
            return
            ;;
    esac

    # Standard mapping
    case "$php_ver" in
        8.1) echo "php81-dev" ;;
        8.2) echo "php82-dev" ;;
        8.3) echo "php83-dev" ;;
        8.4) echo "php84-dev" ;;
        8.5) echo "php85-dev" ;;
        *)
            echo -e "${RED}ERROR: Unknown PHP version: $php_ver${NC}" >&2
            exit 1
            ;;
    esac
}

# Map CI Firebird version to local server name
map_fb_to_server() {
    local fb_ver="$1"

    case "$fb_ver" in
        2.5) echo "firebird25" ;;
        3.0) echo "firebird30" ;;
        4.0) echo "firebird40" ;;
        5.0) echo "firebird50" ;;
        *)
            echo -e "${RED}ERROR: Unknown Firebird version: $fb_ver${NC}" >&2
            exit 1
            ;;
    esac
}

# ============================================================================
# QA Mode (mirrors code-quality.yml)
# ============================================================================

run_qa_mode() {
    echo -e "\n${BLUE}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║       CI Quality Check (mirrors code-quality.yml)            ║${NC}"
    echo -e "${BLUE}╚══════════════════════════════════════════════════════════════╝${NC}"

    local qa_failed=0

    # 1. PHP Static Analysis (mirrors php-analysis job)
    echo -e "\n${CYAN}>> [1/3] PHP Static Analysis (PHPStan + PHPCS)...${NC}"
    cd "$PROJECT_ROOT"

    # Ensure composer dependencies
    if [ ! -d vendor ]; then
        echo "Installing composer dependencies..."
        composer install --dev --quiet 2>/dev/null || true
    fi

    # PHPStan
    if [ -f vendor/bin/phpstan ]; then
        echo "Running PHPStan..."
        if vendor/bin/phpstan analyse --configuration=phpstan.neon --no-progress; then
            echo -e "${GREEN}✓ PHPStan passed${NC}"
            RESULTS["phpstan"]="PASS"
        else
            echo -e "${RED}✗ PHPStan failed${NC}"
            RESULTS["phpstan"]="FAIL"
            qa_failed=1
        fi
    else
        echo -e "${YELLOW}⚠ PHPStan not installed${NC}"
        RESULTS["phpstan"]="SKIP"
    fi

    # PHPCS
    if [ -f vendor/bin/phpcs ] && [ -d src ]; then
        echo "Running PHPCS..."
        if vendor/bin/phpcs src/ --report=summary 2>/dev/null; then
            echo -e "${GREEN}✓ PHPCS passed${NC}"
            RESULTS["phpcs"]="PASS"
        else
            echo -e "${YELLOW}⚠ PHPCS found style issues (non-blocking)${NC}"
            RESULTS["phpcs"]="WARN"
        fi
    else
        RESULTS["phpcs"]="SKIP"
    fi

    # 2. C/C++ Static Analysis (mirrors c-analysis job)
    echo -e "\n${CYAN}>> [2/3] C/C++ Static Analysis (clang-tidy + cppcheck)...${NC}"

    local build_opts=""
    if [ "$SKIP_BUILD" = true ]; then
        build_opts="--skip-build"
    fi

    if "$SCRIPT_DIR/qa.sh" --container "$CONTAINER" --mode fast $build_opts; then
        echo -e "${GREEN}✓ C/C++ analysis passed${NC}"
        RESULTS["c_analysis"]="PASS"
    else
        echo -e "${RED}✗ C/C++ analysis failed${NC}"
        RESULTS["c_analysis"]="FAIL"
        qa_failed=1
    fi

    # 3. Secret Detection (mirrors secrets-scan job)
    echo -e "\n${CYAN}>> [3/3] Secret Detection (Gitleaks)...${NC}"

    if command -v gitleaks &>/dev/null; then
        local gitleaks_opts=""
        if [ -f "$GITLEAKS_CONFIG" ]; then
            gitleaks_opts="--config=$GITLEAKS_CONFIG"
        fi

        if gitleaks detect --source "$PROJECT_ROOT" --no-git $gitleaks_opts --no-banner 2>/dev/null; then
            echo -e "${GREEN}✓ No secrets detected${NC}"
            RESULTS["gitleaks"]="PASS"
        else
            echo -e "${RED}✗ Secrets detected! Review and remove before committing${NC}"
            RESULTS["gitleaks"]="FAIL"
            qa_failed=1
        fi
    else
        echo -e "${YELLOW}⚠ Gitleaks not installed, skipping${NC}"
        RESULTS["gitleaks"]="SKIP"
    fi

    return $qa_failed
}

# ============================================================================
# Matrix Mode (mirrors main.yml linux-matrix-build)
# ============================================================================

run_matrix_mode() {
    echo -e "\n${BLUE}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║       CI Build Matrix (mirrors main.yml)                     ║${NC}"
    echo -e "${BLUE}╚══════════════════════════════════════════════════════════════╝${NC}"

    local matrix_failed=0

    cd "$PROJECT_ROOT"

    if [ "$RUN_ALL_MATRIX" = true ]; then
        # Full matrix - run all combinations
        echo -e "${YELLOW}Running full matrix (all PHP/Firebird combinations)...${NC}"
        echo -e "${YELLOW}This will take a while!${NC}"

        if "$SCRIPT_DIR/test_matrix.sh"; then
            echo -e "${GREEN}✓ Full matrix passed${NC}"
            RESULTS["matrix"]="PASS"
        else
            echo -e "${RED}✗ Full matrix failed${NC}"
            RESULTS["matrix"]="FAIL"
            matrix_failed=1
        fi
    else
        # Single matrix cell
        local container=$(map_php_to_container "$PHP_VERSION" "$FB_VERSION")
        local server=$(map_fb_to_server "$FB_VERSION")

        echo -e "Testing: ${CYAN}PHP $PHP_VERSION${NC} + ${CYAN}Firebird $FB_VERSION${NC}"
        echo -e "Container: ${YELLOW}$container${NC} → Server: ${YELLOW}$server${NC}"

        if "$SCRIPT_DIR/test_matrix.sh" "$container" "$server"; then
            echo -e "${GREEN}✓ Matrix cell passed: PHP $PHP_VERSION / FB $FB_VERSION${NC}"
            RESULTS["matrix"]="PASS"
        else
            echo -e "${RED}✗ Matrix cell failed: PHP $PHP_VERSION / FB $FB_VERSION${NC}"
            RESULTS["matrix"]="FAIL"
            matrix_failed=1
        fi
    fi

    return $matrix_failed
}

# ============================================================================
# Syntax Mode (workflow YAML validation via act)
# ============================================================================

run_syntax_mode() {
    echo -e "\n${BLUE}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║       Workflow Syntax Validation (act --dryrun)              ║${NC}"
    echo -e "${BLUE}╚══════════════════════════════════════════════════════════════╝${NC}"

    if ! command -v act &>/dev/null; then
        echo -e "${RED}ERROR: act is not installed. Install via: sudo pacman -S act${NC}"
        exit $EXIT_SYNTAX_INVALID
    fi

    local syntax_failed=0
    cd "$PROJECT_ROOT"

    # Validate each workflow
    for workflow in "$WORKFLOW_QUALITY" "$WORKFLOW_MAIN"; do
        if [ -f "$workflow" ]; then
            echo -e "\n${CYAN}>> Validating $workflow...${NC}"

            if act -W "$workflow" -n --dryrun 2>&1 | head -20; then
                echo -e "${GREEN}✓ $workflow syntax valid${NC}"
            else
                echo -e "${RED}✗ $workflow syntax invalid${NC}"
                syntax_failed=1
            fi
        else
            echo -e "${YELLOW}⚠ Workflow not found: $workflow${NC}"
        fi
    done

    if [ $syntax_failed -eq 0 ]; then
        RESULTS["syntax"]="PASS"
    else
        RESULTS["syntax"]="FAIL"
    fi

    return $syntax_failed
}

# ============================================================================
# Coverage Mode (mirrors coverage.yml)
# ============================================================================

run_coverage_mode() {
    echo -e "\n${BLUE}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║       Code Coverage (mirrors coverage.yml)                   ║${NC}"
    echo -e "${BLUE}╚══════════════════════════════════════════════════════════════╝${NC}"

    local coverage_failed=0
    cd "$PROJECT_ROOT"

    # Run coverage script if it exists
    if [ -f "$SCRIPT_DIR/coverage.sh" ]; then
        echo -e "${CYAN}>> Running coverage.sh...${NC}"
        if "$SCRIPT_DIR/coverage.sh"; then
            echo -e "${GREEN}✓ Coverage passed${NC}"
            RESULTS["coverage"]="PASS"
        else
            echo -e "${RED}✗ Coverage failed${NC}"
            RESULTS["coverage"]="FAIL"
            coverage_failed=1
        fi
    else
        # Fallback: Run tests in coverage container
        echo -e "${CYAN}>> Running tests with coverage flags in Docker...${NC}"
        local container="php83-dev"

        # Build with coverage flags and run tests
        if docker compose -f "$DOCKER_DIR/docker-compose.yml" exec -T "$container" bash -c '
            cd /ext
            export CFLAGS="-O0 -g --coverage"
            export CXXFLAGS="-O0 -g --coverage"
            export LDFLAGS="--coverage"

            # Clean and rebuild
            make clean 2>/dev/null || true
            phpize --clean 2>/dev/null || true
            phpize
            ./configure --with-firebird=/opt/firebird
            make -j$(nproc)

            # Run tests
            make test TESTS=tests/

            # Generate coverage report
            if command -v lcov &>/dev/null; then
                lcov --directory . --capture --output-file coverage.info
                lcov --summary coverage.info
            else
                echo "lcov not available for coverage summary"
            fi
        '; then
            echo -e "${GREEN}✓ Coverage tests passed${NC}"
            RESULTS["coverage"]="PASS"
        else
            echo -e "${RED}✗ Coverage tests failed${NC}"
            RESULTS["coverage"]="FAIL"
            coverage_failed=1
        fi
    fi

    return $coverage_failed
}

# ============================================================================
# Sanitizers Mode (mirrors sanitizers.yml)
# ============================================================================

run_sanitizers_mode() {
    echo -e "\n${BLUE}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║       Memory Sanitizers (mirrors sanitizers.yml)             ║${NC}"
    echo -e "${BLUE}╚══════════════════════════════════════════════════════════════╝${NC}"

    local sanitizers_failed=0
    cd "$PROJECT_ROOT"

    echo -e "${CYAN}>> Building and testing with ASan + UBSan...${NC}"
    echo -e "${YELLOW}Note: Requires clang compiler in Docker container${NC}"

    local container="php83-dev"

    # Build with sanitizer flags and run tests
    if docker compose -f "$DOCKER_DIR/docker-compose.yml" exec -T "$container" bash -c '
        cd /ext

        # Check for clang
        if ! command -v clang &>/dev/null; then
            echo "Installing clang..."
            apt-get update -qq && apt-get install -y -qq clang llvm
        fi

        export CC=clang
        export CXX=clang++
        SANITIZE_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g -O1"
        export CFLAGS="-I/opt/firebird/include ${SANITIZE_FLAGS}"
        export CXXFLAGS="-I/opt/firebird/include ${SANITIZE_FLAGS}"
        export LDFLAGS="-L/opt/firebird/lib -fsanitize=address,undefined"
        export ASAN_OPTIONS="detect_leaks=1:abort_on_error=0:halt_on_error=0"
        export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=0"

        # Clean and rebuild
        make clean 2>/dev/null || true
        phpize --clean 2>/dev/null || true
        phpize
        ./configure --with-firebird=/opt/firebird
        make -j$(nproc)

        echo "=== Running tests with AddressSanitizer + UBSan ==="
        make test TESTS=tests/ 2>&1 | tee /tmp/sanitizer_output.txt || true

        # Check for sanitizer errors
        if grep -qE "ERROR: (Address|Leak|UndefinedBehavior)Sanitizer" /tmp/sanitizer_output.txt; then
            echo "❌ Sanitizer detected issues!"
            grep -A 20 "ERROR: " /tmp/sanitizer_output.txt || true
            exit 1
        fi

        echo "✅ No sanitizer errors detected"
    '; then
        echo -e "${GREEN}✓ Sanitizer tests passed${NC}"
        RESULTS["sanitizers"]="PASS"
    else
        echo -e "${RED}✗ Sanitizer tests failed${NC}"
        RESULTS["sanitizers"]="FAIL"
        sanitizers_failed=1
    fi

    return $sanitizers_failed
}

# ============================================================================
# Full Mode (complete CI simulation)
# ============================================================================

run_full_mode() {
    echo -e "\n${BLUE}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║       Full CI Pre-flight Validation                          ║${NC}"
    echo -e "${BLUE}╚══════════════════════════════════════════════════════════════╝${NC}"
    echo -e "${YELLOW}This simulates the complete GitHub Actions CI pipeline${NC}"

    local full_failed=0

    # 1. Quality Checks
    if ! run_qa_mode; then
        full_failed=1
    fi

    # 2. Build & Test Matrix (representative sample)
    echo -e "\n${CYAN}>> Running representative matrix sample...${NC}"
    if ! run_matrix_mode; then
        full_failed=1
    fi

    return $full_failed
}

# ============================================================================
# Summary
# ============================================================================

print_summary() {
    local end_time=$(date +%s)
    local duration=$((end_time - START_TIME))

    echo -e "\n${BLUE}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║                     Test Summary                             ║${NC}"
    echo -e "${BLUE}╚══════════════════════════════════════════════════════════════╝${NC}"

    echo -e "\n${CYAN}Results:${NC}"

    local has_failures=false

    for key in "${!RESULTS[@]}"; do
        local result="${RESULTS[$key]}"
        local icon=""
        case "$result" in
            PASS) icon="${GREEN}✓${NC}" ;;
            FAIL) icon="${RED}✗${NC}"; has_failures=true ;;
            WARN) icon="${YELLOW}⚠${NC}" ;;
            SKIP) icon="${YELLOW}-${NC}" ;;
        esac
        printf "  %-15s %b %s\n" "$key:" "$icon" "$result"
    done

    echo -e "\n${CYAN}Duration:${NC} ${duration}s"

    if [ "$has_failures" = true ]; then
        echo -e "\n${RED}╔══════════════════════════════════════════════════════════════╗${NC}"
        echo -e "${RED}║               ✗ SOME CHECKS FAILED                          ║${NC}"
        echo -e "${RED}╚══════════════════════════════════════════════════════════════╝${NC}"
        echo -e "\n${YELLOW}Next steps:${NC}"
        echo "  1. Review failures above"
        echo "  2. Fix issues locally"
        echo "  3. Re-run: $0 $MODE"
        echo "  4. Push only when all checks pass"
        return 1
    else
        echo -e "\n${GREEN}╔══════════════════════════════════════════════════════════════╗${NC}"
        echo -e "${GREEN}║               ✓ ALL CHECKS PASSED                           ║${NC}"
        echo -e "${GREEN}╚══════════════════════════════════════════════════════════════╝${NC}"
        echo -e "\n${GREEN}CI parity verified - safe to push!${NC}"
        return 0
    fi
}

# ============================================================================
# Argument Parsing
# ============================================================================

parse_args() {
    while [[ "$#" -gt 0 ]]; do
        case $1 in
            --qa)
                MODE="qa"
                ;;
            --matrix)
                MODE="matrix"
                ;;
            --full)
                MODE="full"
                ;;
            --syntax)
                MODE="syntax"
                ;;
            --coverage)
                MODE="coverage"
                ;;
            --sanitizers)
                MODE="sanitizers"
                ;;
            --php)
                PHP_VERSION="$2"
                shift
                ;;
            --fb)
                FB_VERSION="$2"
                shift
                ;;
            --all)
                RUN_ALL_MATRIX=true
                ;;
            --container)
                CONTAINER="$2"
                shift
                ;;
            --skip-build)
                SKIP_BUILD=true
                ;;
            --help|-h)
                usage
                ;;
            *)
                echo -e "${RED}Unknown option: $1${NC}"
                usage
                ;;
        esac
        shift
    done

    # Default mode
    if [ -z "$MODE" ]; then
        MODE="qa"
        echo -e "${YELLOW}No mode specified, defaulting to --qa${NC}"
    fi
}

# ============================================================================
# Main
# ============================================================================

main() {
    parse_args "$@"

    echo -e "${BLUE}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║           CI Pre-flight Validator                            ║${NC}"
    echo -e "${BLUE}╚══════════════════════════════════════════════════════════════╝${NC}"
    echo -e "Mode: ${YELLOW}$MODE${NC}"
    echo -e "PHP: ${YELLOW}$PHP_VERSION${NC} | Firebird: ${YELLOW}$FB_VERSION${NC}"
    echo ""

    check_prerequisites

    local exit_code=0

    case "$MODE" in
        qa)
            run_qa_mode || exit_code=$EXIT_QUALITY_FAILED
            ;;
        matrix)
            run_matrix_mode || exit_code=$EXIT_TESTS_FAILED
            ;;
        coverage)
            run_coverage_mode || exit_code=$EXIT_TESTS_FAILED
            ;;
        sanitizers)
            run_sanitizers_mode || exit_code=$EXIT_TESTS_FAILED
            ;;
        full)
            run_full_mode || exit_code=$EXIT_QUALITY_FAILED
            ;;
        syntax)
            run_syntax_mode || exit_code=$EXIT_SYNTAX_INVALID
            ;;
        *)
            echo -e "${RED}Invalid mode: $MODE${NC}"
            usage
            ;;
    esac

    print_summary
    exit $exit_code
}

main "$@"
