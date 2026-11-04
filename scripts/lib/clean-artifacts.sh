#!/bin/bash
# =============================================================================
# scripts/lib/clean-artifacts.sh - Shared test artifact cleanup
# =============================================================================
# Source this file, then call clean_test_artifacts [project_root] [--archive]
#
# Removes stale test artifacts (*.diff, *.out, *.exp, *.log, *.mem) and
# top-level generated *.php/*.sh files from test directories.
# Protects tracked source files via skip-list.
#
# Modes (second argument):
#   delete  (default) - remove artifacts outright. Use for stale-at-start
#           cleanup, where the artifacts are from a PREVIOUS day/run whose
#           triage window has passed.
#   archive - MOVE failure evidence to artifacts/test-failures/<run-id>/
#           instead of deleting (#718). Use between matrix lanes and at
#           final cleanup: run-tests.php leaves *.diff/*.out/*.exp/*.log
#           only for FAILING tests, so anything present after a lane is
#           evidence - deleting it made triage of intermittent failures
#           (#718 fbird_service_001) impossible.
#
# The retention dir is unbounded by design: entries are tiny and per-lane;
# prune manually once triaged.
# =============================================================================

# logging.sh may not be loaded in every consumer (gate oracles source this
# lib standalone); degrade to plain echo instead of dying under set -u.
if ! declare -F log_warn >/dev/null 2>&1; then
    log_warn() { echo "WARN: $*"; }
fi
if ! declare -F log_info >/dev/null 2>&1; then
    log_info() { echo "INFO: $*"; }
fi

clean_test_artifacts() {
    local root="${1:-.}"
    local mode="${2:-delete}"
    # Accept both "archive" and "--archive" spellings
    mode="${mode#--}"
    local cleaned=0
    local archived=0
    local archive_dir=""

    if [ "$mode" != "delete" ] && [ "$mode" != "archive" ]; then
        echo "clean_test_artifacts: unknown mode '$mode' (use delete|archive)" >&2
        return 1
    fi

    if [ "$mode" = "archive" ]; then
        archive_dir="$root/artifacts/test-failures/$(date +%Y%m%d-%H%M%S)-$$"
        if ! mkdir -p "$archive_dir" 2>/dev/null; then
            # Never fall back to deletion: archive requested but impossible
            # (disk full, root-owned dir) -> leave evidence in place (#718).
            log_warn "Could not create retention dir '$archive_dir' - leaving failure artifacts in place"
            return 0
        fi
        # Per-origin subdirs: tests/ and tests/pdo_fbird/ can carry the same
        # basename; a flat dir would let the second mv silently overwrite
        # the first (evidence loss).
        mkdir -p "$archive_dir/tests" "$archive_dir/tests/pdo_fbird" 2>/dev/null
    fi

    for dir in tests tests/pdo_fbird; do
        if [ -d "$root/$dir" ]; then
            # Pass 1: recursive for unambiguous artifact extensions
            cleaned=$(( cleaned + $(find "$root/$dir" \
                \( -name '*.diff' -o -name '*.out' -o -name '*.exp' \
                   -o -name '*.log' -o -name '*.mem' \) \
                2>/dev/null | wc -l) ))
            if [ -n "$archive_dir" ]; then
                while IFS= read -r f; do
                    # Guarded mv: consumers run set -e; one unreadable file
                    # (e.g. root-owned leftover from a root-run lane) must
                    # not abort the whole matrix (#718 review).
                    if mv "$f" "$archive_dir/$dir/"; then
                        archived=$((archived + 1))
                    else
                        log_warn "Could not archive '$f' - leaving in place"
                    fi
                done < <(find "$root/$dir" \
                    \( -name '*.diff' -o -name '*.out' -o -name '*.exp' \
                       -o -name '*.log' -o -name '*.mem' \) \
                    2>/dev/null)
            else
                find "$root/$dir" \
                    \( -name '*.diff' -o -name '*.out' -o -name '*.exp' \
                       -o -name '*.log' -o -name '*.mem' \) \
                    -delete 2>/dev/null || true
            fi
            # Pass 2: top-level only for *.php and *.sh
            # (protects tracked source files in subdirs like tests/sanitizer/)
            cleaned=$(( cleaned + $(find "$root/$dir" -maxdepth 1 \
                \( -name '*.php' -o -name '*.sh' \) \
                -not -name 'common.inc' -not -name 'config.inc' \
                -not -name 'firebird.inc' -not -name 'functions.inc' \
                -not -name 'skipif.inc' \
                2>/dev/null | wc -l) ))
            if [ -n "$archive_dir" ]; then
                while IFS= read -r f; do
                    # Guarded mv: see pass 1 (#718 review).
                    if mv "$f" "$archive_dir/$dir/"; then
                        archived=$((archived + 1))
                    else
                        log_warn "Could not archive '$f' - leaving in place"
                    fi
                done < <(find "$root/$dir" -maxdepth 1 \
                    \( -name '*.php' -o -name '*.sh' \) \
                    -not -name 'common.inc' -not -name 'config.inc' \
                    -not -name 'firebird.inc' -not -name 'functions.inc' \
                    -not -name 'skipif.inc' \
                    2>/dev/null)
            else
                find "$root/$dir" -maxdepth 1 \
                    \( -name '*.php' -o -name '*.sh' \) \
                    -not -name 'common.inc' -not -name 'config.inc' \
                    -not -name 'firebird.inc' -not -name 'functions.inc' \
                    -not -name 'skipif.inc' \
                    -delete 2>/dev/null || true
            fi
        fi
    done

    if [ "$archived" -gt 0 ]; then
        log_info "Archived $archived test failure artifact(s) to $archive_dir (#718 retention)"
    fi
    if [ "$mode" = "delete" ] && [ "$cleaned" -gt 0 ]; then
        log_warn "Cleaned $cleaned test artifact(s) from previous run"
    fi
}
