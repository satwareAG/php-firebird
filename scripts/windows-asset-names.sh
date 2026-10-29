#!/usr/bin/env bash
# Print the exact 16 Windows asset names for a release version.
# Single source of truth for the smoke-marker checksum binding
# (release-windows.yml) and the publish checksum-binding step
# (publish-release.yml). Duplicated literals/patterns in those two
# files were the #687/#689 failure class (council run 6: 4/4
# MERGE-WITH-CHANGE - one shared list, no per-site globs).
set -euo pipefail
VERSION="${1:?usage: windows-asset-names.sh <version-without-v>}"
for php in 8.2 8.3 8.4 8.5; do
  case "$php" in
    8.2|8.3) vs=vs16 ;;
    *)       vs=vs17 ;;
  esac
  for ts in nts ts; do
    for pkg in php_firebird php_pdo_fbird; do
      echo "${pkg}-v${VERSION}-${php}-${ts}-${vs}-x86_64.zip"
    done
  done
done
