#!/usr/bin/env php
<?php
declare(strict_types=1);

/**
 * Version single-source-of-truth generator (issue 672 class).
 *
 * VERSION.txt is the only hand-maintained version. Every derived version
 * surface is written (sync) or verified (check) from it. Humans never
 * hand-sync again; drift is a CI failure, not a discovery.
 *
 * Usage:
 *   php scripts/version-sync.php check   # CI mode: exit 1 on drift
 *   php scripts/version-sync.php sync    # maintainer mode: write derived files
 *
 * Derived surfaces:
 *   - stubs/firebird-stubs.php      @version tag
 *   - stubs/firebird-classes.php    @version tag
 *   - stubs/pdo-fbird-stubs.php     @version tag
 *   - specs/metadata.json           "version" field
 *   - pdo_fbird/VERSION.txt         split-repo version source
 */

$mode = $argv[1] ?? 'check';
if (!in_array($mode, ['sync', 'check'], true)) {
    fwrite(STDERR, "usage: php scripts/version-sync.php sync|check\n");
    exit(2);
}

$root = dirname(__DIR__);
$version = trim((string) @file_get_contents($root . '/VERSION.txt'));

if (!preg_match('/^\d+\.\d+\.\d+(-[0-9A-Za-z.+-]+)?$/', $version) || str_starts_with($version, '0.0.0')) {
    fwrite(STDERR, "FATAL: VERSION.txt missing or not a valid release version: '$version' (issue 672)\n");
    exit(1);
}

/**
 * @return array<string, array{path:string, read:callable(string):?string, write:callable(string,string):string}>
 */
function surfaces(string $root, string $version): array
{
    $atVersion = fn (string $content): ?string => preg_match('/@version\s+(\S+)/', $content, $m) ? $m[1] : null;
    $writeAtVersion = fn (string $content, string $v): string => preg_replace('/(@version\s+)\S+/', '${1}' . $v, $content, 1);

    $jsonVersion = function (string $content): ?string {
        return preg_match('/^\s*"version"\s*:\s*"([^"]+)"/m', $content, $m) ? $m[1] : null;
    };
    $writeJsonVersion = fn (string $content, string $v): string =>
        preg_replace('/^(\s*"version"\s*:\s*)"[^"]+"/m', '${1}"' . $v . '"', $content, 1);

    return [
        'stub:firebird-stubs' => ['path' => "$root/stubs/firebird-stubs.php", 'read' => $atVersion, 'write' => $writeAtVersion],
        'stub:firebird-classes' => ['path' => "$root/stubs/firebird-classes.php", 'read' => $atVersion, 'write' => $writeAtVersion],
        'stub:pdo-fbird-stubs' => ['path' => "$root/stubs/pdo-fbird-stubs.php", 'read' => $atVersion, 'write' => $writeAtVersion],
        'specs:metadata.json' => ['path' => "$root/specs/metadata.json", 'read' => $jsonVersion, 'write' => $writeJsonVersion],
        'pdo:VERSION.txt' => [
            'path' => "$root/pdo_fbird/VERSION.txt",
            'read' => fn (string $c): ?string => trim($c) !== '' ? trim($c) : null,
            'write' => fn (string $c, string $v): string => $v . "\n",
        ],
    ];
}

$errors = 0;
foreach (surfaces($root, $version) as $name => $s) {
    if (!is_file($s['path'])) {
        fwrite(STDERR, "FAIL $name: file missing ({$s['path']})\n");
        $errors++;
        continue;
    }
    $content = (string) file_get_contents($s['path']);
    $actual = ($s['read'])($content);
    if ($actual === $version) {
        echo "OK   $name ($version)\n";
        continue;
    }
    if ($mode === 'check') {
        fwrite(STDERR, "FAIL $name: '" . ($actual ?? 'missing') . "' != '$version'\n");
        $errors++;
        continue;
    }
    if (!file_put_contents($s['path'], ($s['write'])($content, $version))) {
        fwrite(STDERR, "FAIL $name: write failed\n");
        $errors++;
        continue;
    }
    echo "SYNC $name: '" . ($actual ?? 'missing') . "' -> '$version'\n";
}

if ($errors > 0) {
    fwrite(STDERR, "$errors surface(s) drifted" . ($mode === 'check' ? " - run: php scripts/version-sync.php sync\n" : "\n"));
    exit(1);
}
echo "all version surfaces match VERSION.txt ($version)\n";
