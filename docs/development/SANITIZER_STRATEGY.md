# Sanitizer Strategy for PHP Firebird Extension

## Overview

This document outlines the strategy for using dynamic analysis tools (Sanitizers) to ensure memory safety and correctness in the PHP Firebird extension. This strategy is designed to be robust, future-proof, and compatible with modern PHP versions (8.3+).

## Toolchain Selection

Based on research into the state of PHP extension development in late 2025, we utilize the standard LLVM/Clang sanitizer suite. These tools remain the industry standard for C/C++ memory safety analysis.

| Tool | Purpose | Status |
|------|---------|--------|
| **AddressSanitizer (ASan)** | Detects buffer overflows, use-after-free, and memory leaks. | **Essential** |
| **UndefinedBehaviorSanitizer (UBSan)** | Detects integer overflows, null pointer dereferences, alignment issues, and other undefined behaviors. | **Essential** |
| **LeakSanitizer (LSan)** | Detects memory leaks (integrated with ASan). | **Essential** |
| **Valgrind (Memcheck)** | Detects memory management problems. Used as a complementary check to ASan. | **Complementary** |

## Implementation Strategy

### 1. AddressSanitizer (ASan)

*   **Environment**: Runs in a dedicated Docker container (`php83-asan`) built with a custom PHP build.
*   **Build Flags**: PHP is compiled with `-fsanitize=address -fno-omit-frame-pointer -g -O1`.
*   **Execution**: The extension is compiled against this ASan-enabled PHP.
*   **Privileges**: The ASan container runs as **root** (UID 0) to ensure full access to memory introspection features (ptrace) and to avoid issues with `LD_PRELOAD` or seccomp filters often found in restricted Docker environments.
*   **Cleanup**: Because build artifacts are created as root, they must be explicitly cleaned up by the test runner (`scripts/qa.sh`) before subsequent steps can run as a standard user.

### 2. UndefinedBehaviorSanitizer (UBSan)

*   **Environment**: Runs in the standard development container (`php83-dev`).
*   **Build Flags**: The extension is compiled with `-fsanitize=undefined,integer,nullability -fno-sanitize-recover=all`.
*   **Execution**: Runs as a standard user (UID 1000).
*   **Prerequisites**: Requires a clean build directory. The QA script ensures artifacts from the ASan run are removed before this phase begins.

### 3. Valgrind

*   **Environment**: Runs in the standard development container (`php83-dev`).
*   **Execution**: Runs on a standard (non-sanitized) build of the extension.
*   **Purpose**: Catches issues that might be missed by ASan or that manifest differently without compiler instrumentation.

## Workflow Integration

The `scripts/qa.sh` script orchestrates the entire process in the `full` or `security` modes:

1.  **ASan Phase**:
    *   Starts `php83-asan`.
    *   Builds and runs tests.
    *   **CRITICAL**: Cleans up build artifacts as root.
2.  **UBSan Phase**:
    *   Starts `php83-dev`.
    *   Builds with UBSan flags.
    *   Runs tests.
3.  **Valgrind Phase**:
    *   Rebuilds without sanitizers.
    *   Runs tests under Valgrind.

## Future Proofing

*   **PHP 8.4+ Compatibility**: The strategy relies on standard compiler flags supported by Clang/GCC, ensuring compatibility with future PHP versions.
*   **Docker Isolation**: Using separate containers for ASan (custom PHP build) and standard dev (distro PHP) prevents environment pollution.
*   **Explicit Cleanup**: Handling permissions via explicit cleanup rather than implicit user mapping ensures robustness across different host OS configurations.

## Troubleshooting

*   **Permission Denied Errors**: If you see permission errors during the UBSan phase, it likely means the ASan cleanup step failed. Run `docker compose exec -u root php83-asan bash -c "rm -rf /ext/modules /ext/build /ext/.libs"` to fix manually.
*   **ASan False Positives**: Check `scripts/analysis/lsan.supp` for leak suppressions.
