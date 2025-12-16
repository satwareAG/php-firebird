#!/bin/bash
# Optimized Local GitHub Actions Testing using `act`
# Aligned with .github/workflows/main.yml (linux-matrix-build)
# and .github/workflows/code-quality.yml (code-quality)

set -e

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Workflow files
WORKFLOW_MAIN=".github/workflows/main.yml"
WORKFLOW_QUALITY=".github/workflows/code-quality.yml"
JOB_NAME="linux-matrix-build"
JOB_QUALITY="c-analysis"

# Default Matrix Target
DEFAULT_PHP="8.4"
DEFAULT_FB="5.0"

usage() {
    echo -e "${BLUE}Usage:${NC} $0 [options]"
    echo ""
    echo "Options:"
    echo "  --php <ver>      PHP Version (8.1, 8.2, 8.3, 8.4, 8.5) [default: $DEFAULT_PHP]"
    echo "  --fb <ver>       Firebird Version (2.5, 3.0, 4.0, 5.0) [default: $DEFAULT_FB]"
    echo "  --all            Run ALL matrix combinations (Heavy!)"
    echo "  --quality        Run code-quality workflow (clang-tidy + cppcheck)"
    echo "  --help           Show this help"
    echo ""
    echo "Examples:"
    echo "  $0                          # Default: PHP $DEFAULT_PHP, Firebird $DEFAULT_FB"
    echo "  $0 --php 8.3 --fb 4.0       # Specific matrix"
    echo "  $0 --quality                # Static analysis only"
    echo "  $0 --all                    # All matrix combinations"
}

check_act() {
    if ! command -v act &> /dev/null; then
        echo -e "${RED}Error: 'act' is not installed.${NC}"
        echo "Install it via: sudo pacman -S act (Arch) or check https://github.com/nektos/act"
        exit 1
    fi
}

run_act_matrix() {
    local php=$1
    local fb=$2

    echo -e "\n${BLUE}🚀 Running Job:${NC} $JOB_NAME"
    echo -e "${YELLOW}⚙️  Matrix:${NC} PHP=$php | Firebird=$fb"

    # Note: act runs inside docker. We need to ensure it can pull the images.
    # We use --rm to cleanup unless debugging.
    # We force the matrix variables.

    set +e # Allow failure to capture exit code

    # Construct command
    # We map the workspace specific to our setup
    cmd="act -W $WORKFLOW_MAIN -j $JOB_NAME --matrix php-version:$php --matrix firebird-version:$fb --rm"

    echo -e "${BLUE}Exec:${NC} $cmd"
    $cmd

    status=$?
    set -e

    if [ $status -eq 0 ]; then
        echo -e "\n${GREEN}✅ Test Passed:${NC} PHP $php + FB $fb"
        return 0
    else
        echo -e "\n${RED}❌ Test Failed:${NC} PHP $php + FB $fb (Exit Code: $status)"
        return 1
    fi
}

run_code_quality() {
    echo -e "\n${BLUE}🚀 Running Job:${NC} $JOB_QUALITY"
    echo -e "${YELLOW}⚙️  Static Analysis:${NC} clang-tidy + cppcheck"

    set +e # Allow failure to capture exit code

    cmd="act -W $WORKFLOW_QUALITY -j $JOB_QUALITY --rm"

    echo -e "${BLUE}Exec:${NC} $cmd"
    $cmd

    status=$?
    set -e

    if [ $status -eq 0 ]; then
        echo -e "\n${GREEN}✅ Code Quality Passed${NC}"
        return 0
    else
        echo -e "\n${RED}❌ Code Quality Failed${NC} (Exit Code: $status)"
        return 1
    fi
}

run_all() {
    local phps=("8.1" "8.2" "8.3" "8.4" "8.5")
    local fbs=("2.5" "3.0" "4.0" "5.0")
    local failed=0

    for p in "${phps[@]}"; do
        for f in "${fbs[@]}"; do
            if ! run_act_matrix "$p" "$f"; then
                failed=1
            fi
        done
    done

    if [ $failed -eq 0 ]; then
        echo -e "\n${GREEN}🎉 All matrix combinations passed!${NC}"
    else
        echo -e "\n${RED}⚠️  Some matrix combinations failed.${NC}"
        exit 1
    fi
}

# Main Execution
check_act

PHP_VER="$DEFAULT_PHP"
FB_VER="$DEFAULT_FB"
RUN_ALL=0
RUN_QUALITY=0

while [[ "$#" -gt 0 ]]; do
    case $1 in
        --php) PHP_VER="$2"; shift ;;
        --fb) FB_VER="$2"; shift ;;
        --all) RUN_ALL=1 ;;
        --quality) RUN_QUALITY=1 ;;
        --help) usage; exit 0 ;;
        *) echo "Unknown parameter passed: $1"; usage; exit 1 ;;
    esac
    shift
done

if [ "$RUN_QUALITY" -eq 1 ]; then
    run_code_quality
elif [ "$RUN_ALL" -eq 1 ]; then
    run_all
else
    run_act_matrix "$PHP_VER" "$FB_VER"
fi
