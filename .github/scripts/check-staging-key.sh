#!/usr/bin/env bash
# check-staging-key.sh - staging APT signing key watchdog (#666)
#
# Asserts, for the committed packaging/debian/staging-gpg.pub.asc:
#   1. fingerprint matches the documented staging key
#   2. expiry is further out than STAGING_MIN_DAYS (key dies 2027-07-24)
#   3. the key verifies the LIVE packages.auc.de apt Release signature
#      (catches a server-side rotation that did not update this repo)
#
# Env overrides (for testing):
#   STAGING_KEY_FPR   expected fingerprint (default B600F371...FED8)
#   STAGING_MIN_DAYS  minimum days to expiry (default 60)
#   STAGING_APT_BASE  apt repo base (default https://packages.auc.de/apt)
#
# Prints STAGING-KEY-OK only after all assertions pass.

set -euo pipefail

KEY="${1:?usage: check-staging-key.sh <key.asc>}"
STAGING_KEY_FPR="${STAGING_KEY_FPR:-B600F3717315AD4262AB81A95AB7B54BBBC7FED8}"
STAGING_MIN_DAYS="${STAGING_MIN_DAYS:-60}"
STAGING_APT_BASE="${STAGING_APT_BASE:-https://packages.auc.de/apt}"

fail() { echo "FAIL: $*" >&2; exit 1; }

[ -f "$KEY" ] || fail "key file not found: $KEY"

# 1. fingerprint (drift/corruption of the committed copy)
gpg --show-keys --with-colons "$KEY" | grep -Eq "^fpr:.*:${STAGING_KEY_FPR}:" \
    || fail "fingerprint mismatch (expected ${STAGING_KEY_FPR})"

# 2. expiry (rotation tripwire)
exp=$(gpg --show-keys --with-colons "$KEY" | awk -F: '/^pub/ {print $7; exit}')
[ -n "$exp" ] && [ "$exp" != "0" ] || fail "could not parse expiry timestamp"
now=$(date +%s)
deadline=$((now + STAGING_MIN_DAYS * 86400))
[ "$exp" -gt "$deadline" ] \
    || fail "key expires in <= ${STAGING_MIN_DAYS} days ($(date -u -d @"$exp" +%F)) - rotate now (satis-deploy owns rotation, this repo's copy follows)"

# 3. live signature (server-side rotation / substitution tripwire)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
curl -fsSL "${STAGING_APT_BASE}/dists/bookworm/Release" -o "$tmp/Release"
curl -fsSL "${STAGING_APT_BASE}/dists/bookworm/Release.gpg" -o "$tmp/Release.gpg"
gpg --dearmor < "$KEY" > "$tmp/keyring.gpg"
out=$(gpgv --keyring "$tmp/keyring.gpg" "$tmp/Release.gpg" "$tmp/Release" 2>&1) \
    || fail "gpgv rejected the live Release signature: $out"
echo "$out" | grep -q "Good signature" || fail "no Good signature in gpgv output: $out"
echo "$out" | grep -q "packages@auc.de" || fail "unexpected signer: $out"

echo "STAGING-KEY-OK"
