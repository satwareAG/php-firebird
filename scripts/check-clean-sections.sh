#!/usr/bin/env bash
# scripts/check-clean-sections.sh
# Audit: flag tests that create DDL objects but lack --CLEAN-- sections.
# Part of OC-4 (v12.0.0). Exits non-zero if violations found.
set -euo pipefail
source "$(dirname "$0")/lib/logging.sh"

cd "$(dirname "$0")/.."

violations=0
total_ddl=0
total_clean=0

# Find all .phpt files that contain DDL statements
for f in tests/*.phpt tests/coverage/*.phpt tests/pdo_fbird/*.phpt tests/pdo_fbird/conformance/*.phpt tests/parity/*.phpt tests/client_coverage/*.phpt tests/cross_version/*.phpt tests/amicron/*.phpt; do
	[ -f "$f" ] || continue

	# Check for DDL keywords (case-insensitive)
	if ! grep -qiE 'CREATE TABLE|CREATE PROCEDURE|CREATE TRIGGER|CREATE SEQUENCE|CREATE GENERATOR|CREATE INDEX|CREATE VIEW|CREATE DOMAIN|CREATE EXCEPTION|RECREATE TABLE|RECREATE PROCEDURE' "$f" 2>/dev/null; then
		continue
	fi

	total_ddl=$((total_ddl + 1))

	# Check for --CLEAN-- section
	if grep -q '^--CLEAN--' "$f" 2>/dev/null; then
		total_clean=$((total_clean + 1))
	else
		log_fail "$f - contains DDL but has no --CLEAN-- section"
		violations=$((violations + 1))
	fi
done

echo ""
log_info "DDL tests:       $total_ddl"
log_info "With --CLEAN--:  $total_clean"
log_info "Violations:      $violations"

if [ "$violations" -gt 0 ]; then
	echo ""
	log_error "$violations test(s) with DDL but no --CLEAN-- section"
	exit 1
fi

log_pass "All DDL tests have --CLEAN-- sections"
exit 0
