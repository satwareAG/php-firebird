#!/bin/bash
# =============================================================================
# scripts/lib/detect-firebird.sh - Shared Firebird client library detection
# =============================================================================
# Source this file, then call detect_firebird
#
# Sets the following variables:
#   FIREBIRD_PATH      - Installation prefix (/opt/firebird, /usr, etc.)
#   FIREBIRD_INCLUDE   - Include directory
#   CONFIGURE_ARGS     - Arguments for ./configure (--with-firebird=...)
#
# Detection order:
#   1. FIREBIRD_HOME env var (if set and directory exists)
#   2. fb_config on PATH (system package manager install)
#   3. /opt/firebird (standard Firebird install location)
#   4. /usr (fallback for distro packages)
# =============================================================================

detect_firebird() {
    if [ -n "${FIREBIRD_HOME:-}" ] && [ -d "$FIREBIRD_HOME" ]; then
        FIREBIRD_PATH="$FIREBIRD_HOME"
        FIREBIRD_INCLUDE="$FIREBIRD_HOME/include"
        CONFIGURE_ARGS="--with-firebird=$FIREBIRD_PATH"
        log_info "Using FIREBIRD_HOME: $FIREBIRD_PATH"
    elif command -v fb_config >/dev/null 2>&1; then
        # System-installed libfbclient — let config.m4 query fb_config for
        # cflags, libs, and Firebird 3.0+ version check.
        FIREBIRD_PATH="$(dirname "$(dirname "$(command -v fb_config)")")"
        FIREBIRD_INCLUDE="$(fb_config --cflags)"
        CONFIGURE_ARGS="--with-firebird=yes"
        log_info "Using system fb_config ($(fb_config --version)): $FIREBIRD_PATH"
    elif [ -d "/opt/firebird" ]; then
        FIREBIRD_PATH="/opt/firebird"
        FIREBIRD_INCLUDE="/opt/firebird/include"
        CONFIGURE_ARGS="--with-firebird=$FIREBIRD_PATH"
        log_info "Auto-detected Firebird at /opt/firebird"
    else
        FIREBIRD_PATH="/usr"
        FIREBIRD_INCLUDE="/usr/include/firebird"
        CONFIGURE_ARGS="--with-firebird=$FIREBIRD_PATH"
        log_info "Using system Firebird at /usr"
    fi
}
