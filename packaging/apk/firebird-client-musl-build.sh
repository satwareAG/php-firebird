#!/bin/sh
# =============================================================================
# packaging/apk/firebird-client-musl-build.sh
# =============================================================================
#
# Builds the Firebird C/C++ client library (libfbclient) from source for musl
# libc (Alpine Linux). The official FB5 tarball is glibc-linked and cannot be
# used on Alpine without this.
#
# Output: installs libfbclient.so + headers to $FB_ROOT
#
# Usage (inside alpine:3.21 container):
#   FB_ROOT=/tmp/fbclient sh packaging/apk/firebird-client-musl-build.sh
#
# =============================================================================

set -e

FB_ROOT="${FB_ROOT:-/tmp/fbclient}"
FB_VERSION="${FB_VERSION:-5.0.4}"
NPROC="${NPROC:-$(nproc 2>/dev/null || echo 2)}"

echo ">>> Building Firebird client v${FB_VERSION} for musl (Alpine)..."

mkdir -p "${FB_ROOT}"

# Clone Firebird source
FBSRC="/tmp/firebird-src-${FB_VERSION}"
if [ ! -d "${FBSRC}/.git" ]; then
    git clone --depth 1 --branch "v${FB_VERSION}" \
        https://github.com/FirebirdSQL/firebird.git "${FBSRC}"
fi

# Build the yvalve target (produces libfbclient.so).
# jane: Firebird v5.0.4 CMake does NOT support CLIENT_ONLY, install_client,
#       or install_headers targets. The client library target is "yvalve".
#       There are no CMake install() commands - must manually copy output.
mkdir -p "${FBSRC}/build"
cd "${FBSRC}/build"

cmake .. \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release

# Build only the yvalve (client library) target, not the full server.
# jane: yvalve still pulls in some server build dependencies via CMake
#       add_dependencies, so the full source tree compiles. This is heavier
#       than ideal but is the only way to build libfbclient from source.
ninja -j"${NPROC}" yvalve

# Manually copy the built library and headers to FB_ROOT.
# jane: Firebird CMake has no install() targets. Output is in build/gen/.
mkdir -p "${FB_ROOT}/lib" "${FB_ROOT}/include"

# Find the built library (output name is fbclient, symlinked as libfbclient)
cp -a "${FBSRC}/build/gen/libfbclient.so"* "${FB_ROOT}/lib/" 2>/dev/null || true

# If the library is elsewhere, search for it
if [ ! -f "${FB_ROOT}/lib/libfbclient.so" ]; then
    find "${FBSRC}/build" -name "libfbclient.so*" -exec cp -aL {} "${FB_ROOT}/lib/" \;
fi

# Copy public headers
cp -a "${FBSRC}/src/include/firebird" "${FB_ROOT}/include/"
cp -a "${FBSRC}/src/include/iberror.h" "${FB_ROOT}/include/" 2>/dev/null || true
# ibase.h is generated during build
find "${FBSRC}/build" -name "ibase.h" -exec cp {} "${FB_ROOT}/include/" \; 2>/dev/null || true

# Verify
if [ ! -f "${FB_ROOT}/lib/libfbclient.so" ]; then
    echo "ERROR: libfbclient.so not found after build" >&2
    find "${FBSRC}/build" -name "*.so*" | head -20
    return 1
fi

echo ">>> Firebird client installed to ${FB_ROOT}"
ls -la "${FB_ROOT}/lib/libfbclient.so"*
