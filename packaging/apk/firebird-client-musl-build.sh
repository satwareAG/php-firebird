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

# Configure: client-only build, no server, built-in tommath + tomcrypt.
# jane: --enable-client-only sets CLIENT_ONLY_FLG=Y in Makefile.in, which
#       skips engine, fbintl, utilities, gpre, plugins, examples. Only
#       yvalve (libfbclient) and include_generic (headers) are built.
#       --with-builtin-tommath uses bundled tommath (Alpine lacks libtommath-dev).
#       --with-builtin-tomcrypt builds bundled tomcrypt from extern/libtomcrypt/.
#       jane: --without-tomcrypt is broken upstream: TomCryptHash.cpp
#       unconditionally includes <tomcrypt.h> with no preprocessor guard.
#       Must build tomcrypt, not skip it.
./configure \
    --enable-client-only \
    --with-builtin-tommath \
    --with-builtin-tomcrypt \
    --prefix="${FB_ROOT}"

# Build the client library and headers.
# jane: Use `make` alone (not individual targets) because the Firebird
#       build system requires the `all` -> `firebird` -> `master_process`
#       chain to set TARGET=Release, create the autoconfig.h symlink, and
#       run `rest` (generates iberror_c.h). Calling `make yvalve` directly
#       skips these prerequisites and produces broken/missing output.
#       With --enable-client-only, master_process skips all server targets
#       (engine, fbintl, utilities, gpre, plugins, examples) and only builds
#       yvalve (libfbclient) + include_generic (headers).
make -j"${NPROC}"

# Manually copy the built library and headers to FB_ROOT.
# jane: Firebird autotools has an install target but it tries to install
#       the full server layout. We only need the client library + headers.
#       With TARGET=Release (set by master_process), output goes to
#       gen/Release/firebird/. The gen/Native/ path is cross-compile only.
mkdir -p "${FB_ROOT}/lib" "${FB_ROOT}/include"

# Find and copy the built library.
# jane: autotools outputs to gen/Release/firebird/lib/ on Linux.
FB_BUILD_LIB="${FBSRC}/gen/Release/firebird/lib"
if [ ! -f "${FB_BUILD_LIB}/libfbclient.so" ]; then
    # Search alternative locations (e.g. if TARGET differs)
    # jane: guard with || true - set -e would exit silently if find returns empty
    FB_BUILD_LIB=$(find "${FBSRC}/gen" -name "libfbclient.so" -print -quit 2>/dev/null | xargs -r dirname 2>/dev/null || true)
fi
cp -a "${FB_BUILD_LIB}/libfbclient.so"* "${FB_ROOT}/lib/" 2>/dev/null || true

# Copy dependency shared libraries that libfbclient links against.
# jane: libfbclient.so has NEEDED entries for libtommath.so.0 (and
#       libtomcrypt.so if built). Without these in FB_ROOT/lib, the
#       PHP extension configure check fails with "undefined reference
#       to mp_*" because the linker cannot resolve tommath symbols.
#       Use cp -aL (dereference) because the Firebird build output has
#       ABSOLUTE symlinks pointing to /tmp/firebird-src-.../extern/.
#       cp -a would copy the symlinks as-is, breaking at runtime when
#       the build directory no longer exists.
cp -aL "${FB_BUILD_LIB}/libtommath.so"* "${FB_ROOT}/lib/" 2>/dev/null || true
cp -aL "${FB_BUILD_LIB}/libtomcrypt.so"* "${FB_ROOT}/lib/" 2>/dev/null || true

# Copy public headers from the build output directory.
# jane: include_generic copies headers to gen/Release/firebird/include/.
FB_BUILD_INC="${FBSRC}/gen/Release/firebird/include"
if [ -d "${FB_BUILD_INC}/firebird" ]; then
    cp -a "${FB_BUILD_INC}/firebird" "${FB_ROOT}/include/"
fi
cp -a "${FB_BUILD_INC}/iberror.h" "${FB_ROOT}/include/" 2>/dev/null || \
    cp -a "${FBSRC}/src/include/iberror.h" "${FB_ROOT}/include/" 2>/dev/null || true
cp -a "${FB_BUILD_INC}/ib_util.h" "${FB_ROOT}/include/" 2>/dev/null || \
    cp -a "${FBSRC}/src/include/ib_util.h" "${FB_ROOT}/include/" 2>/dev/null || true
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
