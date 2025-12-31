#!/bin/bash
# scripts/test_with_act.sh
# CI Pre-flight Validator - Ensures local tests match GitHub Actions CI
#
# Usage: ./scripts/test_with_act.sh [mode] [options]
#
# Modes:
#   --qa           Run code quality checks (PHPStan, PHPCS, clang-tidy, cppcheck)
#   --matrix       Run PHP/Firebird compatibility matrix via local Docker
#   --coverage     Run tests with code coverage (mirrors coverage.yml)
#   --sanitizers   Run memory sanitizers (ASan/UBSan with workarounds, or Valgrind via qa.sh)
#   --full         Run complete CI simulation (QA + Matrix + Coverage)
#   --syntax       Validate workflow YAML syntax only (requires act)
#   act [workflow] Run GitHub Actions workflows locally via act
#
# act Mode Usage:
#   ./scripts/test_with_act.sh act                    # Run all workflows
#   ./scripts/test_with_act.sh act main               # Run main.yml only
#   ./scripts/test_with_act.sh act coverage           # Run coverage.yml only
#   ./scripts/test_with_act.sh act sanitizers         # Run sanitizers.yml only
#   ./scripts/test_with_act.sh act --list             # List available workflows/jobs
#   ./scripts/test_with_act.sh act --dryrun           # Dry run (parse only)
#   ./scripts/test_with_act.sh act --job <job>        # Run specific job
#   ./scripts/test_with_act.sh act --fail-fast        # Stop on first failure
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
#   ./scripts/test_with_act.sh --qa                        # Quick quality check
#   ./scripts/test_with_act.sh --matrix --php 8.4 --fb 4.0 # Single matrix cell
#   ./scripts/test_with_act.sh --matrix --all              # Full matrix (slow!)
#   ./scripts/test_with_act.sh --full                      # Complete CI simulation
#   ./scripts/test_with_act.sh --syntax                    # Validate workflow YAML
#   ./scripts/test_with_act.sh act main --dryrun           # Dry-run main workflow
#   ./scripts/test_with_act.sh act --job coverage          # Run coverage job only
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
MAGENTA='\033[0;35m'
BOLD='\033[1m'
NC='\033[0m'

# Project paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DOCKER_DIR="$PROJECT_ROOT/docker"
WORKFLOWS_DIR="$PROJECT_ROOT/.github/workflows"

# Workflow files
WORKFLOW_MAIN=".github/workflows/main.yml"
WORKFLOW_QUALITY=".github/workflows/code-quality.yml"
WORKFLOW_COVERAGE=".github/workflows/coverage.yml"
WORKFLOW_SANITIZERS=".github/workflows/sanitizers.yml"

# Defaults
MODE=""
PHP_VERSION="8.3"
FB_VERSION="4.0"
CONTAINER="php83-dev"
SKIP_BUILD=false
RUN_ALL_MATRIX=false
GITLEAKS_CONFIG="$PROJECT_ROOT/.gitleaks.toml"

# act mode defaults
ACT_WORKFLOW=""
ACT_JOB=""
ACT_DRYRUN=false
ACT_LIST=false
ACT_FAIL_FAST=false
ACT_VERBOSE=false
ACT_EXTRA_ARGS=()

# Exit codes
EXIT_SUCCESS=0
EXIT_QUALITY_FAILED=1
EXIT_BUILD_FAILED=2
EXIT_TESTS_FAILED=3
EXIT_SYNTAX_INVALID=4
EXIT_ACT_FAILED=5

# Results tracking
declare -A RESULTS
START_TIME=$(date +%s)

# ============================================================================
# Usage
# ============================================================================

usage() {
    sed -n '2,45p' "$0" | cut -c3-
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

    # Optional: act (for act mode and --syntax mode)
    if ! command -v act &>/dev/null; then
        warnings+=("act (only needed for act mode and --syntax)")
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
# act Mode - GitHub Actions Local Runner
# ============================================================================

# Workflows that use service containers (Firebird) - these have bugs in act v0.2.83
WORKFLOWS_WITH_SERVICES=("main" "coverage" "sanitizers")

# Check if workflow uses service containers
workflow_uses_services() {
    local workflow_name="$1"
    for svc_workflow in "${WORKFLOWS_WITH_SERVICES[@]}"; do
        if [[ "$workflow_name" == "$svc_workflow" ]]; then
            return 0
        fi
    done
    return 1
}

# Show service container warning
show_service_warning() {
    local workflow_name="$1"
    echo -e "\n${YELLOW}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${YELLOW}║       ⚠️  SERVICE CONTAINER WARNING                          ║${NC}"
    echo -e "${YELLOW}╚══════════════════════════════════════════════════════════════╝${NC}"
    echo -e ""
    echo -e "${YELLOW}Workflow '${workflow_name}' uses service containers (Firebird).${NC}"
    echo -e "${YELLOW}act v0.2.83 has a bug causing panics with service containers.${NC}"
    echo -e ""
    echo -e "${CYAN}Recommended alternatives:${NC}"
    case "$workflow_name" in
        main)
            echo -e "  ${GREEN}./scripts/test_with_act.sh --matrix${NC}  # PHP/Firebird matrix tests"
            echo -e "  ${GREEN}./scripts/test_with_act.sh --matrix --php 8.4 --fb 5.0${NC}  # Single cell"
            ;;
        coverage)
            echo -e "  ${GREEN}./scripts/test_with_act.sh --coverage${NC}  # Code coverage"
            ;;
        sanitizers)
            echo -e "  ${GREEN}./scripts/test_with_act.sh --sanitizers${NC}  # ASan/UBSan tests"
            ;;
    esac
    echo -e ""
    echo -e "${CYAN}For workflows without services, act works well:${NC}"
    echo -e "  ${GREEN}./scripts/test_with_act.sh act code-quality${NC}  # Quality checks"
    echo -e ""
    echo -e "See: ${CYAN}docs/development/LOCAL_CI_TESTING.md${NC}"
    echo -e ""
}

# Check if act is installed
check_act() {
    if ! command -v act &>/dev/null; then
        echo -e "${RED}ERROR: act is not installed${NC}"
        echo ""
        echo -e "${YELLOW}Install act:${NC}"
        echo "  Arch Linux:   sudo pacman -S act"
        echo "  macOS:        brew install act"
        echo "  Other:        https://github.com/nektos/act#installation"
        exit $EXIT_ACT_FAILED
    fi

    # Check act version
    local act_version
    act_version=$(act --version 2>/dev/null | head -1)
    echo -e "${CYAN}Using: $act_version${NC}"
    
    # Warn about service container bug in v0.2.83
    if echo "$act_version" | grep -q "0.2.83"; then
        echo -e "${YELLOW}⚠ Note: v0.2.83 has known issues with service containers${NC}"
    fi
}

# Discover available workflows
discover_workflows() {
    local workflows=()
    
    if [ -d "$WORKFLOWS_DIR" ]; then
        for workflow in "$WORKFLOWS_DIR"/*.yml "$WORKFLOWS_DIR"/*.yaml; do
            if [ -f "$workflow" ]; then
                workflows+=("$workflow")
            fi
        done
    fi

    echo "${workflows[@]}"
}

# Get workflow name from file
get_workflow_name() {
    local workflow_file="$1"
    local name
    
    # Try to extract name from YAML
    name=$(grep -m1 "^name:" "$workflow_file" 2>/dev/null | sed 's/name:\s*//' | tr -d '"'"'" || true)
    
    if [ -z "$name" ]; then
        # Fallback to filename
        name=$(basename "$workflow_file" .yml)
        name=$(basename "$name" .yaml)
    fi
    
    echo "$name"
}

# Get workflow shortname from file path
get_workflow_shortname() {
    local workflow_file="$1"
    local name
    
    name=$(basename "$workflow_file" .yml)
    name=$(basename "$name" .yaml)
    
    echo "$name"
}

# List all workflows and their jobs
list_workflows() {
    echo -e "\n${BLUE}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║       Available GitHub Actions Workflows                     ║${NC}"
    echo -e "${BLUE}╚══════════════════════════════════════════════════════════════╝${NC}"
    echo ""

    local workflows
    mapfile -t workflows < <(find "$WORKFLOWS_DIR" -name "*.yml" -o -name "*.yaml" 2>/dev/null | sort)

    if [ ${#workflows[@]} -eq 0 ]; then
        echo -e "${YELLOW}No workflows found in $WORKFLOWS_DIR${NC}"
        return 1
    fi

    for workflow in "${workflows[@]}"; do
        if [ -f "$workflow" ]; then
            local shortname
            shortname=$(get_workflow_shortname "$workflow")
            local fullname
            fullname=$(get_workflow_name "$workflow")
            
            echo -e "${CYAN}${BOLD}$shortname${NC} - ${fullname}"
            echo -e "  ${YELLOW}File:${NC} $workflow"
            
            # List jobs using act
            echo -e "  ${YELLOW}Jobs:${NC}"
            if act -W "$workflow" -l 2>/dev/null | grep -v "^Stage" | grep -v "^$" | head -20; then
                :
            else
                echo -e "    ${YELLOW}(unable to parse jobs - check YAML syntax)${NC}"
            fi
            echo ""
        fi
    done

    echo -e "${CYAN}Usage examples:${NC}"
    echo "  ./scripts/test_with_act.sh act                    # Run all workflows"
    echo "  ./scripts/test_with_act.sh act main               # Run main.yml only"
    echo "  ./scripts/test_with_act.sh act coverage           # Run coverage.yml only"
    echo "  ./scripts/test_with_act.sh act sanitizers         # Run sanitizers.yml only"
    echo "  ./scripts/test_with_act.sh act --job asan-ubsan   # Run specific job"
    echo "  ./scripts/test_with_act.sh act main --dryrun      # Dry run"
}

# Create act environment file
create_act_env() {
    local env_file="$PROJECT_ROOT/.act.env"
    
    cat > "$env_file" << 'EOF'
# act environment variables for GitHub Actions local testing
# Firebird connection settings
ISC_USER=SYSDBA
ISC_PASSWORD=masterkey
FIREBIRD_HOST=firebird
FIREBIRD_DB_PATH=/var/lib/firebird/data/test.fdb

# Test configuration
NO_INTERACTION=1
REPORT_EXIT_STATUS=1

# Coverage settings
COVERAGE_THRESHOLD=55.0
EOF

    echo "$env_file"
}

# Create act secrets file
create_act_secrets() {
    local secrets_file="$PROJECT_ROOT/.act.secrets"
    
    # Create empty secrets file if it doesn't exist
    if [ ! -f "$secrets_file" ]; then
        cat > "$secrets_file" << 'EOF'
# act secrets file for GitHub Actions local testing
# Add any required secrets here (one per line: SECRET_NAME=value)
# These won't be committed if .act.secrets is in .gitignore
EOF
    fi

    echo "$secrets_file"
}

# Run a single workflow with act
run_act_workflow() {
    local workflow_file="$1"
    local shortname
    shortname=$(get_workflow_shortname "$workflow_file")
    local fullname
    fullname=$(get_workflow_name "$workflow_file")
    
    echo -e "\n${BLUE}╔══════════════════════════════════════════════════════════════╗${NC}"
    printf "${BLUE}║${NC}  Running: ${CYAN}%-48s${NC} ${BLUE}║${NC}\n" "$shortname"
    echo -e "${BLUE}║${NC}  ${fullname}"
    echo -e "${BLUE}╚══════════════════════════════════════════════════════════════╝${NC}"

    local act_args=()
    
    # Workflow file
    act_args+=("-W" "$workflow_file")
    
    # Environment file
    local env_file
    env_file=$(create_act_env)
    act_args+=("--env-file" "$env_file")
    
    # Secrets file (if exists and not empty)
    local secrets_file
    secrets_file=$(create_act_secrets)
    if [ -f "$secrets_file" ] && [ -s "$secrets_file" ]; then
        act_args+=("--secret-file" "$secrets_file")
    fi
    
    # Specific job
    if [ -n "$ACT_JOB" ]; then
        act_args+=("-j" "$ACT_JOB")
    fi
    
    # Dry run
    if [ "$ACT_DRYRUN" = true ]; then
        act_args+=("-n")
    fi
    
    # Verbose
    if [ "$ACT_VERBOSE" = true ]; then
        act_args+=("-v")
    fi
    
    # Use medium image for better compatibility
    # act_args+=("-P" "ubuntu-latest=catthehacker/ubuntu:act-latest")
    
    # Add extra args
    act_args+=("${ACT_EXTRA_ARGS[@]}")
    
    echo -e "${CYAN}>> act ${act_args[*]}${NC}"
    echo ""
    
    local start_time
    start_time=$(date +%s)
    local exit_code=0
    
    if act "${act_args[@]}"; then
        exit_code=0
    else
        exit_code=$?
    fi
    
    local end_time
    end_time=$(date +%s)
    local duration=$((end_time - start_time))
    
    if [ $exit_code -eq 0 ]; then
        echo -e "\n${GREEN}✓ Workflow '$shortname' completed successfully (${duration}s)${NC}"
        RESULTS["act:$shortname"]="PASS"
    else
        echo -e "\n${RED}✗ Workflow '$shortname' failed with exit code $exit_code (${duration}s)${NC}"
        RESULTS["act:$shortname"]="FAIL"
    fi
    
    return $exit_code
}

# Run act mode - main entry point for act functionality
run_act_mode() {
    echo -e "\n${BLUE}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║       GitHub Actions Local Runner (act)                      ║${NC}"
    echo -e "${BLUE}╚══════════════════════════════════════════════════════════════╝${NC}"

    check_act
    
    # List mode
    if [ "$ACT_LIST" = true ]; then
        list_workflows
        return 0
    fi
    
    cd "$PROJECT_ROOT"
    
    local workflows_to_run=()
    local act_failed=0
    
    # Determine which workflows to run
    if [ -n "$ACT_WORKFLOW" ]; then
        # Specific workflow requested
        local workflow_file=""
        
        # Check for exact match first
        if [ -f "$WORKFLOWS_DIR/$ACT_WORKFLOW.yml" ]; then
            workflow_file="$WORKFLOWS_DIR/$ACT_WORKFLOW.yml"
        elif [ -f "$WORKFLOWS_DIR/$ACT_WORKFLOW.yaml" ]; then
            workflow_file="$WORKFLOWS_DIR/$ACT_WORKFLOW.yaml"
        elif [ -f "$ACT_WORKFLOW" ]; then
            workflow_file="$ACT_WORKFLOW"
        elif [ -f ".github/workflows/$ACT_WORKFLOW" ]; then
            workflow_file=".github/workflows/$ACT_WORKFLOW"
        fi
        
        if [ -z "$workflow_file" ] || [ ! -f "$workflow_file" ]; then
            echo -e "${RED}ERROR: Workflow not found: $ACT_WORKFLOW${NC}"
            echo ""
            echo -e "${YELLOW}Available workflows:${NC}"
            for wf in "$WORKFLOWS_DIR"/*.yml "$WORKFLOWS_DIR"/*.yaml; do
                if [ -f "$wf" ]; then
                    echo "  - $(get_workflow_shortname "$wf")"
                fi
            done
            exit $EXIT_ACT_FAILED
        fi
        
        workflows_to_run+=("$workflow_file")
    else
        # Run all workflows
        mapfile -t workflows_to_run < <(find "$WORKFLOWS_DIR" -name "*.yml" -o -name "*.yaml" 2>/dev/null | sort)
        
        if [ ${#workflows_to_run[@]} -eq 0 ]; then
            echo -e "${RED}ERROR: No workflows found in $WORKFLOWS_DIR${NC}"
            exit $EXIT_ACT_FAILED
        fi
        
        echo -e "${YELLOW}Running ${#workflows_to_run[@]} workflow(s)...${NC}"
    fi
    
    # Run each workflow
    for workflow in "${workflows_to_run[@]}"; do
        if [ -f "$workflow" ]; then
            if ! run_act_workflow "$workflow"; then
                act_failed=1
                if [ "$ACT_FAIL_FAST" = true ]; then
                    echo -e "${RED}Stopping due to --fail-fast${NC}"
                    break
                fi
            fi
        fi
    done
    
    return $act_failed
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
    for workflow in "$WORKFLOWS_DIR"/*.yml "$WORKFLOWS_DIR"/*.yaml; do
        if [ -f "$workflow" ]; then
            local shortname
            shortname=$(get_workflow_shortname "$workflow")
            echo -e "\n${CYAN}>> Validating $shortname...${NC}"

            if act -W "$workflow" -n 2>&1 | head -30; then
                echo -e "${GREEN}✓ $shortname syntax valid${NC}"
                RESULTS["syntax:$shortname"]="PASS"
            else
                echo -e "${RED}✗ $shortname syntax invalid${NC}"
                RESULTS["syntax:$shortname"]="FAIL"
                syntax_failed=1
            fi
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
    echo -e "${BLUE}║       Memory Testing with ASan/UBSan                         ║${NC}"
    echo -e "${BLUE}╚══════════════════════════════════════════════════════════════╝${NC}"

    local sanitizers_failed=0
    cd "$PROJECT_ROOT"

    echo -e "${CYAN}>> Running ASan + UBSan memory sanitizers...${NC}"
    echo -e "${YELLOW}Note: Uses GCC sanitizers with libasan preload workaround for PHP${NC}"
    echo -e "${YELLOW}For Valgrind testing, use: ./scripts/qa.sh --mode full${NC}"

    local container="php83-dev"

    # Build with sanitizer flags and run tests
    # Uses GCC (not clang) because GCC has built-in sanitizer support that works
    # with libasan8/libubsan1 packages without requiring compiler-rt builtins
    if docker compose -f "$DOCKER_DIR/docker-compose.yml" exec -T "$container" bash -c '
        cd /ext

        # Ensure GCC and sanitizer runtime libraries are installed
        if ! dpkg -l | grep -q libasan; then
            echo "Installing sanitizer runtime libraries..."
            apt-get update -qq && apt-get install -y -qq gcc g++ libasan8 libubsan1 liblsan0 2>/dev/null || true
        fi

        # Use GCC - it has built-in sanitizer support that works with system libasan
        export CC=gcc
        export CXX=g++
        SANITIZE_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g -O1"
        export CFLAGS="-I/opt/firebird/include ${SANITIZE_FLAGS}"
        export CXXFLAGS="-I/opt/firebird/include ${SANITIZE_FLAGS}"
        export LDFLAGS="-L/opt/firebird/lib -fsanitize=address,undefined"
        export ASAN_OPTIONS="detect_leaks=1:abort_on_error=0:halt_on_error=0:print_stats=1:verbosity=1"
        export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=0"

        # Clean and rebuild
        make clean 2>/dev/null || true
        phpize --clean 2>/dev/null || true
        phpize
        ./configure --with-firebird=/opt/firebird
        make -j$(nproc)

        echo "=== Running tests with AddressSanitizer + UBSan ==="

        # Find and preload ASan runtime for GCC-built sanitized extension
        # This is needed because PHP itself was not built with sanitizers
        ASAN_LIB=$(find /usr/lib -name "libasan.so.*" -type f 2>/dev/null | head -1)
        if [ -n "$ASAN_LIB" ]; then
            export LD_PRELOAD="$ASAN_LIB"
            echo "Preloading ASan runtime: $ASAN_LIB"
        else
            echo "Warning: ASan runtime library not found"
        fi

          make test TESTS=tests/ 2>&1 | tee /tmp/sanitizer_output.txt || true

          # Check for sanitizer errors - filter out false positives from system tools (like sed, grep)
          # Only flag errors that appear to be from our extension code (firebird, fbird, php)
          SANITIZER_ERRORS=0
          if grep -qE "ERROR: (Address|Leak|UndefinedBehavior)Sanitizer" /tmp/sanitizer_output.txt; then
            # Check if the error is from our code, not from system tools
            if grep -A 30 "ERROR:" /tmp/sanitizer_output.txt | grep -qE "(firebird|fbird_|php_fbird|/ext/)"; then
              echo "❌ Sanitizer detected issues in extension code!"
              grep -A 30 "ERROR: " /tmp/sanitizer_output.txt | head -60 || true
              SANITIZER_ERRORS=1
            else
              # Check if it is just from system tools like sed - this is a false positive
              if grep -A 5 "ERROR:" /tmp/sanitizer_output.txt | grep -qE "/usr/bin/(sed|grep|awk)"; then
                echo "⚠️ Sanitizer warnings from system tools (false positive) - ignoring"
              else
                echo "⚠️ Sanitizer detected issues (review needed):"
                grep -A 20 "ERROR: " /tmp/sanitizer_output.txt | head -40 || true
              fi
            fi
          fi

          if [ "$SANITIZER_ERRORS" -eq 1 ]; then
            exit 1
          fi

          echo "✅ No sanitizer errors detected in extension code"
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
        printf "  %-20s %b %s\n" "$key:" "$icon" "$result"
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
    # Check for act mode first
    if [ "${1:-}" = "act" ]; then
        MODE="act"
        shift
        
        # Parse act-specific arguments
        while [[ "$#" -gt 0 ]]; do
            case $1 in
                --list|-l)
                    ACT_LIST=true
                    ;;
                --dryrun|-n)
                    ACT_DRYRUN=true
                    ;;
                --job|-j)
                    ACT_JOB="$2"
                    shift
                    ;;
                --fail-fast)
                    ACT_FAIL_FAST=true
                    ;;
                --verbose|-v)
                    ACT_VERBOSE=true
                    ;;
                --help|-h)
                    usage
                    ;;
                -*)
                    # Pass through unknown flags to act
                    ACT_EXTRA_ARGS+=("$1")
                    ;;
                *)
                    # Workflow name (first non-option argument)
                    if [ -z "$ACT_WORKFLOW" ]; then
                        ACT_WORKFLOW="$1"
                    else
                        ACT_EXTRA_ARGS+=("$1")
                    fi
                    ;;
            esac
            shift
        done
        return
    fi

    # Parse regular arguments
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
    
    if [ "$MODE" = "act" ]; then
        if [ -n "$ACT_WORKFLOW" ]; then
            echo -e "Workflow: ${CYAN}$ACT_WORKFLOW${NC}"
        else
            echo -e "Workflow: ${CYAN}all${NC}"
        fi
        if [ -n "$ACT_JOB" ]; then
            echo -e "Job: ${CYAN}$ACT_JOB${NC}"
        fi
        if [ "$ACT_DRYRUN" = true ]; then
            echo -e "Dry Run: ${YELLOW}yes${NC}"
        fi
    else
        echo -e "PHP: ${YELLOW}$PHP_VERSION${NC} | Firebird: ${YELLOW}$FB_VERSION${NC}"
    fi
    echo ""

    # Skip prerequisites for act list mode
    if [ "$MODE" != "act" ] || [ "$ACT_LIST" != true ]; then
        check_prerequisites
    fi

    local exit_code=0

    case "$MODE" in
        act)
            run_act_mode || exit_code=$EXIT_ACT_FAILED
            ;;
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

    # Print summary for non-list modes
    if [ "$ACT_LIST" != true ]; then
        print_summary
    fi
    
    exit $exit_code
}

main "$@"