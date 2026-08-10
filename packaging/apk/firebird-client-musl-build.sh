#!/bin/sh
# =============================================================================
# packaging/apk/firebird-client-musl-build.sh
# =============================================================================
#
# Builds the Firebird C/C++ client library (libfbclient) from source for musl
# libc (Alpine Linux). The official FB5 tarball is glibc-linked and cannot be
# used on Alpine without this.
#
# Uses autotools (configure + make), NOT CMake. The CMake build is a broken
# third-party effort: it references misc/makeHeader.cpp and src/msgs/
# facilities2.sql, both deleted from the Firebird source tree in 2019/2021
# (commits 45d5e3aa, ee088c22). The Firebird team does not use CMake
# (see FirebirdSQL/firebird#7152). Autotools with --enable-client-only
# builds only the yvalve (libfbclient) + headers, avoiding the missing
# bootstrap artifacts entirely.
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

echo ">>> Building Firebird client v${FB_VERSION} for musl (Alpine) via autotools..."

mkdir -p "${FB_ROOT}"

# Clone Firebird source
FBSRC="/tmp/firebird-src-${FB_VERSION}"
if [ ! -d "${FBSRC}/.git" ]; then
    git clone --depth 1 --branch "v${FB_VERSION}" \
        https://github.com/FirebirdSQL/firebird.git "${FBSRC}"
fi

cd "${FBSRC}"

# Generate configure from configure.ac (git checkout has no pre-generated configure).
# jane: autogen.sh runs autoreconf --install --force --verbose then configure.
#       NOCONFIGURE=1 skips the configure step in autogen.sh so we can pass
#       our own flags.
export NOCONFIGURE=1
./autogen.sh

# Configure: client-only build, no server, no editline, no tomcrypt.
# jane: --enable-client-only sets CLIENT_ONLY_FLG=Y in Makefile.in, which
#       skips engine, fbintl, utilities, gpre, plugins, examples. Only
#       yvalve (libfbclient) and include_generic (headers) are built.
#       --without-tomcrypt avoids a dependency we don't need for the client.
./configure \
    --enable-client-only \
    --without-tomcrypt \
    --with-builtin-tommath \
    --prefix="${FB_ROOT}"

# Build the client library and headers.
# jane: The full client-only build sequence is:
#   make external               (cloop, decNumber, int128 - prerequisites)
#   make updateCloopInterfaces  (generate IdlFbInterfaces.h from IDL)
#   make yvalve                 (libfbclient.so)
#   make include_generic        (copy public headers to build dir)
#   make updateBuildNum         (writeBuildNum.h - needed by some headers)
#   make export_lists           (linker symbol export lists)
# We call them explicitly because `make` alone (master_process) also builds
# the server when CLIENT_ONLY_FLG=N (the default if configure wasn't run
# correctly). With --enable-client-only, the master_process skips server
# targets, but calling prerequisites explicitly is safer and clearer.
make -j"${NPROC}" external
make -j"${NPROC}" updateCloopInterfaces
make -j"${NPROC}" yvalve
make -j"${NPROC}" include_generic

# Manually copy the built library and headers to FB_ROOT.
# jane: Firebird autotools has an install target but it tries to install
#       the full server layout. We only need the client library + headers.
mkdir -p "${FB_ROOT}/lib" "${FB_ROOT}/include"

# Find and copy the built library.
# jane: autotools outputs to gen/Native/firebird/lib/ (TARGET=Native default).
FB_BUILD_LIB="${FBSRC}/gen/Native/firebird/lib"
if [ ! -f "${FB_BUILD_LIB}/libfbclient.so" ]; then
    # Search alternative locations
    # jane: guard with || true - set -e would exit silently if find returns empty
    FB_BUILD_LIB=$(find "${FBSRC}/gen" -name "libfbclient.so" -print -quit 2>/dev/null | xargs -r dirname 2>/dev/null || true)
fi
cp -a "${FB_BUILD_LIB}/libfbclient.so"* "${FB_ROOT}/lib/" 2>/dev/null || true

# Copy public headers from the build output directory.
# jane: include_generic copies headers to gen/Native/firebird/include/.
FB_BUILD_INC="${FBSRC}/gen/Native/firebird/include"
if [ -d "${FB_BUILD_INC}/firebird" ]; then
    cp -a "${FB_BUILD_INC}/firebird" "${FB_ROOT}/include/"
fi
cp -a "${FB_BUILD_INC}/iberror.h" "${FB_ROOT}/include/" 2>/dev/null || true
cp -a "${FB_BUILD_INC}/ib_util.h" "${FB_ROOT}/include/" 2>/dev/null || true
# ibase.h is in the firebird/ subdirectory (flattened by include_generic)
cp -a "${FB_BUILD_INC}/firebird/ibase.h" "${FB_ROOT}/include/" 2>/dev/null || \
    cp -a "${FBSRC}/src/include/ibase.h" "${FB_ROOT}/include/" 2>/dev/null || true

# Verify
if [ ! -f "${FB_ROOT}/lib/libfbclient.so" ]; then
    echo "ERROR: libfbclient.so not found after build" >&2
    find "${FBSRC}/gen" -name "*.so*" | head -20
    return 1
fi

echo ">>> Firebird client installed to ${FB_ROOT}"
ls -la "${FB_ROOT}/lib/libfbclient.so"*
