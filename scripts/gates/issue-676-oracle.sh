#!/usr/bin/env bash
# Oracle for issue #676: a red phpunit run inside the amicron gate must keep
# its failure identity (test name + assertion) in the gate output.
#
# Method: fixture platform checkout + stub php/composer/phpunit. The stub
# phpunit prints the failure block FIRST, then 30 testdox lines - exactly the
# shape where the pre-fix `tail -20` pipe cut the identity off. The gate
# script must exit nonzero AND surface the identity.
#
# Prints ISSUE676_IDENTITY_OK only if all assertions pass; exits 1 otherwise.
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

# --- stub phpunit: identity early, filler late (tail -20 loses the head) ---
cat > "$FIX/platform/vendor/bin/phpunit" <<'EOF'
#!/usr/bin/env bash
echo "There was 1 failure:"
echo ""
echo "1) Amicron\Demo\AdressenTest::test_roundtrip"
echo "   Failed asserting that 1 matches expected 2."
for i in $(seq 1 30); do echo " OK  Adressen roundtrip case $i"; done
exit 1
EOF
chmod +x "$FIX/platform/vendor/bin/phpunit"

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

rc=0
OUT="$("$REPO_ROOT/scripts/test-amicron-platform.sh" --skip-phpstan 2>&1)" || rc=$?

if [[ "$rc" -eq 0 ]]; then
    echo "oracle FAIL: gate exited 0 on a red phpunit run"
    exit 1
fi
# The gate must actually REACH the PHPUnit section - a prerequisite failure
# (exit 2 before phpunit) must not masquerade as a phpunit-path verdict.
if ! grep -q '=== PHPUnit ===' <<<"$OUT"; then
    echo "oracle FAIL: gate never reached the PHPUnit section (prerequisite failure)"
    exit 1
fi
if ! grep -qF 'Amicron\Demo\AdressenTest::test_roundtrip' <<<"$OUT"; then
    echo "oracle FAIL: failure identity lost in gate output (the #676 defect)"
    exit 1
fi
if ! grep -qF 'Failed asserting that 1 matches expected 2' <<<"$OUT"; then
    echo "oracle FAIL: assertion detail lost in gate output"
    exit 1
fi
echo "ISSUE676_IDENTITY_OK"
