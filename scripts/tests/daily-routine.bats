#!/usr/bin/env bats
# tests/daily-routine.bats — Unit tests for scripts/daily-routine.sh

SCRIPT="$(cd "$(dirname "$BATS_TEST_FILENAME")/.." && pwd)/daily-routine.sh"

setup() {
    # Create an isolated temporary git repo for each test
    TEST_REPO="$(mktemp -d)"
    git -C "$TEST_REPO" init -q
    git -C "$TEST_REPO" config user.email "test@example.com"
    git -C "$TEST_REPO" config user.name "Test"
    # Initial commit so HEAD exists
    touch "$TEST_REPO/README"
    git -C "$TEST_REPO" add README
    git -C "$TEST_REPO" commit -q -m "init"
}

teardown() {
    rm -rf "$TEST_REPO"
}

# Helper: run the script with REPO_ROOT pointing at our temp repo
run_eod() {
    REPO_ROOT="$TEST_REPO" bash "$SCRIPT" eod
}

# ---------------------------------------------------------------------------
# Unknown / missing command
# ---------------------------------------------------------------------------

@test "no arguments prints usage and exits 1" {
    run bash "$SCRIPT"
    [ "$status" -eq 1 ]
    [[ "$output" == *"Usage:"* ]]
}

@test "unknown command prints usage and exits 1" {
    run bash "$SCRIPT" foobar
    [ "$status" -eq 1 ]
    [[ "$output" == *"Usage:"* ]]
}

# ---------------------------------------------------------------------------
# eod — clean working directory
# ---------------------------------------------------------------------------

@test "eod exits 0 when working directory is clean" {
    run run_eod
    [ "$status" -eq 0 ]
}

@test "eod prints header with project name" {
    run run_eod
    [[ "$output" == *"php-firebird"* ]]
}

@test "eod prints branch name" {
    run run_eod
    [[ "$output" == *"Branch"* ]]
}

@test "eod prints last commit" {
    run run_eod
    [[ "$output" == *"Commit"* ]]
}

@test "eod prints clean status message when no uncommitted changes" {
    run run_eod
    [[ "$output" == *"Working directory clean"* ]]
}

@test "eod prints EOD master checklist" {
    run run_eod
    [[ "$output" == *"EOD Master Checklist"* ]]
    [[ "$output" == *"Validation"* ]]
    [[ "$output" == *"Hygiene"* ]]
    [[ "$output" == *"Git"* ]]
}

@test "eod prints workflow references" {
    run run_eod
    [[ "$output" == *"eod.hygiene-git.standards.md"* ]]
    [[ "$output" == *"eod.knowledge-documentation.md"* ]]
    [[ "$output" == *"eod.ops-automation.md"* ]]
}

# ---------------------------------------------------------------------------
# eod — dirty working directory
# ---------------------------------------------------------------------------

@test "eod exits 1 when there are uncommitted changes" {
    echo "dirty" > "$TEST_REPO/dirty.txt"
    run run_eod
    [ "$status" -eq 1 ]
}

@test "eod prints warning when uncommitted changes exist" {
    echo "dirty" > "$TEST_REPO/dirty.txt"
    run run_eod
    [[ "$output" == *"Uncommitted changes detected"* ]]
}

@test "eod prints ACTION REQUIRED when dirty" {
    echo "dirty" > "$TEST_REPO/dirty.txt"
    run run_eod
    [[ "$output" == *"ACTION REQUIRED"* ]]
}

@test "eod does not print clean message when dirty" {
    echo "dirty" > "$TEST_REPO/dirty.txt"
    run run_eod
    [[ "$output" != *"Working directory clean"* ]]
}
