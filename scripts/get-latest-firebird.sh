#!/bin/bash
# scripts/get-latest-firebird.sh
# Dynamically resolves the latest Firebird release for a given major version.
#
# Usage: ./scripts/get-latest-firebird.sh <major_version>
# Example: ./scripts/get-latest-firebird.sh 5
#
# Outputs:
#   FB_VERSION=5.0.3
#   FB_URL=https://...
#   FB_EXTRACT_DIR=Firebird-5.0.3.1683-0-linux-x64

set -euo pipefail

MAJOR_VERSION="${1:-5}"
GITHUB_REPO="FirebirdSQL/firebird"
GITHUB_API="https://api.github.com/repos/${GITHUB_REPO}"

# Build curl options as an array to avoid word-splitting issues with the
# Authorization header value when GITHUB_TOKEN contains special characters.
CURL_OPTS=(-s -f --retry 3 --retry-delay 5)
if [ -n "${GITHUB_TOKEN:-}" ]; then
    CURL_OPTS+=(-H "Authorization: token ${GITHUB_TOKEN}")
fi

# 1. Resolve latest tag for major version
# Filters: start with 'v', match major version, exclude Beta/RC/Release candidates
TAG=$(curl "${CURL_OPTS[@]}" "${GITHUB_API}/releases" | \
    jq -r '.[].tag_name' | \
    grep -E "^v${MAJOR_VERSION}\." | \
    grep -vE "(Beta|RC|Release)" | \
    sort -V | tail -n 1)

if [ -z "${TAG}" ]; then
    echo "ERROR: Could not resolve latest tag for Firebird ${MAJOR_VERSION}" >&2
    exit 1
fi

# 2. Fetch release details to get assets
RELEASE_JSON=$(curl "${CURL_OPTS[@]}" "${GITHUB_API}/releases/tags/${TAG}")

# 3. Identify correct asset and download URL
# FB 3/4 use amd64.tar.gz; FB 5 uses linux-x64.tar.gz
# Exclude debugSymbols and debuginfo
case "${MAJOR_VERSION}" in
    3|4)
        ASSET_MATCH='amd64\\.tar\\.gz$'
        ;;
    5)
        ASSET_MATCH='linux-x64\\.tar\\.gz$'
        ;;
    *)
        echo "ERROR: Unsupported major version ${MAJOR_VERSION}" >&2
        exit 1
        ;;
esac

ASSET_NAME=$(echo "${RELEASE_JSON}" | jq -r ".assets[].name | select(test(\"${ASSET_MATCH}\") and (test(\"(debugSymbols|debuginfo)\") | not))" | head -n 1)
ASSET_URL=$(echo "${RELEASE_JSON}" | jq -r ".assets[] | select(.name == \"${ASSET_NAME}\") | .browser_download_url")

if [ -z "${ASSET_URL}" ]; then
    echo "ERROR: Could not find suitable .tar.gz asset for ${TAG}" >&2
    exit 1
fi

# 4. Resolve extraction directory
# Usually the asset name minus .tar.gz
EXTRACT_DIR="${ASSET_NAME%.tar.gz}"

# Output results
echo "FB_VERSION=${TAG#v}"
echo "FB_URL=${ASSET_URL}"
echo "FB_EXTRACT_DIR=${EXTRACT_DIR}"
echo "FB_TARBALL=${ASSET_NAME}"
