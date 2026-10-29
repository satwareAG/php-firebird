#!/usr/bin/env node
// Version SSOT gate oracles for the unlazy ledger (GATES.md, issue 672 class).
// Each subcommand prints a success token ONLY after all its assertions pass
// and exits nonzero otherwise. Negative controls included per gate.
//
// Usage: node scripts/version-gates.mjs <subcommand>
//   generator          G1: version-sync syncs all surfaces, rejects seeded drift
//   configure-negative G2: configure fails loud when VERSION.txt missing (both native paths)
//   mutation-build     G3: built extensions report a MUTATED VERSION.txt value
//   lane-observation   G4: SSOT phpt oracles exist and pass in a docker lane
//   stamp-gate         G5: check-version-stamps.sh rejects synthetic drift, passes clean
//   canaries           G6: canary assertions fail when their target is violated
//   pe-stamp           G7: srv-app-2-built DLL carries the VERSION.txt FileVersion
//   publish-choke-point G8: publish workflow calls the fail-closed gate bound to the tag

import { execSync, spawnSync } from "node:child_process";
import { cpSync, existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import path from "node:path";

const ROOT = path.resolve(import.meta.dirname, "..");
const DOCKER = "docker compose -f docker/docker-compose.yml";
const SRVAPP2_SSH = "ssh -o BatchMode=yes srv-app-2";
const CONTAINER = "php84-fb3-dev";

function sh(cmd, opts = {}) {
  return execSync(cmd, { encoding: "utf8", cwd: ROOT, ...opts });
}
function trySh(cmd, opts = {}) {
  try { return { code: 0, out: sh(cmd, opts) }; }
  catch (e) { return { code: e.status ?? 1, out: `${e.stdout ?? ""}${e.stderr ?? ""}` }; }
}
function ok(token, msgs) { for (const m of msgs) console.log(`  ${m}`); console.log(token); }
function die(msgs) { for (const m of msgs) console.error(`  ${m}`); process.exit(1); }

function repoExport(dest) {
  execSync(`git archive HEAD | tar -x -C "${dest}"`, { cwd: ROOT, stdio: "pipe" });
}

// ---------------------------------------------------------------- G1
function generator() {
  const results = [];
  for (const surface of ["stubs/firebird-stubs.php", "stubs/firebird-classes.php",
    "stubs/pdo-fbird-stubs.php", "specs/metadata.json", "pdo_fbird/VERSION.txt"]) {
    const sandbox = mkdtempSync(path.join(tmpdir(), "vg-g1-"));
    try {
      repoExport(sandbox);
      const file = path.join(sandbox, surface);
      if (!existsSync(file)) die([`G1: surface missing in export: ${surface}`]);
      const original = readFileSync(file, "utf8");
      // Seed drift: change the version on that surface only.
      const drifted = original.includes("@version")
        ? original.replace(/(@version\s+)\S+/, "$19.8.7-drift")
        : original.replace(/13\.\d+\.\d+/, "9.8.7-drift");
      if (drifted === original) die([`G1: could not seed drift in ${surface}`]);
      writeFileSync(file, drifted);
      const check = spawnSync("php", ["scripts/version-sync.php", "check"], { cwd: sandbox, encoding: "utf8" });
      if (check.status === 0) die([`G1: check mode ACCEPTED seeded drift in ${surface}`]);
      results.push(`${surface}: drift rejected`);
      // Sync mode must repair it.
      const sync = spawnSync("php", ["scripts/version-sync.php", "sync"], { cwd: sandbox, encoding: "utf8" });
      if (sync.status !== 0) die([`G1: sync mode failed for ${surface}: ${sync.stderr}`]);
      const after = readFileSync(file, "utf8");
      const version = readFileSync(path.join(sandbox, "VERSION.txt"), "utf8").trim();
      if (!after.includes(version)) die([`G1: sync did not restore VERSION.txt value in ${surface}`]);
      const clean = spawnSync("php", ["scripts/version-sync.php", "check"], { cwd: sandbox, encoding: "utf8" });
      if (clean.status !== 0) die([`G1: check fails after sync for ${surface}`]);
    } finally { rmSync(sandbox, { recursive: true, force: true }); }
  }
  ok("G1_PASS", results);
}

// ---------------------------------------------------------------- G2
function configureNegative() {
  const msgs = [];
  // Linux leg: fresh export in the canonical container, 4 cases.
  const sandbox = mkdtempSync(path.join(tmpdir(), "vg-g2-"));
  repoExport(sandbox);
  sh(`tar -C "${sandbox}" -cf - . | ${DOCKER} exec -T ${CONTAINER} sh -c 'rm -rf /tmp/g2gate && mkdir -p /tmp/g2gate && tar -xf - -C /tmp/g2gate'`);
  const run = (c) => trySh(`${DOCKER} exec -T ${CONTAINER} bash -c '${c}'`);
  rmSync(sandbox, { recursive: true, force: true });

  let r = run("cd /tmp/g2gate && phpize >/dev/null 2>&1 && ./configure --enable-firebird >/tmp/g2pos.log 2>&1; echo exit=$?");
  if (!r.out.includes("exit=0")) die([`G2: positive configure failed`, r.out]);
  msgs.push("positive configure passes");
  r = run("cd /tmp/g2gate && mv VERSION.txt VERSION.txt.away && ./configure --enable-firebird >/tmp/g2neg.log 2>&1; echo exit=$?");
  if (!r.out.includes("exit=1") && !/exit=[1-9]/.test(r.out)) die([`G2: missing VERSION.txt did NOT fail configure`, r.out]);
  r = run('grep -c "version source missing or invalid" /tmp/g2neg.log');
  if (!r.out.trim().startsWith("1")) die([`G2: failure is not the SSOT error`, r.out]);
  msgs.push("missing VERSION.txt -> SSOT error");
  r = run("cd /tmp/g2gate && FBIRD_ALLOW_UNKNOWN_VERSION=1 GITHUB_REF=refs/tags/v9.9.9 ./configure --enable-firebird >/dev/null 2>&1; echo exit=$?");
  if (/exit=0/.test(r.out)) die([`G2: escape env honored on a TAG ref - must be ignored`, r.out]);
  msgs.push("escape env ignored on tag ref");
  r = run("cd /tmp/g2gate && FBIRD_ALLOW_UNKNOWN_VERSION=1 GITHUB_REF=refs/heads/main ./configure --enable-firebird >/tmp/g2esc.log 2>&1; echo exit=$?");
  if (!/exit=0/.test(r.out)) die([`G2: escape env refused off-tag (over-strict)`, r.out]);
  msgs.push("escape env works off-tag");

  // Windows leg: srv-app-2 clone, rename VERSION.txt, configure must ERROR.
  r = trySh(`${SRVAPP2_SSH} pwsh -NoProfile -ExecutionPolicy Bypass -File C:/php-build/configure-negative.ps1`);
  if (r.code !== 0 || !r.out.includes("G2_WIN_PASS")) {
    die([`G2: Windows negative control not available (srv-app-2):`, r.out.split("\n").slice(0, 6).join("\n")]);
  }
  msgs.push("Windows configure fails loud (srv-app-2)");
  ok("G2_PASS", msgs);
}

// ---------------------------------------------------------------- G3
function mutationBuild() {
  const MUTATED = "9.9.9-mutation";
  const msgs = [];
  let r = trySh(`${DOCKER} exec -T ${CONTAINER} bash -c 'cd /ext && git status --porcelain | head -3 && sed -i "s/^13\\.[0-9]*\\.[0-9]*\$/${MUTATED}/" VERSION.txt && cat VERSION.txt && bash scripts/build.sh >/tmp/g3build.log 2>&1; echo build_exit=$?'`);
  if (!r.out.includes("build_exit=0")) {
    // restore and fail honestly
    trySh(`${DOCKER} exec -T ${CONTAINER} bash -c 'cd /ext && git checkout VERSION.txt'`);
    die([`G3: mutated build failed`, r.out.split("\n").slice(-5).join("\n")]);
  }
  r = trySh(`${DOCKER} exec -T ${CONTAINER} bash -c 'cd /ext && php run-tests.php -d extension=modules/firebird.so -d extension=pdo_fbird/modules/pdo_fbird.so -p $(which php) tests/version_ssot_firebird.phpt tests/version_ssot_pdo_fbird.phpt 2>&1 | grep "Tests passed"'`);
  if (!r.out.includes("Tests passed") || !r.out.includes("2 (100.0%)")) {
    trySh(`${DOCKER} exec -T ${CONTAINER} bash -c 'cd /ext && git checkout VERSION.txt'`);
    die([`G3: mutated lane did not report the mutated value`, r.out]);
  }
  // Independent oracle: the runtime version must EQUAL the mutated string,
  // not merely match the (mutated) file.
  r = trySh(`${DOCKER} exec -T ${CONTAINER} bash -c 'cd /ext && php -n -d extension=modules/firebird.so -r "echo phpversion('"'"'firebird'"'"');"'`);
  if (!r.out.includes(MUTATED)) {
    trySh(`${DOCKER} exec -T ${CONTAINER} bash -c 'cd /ext && git checkout VERSION.txt'`);
    die([`G3: phpversion() != mutated VERSION.txt (${MUTATED})`, r.out]);
  }
  msgs.push(`mutated build reports ${MUTATED} (firebird, pdo) - oracle binds to the file`);
  trySh(`${DOCKER} exec -T ${CONTAINER} bash -c 'cd /ext && git checkout VERSION.txt && bash scripts/build.sh >/dev/null 2>&1'`);
  ok("G3_PASS", msgs);
}

// ---------------------------------------------------------------- G4
function laneObservation() {
  const msgs = [];
  for (const t of ["tests/version_ssot_firebird.phpt", "tests/version_ssot_pdo_fbird.phpt"]) {
    if (!existsSync(path.join(ROOT, t))) die([`G4: missing ${t}`]);
    const content = readFileSync(path.join(ROOT, t), "utf8");
    if (!content.includes("VERSION.txt") || !content.includes("phpversion(")) {
      die([`G4: ${t} does not observe phpversion() against VERSION.txt`]);
    }
  }
  msgs.push("SSOT phpt oracles exist and observe the artifact");
  const r = trySh(`${DOCKER} exec -T ${CONTAINER} bash -c 'cd /ext && php run-tests.php -d extension=modules/firebird.so -d extension=pdo_fbird/modules/pdo_fbird.so -p $(which php) tests/version_ssot_firebird.phpt tests/version_ssot_pdo_fbird.phpt 2>&1 | grep "Tests passed"'`);
  if (!r.out.includes("2 (100.0%)")) die([`G4: SSOT tests do not pass in the canonical lane`, r.out]);
  msgs.push("2/2 pass in php84-fb3-dev");
  ok("G4_PASS", msgs);
}

// ---------------------------------------------------------------- G5
function stampGate() {
  const msgs = [];
  let r = trySh("bash scripts/check-version-stamps.sh");
  if (r.code !== 0) die([`G5: clean tree fails the stamp gate`, r.out]);
  msgs.push("clean tree passes");
  const sandbox = mkdtempSync(path.join(tmpdir(), "vg-g5-"));
  repoExport(sandbox);
  writeFileSync(path.join(sandbox, "VERSION.txt"), "14.0.0-drift\n");
  r = spawnSync("bash", ["scripts/check-version-stamps.sh"], { cwd: sandbox, encoding: "utf8" });
  if (r.status === 0) die([`G5: gate ACCEPTED a drifted VERSION.txt vs stale stamps`]);
  msgs.push("synthetic mismatch rejected (14.0.0-drift vs 13.2.11 stamps)");
  writeFileSync(path.join(sandbox, "VERSION.txt"), "0.0.0-unknown\n");
  r = spawnSync("bash", ["scripts/check-version-stamps.sh"], { cwd: sandbox, encoding: "utf8" });
  if (r.status === 0) die([`G5: gate ACCEPTED sentinel version in VERSION.txt`]);
  msgs.push("sentinel VERSION.txt rejected");
  rmSync(sandbox, { recursive: true, force: true });
  ok("G5_PASS", msgs);
}

// ---------------------------------------------------------------- G6
function canaries() {
  const msgs = [];
  let r = trySh("bash scripts/check-version-stamps.sh");
  if (r.code !== 0) die([`G6: canaries fail on clean tree`, r.out]);
  msgs.push("clean tree: all canaries pass");
  // Negative controls in a throwaway worktree: break one canary target each.
  const sandbox = mkdtempSync(path.join(tmpdir(), "vg-g6-"));
  repoExport(sandbox);
  const cases = [
    ["config.w32", /VERSION\.txt/g, "VERSION" + ".BAK", "configure no longer reads VERSION.txt"],
    [".github/workflows/windows-smoke.yml", /(\n\s*DRIVER_SHA:)/, "\n      DEFAULT_TAG: v0.0.0-pin\n$1", "DEFAULT_TAG pin reintroduced"],
  ];
  for (const [file, pat, repl, what] of cases) {
    const broken = mkdtempSync(path.join(tmpdir(), "vg-g6b-"));
    cpSync(sandbox, broken, { recursive: true });
    const p = path.join(broken, file);
    writeFileSync(p, readFileSync(p, "utf8").replace(pat, repl));
    const rr = spawnSync("bash", ["scripts/check-version-stamps.sh"], { cwd: broken, encoding: "utf8" });
    if (rr.status === 0) die([`G6: canary did NOT catch: ${what}`]);
    msgs.push(`canary catches: ${what}`);
    rmSync(broken, { recursive: true, force: true });
  }
  rmSync(sandbox, { recursive: true, force: true });
  ok("G6_PASS", msgs);
}

// ---------------------------------------------------------------- G7
function peStamp() {
  let r = trySh(`${SRVAPP2_SSH} pwsh -NoProfile -ExecutionPolicy Bypass -File C:/php-build/pe-stamp-gate.ps1`);
  if (r.code !== 0 || !r.out.includes("G7_PASS")) {
    die([`G7: srv-app-2 PE stamp gate not satisfied:`, r.out.split("\n").slice(0, 8).join("\n")]);
  }
  ok("G7_PASS", r.out.split("\n").filter((l) => l.trim()).slice(0, 4));
}

// ---------------------------------------------------------------- G8
function publishChokePoint() {
  const msgs = [];
  const wf = readFileSync(path.join(ROOT, ".github/workflows/publish-release.yml"), "utf8");
  if (!wf.includes("publish-version-gate.py")) die(["G8: publish workflow does not call the version gate"]);
  if (!wf.includes("steps.tag.outputs.tag")) die(["G8: gate is not bound to the tag output"]);
  const gate = readFileSync(path.join(ROOT, "scripts/publish-version-gate.py"), "utf8");
  for (const marker of ["SENTINELS", "VS_FIXEDFILEINFO", "GATE PASS"]) {
    if (!gate.includes(marker)) die([`G8: gate script missing ${marker}`]);
  }
  msgs.push("workflow wires the gate to steps.tag before attest/publish");
  // Fixture negatives with real srv-app-2-built DLLs.
  const fixtures = path.join(tmpdir(), "vg-g8-assets");
  rmSync(fixtures, { recursive: true, force: true });
  mkdirSync(fixtures, { recursive: true });
  trySh(`scp -o BatchMode=yes 'srv-app-2:C:/php-build/out-green/php_firebird.dll' '${fixtures}/php_firebird.dll'`);
  if (existsSync(path.join(fixtures, "php_firebird.dll"))) {
    let rr = trySh(`python3 scripts/publish-version-gate.py v13.2.11 '${fixtures}'`);
    if (rr.code !== 0 || !rr.out.includes("GATE PASS")) die([`G8: fixture positive failed`, rr.out]);
    rr = trySh(`python3 scripts/publish-version-gate.py v13.2.12 '${fixtures}'`);
    if (rr.code === 0) die([`G8: gate ACCEPTED wrong tag`]);
    msgs.push("fixture: correct tag passes, wrong tag fails (real DLL)");
  } else {
    msgs.push("fixtures unavailable (srv-app-2 offline) - static wiring only");
    process.exit(1);
  }
  ok("G8_PASS", msgs);
}

const commands = { generator, configureNegative, mutationBuild, laneObservation, stampGate, canaries, peStamp, publishChokePoint };
const raw = process.argv[2] ?? "";
const cmd = raw.replace(/-([a-z])/g, (_, c) => c.toUpperCase());
if (!commands[cmd]) {
  console.error(`usage: node scripts/version-gates.mjs <${Object.keys(commands).join("|")}>`);
  process.exit(2);
}
commands[cmd]();
