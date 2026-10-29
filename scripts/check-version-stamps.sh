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

# js #672: the source itself must be a release version - sentinels and
# malformed strings must fail the gate, never pass through.
if ! echo "${EXPECTED}" | grep -qE '^[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z.+-]+)?$' || [[ "${EXPECTED}" == 0.0.0* ]]; then
  log_error "VERSION.txt is not a valid release version: '${EXPECTED}' (issue 672)"
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

# jane: specs/metadata.json "version" drifts silently between releases (found
# at 13.2.7 while VERSION.txt said 13.2.8); IPADP consumers read it. Same gate.
METADATA_FILE="${ROOT_DIR}/specs/metadata.json"
if [ -f "${METADATA_FILE}" ]; then
  # First top-level "version" key only (jq not a hard dep; sed suffices for the known layout).
  MD_ACTUAL=$(sed -n 's/^  "version":[[:space:]]*"\([^"]*\)".*/\1/p' "${METADATA_FILE}" | head -1)
  if [ "${MD_ACTUAL}" = "${EXPECTED}" ]; then
    log_pass "specs/metadata.json (version ${MD_ACTUAL})"
  else
    log_fail "specs/metadata.json (version ${MD_ACTUAL:-missing} != ${EXPECTED})"
    ERRORS=$((ERRORS + 1))
  fi
else
  log_warn "specs/metadata.json not found, skipping"
fi

# pdo_fbird/VERSION.txt is the version source for the split pdo_fbird extension
# (pdo_fbird/config.m4 reads it at configure time; IPADP split satwareAG/pdo-fbird
# bundles it). Drifted silently at 13.2.6 (found while preparing 13.2.11: the pdo
# build stamped 13.2.6 while VERSION.txt said 13.2.11). Same gate as metadata.json.
PDO_VERSION_FILE="${ROOT_DIR}/pdo_fbird/VERSION.txt"
if [ -f "${PDO_VERSION_FILE}" ]; then
  PV_ACTUAL=$(tr -d ' \n\r\t' < "${PDO_VERSION_FILE}")
  if [ "${PV_ACTUAL}" = "${EXPECTED}" ]; then
    log_pass "pdo_fbird/VERSION.txt (version ${PV_ACTUAL})"
  else
    log_fail "pdo_fbird/VERSION.txt (version ${PV_ACTUAL:-missing} != ${EXPECTED})"
    ERRORS=$((ERRORS + 1))
  fi
else
  log_warn "pdo_fbird/VERSION.txt not found, skipping"
fi

echo ""

# js #672 canaries: presence/absence assertions that keep the SSOT
# machinery from silently rotting (each has a negative control in
# scripts/version-gates.mjs canaries).
CANARY_ERRORS=0

canary_expect_match() { # file pattern description
  if grep -qE "$2" "$1" 2>/dev/null; then
    log_pass "canary: $3"
  else
    log_fail "canary: $3 (pattern '$2' not found in $1)"
    CANARY_ERRORS=$((CANARY_ERRORS + 1))
  fi
}

canary_expect_absent() { # file pattern description
  if grep -qE "$2" "$1" 2>/dev/null; then
    log_fail "canary: $3 (forbidden pattern '$2' found in $1)"
    CANARY_ERRORS=$((CANARY_ERRORS + 1))
  else
    log_pass "canary: $3"
  fi
}

canary_expect_match "${ROOT_DIR}/config.m4" "VERSION\.txt" "Linux configure reads VERSION.txt (SSOT)"
canary_expect_match "${ROOT_DIR}/config.w32" "VERSION\.txt" "Windows configure reads VERSION.txt (SSOT)"
canary_expect_match "${ROOT_DIR}/pdo_fbird/config.m4" "VERSION\.txt" "pdo Linux configure reads VERSION.txt (SSOT)"
canary_expect_match "${ROOT_DIR}/.github/workflows/windows-smoke.yml" "VERSION\.txt" "smoke lane derives tag from VERSION.txt"
canary_expect_absent "${ROOT_DIR}/.github/workflows/windows-smoke.yml" "DEFAULT_TAG:[[:space:]]*v[0-9]" "smoke lane has no hand-edited DEFAULT_TAG pin"
if [ -d "${ROOT_DIR}/packaging" ]; then
  if grep -rn "13\.0\.0-unknown" "${ROOT_DIR}/packaging" >/dev/null 2>&1; then
    log_fail "canary: packaging has no silent version fallback (found '13.0.0-unknown')"
    CANARY_ERRORS=$((CANARY_ERRORS + 1))
  else
    log_pass "canary: packaging has no silent version fallback"
  fi
fi

ERRORS=$((ERRORS + CANARY_ERRORS))

if [ "${ERRORS}" -gt 0 ]; then
  log_error "${ERRORS} stamp(s) have mismatched version tags."
  log_info "Fix: update @version / version fields in the listed files to match VERSION (${EXPECTED})."
  exit 1
fi

log_pass "All version stamps match VERSION.txt file."
