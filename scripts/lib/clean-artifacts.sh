#!/bin/bash
# =============================================================================
# scripts/lib/clean-artifacts.sh - Shared test artifact cleanup
# =============================================================================
# Source this file, then call clean_test_artifacts [project_root]
#
# Removes stale test artifacts (*.diff, *.out, *.exp, *.log, *.mem) and
# top-level generated *.php/*.sh files from test directories.
# Protects tracked source files via skip-list.
# =============================================================================

clean_test_artifacts() {
    local root="${1:-.}"
    local cleaned=0

    for dir in tests tests/pdo_fbird; do
        if [ -d "$root/$dir" ]; then
            # Pass 1: recursive for unambiguous artifact extensions
            cleaned=$(( cleaned + $(find "$root/$dir" \
                \( -name '*.diff' -o -name '*.out' -o -name '*.exp' \
                   -o -name '*.log' -o -name '*.mem' \) \
                2>/dev/null | wc -l) ))
            find "$root/$dir" \
                \( -name '*.diff' -o -name '*.out' -o -name '*.exp' \
                   -o -name '*.log' -o -name '*.mem' \) \
                -delete 2>/dev/null || true
            # Pass 2: top-level only for *.php and *.sh
            # (protects tracked source files in subdirs like tests/sanitizer/)
            cleaned=$(( cleaned + $(find "$root/$dir" -maxdepth 1 \
                \( -name '*.php' -o -name '*.sh' \) \
                -not -name 'common.inc' -not -name 'config.inc' \
                -not -name 'firebird.inc' -not -name 'functions.inc' \
                -not -name 'skipif.inc' \
                2>/dev/null | wc -l) ))
            find "$root/$dir" -maxdepth 1 \
                \( -name '*.php' -o -name '*.sh' \) \
                -not -name 'common.inc' -not -name 'config.inc' \
                -not -name 'firebird.inc' -not -name 'functions.inc' \
                -not -name 'skipif.inc' \
                -delete 2>/dev/null || true
        fi
    done

    if [ "$cleaned" -gt 0 ]; then
        log_warn "Cleaned $cleaned test artifact(s) from previous run"
    fi
}
