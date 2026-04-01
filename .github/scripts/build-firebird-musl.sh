#!/bin/bash
# =============================================================================
# Build Firebird Client Library from Source for musl-libc (Alpine/musllinux)
# =============================================================================
#
# Firebird has no pre-built musl binaries. This script builds libfbclient
# from the official source tarball inside a musl-based container.
#
# Usage:
#   ./build-firebird-musl.sh [--arch x86_64|aarch64] [--fb-version 5.0.3]
#
# Environment:
#   GITHUB_TOKEN  - GitHub token for authenticated downloads (optional)
#   FB_ROOT       - Installation prefix [default: /opt/firebird]
#
# Output:
#   ${FB_ROOT}/lib/libfbclient.so*
#   ${FB_ROOT}/include/ibase.h
#   ${FB_ROOT}/include/firebird/*.h
#
# =============================================================================

set -euo pipefail

# Configuration
ARCH="${1:-x86_64}"
FB_VERSION="${2:-5.0.3}"
FB_BUILD="${3:-1683}"
FB_ROOT="${FB_ROOT:-/opt/firebird}"

# Parse named args
while [[ $# -gt 0 ]]; do
  case $1 in
    --arch) ARCH="$2"; shift 2 ;;
    --fb-version) FB_VERSION="$2"; shift 2 ;;
    --fb-build) FB_BUILD="$2"; shift 2 ;;
    *) shift ;;
  esac
done

echo "=== Building Firebird ${FB_VERSION} client for musl (${ARCH}) ==="

# ---- Step 1: Install build dependencies via apk ----
echo ">>> Installing build dependencies..."
apk add --no-cache \
  autoconf automake bison make gcc g++ git curl wget \
  libtool libxml2-dev sqlite-dev openssl-dev zlib-dev \
  curl-dev oniguruma-dev icu-dev icu-libs ncurses-dev \
  linux-headers patchelf binutils file tar xz \
  re2-dev libedit-dev coreutils bash perl \
  libc-dev musl-dev

# ---- Step 2: Download Firebird source ----
echo ">>> Downloading Firebird ${FB_VERSION} source..."
FB_SRC_URL="https://github.com/FirebirdSQL/firebird/releases/download/v${FB_VERSION}/Firebird-${FB_VERSION}.${FB_BUILD}-0-source.tar.xz"

CURL_OPTS=(--retry 3 --retry-delay 5 --retry-connrefused -fsSL)
if [ -n "${GITHUB_TOKEN:-}" ]; then
  CURL_OPTS+=(-H "Authorization: token ${GITHUB_TOKEN}")
fi

mkdir -p /tmp/fb-src
for attempt in 1 2 3; do
  echo "Download attempt ${attempt}/3..."
  if curl "${CURL_OPTS[@]}" "${FB_SRC_URL}" -o /tmp/fb-source.tar.xz; then
    FSIZE=$(stat -c%s /tmp/fb-source.tar.xz 2>/dev/null || echo "0")
    if [ "${FSIZE}" -gt 5000000 ]; then
      echo "Download succeeded (${FSIZE} bytes)"
      break
    fi
    rm -f /tmp/fb-source.tar.xz
  fi
  [ "${attempt}" -lt 3 ] && sleep $((attempt * 10))
done

if [ ! -f /tmp/fb-source.tar.xz ]; then
  echo "ERROR: Failed to download Firebird source" >&2
  exit 1
fi

echo ">>> Extracting source..."
tar -xJf /tmp/fb-source.tar.xz -C /tmp/fb-src --strip-components=1

# ---- Step 3: Patch config.guess/config.sub for musl recognition ----
echo ">>> Patching config.guess/config.sub for musl..."
cd /tmp/fb-src

# Download latest GNU config.guess and config.sub that recognize musl
for cfg_file in config.guess config.sub; do
  if curl -fsSL --retry 2 \
    "https://git.savannah.gnu.org/cgit/config.git/plain/${cfg_file}" \
    -o "/tmp/${cfg_file}" 2>/dev/null; then
    # Replace all instances in the source tree
    find /tmp/fb-src -name "${cfg_file}" -exec cp "/tmp/${cfg_file}" {} \;
    echo "  Updated ${cfg_file} in source tree"
  else
    echo "  WARNING: Could not download ${cfg_file}, using existing"
  fi
done

# ---- Step 4: Configure Firebird (client-only) ----
echo ">>> Configuring Firebird (client library only)..."
cd /tmp/fb-src

# autoreconf if needed
if [ -f autogen.sh ]; then
  chmod +x autogen.sh
  ./autogen.sh 2>/dev/null || true
fi

# Firebird configure flags for client-only musl build:
# --with-builtin-tommath/tomcrypt: Avoids missing Alpine packages
# --without-fbsample/fbintl: Skip unnecessary components
CONFIGURE_OPTS=(
  "--prefix=${FB_ROOT}"
  "--with-builtin-tommath"
  "--with-builtin-tomcrypt"
  "--without-fbsample"
  "--without-fbintl"
)

# Run configure
./configure "${CONFIGURE_OPTS[@]}" 2>&1 || {
  echo ">>> Configure failed. Checking config.log..."
  tail -50 config.log 2>/dev/null || true
  exit 1
}

# ---- Step 5: Build (client library only) ----
echo ">>> Building Firebird client library..."

# Build the full project first (Firebird's build system requires this)
make -j"$(nproc)" 2>&1 || {
  echo ">>> Full build failed, attempting client-only target..."
  # Some Firebird versions support building just the client
  make -j"$(nproc)" libfbclient 2>&1 || make -j"$(nproc)" client_only 2>&1 || {
    echo "ERROR: Firebird build failed" >&2
    exit 1
  }
}

# ---- Step 6: Install client library and headers ----
echo ">>> Installing to ${FB_ROOT}..."
mkdir -p "${FB_ROOT}/lib" "${FB_ROOT}/include/firebird"

# Find the Firebird build output lib directory
# Firebird places all built libraries (libfbclient, libtommath, libtomcrypt, etc.)
# in gen/Release/firebird/lib/
# IMPORTANT: gen/Debug/firebird/lib/ may also exist (empty). Prefer Release.
FB_BUILD_LIB=""
for candidate in Release Debug; do
  _d="/tmp/fb-src/gen/${candidate}/firebird/lib"
  if [ -d "${_d}" ] && ls "${_d}"/libfbclient.so* >/dev/null 2>&1; then
    FB_BUILD_LIB="${_d}"
    break
  fi
done
if [ -z "${FB_BUILD_LIB}" ]; then
  FB_BUILD_LIB=$(find /tmp/fb-src/gen -path "*/firebird/lib" -type d -print -quit 2>/dev/null)
fi
if [ -z "${FB_BUILD_LIB}" ]; then
  FB_BUILD_LIB=$(dirname "$(find /tmp/fb-src -name "libfbclient.so*" -type f -print -quit 2>/dev/null)")
fi

if [ -z "${FB_BUILD_LIB}" ] || [ ! -d "${FB_BUILD_LIB}" ]; then
  echo "ERROR: Firebird build lib directory not found" >&2
  echo "Build output structure:"
  find /tmp/fb-src/gen -name "*.so*" -type f 2>/dev/null | head -20
  exit 1
fi

echo "  Build lib dir: ${FB_BUILD_LIB}"
echo "  Contents:"
ls -la "${FB_BUILD_LIB}/"

# Copy ALL shared libraries from the build output - not just libfbclient.
# libfbclient.so has DT_NEEDED entries for libtommath.so, libtomcrypt.so, etc.
# that were built from Firebird's bundled sources (--with-builtin-tommath/tomcrypt).
# If we only copy libfbclient.so, the linker test in PHP's configure fails because
# the transitive dependencies cannot be resolved.
echo "  Copying all shared libraries from build lib dir..."
for f in "${FB_BUILD_LIB}"/*.so*; do
  [ -e "$f" ] && cp -a "$f" "${FB_ROOT}/lib/" 2>/dev/null || true
done

# Also search for transitive dependencies (libtommath, libtomcrypt, etc.) that
# may have been built in extern/ subdirectories via libtool (.libs/).
# libfbclient.so has DT_NEEDED entries for these; without them the linker test
# in PHP's configure will fail with "libfbclient not found".
echo "  Searching for transitive dependency libraries..."
for dep_lib in libtommath libtomcrypt; do
  DEP_SO=$(find /tmp/fb-src -path "*/.libs/${dep_lib}.so*" -type f 2>/dev/null | head -1)
  if [ -z "${DEP_SO}" ]; then
    DEP_SO=$(find /tmp/fb-src -name "${dep_lib}.so*" -type f 2>/dev/null | head -1)
  fi
  if [ -n "${DEP_SO}" ]; then
    DEP_DIR=$(dirname "${DEP_SO}")
    echo "  Found ${dep_lib} in ${DEP_DIR}"
    for f in "${DEP_DIR}"/${dep_lib}.so*; do
      [ -e "$f" ] && cp -a "$f" "${FB_ROOT}/lib/" 2>/dev/null || true
    done
  else
    echo "  ${dep_lib} not found as shared lib (may be statically linked)"
  fi
done

# Ensure symlinks exist
cd "${FB_ROOT}/lib"
if [ ! -e libfbclient.so ]; then
  REAL=$(ls libfbclient.so.* 2>/dev/null | head -1 || true)
  [ -n "${REAL}" ] && ln -sf "${REAL}" libfbclient.so
fi

# Copy headers
echo "  Copying headers..."
# ibase.h is the primary header
find /tmp/fb-src -path "*/include/ibase.h" -exec cp {} "${FB_ROOT}/include/" \; 2>/dev/null
find /tmp/fb-src -path "*/include/iberror.h" -exec cp {} "${FB_ROOT}/include/" \; 2>/dev/null

# Copy firebird subdirectory headers (Interface.h etc.)
FB_INCLUDE_DIR=$(find /tmp/fb-src -path "*/include/firebird" -type d -print -quit)
if [ -n "${FB_INCLUDE_DIR}" ]; then
  cp -r "${FB_INCLUDE_DIR}"/* "${FB_ROOT}/include/firebird/" 2>/dev/null || true
fi

# Also check gen/Release/firebird/include for generated headers
GEN_INCLUDE=$(find /tmp/fb-src/gen -path "*/include" -type d -print -quit 2>/dev/null || true)
if [ -n "${GEN_INCLUDE}" ]; then
  cp -n "${GEN_INCLUDE}"/*.h "${FB_ROOT}/include/" 2>/dev/null || true
  [ -d "${GEN_INCLUDE}/firebird" ] && cp -rn "${GEN_INCLUDE}/firebird/"* "${FB_ROOT}/include/firebird/" 2>/dev/null || true
fi

# ---- Step 7: Verify installation ----
echo ">>> Verifying Firebird SDK installation..."
echo "Libraries:"
ls -la "${FB_ROOT}/lib/"
echo ""
echo "Headers:"
ls -la "${FB_ROOT}/include/"

if [ ! -f "${FB_ROOT}/lib/libfbclient.so" ]; then
  echo "ERROR: libfbclient.so not found at ${FB_ROOT}/lib/" >&2
  exit 1
fi

if [ ! -f "${FB_ROOT}/include/ibase.h" ]; then
  echo "ERROR: ibase.h not found at ${FB_ROOT}/include/" >&2
  exit 1
fi

# Register library path with musl dynamic linker so that transitive dependencies
# (libtommath.so, libtomcrypt.so) can be found during PHP configure link tests.
# The GNU linker's -L flag does NOT resolve DT_NEEDED entries - only -rpath-link
# or system paths work for that. On musl, /etc/ld-musl-*.path is the config file.
MUSL_LD_PATH="/etc/ld-musl-${ARCH}.path"
if [ ! -f "${MUSL_LD_PATH}" ] || ! grep -q "${FB_ROOT}/lib" "${MUSL_LD_PATH}" 2>/dev/null; then
  echo "${FB_ROOT}/lib" >> "${MUSL_LD_PATH}"
  echo "  Registered ${FB_ROOT}/lib in ${MUSL_LD_PATH}"
fi

# Also run ldconfig if available (glibc systems)
ldconfig 2>/dev/null || true

echo ""
echo "=== Firebird ${FB_VERSION} client library built successfully for musl ==="

# Cleanup
rm -rf /tmp/fb-src /tmp/fb-source.tar.xz /tmp/config.guess /tmp/config.sub
