#!/usr/bin/env node
/**
 * #636: assert a Windows PHP extension DLL exports the required symbols.
 *
 * Every php_pdo_fbird.dll released before this check was an export-less
 * 11776-byte stub: the HAVE_PDO_FBIRD guard evaluated before php.h made the
 * AC_DEFINE invisible, so all translation units compiled empty and nmake
 * linked the empty objects into a valid-but-useless DLL. This script parses
 * the PE export table directly (no dumpbin dependency) and exits nonzero on:
 *   - not a PE file / missing export directory / zero named exports
 *   - any requested symbol missing from the export table
 *
 * Usage: check-dll-exports.mjs <file.dll> [symbol ...]   (default: get_module)
 */
'use strict';

import fs from 'node:fs';

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
    // Data directory 0 (export table): PE32 at +96, PE32+ at +112
    const dataDir = opt + (magic === 0x20b ? 112 : 96);
    const exportRva = buf.readUInt32LE(dataDir);
    const exportSize = buf.readUInt32LE(dataDir + 4);

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
    return { buf, exportRva, exportSize, rvaToOff };
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
console.log(`exports-ok: ${file} ${want.join(',')}`);
