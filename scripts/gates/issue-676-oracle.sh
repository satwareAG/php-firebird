#!/usr/bin/env bash
# Oracle for issue #676: a red phpunit run inside the amicron gate must keep
# its failure identity (test name + assertion) in the gate output.
#
# Three stub variants (council run 9 adjustments A + D):
#   v1 "There was" block + 30 filler lines  - the classic tail -20 cut
#   v2 marker-less inline failure + 100 filler lines - no "There was" line,
#      log >60 lines: the tail -60 fallback loses the name (S4 audit finding)
#   v3 "There was" block + 200 filler lines - exercises the summary pipeline:
#      an early-closing limiter (head -80) under pipefail SIGPIPEs the extract
#      and the ERR trap aborts before the gate's final summary (S1/S2 finding)
#
# Each variant: gate must exit nonzero, reach the PHPUnit section, and surface
# the failing test name; v3 additionally must print the final summary line.
# Prints ISSUE676_IDENTITY_OK only if every variant passes; exits 1 otherwise.
set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
FIX="$(mktemp -d)"
STUB="$FIX/stub/bin"
mkdir -p "$FIX/platform/bin" "$FIX/platform/vendor/bin" "$STUB"
trap 'rm -rf "$FIX"' EXIT

# --- fixture platform checkout: branch 3.x-next, one commit, clean tree ---
export GIT_AUTHOR_NAME=oracle GIT_AUTHOR_EMAIL=oracle@local
export GIT_COMMITTER_NAME=oracle GIT_COMMITTER_EMAIL=oracle@local
git init -q -b 3.x-next "$FIX/platform"
echo '{"name":"acme/issue-676-fixture","description":"oracle fixture - not a real platform"}' > "$FIX/platform/composer.json"
echo '[]' > "$FIX/platform/composer.lock"
git -C "$FIX/platform" add -A
git -C "$FIX/platform" commit -qm "fixture"

# --- stub php: console calls succeed; -m reports the firebird extensions;
# --- everything else delegates to the real php (version checks etc.) ---
REAL_PHP="$(command -v php)"
export STUB_REAL_PHP="$REAL_PHP"
cat > "$STUB/php" <<'EOF'
#!/usr/bin/env bash
if [[ "${1:-}" == "bin/console" ]]; then
    case "$*" in
        *schema:validate*|*migrations:diff*) exit 0 ;;
        *) echo "console stub: unhandled invocation: $*" >&2; exit 1 ;;
    esac
fi
if [[ "${1:-}" == "-m" ]]; then
    # the fixture has no real extensions loaded; the gate only greps for these
    "$STUB_REAL_PHP" -m
    echo "firebird"
    echo "pdo_fbird"
    exit 0
fi
exec "$STUB_REAL_PHP" "$@"
EOF
chmod +x "$STUB/php"

# --- stub composer: succeed without touching the fixture lock ---
cat > "$STUB/composer" <<'EOF'
#!/usr/bin/env bash
exit 0
EOF
chmod +x "$STUB/composer"

export PATH="$STUB:$PATH"
export AMICRON_PLATFORM_DIR="$FIX/platform"
export DBPASS="${DBPASS:-masterkey}"   # schema-validation gate needs it set; stub console ignores the value

# --- the three phpunit stub variants ---
write_stub_v1() {
    cat > "$FIX/platform/vendor/bin/phpunit" <<'EOF'
#!/usr/bin/env bash
echo "There was 1 failure:"
echo ""
echo "1) Amicron\Demo\AdressenTest::test_roundtrip"
echo "   Failed asserting that 1 matches expected 2."
for i in $(seq 1 30); do echo " OK  Adressen roundtrip case $i"; done
exit 1
EOF
}

write_stub_v2() {
    # marker-less: no "There was" line; failure line early; log grows past 60
    cat > "$FIX/platform/vendor/bin/phpunit" <<'EOF'
#!/usr/bin/env bash
echo " OK  Bootstrap case 0"
echo " FAIL Amicron\Demo\AdressenTest::test_roundtrip"
echo "      Failed asserting that 1 matches expected 2."
for i in $(seq 1 100); do echo " OK  Adressen roundtrip case $i"; done
echo "FAILURES!"
echo "Tests: 101, Assertions: 101, Failures: 1."
exit 1
EOF
}

write_stub_v3() {
    # ~3000 filler lines (~95KB) > 64KB pipe buffer: an early-closing limiter
    # (head -80) now really SIGPIPEs the extraction mid-stream and the ERR
    # trap aborts before the gate's final summary line.
    cat > "$FIX/platform/vendor/bin/phpunit" <<'EOF'
#!/usr/bin/env bash
echo "There was 1 failure:"
echo ""
echo "1) Amicron\Demo\AdressenTest::test_roundtrip"
echo "   Failed asserting that 1 matches expected 2."
for i in $(seq 1 3000); do echo " OK  Adressen roundtrip case $i padding padding padding"; done
exit 1
EOF
}

run_variant() {
    local variant="$1" stub_fn="$2" expect_summary="$3"
    "$stub_fn"
    chmod +x "$FIX/platform/vendor/bin/phpunit"
    local rc=0
    OUT="$("$REPO_ROOT/scripts/test-amicron-platform.sh" --skip-phpstan 2>&1)" || rc=$?

    if [[ "$rc" -eq 0 ]]; then
        echo "oracle FAIL [$variant]: gate exited 0 on a red phpunit run"
        return 1
    fi
    if ! grep -q '=== PHPUnit ===' <<<"$OUT"; then
        echo "oracle FAIL [$variant]: gate never reached the PHPUnit section (prerequisite failure)"
        return 1
    fi
    if ! grep -qF 'Amicron\Demo\AdressenTest::test_roundtrip' <<<"$OUT"; then
        echo "oracle FAIL [$variant]: failure identity lost in gate output (the #676 defect)"
        return 1
    fi
    if ! grep -qF 'Failed asserting that 1 matches expected 2' <<<"$OUT"; then
        echo "oracle FAIL [$variant]: assertion detail lost in gate output"
        return 1
    fi
    if [[ "$expect_summary" == "yes" ]] && ! grep -qE 'CHECK\(S\) FAILED' <<<"$OUT"; then
        echo "oracle FAIL [$variant]: gate summary line missing (extraction pipeline aborted early)"
        return 1
    fi
}

FAILS=0
run_variant v1 write_stub_v1 no  || FAILS=$((FAILS + 1))
run_variant v2 write_stub_v2 no  || FAILS=$((FAILS + 1))
run_variant v3 write_stub_v3 yes || FAILS=$((FAILS + 1))

if [[ "$FAILS" -gt 0 ]]; then
    echo "oracle FAIL: $FAILS variant(s) failed"
    exit 1
fi
echo "ISSUE676_IDENTITY_OK"
