#!/usr/bin/env bash
# check-smart-str-imports.sh - #639 regression gate.
#
# Fails when a built module references smart_str_* dynamic symbols.
# The smart_str helper smart_str_ferealloc is NOT exported by php8.dll on
# PHP 8.5.0-8.5.11 Windows runtimes (6121-symbol export set starts at
# 8.5.12), so any reference makes the DLL unloadable there (#639, v13.2.9
# windows-smoke UNLOADABLE leg). The pdo_fbird TUs must not use smart_str.
#
# Works on ELF modules (nm -D undefined dynsym) and PE modules (objdump -p
# import table). A module type with no available inspector is skipped with
# a warning; references found => exit 1.
#
# Usage: check-smart-str-imports.sh <module> [<module> ...]
set -euo pipefail
source "$(dirname "$0")/lib/logging.sh"

if [ $# -lt 1 ]; then
    log_error "usage: $0 <module> [<module> ...]"
    exit 2
fi

rc=0
for f in "$@"; do
    if [ ! -f "$f" ]; then
        log_error "$f: not found"
        rc=1
        continue
    fi
    magic=$(head -c 2 "$f")
    hits=""
    case "$magic" in
        $'\x7fE')
            if command -v nm >/dev/null 2>&1; then
                hits=$(nm -D --undefined-only "$f" 2>/dev/null | awk '{print $NF}' | grep '^smart_str_' || true)
            else
                log_warn "$f: ELF module but nm not available, skipped"
                continue
            fi
            ;;
        MZ)
            if command -v objdump >/dev/null 2>&1; then
                hits=$(objdump -p "$f" 2>/dev/null | grep -i 'smart_str_' || true)
            else
                log_warn "$f: PE module but objdump not available, skipped (windows-smoke lane is the gate)"
                continue
            fi
            ;;
        *)
            log_warn "$f: unknown module format (magic '$magic'), skipped"
            continue
            ;;
    esac
    if [ -n "$hits" ]; then
        log_fail "$f: references smart_str_* symbols (unexported by php8.dll <= 8.5.11, #639):"
        echo "$hits" | sed 's/^/    /'
        rc=1
    else
        log_pass "$f: no smart_str_* references"
    fi
done

exit $rc
