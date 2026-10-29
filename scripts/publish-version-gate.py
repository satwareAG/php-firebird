#!/usr/bin/env python3
"""Publish choke point for the version SSOT (issue 672 class).

Fail-closed verification that every uploaded release asset carries the
TAG's version in every version channel before the release is published:

  - Windows DLLs  (.dll): PE VS_FIXEDFILEINFO numeric FileVersion AND the
    UTF-16 "FileVersion" string must equal the tag version.
  - ELF modules   (.so):  the tag version string must be present, and NO
    fallback/sentinel version ("0.0.0-unknown", "0.0.0-empty") may appear.
  - Package archives (.zip/.tar.gz): inspected recursively (they wrap the
    binaries above).

Exit 0 only when every checked channel matches. Any missing channel,
unknown string, or mismatch fails the publish.

Usage: publish-version-gate.py <tag> <dir-with-assets>
"""

import struct
import sys
import tarfile
import zipfile
from pathlib import Path

SENTINELS = (b"0.0.0-unknown", b"0.0.0-empty", b"13.0.0-unknown")
VERSION_RE = __import__("re").compile(rb"\d+\.\d+\.\d+(?:-[0-9A-Za-z.+-]+)?")


def fail(msg: str) -> None:
    print(f"GATE FAIL: {msg}")
    sys.exit(1)


def tag_version(tag: str) -> str:
    version = tag[1:] if tag.startswith("v") else tag
    if not VERSION_RE.fullmatch(version.encode()):
        fail(f"tag '{tag}' does not carry a parseable version")
    return version


def pe_numeric_fileversion(data: bytes) -> str | None:
    """Read VS_FIXEDFILEINFO.dwFileVersion from raw PE bytes (stdlib only)."""
    sig = struct.pack("<I", 0xFEEF04BD)
    idx = data.find(sig)
    while idx != -1:
        # VS_FIXEDFILEINFO: sig(4) pad(8) ver(8) [we need offset 8 for version]
        if idx + 16 <= len(data):
            ms, ls = struct.unpack_from("<II", data, idx + 8)
            if (ms, ls) != (0, 0):
                return f"{ms >> 16}.{ms & 0xFFFF}.{ls >> 16}.{ls & 0xFFFF}"
        idx = data.find(sig, idx + 1)
    return None


def pe_string_fileversion(data: bytes) -> str | None:
    """Read the UTF-16 'FileVersion' string value from a VERSIONINFO blob."""
    key = "FileVersion".encode("utf-16-le")
    idx = data.find(key)
    while idx != -1:
        # key is NUL-terminated, then (aligned) the UTF-16 value
        scan = idx + len(key)
        while scan + 1 < len(data) and data[scan:scan + 2] == b"\x00\x00":
            scan += 2
        end = data.find(b"\x00\x00", scan)
        if end == -1:
            idx = data.find(key, idx + 1)
            continue
        raw_start = scan
        chars = []
        p = scan
        while p + 1 < len(data):
            ch = data[p] | (data[p + 1] << 8)
            if ch == 0:
                break
            chars.append(chr(ch))
            p += 2
        decoded = "".join(chars).strip()
        if decoded and VERSION_RE.search(decoded.encode()):
            return decoded
        idx = data.find(key, idx + 1)
    return None


def check_binary(name: str, data: bytes, version: str) -> None:
    for sentinel in SENTINELS:
        if sentinel in data:
            fail(f"{name}: contains fallback version string {sentinel!r} (issue 672 class)")
    member = name.rsplit("!", 1)[-1]
    base = member.rsplit("/", 1)[-1]
    if base.endswith(".dll"):
        numeric = pe_numeric_fileversion(data)
        if numeric is None:
            fail(f"{name}: no VS_FIXEDFILEINFO found - PE stamp missing")
        if not numeric.startswith(version):
            fail(f"{name}: PE FileVersion {numeric} != tag version {version}")
        pe_str = pe_string_fileversion(data)
        if pe_str is None:
            fail(f"{name}: no 'FileVersion' string resource found")
        if pe_str.split(" ")[0].rstrip(",0") not in (version, f"{version}.0"):
            if version not in pe_str:
                fail(f"{name}: PE FileVersion string '{pe_str}' != tag version {version}")
        print(f"  OK {name}: PE FileVersion {numeric} / '{pe_str}'")
    elif base.endswith(".so"):
        # Product modules only: Linux release zips bundle third-party client
        # libs (e.g. lib/libtommath.so) that legitimately carry no version
        # string (first live G8 run, v13.2.12, 2026-10-29). The sentinel
        # scan above still covers every binary.
        if not (base == "firebird.so" or "pdo_fbird" in base):
            print(f"  SKIP {name}: third-party bundled library")
            return
        if version.encode() not in data:
            fail(f"{name}: tag version {version} not found in binary")
        print(f"  OK {name}: version {version} present")


def iter_binaries(path: Path):
    """Yield (name, bytes) for binaries inside an asset or as a bare file."""
    name = path.name
    if name.endswith(".zip"):
        with zipfile.ZipFile(path) as zf:
            for member in zf.namelist():
                if member.endswith((".dll", ".so")):
                    yield f"{name}!{member}", zf.read(member)
    elif name.endswith((".tar.gz", ".tgz")):
        with tarfile.open(path) as tf:
            for member in tf.getmembers():
                if member.isfile() and member.name.endswith((".dll", ".so")):
                    fh = tf.extractfile(member)
                    if fh:
                        yield f"{name}!{member.name}", fh.read()
    elif name.endswith((".dll", ".so")):
        yield name, path.read_bytes()


def main() -> None:
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(2)
    tag, asset_dir = sys.argv[1], Path(sys.argv[2])
    version = tag_version(tag)

    assets = sorted(p for p in asset_dir.iterdir() if p.is_file())
    if not assets:
        fail(f"no assets found in {asset_dir}")
    print(f"publish-version-gate: tag={tag} version={version} assets={len(assets)}")

    checked = 0
    for asset in assets:
        names = list(iter_binaries(asset))
        if names:
            print(f"asset {asset.name}: {len(names)} binary channel(s)")
        else:
            # Non-binary assets (checksums, marker jsons) carry no version
            # channel - the smoke-marker binding (#652) covers those.
            print(f"asset {asset.name}: no binary version channel (skipped)")
            continue
        for bin_name, data in names:
            check_binary(bin_name, data, version)
            checked += 1

    if checked == 0:
        fail("no binary version channel observed in any asset - gate cannot verify")
    print(f"GATE PASS: {checked} binary channel(s) match tag {tag}, zero sentinels")


if __name__ == "__main__":
    main()
