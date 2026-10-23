#!/usr/bin/env node
/**
 * #643: test fixture for check-dll-exports.mjs - synthetic PE builder.
 *
 * Why synthetic, not a checked-in binary DLL: the parser is release-path
 * code; a deterministic, readable builder makes every PE field the parser
 * reads (and everything it must NOT read) explicit, and avoids an
 * unverifiable binary blob in git.
 *
 * Cases:
 *   1. PE32+ with a smart_str_* import       -> script MUST exit 1 (#643)
 *   2. PE32+ with only allowed imports       -> script MUST exit 0
 *   3. PE32 (0x10b) import table             -> script MUST exit 0 (4-byte thunks)
 *   4. Ordinal-only imports                  -> script MUST exit 0 (no name read)
 *   5. No import directory at all (RVA 0)    -> script MUST exit 0
 *   6. Missing required export (#636 reg.)   -> script MUST exit 1
 *
 * Usage: node scripts/windows/test-check-dll-exports.mjs
 */
'use strict';

import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const script = path.join(path.dirname(fileURLToPath(import.meta.url)), 'check-dll-exports.mjs');

// ---------------------------------------------------------------------------
// Synthetic PE builder
// ---------------------------------------------------------------------------

/**
 * Build a minimal DLL image with an export table and an import table.
 * @param {object} o
 * @param {0x20b|0x10b} [o.magic]   optional-header magic (default PE32+)
 * @param {string[]} [o.exports]    named exports (default ['get_module'])
 * @param {{dll: string, names?: string[], ordinals?: number[]}[]} [o.imports]
 * @param {boolean} [o.noImportDir] leave data directory 1 empty
 * @returns {Buffer}
 */
export function buildPe({ magic = 0x20b, exports = ['get_module'], imports = [], noImportDir = false } = {}) {
    const pe32p = magic === 0x20b;
    const thunkSize = pe32p ? 8 : 4;
    const ordFlag = pe32p ? 0x8000000000000000n : 0x80000000n;

    const dosSize = 0x40;
    const sigOff = dosSize;      // e_lfanew target: 'PE\0\0'
    const coffOff = sigOff + 4;  // COFF header follows the signature
    const optOff = coffOff + 20;
    const optSize = (pe32p ? 112 : 96) + 16 * 8;
    const secOff = optOff + optSize;
    const secVA = 0x1000;
    const secFile = 0x200;

    // Phase 1: assign RVAs sequentially inside the single section.
    let rva = secVA;
    const align4 = () => { rva = (rva + 3) & ~3; };
    const A = (n) => { align4(); const p = rva; rva += n; return p; };

    const expDirRva = exports.length ? A(40) : 0;
    const namesArrRva = exports.length ? A(4 * exports.length) : 0;
    const nameRva = {};
    for (const n of exports) nameRva[n] = A(n.length + 1);

    const impDirRva = (!noImportDir && imports.length) ? A(20 * (imports.length + 1)) : 0;
    const desc = imports.map((imp) => {
        imp.names ??= [];
        imp.ordinals ??= [];
        const count = imp.names.length + imp.ordinals.length;
        const intRva = A(thunkSize * (count + 1));
        const iatRva = A(thunkSize * (count + 1));
        const dllRva = A(imp.dll.length + 1);
        const hints = imp.names.map((n) => ({ n, rva: A(2 + n.length + 1) }));
        return { imp, intRva, iatRva, dllRva, hints };
    });

    const payloadSize = rva - secVA;
    const rawSize = Math.max(0x200, Math.ceil(payloadSize / 0x200) * 0x200);

    // Phase 2: write bytes.
    const buf = Buffer.alloc(secFile + rawSize);
    buf.write('MZ', 0, 'ascii');
    buf.writeUInt32LE(sigOff, 0x3c);        // e_lfanew
    buf.writeUInt32LE(0x00004550, sigOff);  // 'PE\0\0'
    buf.writeUInt16LE(pe32p ? 0x8664 : 0x14c, coffOff); // machine
    buf.writeUInt16LE(1, coffOff + 2);                  // NumberOfSections
    buf.writeUInt16LE(optSize, coffOff + 16);           // SizeOfOptionalHeader
    buf.writeUInt16LE(0x2000 | 0x0002, coffOff + 18);   // DLL | EXECUTABLE_IMAGE

    buf.writeUInt16LE(magic, optOff);
    buf.writeUInt32LE(secVA + payloadSize, optOff + 56); // SizeOfImage
    buf.writeUInt32LE(secFile, optOff + 60);             // SizeOfHeaders
    buf.writeUInt16LE(3, optOff + 68);                   // Subsystem: console

    const dd = optOff + (pe32p ? 112 : 96);              // data directories
    if (expDirRva) {
        buf.writeUInt32LE(expDirRva, dd + 0);
        buf.writeUInt32LE(40 + 4 * exports.length, dd + 4);
    }
    if (impDirRva) {
        buf.writeUInt32LE(impDirRva, dd + 8);            // directory 1: imports
        buf.writeUInt32LE(20 * (imports.length + 1), dd + 12);
    }

    buf.write('.rdata', secOff, 'ascii');
    buf.writeUInt32LE(payloadSize, secOff + 8);  // VirtualSize
    buf.writeUInt32LE(secVA, secOff + 12);       // VirtualAddress
    buf.writeUInt32LE(rawSize, secOff + 16);     // SizeOfRawData
    buf.writeUInt32LE(secFile, secOff + 20);     // PointerToRawData
    buf.writeUInt32LE(0x40000040, secOff + 36);  // CNT_INITIALIZED_DATA | READ

    const wStr = (r, s) => Buffer.from(s + '\0', 'ascii').copy(buf, r - secVA + secFile);
    const wU16 = (r, v) => buf.writeUInt16LE(v, r - secVA + secFile);
    const wU32 = (r, v) => buf.writeUInt32LE(v, r - secVA + secFile);
    const wU64 = (r, v) => buf.writeBigUInt64LE(v, r - secVA + secFile);
    const wThunk = (r, v) => { if (thunkSize === 8) wU64(r, v); else wU32(r, Number(v & 0xffffffffn)); };

    if (expDirRva) {
        wU32(expDirRva + 24, exports.length);    // NumberOfNames
        wU32(expDirRva + 32, namesArrRva);       // AddressOfNames
        exports.forEach((n, i) => wU32(namesArrRva + 4 * i, nameRva[n]));
        for (const n of exports) wStr(nameRva[n], n);
    }

    desc.forEach(({ imp, intRva, iatRva, dllRva, hints }, idx) => {
        const d = impDirRva + 20 * idx;
        wU32(d + 0, intRva);   // OriginalFirstThunk (INT)
        wU32(d + 12, dllRva);  // Name
        wU32(d + 16, iatRva);  // FirstThunk (IAT)
        wStr(dllRva, imp.dll);
        let k = 0;
        for (const { n, rva: hintRva } of hints) {
            wThunk(intRva + thunkSize * k, BigInt(hintRva));
            wThunk(iatRva + thunkSize * k, BigInt(hintRva));
            k++;
        }
        for (const o of imp.ordinals) {
            wThunk(intRva + thunkSize * k, BigInt(o) | ordFlag);
            wThunk(iatRva + thunkSize * k, BigInt(o) | ordFlag);
            k++;
        }
        for (const { n, rva: hintRva } of hints) {
            wU16(hintRva, 0); // hint
            wStr(hintRva + 2, n);
        }
    });
    return buf;
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
    const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'dll-check-test-'));
    const run = (img, ...args) => {
        const f = path.join(tmp, `case-${Math.random().toString(36).slice(2)}.dll`);
        fs.writeFileSync(f, img);
        return spawnSync(process.execPath, [script, f, ...args], { encoding: 'utf8' });
    };

    const cases = [
        { name: 'PE32+ smart_str_* import rejected (#643)', want: 1, args: ['get_module'],
          img: buildPe({ imports: [{ dll: 'php8.dll', names: ['smart_str_append', 'php_fbird_ok'] }] }) },
        { name: 'PE32+ allowed imports only', want: 0, args: ['get_module'],
          img: buildPe({ imports: [{ dll: 'php8.dll', names: ['php_fbird_ok'] }, { dll: 'KERNEL32.dll', names: ['Sleep'] }] }) },
        { name: 'PE32 (0x10b) import table (4-byte thunks)', want: 0, args: ['get_module'],
          img: buildPe({ magic: 0x10b, imports: [{ dll: 'php8.dll', names: ['php_fbird_ok'] }] }) },
        { name: 'ordinal-only imports skipped', want: 0, args: ['get_module'],
          img: buildPe({ imports: [{ dll: 'php8.dll', ordinals: [42] }] }) },
        { name: 'no import directory (RVA 0)', want: 0, args: ['get_module'],
          img: buildPe({ noImportDir: true }) },
        { name: 'missing required export still rejected (#636 regression)', want: 1, args: ['get_module'],
          img: buildPe({ exports: ['other_sym'] }) },
        { name: 'not a PE file still rejected (#636 regression)', want: 1, args: ['get_module'],
          img: Buffer.from('MZ this is not a PE file at all, just long enough') },
    ];

    let failed = 0;
    for (const c of cases) {
        const r = run(c.img, ...c.args);
        const ok = r.status === c.want;
        console.log(`${ok ? 'PASS' : 'FAIL'}: ${c.name} (exit ${r.status}, want ${c.want})`);
        if (!ok) {
            failed++;
            if (r.stdout) console.log('  stdout: ' + r.stdout.trim());
            if (r.stderr) console.log('  stderr: ' + r.stderr.trim());
        }
    }
    fs.rmSync(tmp, { recursive: true, force: true });
    console.log(failed === 0 ? `=== PASS: all ${cases.length} fixture cases ===` : `=== FAIL: ${failed}/${cases.length} cases ===`);
    process.exit(failed ? 1 : 0);
}
