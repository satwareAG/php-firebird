#!/usr/bin/env bash
# check-header-parity.sh — Fail when pdo_fbird's copies of shared headers
# drift from the main extension's (#602).
#
# pdo_fbird compiles against its OWN copies of firebird_utils.h and
# php_fbird_includes.h but LINKS against the main extension's exported
# functions. A signature or struct change in the main ext that is not
# mirrored in pdo's copies produces garbage-argument calls at runtime
# (incident 2026-08-24: fbs_prepare 8-param call into 9-param export ->
# SIGSEGV in pdo_fbird_handle_preparer, ~50 PDO test failures across the
# whole CI matrix - PR #601, hotfix 54e43a7).
#
# Exit codes:
#   0 — all shared headers in sync
#   1 — firebird_utils.h drifted (ABI-load-bearing: function prototypes)
#   2 — php_fbird_includes.h drifted (struct layouts; advisory while pdo
#       only touches opaque handles, but must not diverge further)

set -euo pipefail
source "$(dirname "$0")/lib/logging.sh"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

UTILS_MAIN="${ROOT_DIR}/firebird_utils.h"
UTILS_PDO="${ROOT_DIR}/pdo_fbird/firebird_utils.h"
INCLUDES_MAIN="${ROOT_DIR}/php_fbird_includes.h"
INCLUDES_PDO="${ROOT_DIR}/pdo_fbird/php_fbird_includes.h"

RC=0

if [[ ! -f "${UTILS_PDO}" ]]; then
    error "pdo_fbird/firebird_utils.h not found at ${UTILS_PDO}"
    exit 1
fi

if ! diff -q "${UTILS_MAIN}" "${UTILS_PDO}" >/dev/null; then
    error "firebird_utils.h DRIFT between main ext and pdo_fbird (ABI-load-bearing!):"
    diff "${UTILS_MAIN}" "${UTILS_PDO}" | head -30 || true
    error "Any signature change must touch BOTH copies in the same commit (#602)."
    RC=1
else
    log_info "firebird_utils.h: main and pdo_fbird copies identical"
fi

if ! diff -q "${INCLUDES_MAIN}" "${INCLUDES_PDO}" >/dev/null; then
    # Struct-layout drift: pdo currently touches only opaque handles, so
    # this is advisory - but it must never grow. Fail the gate so the drift
    # gets reconciled deliberately instead of silently.
    warn "php_fbird_includes.h drifted between main ext and pdo_fbird:"
    diff "${INCLUDES_MAIN}" "${INCLUDES_PDO}" | head -20 || true
    warn "pdo_fbird must not touch fbird_transaction/fbird_query struct fields."
    if [[ "${ALLOW_INCLUDES_DRIFT:-0}" != "1" ]]; then
        error "Set ALLOW_INCLUDES_DRIFT=1 only with a reviewed reason; default is fail."
        [[ $RC -eq 1 ]] || RC=2
    fi
else
    log_info "php_fbird_includes.h: main and pdo_fbird copies identical"
fi

if [[ $RC -ne 0 ]]; then
    error "Header parity check FAILED (exit ${RC}) - see #602."
fi
exit ${RC}
