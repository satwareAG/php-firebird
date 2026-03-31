#!/usr/bin/env bash
set -euo pipefail
IFS=$'\n\t'

# check-resource-lifecycle.sh - Static analysis for PHP resource lifecycle issues
#
# Scans C source files for zend_register_resource() calls and cross-references
# with zend_list_delete()/zend_list_close() to flag potential resource leaks.
#
# This is a heuristic tool - it cannot prove correctness, but it catches common
# patterns like:
#   - Functions that register resources without any visible delete/close path
#   - New zend_register_resource() calls added without corresponding cleanup
#
# Usage: bash scripts/analysis/check-resource-lifecycle.sh
# Exit code: 0 if all checks pass, 1 if potential issues found

show_help() {
    echo "Usage: $0 [-v|--verbose] [-h|--help]"
    echo ""
    echo "Scans C sources for resource lifecycle issues (register without delete)."
    echo ""
    echo "Options:"
    echo "  -v, --verbose   Show all resource registrations, not just potential issues"
    echo "  -h, --help      Show this help message"
}

VERBOSE=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        -v|--verbose) VERBOSE=1; shift ;;
        -h|--help) show_help; exit 0 ;;
        *) echo "Unknown option: $1"; show_help; exit 1 ;;
    esac
done

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

issues=0
total_registers=0

echo "=== Resource Lifecycle Analysis ==="
echo ""

# 1. Find all zend_register_resource() calls with context
echo "--- zend_register_resource() calls ---"
while IFS=: read -r file line content; do
    total_registers=$((total_registers + 1))

    # Skip return-to-userland pattern: RETVAL_RES(zend_register_resource(...))
    # These intentionally hand ownership to PHP userland; cleanup happens via
    # resource destructor when PHP GC collects or request shuts down.
    trimmed=$(echo "$content" | sed 's/^[[:space:]]*//')
    if [[ "$trimmed" == RETVAL_RES* ]]; then
        if [[ "$VERBOSE" -eq 1 ]]; then
            echo -e "${GREEN}OK${NC}: $file:$line - returned to userland (RETVAL_RES)"
        fi
        continue
    fi

    # Extract function name containing this line
    func_name=$(awk -v target="$line" '
        /^[a-zA-Z_].*\(/ && !/;$/ { current_func = $0; gsub(/\(.*/, "", current_func) }
        NR == target { print current_func; exit }
    ' "$file" 2>/dev/null || echo "unknown")

    # Check if the same file has a matching zend_list_delete or zend_list_close
    # within the same function scope (heuristic: within 200 lines)
    line_num=$((line))
    search_start=$((line_num > 200 ? line_num - 200 : 1))
    search_end=$((line_num + 200))

    has_delete=$(sed -n "${search_start},${search_end}p" "$file" | grep -c 'zend_list_delete\|zend_list_close' || true)

    # Internal resources (e.g., result_query->res in _php_fbird_exec) are cleaned
    # up by the caller function, not within _php_fbird_exec itself. Check if the
    # resource is stored in a struct field (caller-managed pattern).
    is_struct_field=0
    if echo "$content" | grep -q -- '->res.*=' ; then
        is_struct_field=1
    fi

    if [[ "$has_delete" -eq 0 ]] && [[ "$is_struct_field" -eq 0 ]]; then
        echo -e "${YELLOW}WARNING${NC}: $file:$line - zend_register_resource() in $func_name"
        echo "         No zend_list_delete/close within 200 lines"
        echo "         $content"
        issues=$((issues + 1))
    elif [[ "$has_delete" -eq 0 ]] && [[ "$is_struct_field" -eq 1 ]]; then
        if [[ "$VERBOSE" -eq 1 ]]; then
            echo -e "${GREEN}OK${NC}: $file:$line - $func_name (struct field, caller manages lifecycle)"
        fi
    elif [[ "$VERBOSE" -eq 1 ]]; then
        echo -e "${GREEN}OK${NC}: $file:$line - $func_name (has $has_delete delete/close nearby)"
    fi
done < <(grep -rn 'zend_register_resource(' "$PROJECT_ROOT"/*.c "$PROJECT_ROOT"/pdo_fbird/*.c 2>/dev/null | grep -v '^\s*//' || true)

echo ""

# 2. Check for ownership pattern consistency
echo "--- owns_stmt_handle pattern audit ---"
# Every function that sets owns_stmt_handle=1 on a result should also
# set the parent's owns_stmt_handle=0 and fbs_statement=NULL
ownership_transfers=$(grep -n 'result_query->owns_stmt_handle = 1' "$PROJECT_ROOT"/*.c 2>/dev/null || true)
if [[ -n "$ownership_transfers" ]]; then
    while IFS=: read -r file line content; do
        # Check that within 5 lines, the parent's fields are cleared
        line_num=$((line))
        end=$((line_num + 5))
        context=$(sed -n "${line_num},${end}p" "$file")

        has_parent_null=$(echo "$context" | grep -c 'ib_query->fbs_statement = NULL' || true)
        has_parent_disown=$(echo "$context" | grep -c 'ib_query->owns_stmt_handle = 0' || true)

        if [[ "$has_parent_null" -eq 0 ]] || [[ "$has_parent_disown" -eq 0 ]]; then
            echo -e "${RED}ERROR${NC}: $file:$line - Ownership transfer without parent cleanup!"
            echo "         Missing: ib_query->fbs_statement=NULL ($has_parent_null) or owns_stmt_handle=0 ($has_parent_disown)"
            issues=$((issues + 1))
        elif [[ "$VERBOSE" -eq 1 ]]; then
            echo -e "${GREEN}OK${NC}: $file:$line - Ownership transfer pattern correct"
        fi
    done <<< "$ownership_transfers"
else
    echo "No ownership transfer patterns found (expected in fbird_query_exec.c)"
fi

echo ""

# 3. Check destructors call fbs_free() when owns_stmt_handle is set
echo "--- Resource destructor audit ---"
destructors=$(grep -n 'php_fbird_free_.*rsrc\|_php_fbird_free_.*_rsrc' "$PROJECT_ROOT"/*.c 2>/dev/null || true)
if [[ -n "$destructors" ]]; then
    while IFS=: read -r file line content; do
        # Check destructor has fbs_free or owns_stmt_handle check
        end=$((line + 80))
        context=$(sed -n "${line},${end}p" "$file")
        has_fbs_free=$(echo "$context" | grep -c 'fbs_free' || true)
        has_owns_check=$(echo "$context" | grep -c 'owns_stmt_handle' || true)

        if [[ "$has_fbs_free" -eq 0 ]] && [[ "$has_owns_check" -eq 0 ]]; then
            if [[ "$VERBOSE" -eq 1 ]]; then
                echo -e "${YELLOW}NOTE${NC}: $file:$line - Destructor without fbs_free (may be non-statement resource)"
            fi
        elif [[ "$VERBOSE" -eq 1 ]]; then
            echo -e "${GREEN}OK${NC}: $file:$line - Destructor has fbs_free/owns_stmt_handle check"
        fi
    done <<< "$destructors"
fi

echo ""

# 4. Summary
echo "=== Summary ==="
echo "Total zend_register_resource() calls scanned: $total_registers"

if [[ "$issues" -eq 0 ]]; then
    echo -e "${GREEN}No resource lifecycle issues detected.${NC}"
    exit 0
else
    echo -e "${RED}$issues potential issue(s) found.${NC}"
    echo "Review each WARNING/ERROR above. Some may be false positives"
    echo "(e.g., cleanup via caller function or goto label in a different range)."
    exit 1
fi
