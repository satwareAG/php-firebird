#!/usr/bin/env node
/**
 * #636: assert a Windows PHP extension DLL exports the required symbols.
 * #643: additionally parse the PE import table (data directory 1) and fail
 *       on forbidden symbol prefixes - default: smart_str_* (the #639
 *       failure: smart_str_ferealloc is unexported by php8.dll 8.5.0-8.5.11,
 *       so an importing DLL cannot load on those runtimes). Override the
 *       forbidden list via FBIRD_FORBIDDEN_IMPORTS (comma-separated;
 *       "none" disables). Bound and delay imports are out of scope.
 *
 * Every php_pdo_fbird.dll released before this check was an export-less
 * 11776-byte stub: the HAVE_PDO_FBIRD guard evaluated before php.h made the
 * AC_DEFINE invisible, so all translation units compiled empty and nmake
 * linked the empty objects into a valid-but-useless DLL. This script parses
 * the PE export table directly (no dumpbin dependency) and exits nonzero on:
 *   - not a PE file / missing export directory / zero named exports
 *   - any requested symbol missing from the export table
 *   - any imported symbol matching a forbidden prefix (#643)
 *
 * Usage: check-dll-exports.mjs <file.dll> [symbol ...]   (default: get_module)
 */
'use strict';

import fs from 'node:fs';

const DEFAULT_FORBIDDEN_IMPORTS = ['smart_str_'];

function fail(msg) {
    console.error('exports-check-failed: ' + msg);
    process.exit(1);
}

function parsePe(file) {
    const buf = fs.readFileSync(file);
    if (buf.length < 0x40 || buf.readUInt16LE(0) !== 0x5a4d) fail(`${file}: not a PE file (no MZ header)`);
    const peOff = buf.readUInt32LE(0x3c);
    if (peOff + 4 > buf.length || buf.readUInt32LE(peOff) !== 0x00004550) fail(`${file}: no PE signature`);
    const coff = peOff + 4;
    const numSections = buf.readUInt16LE(coff + 2);
    const optSize = buf.readUInt16LE(coff + 16);
    const opt = coff + 20;
    const magic = buf.readUInt16LE(opt);
    if (magic !== 0x10b && magic !== 0x20b) fail(`${file}: unknown optional header magic 0x${magic.toString(16)}`);
    // Data directories: exports (dir 0) and imports (dir 1).
    // PE32 optional header: dirs at +96; PE32+ at +112.
    const dataDir = opt + (magic === 0x20b ? 112 : 96);
    const exportRva = buf.readUInt32LE(dataDir);
    const exportSize = buf.readUInt32LE(dataDir + 4);
    const importRva = buf.readUInt32LE(dataDir + 8);
    const importSize = buf.readUInt32LE(dataDir + 12);

    const sections = [];
    const secBase = opt + optSize;
    for (let i = 0; i < numSections; i++) {
        const s = secBase + i * 40;
        sections.push({
            vaddr: buf.readUInt32LE(s + 12),
            virtSize: buf.readUInt32LE(s + 8),
            rawSize: buf.readUInt32LE(s + 16),
            rawPtr: buf.readUInt32LE(s + 20),
        });
    }
    const rvaToOff = (rva) => {
        for (const s of sections) {
            const span = Math.max(s.rawSize, s.virtSize);
            if (rva >= s.vaddr && rva < s.vaddr + span) {
                const off = rva - s.vaddr + s.rawPtr;
                if (off >= buf.length) fail(`${file}: RVA 0x${rva.toString(16)} maps past EOF`);
                return off;
            }
        }
        fail(`${file}: RVA 0x${rva.toString(16)} not in any section`);
    };
    return { buf, exportRva, exportSize, importRva, importSize, rvaToOff, pe32p: magic === 0x20b };
}

function exportNames(file) {
    const { buf, exportRva, exportSize, rvaToOff } = parsePe(file);
    if (exportRva === 0 || exportSize === 0) {
        fail(`${file}: no export directory (empty-export stub DLL, #636 signature)`);
    }
    // IMAGE_EXPORT_DIRECTORY: NumberOfNames at +24, AddressOfNames at +32
    const ed = rvaToOff(exportRva);
    const numNames = buf.readUInt32LE(ed + 24);
    if (numNames === 0) {
        fail(`${file}: export directory has 0 named symbols (empty-export stub DLL, #636 signature)`);
    }
    const namesArr = rvaToOff(buf.readUInt32LE(ed + 32));
    const names = [];
    for (let i = 0; i < numNames; i++) {
        const nOff = rvaToOff(buf.readUInt32LE(namesArr + i * 4));
        let end = nOff;
        while (end < buf.length && buf[end] !== 0) end++;
        names.push(buf.toString('ascii', nOff, end));
    }
    return names;
}

// #643: collect imported symbol names (import directory, data dir 1).
// Walks each IMAGE_IMPORT_DESCRIPTOR's INT (OriginalFirstThunk), falling
// back to the IAT (FirstThunk) when the INT RVA is 0 (bound imports).
// Ordinal entries carry no name and are skipped. Bound/delay tables
// (directories 11/13) are out of scope.
function importedNames(file) {
    const { buf, importRva, importSize, rvaToOff, pe32p } = parsePe(file);
    if (importRva === 0 || importSize === 0) return [];
    const thunkSize = pe32p ? 8 : 4;
    const ordFlag = pe32p ? 0x8000000000000000n : 0x80000000n;
    const names = [];
    const descBase = rvaToOff(importRva);
    for (let d = descBase; ; d += 20) {
        if (d + 20 > buf.length) fail(`${file}: import descriptor table runs past EOF`);
        const origFirst = buf.readUInt32LE(d);
        const dllRva = buf.readUInt32LE(d + 12);
        const firstThunk = buf.readUInt32LE(d + 16);
        if (origFirst === 0 && dllRva === 0 && firstThunk === 0) break; // null terminator
        let dll = '<unknown>';
        if (dllRva !== 0) {
            const nOff = rvaToOff(dllRva);
            let end = nOff;
            while (end < buf.length && buf[end] !== 0) end++;
            dll = buf.toString('ascii', nOff, end);
        }
        const thunkRva = origFirst !== 0 ? origFirst : firstThunk;
        if (thunkRva === 0) continue;
        const tBase = rvaToOff(thunkRva);
        for (let t = tBase; ; t += thunkSize) {
            if (t + thunkSize > buf.length) fail(`${file}: import thunk array for ${dll} runs past EOF`);
            const val = pe32p ? buf.readBigUInt64LE(t) : BigInt(buf.readUInt32LE(t));
            if (val === 0n) break; // null terminator
            if ((val & ordFlag) !== 0n) continue; // import by ordinal, no name
            const nOff = rvaToOff(Number(val));
            let end = nOff + 2; // skip hint
            while (end < buf.length && buf[end] !== 0) end++;
            names.push({ dll, name: buf.toString('ascii', nOff + 2, end) });
        }
    }
    return names;
}

const args = process.argv.slice(2);
if (args.length < 1) {
    console.error('usage: check-dll-exports.mjs <file.dll> [symbol ...]');
    process.exit(2);
}
const [file, ...expected] = args;
const names = exportNames(file);
const want = expected.length > 0 ? expected : ['get_module'];
for (const sym of want) {
    if (!names.includes(sym)) {
        fail(`${file}: '${sym}' not exported (found ${names.length} symbols: ${names.slice(0, 8).join(', ')}${names.length > 8 ? ', ...' : ''})`);
    }
}

// #643: forbidden import prefixes. smart_str_* symbols are unexported by
// php8.dll 8.5.0-8.5.11 (#639), so an importing DLL cannot load there.
const envList = process.env.FBIRD_FORBIDDEN_IMPORTS;
const forbidden = envList === 'none' ? [] : (envList ? envList.split(',').map((s) => s.trim()).filter(Boolean) : DEFAULT_FORBIDDEN_IMPORTS);
if (forbidden.length > 0) {
    const imported = importedNames(file);
    for (const { dll, name } of imported) {
        for (const prefix of forbidden) {
            if (name.startsWith(prefix)) {
                fail(`${file}: forbidden import '${name}' from ${dll} (matches '${prefix}', #639/#643)`);
            }
        }
    }
    console.log(`imports-ok: ${file} (${imported.length} imported symbols, no match for: ${forbidden.join(', ')})`);
}
console.log(`exports-ok: ${file} ${want.join(',')}`);
