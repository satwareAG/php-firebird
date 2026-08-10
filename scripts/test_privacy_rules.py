#!/usr/bin/env python3
"""
IPADP L2 Privacy Validation: scan tracked repo files for private forge URLs.

Ensures no private forge URLs appear in public repo files. Per IPADP rules,
public-to-private linking is FORBIDDEN.

Forbidden hostnames are constructed at runtime (not stored as literals) so
this script does not flag itself.

Exit codes:
  0 = PASS (no private forge URLs found)
  1 = FAIL (private forge URLs found, printed to stderr)

Usage:
  python3 scripts/test_privacy_rules.py
"""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

# Forbidden hostnames - constructed at runtime to avoid self-flagging.
# jane: literal hostnames would make the scanner find itself; assembling
# from fragments keeps the scanner self-policing.
_GL = "gitlab" + "." + "satware" + "." + "com"
_GT = "git" + "." + "satware" + "." + "ai"

FORBIDDEN = [
    (_GL, re.compile(re.escape(_GL))),
    (_GT, re.compile(re.escape(_GT))),
]

# Directories to skip (not tracked or not relevant).
SKIP_DIRS = {".git", "vendor", "node_modules", "__pycache__"}

# File extensions to skip (binary or non-text).
SKIP_EXTENSIONS = {
    ".png", ".jpg", ".jpeg", ".gif", ".bmp", ".ico", ".so", ".dll",
    ".dylib", ".a", ".o", ".lo", ".la", ".exe", ".zip", ".tar", ".gz",
    ".tgz", ".bz2", ".xz", ".fdb", ".fbk", ".gdb", ".dat", ".bin",
    ".pdf", ".doc", ".docx", ".xls", ".xlsx", ".ppt", ".pptx",
}

# Files to skip by name.
SKIP_NAMES = {".gitignore", ".gitattributes", ".gitmodules"}


def get_tracked_files() -> list[str]:
    """Return list of git-tracked files."""
    result = subprocess.run(
        ["git", "ls-files"],
        capture_output=True,
        text=True,
        check=True,
    )
    return [f for f in result.stdout.strip().splitlines() if f]


def should_scan(path: Path) -> bool:
    """Return True if the file should be scanned."""
    if path.name in SKIP_NAMES:
        return False
    if path.suffix.lower() in SKIP_EXTENSIONS:
        return False
    for part in path.parts:
        if part in SKIP_DIRS:
            return False
    return True


def scan_file(content: bytes) -> list[tuple[int, str, str]]:
    """Scan file content for forbidden hostnames. Returns list of (line_no, line, hostname)."""
    findings: list[tuple[int, str, str]] = []
    text = content.decode("utf-8", errors="replace")

    for lineno, line in enumerate(text.splitlines(), 1):
        for hostname, pattern in FORBIDDEN:
            if pattern.search(line):
                findings.append((lineno, line.strip(), hostname))
    return findings


def main() -> int:
    files = get_tracked_files()
    total_findings = 0

    for filepath in files:
        path = Path(filepath)
        if not should_scan(path):
            continue
        try:
            content = path.read_bytes()
        except (OSError, PermissionError):
            continue

        findings = scan_file(content)
        for lineno, line, hostname in findings:
            print(
                f"[FAIL] {filepath}:{lineno}: found '{hostname}' in: {line[:120]}",
                file=sys.stderr,
            )
            total_findings += 1

    if total_findings > 0:
        hostnames = ", ".join(h for h, _ in FORBIDDEN)
        print(f"\n[ERROR] {total_findings} private forge URL(s) found.", file=sys.stderr)
        print(f"Private forge URLs ({hostnames}) are FORBIDDEN in public repos.", file=sys.stderr)
        print("See: IPADP L2 Privacy Validation (issue #567).", file=sys.stderr)
        return 1

    print("[PASS] No private forge URLs found.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
