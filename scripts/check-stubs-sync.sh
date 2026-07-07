#!/usr/bin/env bash
# check-stubs-sync.sh — Verify stub files are in sync with PHP_FE registrations in firebird.c
#
# Usage: ./scripts/check-stubs-sync.sh [--no-phantom-check]
#
# Exit codes:
#   0 — all stubs in sync
#   1 — one or more functions missing from stubs (breaks PHPStan consumers)
#   2 — phantom stubs found (in stubs but not exported via PHP_FE) — warning only
#
# This script protects doctrine-firebird-driver and other PHPStan consumers that
# rely on satwareag/php-firebird-stubs for type checking.

set -euo pipefail
source "$(dirname "$0")/lib/logging.sh"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

FIREBIRD_C="${ROOT_DIR}/firebird.c"
STUBS_FILE="${ROOT_DIR}/stubs/firebird-stubs.php"
PHPSTAN_STUB="${ROOT_DIR}/phpstan/fbird.stub.php"

CHECK_PHANTOMS=true
if [[ "${1:-}" == "--no-phantom-check" ]]; then
    CHECK_PHANTOMS=false
fi

if [[ ! -f "${FIREBIRD_C}" ]]; then
    error "Source file not found: ${FIREBIRD_C}"
    exit 1
fi
if [[ ! -f "${STUBS_FILE}" ]]; then
    error "Stubs file not found: ${STUBS_FILE}"
    exit 1
fi
if [[ ! -f "${PHPSTAN_STUB}" ]]; then
    error "PHPStan stub file not found: ${PHPSTAN_STUB}"
    exit 1
fi

# Extract fbird_* functions registered via PHP_FE (exclude ibase_* aliases)
C_FUNCS=$(grep "PHP_FE(fbird_" "${FIREBIRD_C}" \
    | grep -v "ibase_" \
    | sed 's/.*PHP_FE(\(fbird_[^,]*\).*/\1/' \
    | sort -u)

# Extract fbird_* functions declared in stubs/firebird-stubs.php
STUB_FUNCS=$(grep "^function fbird_" "${STUBS_FILE}" \
    | sed 's/function \(fbird_[^(]*\).*/\1/' \
    | sort -u)

# Extract fbird_* functions declared in phpstan/fbird.stub.php
PHPSTAN_FUNCS=$(grep "^function fbird_" "${PHPSTAN_STUB}" \
    | sed 's/function \(fbird_[^(]*\).*/\1/' \
    | sort -u)

EXIT_CODE=0

log_info "Checking stub sync against firebird.c PHP_FE registrations..."
echo ""

# ── Check 1: Functions in C but missing from stubs/firebird-stubs.php ──────────
MISSING_FROM_STUBS=$(comm -23 \
    <(echo "${C_FUNCS}") \
    <(echo "${STUB_FUNCS}"))

if [[ -n "${MISSING_FROM_STUBS}" ]]; then
    error "Functions registered in PHP_FE but MISSING from stubs/firebird-stubs.php:"
    while IFS= read -r fn; do
        echo "    - ${fn}"
    done <<< "${MISSING_FROM_STUBS}"
    echo ""
    error "These missing stubs will cause PHPStan errors in doctrine-firebird-driver!"
    EXIT_CODE=1
else
    log_pass "stubs/firebird-stubs.php: all PHP_FE functions covered"
fi

# ── Check 2: Functions in C but missing from phpstan/fbird.stub.php ────────────
MISSING_FROM_PHPSTAN=$(comm -23 \
    <(echo "${C_FUNCS}") \
    <(echo "${PHPSTAN_FUNCS}"))

if [[ -n "${MISSING_FROM_PHPSTAN}" ]]; then
    error "Functions registered in PHP_FE but MISSING from phpstan/fbird.stub.php:"
    while IFS= read -r fn; do
        echo "    - ${fn}"
    done <<< "${MISSING_FROM_PHPSTAN}"
    EXIT_CODE=1
else
    log_pass "phpstan/fbird.stub.php: all PHP_FE functions covered"
fi

# ── Check 3: Phantom stubs (in stubs but not in PHP_FE) ────────────────────────
if [[ "${CHECK_PHANTOMS}" == "true" ]]; then
    PHANTOM_IN_STUBS=$(comm -23 \
        <(echo "${STUB_FUNCS}") \
        <(echo "${C_FUNCS}"))

    PHANTOM_IN_PHPSTAN=$(comm -23 \
        <(echo "${PHPSTAN_FUNCS}") \
        <(echo "${C_FUNCS}"))

    if [[ -n "${PHANTOM_IN_STUBS}" ]]; then
        warn "Phantom stubs in stubs/firebird-stubs.php (not registered via PHP_FE):"
        while IFS= read -r fn; do
            echo "    - ${fn}"
        done <<< "${PHANTOM_IN_STUBS}"
        warn "These functions are not callable at runtime — stubs may mislead consumers."
        if [[ ${EXIT_CODE} -eq 0 ]]; then
            EXIT_CODE=2
        fi
    else
        log_pass "stubs/firebird-stubs.php: no phantom stubs"
    fi

    if [[ -n "${PHANTOM_IN_PHPSTAN}" ]]; then
        warn "Phantom stubs in phpstan/fbird.stub.php (not registered via PHP_FE):"
        while IFS= read -r fn; do
            echo "    - ${fn}"
        done <<< "${PHANTOM_IN_PHPSTAN}"
        if [[ ${EXIT_CODE} -eq 0 ]]; then
            EXIT_CODE=2
        fi
    else
        log_pass "phpstan/fbird.stub.php: no phantom stubs"
    fi
fi

# ── Summary ──────────────────────────────────────────────────────────────────
echo ""
C_COUNT=$(echo "${C_FUNCS}" | wc -l | tr -d ' ')
STUB_COUNT=$(echo "${STUB_FUNCS}" | wc -l | tr -d ' ')
PHPSTAN_COUNT=$(echo "${PHPSTAN_FUNCS}" | wc -l | tr -d ' ')

log_info "Summary: C exports ${C_COUNT} functions | stubs: ${STUB_COUNT} | phpstan stub: ${PHPSTAN_COUNT}"

if [[ ${EXIT_CODE} -eq 0 ]]; then
    log_pass "All stubs are in sync with firebird.c PHP_FE registrations."
elif [[ ${EXIT_CODE} -eq 1 ]]; then
    error "FAIL: Stub drift detected — doctrine-firebird-driver PHPStan will break!"
elif [[ ${EXIT_CODE} -eq 2 ]]; then
    warn "WARNING: Phantom stubs found — review and remove or register via PHP_FE."
fi

exit ${EXIT_CODE}
