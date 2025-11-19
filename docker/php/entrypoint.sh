#!/bin/bash
set -e

# Link Firebird client libraries if needed
if [ ! -f /usr/lib/libfbclient.so ]; then
    ln -s /usr/lib/x86_64-linux-gnu/libfbclient.so /usr/lib/libfbclient.so
fi

exec "$@"
