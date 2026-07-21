#!/usr/bin/env bash
# =============================================================================
# packaging/debian/build-matrix.sh
# =============================================================================
#
# Builds all .deb packages for php-firebird across the full matrix:
#   4 PHP versions (8.2, 8.3, 8.4, 8.5)
#   x 4 distros (bookworm, trixie, jammy, noble)
#   x 2 archs (x86_64, aarch64)
#   = 32 packages
#
# Usage:
#   packaging/debian/build-matrix.sh [options]
#
# Options:
#   --php-versions 8.4,8.5     Comma-separated PHP versions (default: all)
#   --distros bookworm,noble   Comma-separated distros (default: all)
#   --archs x86_64             Comma-separated archs (default: all)
#   --output-dir dist/deb      Output directory (default: dist/deb)
#   --verbose                  Verbose output
#   --help                     Show this help
#
# This script runs Docker containers for each combination, invoking
# packaging/debian/build.sh inside each one.
#
# =============================================================================

set -euo pipefail

# -----------------------------------------------------------------------------
# Defaults
# -----------------------------------------------------------------------------

PHP_VERSIONS="8.2,8.3,8.4,8.5"
DISTROS="bookworm,trixie,jammy,noble"
ARCHS="x86_64,aarch64"
OUTPUT_DIR="dist/deb"
VERBOSE=0

# -----------------------------------------------------------------------------
# Logging
# -----------------------------------------------------------------------------

log()    { echo "[$(date +%H:%M:%S)] $*" >&2; }
log_info() { echo "[$(date +%H:%M:%S)] INFO: $*" >&2; }
log_pass() { echo "[$(date +%H:%M:%S)] PASS: $*" >&2; }
log_fail() { echo "[$(date +%H:%M:%S)] FAIL: $*" >&2; }
log_verbose() { [ "$VERBOSE" -eq 1 ] && echo "[$(date +%H:%M:%S)] >>> $*" >&2 || true; }

# -----------------------------------------------------------------------------
# Argument parsing
# -----------------------------------------------------------------------------

usage() {
    sed -n '3,30p' "$0" | sed 's/^# \{0,1\}//'
    exit 0
}

while [ $# -gt 0 ]; do
    case "$1" in
        --php-versions) PHP_VERSIONS="$2"; shift 2 ;;
        --php-versions=*) PHP_VERSIONS="${1#--php-versions=}"; shift ;;
        --distros) DISTROS="$2"; shift 2 ;;
        --distros=*) DISTROS="${1#--distros=}"; shift ;;
        --archs) ARCHS="$2"; shift 2 ;;
        --archs=*) ARCHS="${1#--archs=}"; shift ;;
        --output-dir) OUTPUT_DIR="$2"; shift 2 ;;
        --output-dir=*) OUTPUT_DIR="${1#--output-dir=}"; shift ;;
        --verbose|-v) VERBOSE=1; shift ;;
        --help|-h) usage ;;
        *) echo "ERROR: Unknown argument: $1" >&2; exit 1 ;;
    esac
done

# -----------------------------------------------------------------------------
# Resolve paths
# -----------------------------------------------------------------------------

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
cd "$REPO_ROOT"

mkdir -p "$OUTPUT_DIR"

# -----------------------------------------------------------------------------
# Parse comma-separated lists
# -----------------------------------------------------------------------------

IFS=',' read -ra PHP_VER_LIST <<< "$PHP_VERSIONS"
IFS=',' read -ra DISTRO_LIST <<< "$DISTROS"
IFS=',' read -ra ARCH_LIST <<< "$ARCHS"

TOTAL=$(( ${#PHP_VER_LIST[@]} * ${#DISTRO_LIST[@]} * ${#ARCH_LIST[@]} ))

log_info "=== php-firebird .deb Build Matrix ==="
log_info "  PHP versions: ${PHP_VERSIONS}"
log_info "  Distros:       ${DISTROS}"
log_info "  Architectures: ${ARCHS}"
log_info "  Total packages: $TOTAL"
log_info "  Output: $OUTPUT_DIR"
echo ""

# -----------------------------------------------------------------------------
# Docker image mapping
# -----------------------------------------------------------------------------

# Map distro name to Docker image
get_docker_image() {
    local distro="$1"
    local arch="$2"
    local php_ver="$3"

    # Validate arch (the --platform flag handles arch mapping)
    case "$arch" in
        x86_64|amd64|aarch64|arm64) ;;
        *) echo ""; return 1 ;;
    esac

    case "$distro" in
        # Debian: official PHP Docker images exist with Debian codenames
        bookworm) echo "php:${php_ver}-cli-bookworm" ;;
        trixie)   echo "php:${php_ver}-cli-trixie" ;;
        # Ubuntu: no official PHP Docker images (only Debian codenames exist).
        # Use Ubuntu base image + install PHP from packages.sury.org PPA.
        # jane: PHP Docker images only use Debian codenames (bookworm, trixie),
        # never Ubuntu codenames (jammy, noble). The Sury PPA provides PHP for
        # both Debian AND Ubuntu suites.
        jammy)    echo "ubuntu:22.04" ;;
        noble)    echo "ubuntu:24.04" ;;
        *) echo ""; return 1 ;;
    esac
}

# Check if a distro is Ubuntu (needs Sury PPA for PHP)
is_ubuntu() {
    case "$1" in
        jammy|noble) echo "true" ;;
        *) echo "false" ;;
    esac
}

# Map arch name to Docker platform
get_docker_platform() {
    local arch="$1"
    case "$arch" in
        x86_64|amd64) echo "linux/amd64" ;;
        aarch64|arm64) echo "linux/arm64" ;;
        *) echo ""; return 1 ;;
    esac
}

# -----------------------------------------------------------------------------
# Build function (single package)
# -----------------------------------------------------------------------------

build_package() {
    local php_ver="$1"
    local distro="$2"
    local arch="$3"
    local idx="$4"

    local docker_image=$(get_docker_image "$distro" "$arch" "$php_ver")
    local platform=$(get_docker_platform "$arch")

    if [ -z "$docker_image" ]; then
        log_fail "[$idx/$TOTAL] $php_ver/$distro/$arch: unknown distro"
        return 1
    fi

    local deb_name="php${php_ver}-firebird_$(cat VERSION.txt 2>/dev/null || echo 0.0.0)-1_$(echo $arch | sed 's/x86_64/amd64/;s/aarch64/arm64/').deb"
    # jane: per-distro subdirectory to avoid filename collisions across distros
    # (bookworm/trixie/jammy/noble all produce the same .deb filename)
    local deb_subdir="${OUTPUT_DIR}/${distro}"
    local deb_path="${deb_subdir}/${deb_name}"

    mkdir -p "$deb_subdir"

    # Skip if already built
    if [ -f "$deb_path" ]; then
        log_pass "[$idx/$TOTAL] $php_ver/$distro/$arch: cached ($deb_name)"
        return 0
    fi

    log "[$idx/$TOTAL] Building $php_ver/$distro/$arch ($docker_image)..."

    # Run build.sh inside Docker container
    local result_log="/tmp/build-${php_ver//./}-${distro}-${arch}.log"
    local exit_code=0
    local abs_output="$(cd "${deb_subdir}" && pwd)"

    # Pre-fetch FB5 client on HOST (Docker container can't reach GitHub API reliably)
    # jane: uses /tmp/fb-test-{x64,arm64} naming from fetch-client.sh --arch
    local arch_short="x64"
    case "$arch" in
        aarch64|arm64) arch_short="arm64" ;;
        x86_64|amd64) arch_short="x64" ;;
    esac
    local fb_host_dir="/tmp/fb-test-${arch_short}"
    if [ ! -f "${fb_host_dir}/lib/libfbclient.so" ]; then
        log_verbose "  Pre-fetching FB5 client for $arch on host..."
        bash "${REPO_ROOT}/packaging/fb-client-bundle/fetch-client.sh" \
            --arch "$arch" --output-dir "$fb_host_dir" 2>&1 | tail -5 || true
    fi

    # Mount the FB5 client from host into the container (read-write for multiarch symlinks)
    local fb_mount="/opt/firebird"
    local fb_vol=""
    [ -d "$fb_host_dir" ] && fb_vol="-v ${fb_host_dir}:${fb_mount}"

    # Out-of-tree build: source mounted READ-ONLY, build happens in /build (ephemeral)
    # jane: No Docker volume for /build — the container is --rm, so /build is
    # automatically cleaned when the container exits. The source tree at /src
    # is never written to.
    local build_dir="/build"

    docker run --rm \
        --platform "$platform" \
        -v "${REPO_ROOT}:/src:ro" \
        -v "${abs_output}:/output" \
        $fb_vol \
        -e PHP_VERSION="$php_ver" \
        -e DISTRO="$distro" \
        -e ARCH="$arch" \
        -e OUTPUT_DIR="/output" \
        -e FB_ROOT="${fb_mount}" \
        -e BUILD_DIR="${build_dir}" \
        -e SOURCE_DIR="/src" \
        "$docker_image" \
        bash -c "export DEBIAN_FRONTEND=noninteractive && \
                 $(if [ "$(is_ubuntu "$distro")" = "true" ]; then
                     echo "apt-get update -qq && \
                     apt-get install -y -qq lsb-release ca-certificates curl && \
                     curl -sSLo /tmp/keyring.deb https://packages.sury.org/debsuryorg-archive-keyring.deb && \
                     dpkg -i /tmp/keyring.deb && \
                     echo 'Types: deb' > /etc/apt/sources.list.d/php.sources && \
                     echo 'URIs: https://packages.sury.org/php/' >> /etc/apt/sources.list.d/php.sources && \
                     echo 'Suites: $distro' >> /etc/apt/sources.list.d/php.sources && \
                     echo 'Components: main' >> /etc/apt/sources.list.d/php.sources && \
                     echo 'Signed-By: /usr/share/keyrings/debsuryorg-archive-keyring.gpg' >> /etc/apt/sources.list.d/php.sources && \
                     apt-get update -qq && \
                     apt-get install -y -qq --no-install-recommends \
                         php${php_ver}-cli php${php_ver}-dev \
                         autoconf build-essential gcc g++ make libtool bison re2c \
                         libicu-dev libxml2-dev libtommath1 patchelf dpkg-dev debhelper \
                         jq curl wget git ca-certificates &&"
                 else
                     echo "apt-get update -qq && \
                     apt-get install -y -qq --no-install-recommends \
                         autoconf build-essential gcc g++ make libtool bison re2c \
                         libicu-dev libxml2-dev libtommath1 patchelf dpkg-dev debhelper \
                         jq curl wget git ca-certificates &&"
                 fi) \
                 bash /src/packaging/debian/build.sh --php-version $php_ver --distro $distro --arch $arch --output-dir /output --build-dir ${build_dir}" \
        > "$result_log" 2>&1 || exit_code=$?

    if [ $exit_code -eq 0 ]; then
        # Verify .deb was produced (exact path, not glob — avoids false-positive from other distros)
        if [ -f "${abs_output}/${deb_name}" ]; then
            log_pass "[$idx/$TOTAL] $php_ver/$distro/$arch: $deb_name"
            return 0
        else
            log_fail "[$idx/$TOTAL] $php_ver/$distro/$arch: build OK but no .deb found"
            return 1
        fi
    else
        log_fail "[$idx/$TOTAL] $php_ver/$distro/$arch: build failed (exit $exit_code, see $result_log)"
        tail -10 "$result_log" >&2
        return 1
    fi
}

# -----------------------------------------------------------------------------
# Generate APT repo metadata
# -----------------------------------------------------------------------------

generate_apt_metadata() {
    log_info "=== Generating APT repo metadata ==="

    # Check for dpkg-scanpackages on host
    if ! command -v dpkg-scanpackages >/dev/null 2>&1; then
        log "dpkg-scanpackages not found on host — skipping APT metadata (install dpkg-dev)"
        return 0
    fi

    # For each distro, create the dists/ structure from per-distro subdir
    for distro in "${DISTRO_LIST[@]}"; do
        local dist_dir="${OUTPUT_DIR}/dists/${distro}/main"
        local distro_pool="${OUTPUT_DIR}/${distro}"

        for arch in "${ARCH_LIST[@]}"; do
            local deb_arch=$(echo "$arch" | sed 's/x86_64/amd64/;s/aarch64/arm64/')
            local arch_dir="${dist_dir}/binary-${deb_arch}"
            mkdir -p "$arch_dir"

            # Scan only this distro's subdirectory (not all of OUTPUT_DIR)
            local deb_count=0
            if [ -d "$distro_pool" ]; then
                deb_count=$(find "$distro_pool" -name "*.deb" -type f 2>/dev/null | wc -l)
            fi

            if [ "$deb_count" -gt 0 ]; then
                dpkg-scanpackages --arch "$deb_arch" "$distro_pool" /dev/null 2>/dev/null | gzip -9 > "${arch_dir}/Packages.gz"
                log "  $distro/$deb_arch: $deb_count packages"
            fi
        done
    done

    # Generate Release file for each distro
    for distro in "${DISTRO_LIST[@]}"; do
        local dist_dir="${OUTPUT_DIR}/dists/${distro}"
        if [ -d "$dist_dir" ]; then
            cat > "${dist_dir}/Release" << EOF
Origin: satware AG
Label: php-firebird
Suite: ${distro}
Codename: ${distro}
Architectures: $(echo "$ARCHS" | sed 's/x86_64/amd64/g;s/aarch64/arm64/g' | tr ',' ' ')
Components: main
Description: php-firebird native packages for ${distro}
Date: $(date -Ru)
EOF
            log "  $distro: Release file generated"
        fi
    done

    log_info "APT metadata generated in ${OUTPUT_DIR}/dists/"
}

# -----------------------------------------------------------------------------
# Main build loop
# -----------------------------------------------------------------------------

PASSED=0
FAILED=0
FAILED_LIST=()
idx=0

for php_ver in "${PHP_VER_LIST[@]}"; do
    for distro in "${DISTRO_LIST[@]}"; do
        for arch in "${ARCH_LIST[@]}"; do
            idx=$((idx + 1))
            if build_package "$php_ver" "$distro" "$arch" "$idx"; then
                PASSED=$((PASSED + 1))
            else
                FAILED=$((FAILED + 1))
                FAILED_LIST+=("${php_ver}/${distro}/${arch}")
            fi
        done
    done
done

# -----------------------------------------------------------------------------
# Generate APT metadata
# -----------------------------------------------------------------------------

if [ $PASSED -gt 0 ]; then
    generate_apt_metadata
fi

# -----------------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------------

echo ""
log_info "=== Build Matrix Summary ==="
log_info "  Passed: $PASSED / $TOTAL"
log_info "  Failed: $FAILED / $TOTAL"

if [ $FAILED -gt 0 ]; then
    log_fail "  Failed combinations:"
    for f in "${FAILED_LIST[@]}"; do
        echo "    - $f"
    done
fi

echo ""
log_info "Output directory: $OUTPUT_DIR"
find "${OUTPUT_DIR}" -name "*.deb" -type f 2>/dev/null | head -20

if [ $FAILED -eq 0 ]; then
    log_pass "=== All packages built successfully ==="
    exit 0
else
    log_fail "=== Some packages failed ==="
    exit 1
fi
