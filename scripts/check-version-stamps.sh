#!/usr/bin/env bash
# =============================================================================
# check-version-stamps.sh - Validate @version tags in stubs match VERSION file
# =============================================================================
# Ensures stubs shipped via Composer (satwareag/php-firebird-stubs) carry the
# correct version. Prevents stale @version tags from confusing consumers.
#
# Usage: bash scripts/check-version-stamps.sh
# Exit:  0 = all match, 1 = mismatch found
# =============================================================================

set -euo pipefail
source "$(dirname "$0")/lib/logging.sh"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

VERSION_FILE="${ROOT_DIR}/VERSION.txt"

if [ ! -f "${VERSION_FILE}" ]; then
  log_error "VERSION file not found at ${VERSION_FILE}"
  exit 1
fi

EXPECTED=$(tr -d '[:space:]' < "${VERSION_FILE}")

if [ -z "${EXPECTED}" ]; then
  log_error "VERSION file is empty"
  exit 1
fi

log_info "Expected @version: ${EXPECTED}"
echo ""

STUBS_FILES=(
  "stubs/firebird-stubs.php"
  "stubs/firebird-classes.php"
  "stubs/pdo-fbird-stubs.php"
)

ERRORS=0

for file in "${STUBS_FILES[@]}"; do
  FULL_PATH="${ROOT_DIR}/${file}"
  if [ ! -f "${FULL_PATH}" ]; then
    log_warn "${file} not found, skipping"
    continue
  fi

  # Extract @version value from PHPDoc header
  ACTUAL=$(grep -m1 '@version' "${FULL_PATH}" | sed 's/.*@version[[:space:]]*//' | tr -d '[:space:]')

  if [ "${ACTUAL}" = "${EXPECTED}" ]; then
    log_pass "${file} (@version ${ACTUAL})"
  else
    log_fail "${file} (@version ${ACTUAL} != ${EXPECTED})"
    ERRORS=$((ERRORS + 1))
  fi
done

echo ""

if [ "${ERRORS}" -gt 0 ]; then
  log_error "${ERRORS} stub(s) have mismatched @version tags."
  log_info "Fix: update @version in the listed files to match VERSION (${EXPECTED})."
  exit 1
fi

log_pass "All stubs @version tags match VERSION.txt file."
