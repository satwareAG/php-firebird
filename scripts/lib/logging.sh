#!/bin/bash
# =============================================================================
# scripts/lib/logging.sh - Shared color-coded logging for all php-firebird scripts
# =============================================================================
# Source this file at the top of any script:
#   source "$(dirname "$0")/lib/logging.sh"
#
# Provides:
#   log_debug   "msg"   - Only when VERBOSE=1
#   log_info    "msg"   - Standard output (stdout)
#   log_verbose "msg"   - Only when VERBOSE=1 (stdout)
#   log_warn    "msg"   - Warnings (stderr)
#   log_error   "msg"   - Errors (stderr)
#   log_pass    "msg"   - Pass with checkmark (stdout), increments LOG_PASS_COUNT
#   log_fail    "msg"   - Fail with X (stderr), increments LOG_FAIL_COUNT
#   log_dry_run "msg"   - Only when DRY_RUN=1 (stdout)
#   log_summary         - Print pass/fail counts
#
# Backward-compatible aliases (deprecated, will be removed in v13):
#   info  = log_info
#   warn  = log_warn
#   error = log_error
#   pass  = log_pass
#   fail  = log_fail
#
# Environment variables:
#   VERBOSE=1     Enable debug/verbose output
#   DRY_RUN=1     Enable dry-run messages
#   NO_COLOR=1    Disable all color output
#   FORCE_COLOR=1 Force color even when not a TTY (for CI)
#
# Exports color vars (empty if no color): RED GREEN YELLOW BLUE CYAN NC
# =============================================================================

# --- Color detection ---
if [[ -n "${NO_COLOR:-}" ]]; then
    RED="" GREEN="" YELLOW="" BLUE="" CYAN="" NC=""
elif [[ -n "${FORCE_COLOR:-}" ]] || [[ -t 1 ]]; then
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    BLUE='\033[0;34m'
    CYAN='\033[0;36m'
    NC='\033[0m'
else
    RED="" GREEN="" YELLOW="" BLUE="" CYAN="" NC=""
fi
export RED GREEN YELLOW BLUE CYAN NC

# --- Counters (scripts can read these for exit decisions) ---
LOG_PASS_COUNT=0
LOG_FAIL_COUNT=0
LOG_WARN_COUNT=0

# --- Logging functions ---
log_debug() {
    if [[ "${VERBOSE:-0}" == "1" ]]; then
        echo -e "${CYAN}[DEBUG]${NC} $*" >&2
    fi
}

log_info() {
    echo -e "${GREEN}>>>${NC} $*"
}

log_verbose() {
    if [[ "${VERBOSE:-0}" == "1" ]]; then
        echo -e "${BLUE}[VERBOSE]${NC} $*"
    fi
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $*" >&2
    ((LOG_WARN_COUNT++)) || true
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $*" >&2
    ((LOG_FAIL_COUNT++)) || true
}

log_pass() {
    echo -e "${GREEN}[PASS]${NC} $*"
    ((LOG_PASS_COUNT++)) || true
}

log_fail() {
    echo -e "${RED}[FAIL]${NC} $*" >&2
    ((LOG_FAIL_COUNT++)) || true
}

log_dry_run() {
    if [[ "${DRY_RUN:-0}" == "1" ]]; then
        echo -e "${BLUE}[DRY-RUN]${NC} $*"
    fi
}

log_summary() {
    local total=$((LOG_PASS_COUNT + LOG_FAIL_COUNT))
    if [[ "$total" -gt 0 ]]; then
        echo ""
        echo -e "Results: ${GREEN}$LOG_PASS_COUNT passed${NC}, ${RED}$LOG_FAIL_COUNT failed${NC}"
        if [[ "$LOG_WARN_COUNT" -gt 0 ]]; then
            echo -e "Warnings: ${YELLOW}$LOG_WARN_COUNT${NC}"
        fi
    fi
}

# --- Backward-compatible aliases (deprecated) ---
info()  { log_info  "$@"; }
warn()  { log_warn  "$@"; }
error() { log_error "$@"; }
pass()  { log_pass  "$@"; }
fail()  { log_fail  "$@"; }
