#!/bin/bash
# scripts/run-sanitizer.sh
# Host-side wrapper for running sanitizer tests in Docker
# Usage: ./scripts/run-sanitizer.sh [options] [test_name]
#
# Options:
#   --container NAME   Container to use (default: php83-dev)
#   --mode MODE        asan|ubsan|all (default: asan)
#   --list             List available tests
#   --verbose          Show full output
#   --help             Show help

set -e

# Defaults
CONTAINER="php83-asan"
MODE="asan"
TEST_NAME=""
VERBOSE=false
LIST_TESTS=false

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
        --list)
            LIST_TESTS=true
            shift
            ;;
        --verbose)
            VERBOSE=true
            shift
            ;;
        --help)
            echo "Usage: $0 [options] [test_name]"
            echo "Options:"
            echo "  --container NAME   Container to use (default: php83-asan)"
            echo "  --mode MODE        asan|ubsan|all (default: asan)"
            echo "  --list             List available tests"
            echo "  --verbose          Show full output"
            exit 0
            ;;
        *)
            if [ -z "$TEST_NAME" ]; then
                TEST_NAME="$1"
                shift
            else
                echo "Unknown argument: $1"
                exit 1
            fi
            ;;
    esac
done

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DOCKER_DIR="$PROJECT_ROOT/docker"

# Ensure container is running
cd "$DOCKER_DIR"
if ! docker compose ps --services --filter "status=running" | grep -q "$CONTAINER"; then
    echo "Starting container $CONTAINER..."
    docker compose up -d "$CONTAINER"
fi

# Build command
CMD="/ext/scripts/analysis/sanitizers.sh"

if [ "$LIST_TESTS" = true ]; then
    CMD="$CMD --list"
elif [ -n "$TEST_NAME" ]; then
    CMD="$CMD --test $TEST_NAME"
fi

if [ "$VERBOSE" = true ]; then
    CMD="$CMD --verbose"
fi

# Add mode
CMD="$CMD $MODE"

# Run in container
echo "Running: $CMD"
docker compose exec -T "$CONTAINER" bash -c "$CMD"
