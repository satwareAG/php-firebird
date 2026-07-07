#!/bin/bash
# scripts/run-valgrind.sh
# Host-side wrapper for running Valgrind memory checks in Docker
#
# Purpose:
#   Complementary to ASan for detecting uninitialized memory reads
#   (a class of bugs ASan cannot catch)
#
# Usage:
#   ./scripts/run-valgrind.sh [options] [mode]
#
# Options:
#   --container NAME   Container to use (default: php83-dev)
#   --mode MODE        quick|full|tests (default: quick)
#   --help             Show this help
#
# Examples:
#   ./scripts/run-valgrind.sh                      # Quick mode
#   ./scripts/run-valgrind.sh --mode full           # Full mode
#   ./scripts/run-valgrind.sh --mode tests          # PHPT test mode
#   ./scripts/run-valgrind.sh --container php84-dev # Use PHP 8.4 container
#
# See: docs/research/asan-vs-valgrind-php-extensions.md

set -e

source "$(dirname "$0")/lib/logging.sh"

# Defaults
CONTAINER="php83-dev"
MODE="quick"

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --container)
            CONTAINER="$2"
            shift 2
            ;;
        --mode)
            MODE="$2"
            shift 2
            ;;
        --help|-h)
            echo "Usage: $0 [options] [mode]"
            echo ""
            echo "Options:"
            echo "  --container NAME   Container to use (default: php83-dev)"
            echo "  --mode MODE        quick|full|tests (default: quick)"
            echo "  --help             Show this help"
            echo ""
            echo "Modes:"
            echo "  quick   Extension load + client version check"
            echo "  full    Quick + connection + error handling tests"
            echo "  tests   Curated PHPT test subset under Valgrind"
            exit 0
            ;;
        *)
            echo "Unknown argument: $1"
            exit 1
            ;;
    esac
done

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DOCKER_DIR="$PROJECT_ROOT/docker"

# Ensure container is running
cd "$DOCKER_DIR"
if ! docker compose ps --services --filter "status=running" | grep -q "$CONTAINER"; then
    log_info "Starting container $CONTAINER..."
    docker compose up -d "$CONTAINER"
fi

# Build command
CMD="/ext/scripts/analysis/valgrind.sh --$MODE"

log_info "Running: $CMD in $CONTAINER"
docker compose exec -T "$CONTAINER" bash -c "$CMD"
