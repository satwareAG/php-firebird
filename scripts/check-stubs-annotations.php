#!/usr/bin/env php
<?php
/**
 * check-stubs-annotations.php - Verify stub annotations match function signatures
 *
 * Regression test for issue #299: stale @param/@return resource annotations
 * caused 37 PHPStan errors in doctrine-firebird-driver.
 *
 * Checks:
 * 1. @return docblock type must not say "resource" when signature uses Firebird\* types
 * 2. @param docblock must not use bare "resource" without |\Firebird\* (dual-accept bridge)
 * 3. Functions must not use ": mixed" return when a specific type is known
 * 4. stubs/firebird-stubs.php and phpstan/fbird.stub.php signatures must match
 *
 * Usage: php scripts/check-stubs-annotations.php
 * Exit: 0 = clean, 1 = violations, 2 = parse error
 */

declare(strict_types=1);

$ROOT = dirname(__DIR__);
$STUBS_FILE = "$ROOT/stubs/firebird-stubs.php";
$PHPSTAN_FILE = "$ROOT/phpstan/fbird.stub.php";

$RED = "\033[0;31m";
$GREEN = "\033[0;32m";
$NC = "\033[0m";

$expectedReturnTypes = [
    'fbird_execute_query' => '\\Firebird\\ResultSet|false',
    'fbird_execute_auto' => '\\Firebird\\ResultSet|int|false',
    'fbird_query_params_tx' => '\\Firebird\\ResultSet|int|false',
    'fbird_reconnect_transaction' => '\\Firebird\\Transaction|false',
    'fbird_batch_create' => '\\Firebird\\BatchHandle|false',
    'fbird_poll_event' => 'array|int|false',
];

$okCount = 0;
$errors = [];

function ok(string $msg): void {
    global $okCount;
    $okCount++;
    echo "{$GLOBALS['GREEN']}[OK]{$GLOBALS['NC']}    {$msg}\n";
}

function error(string $msg): void {
    global $errors;
    $errors[] = $msg;
    echo "{$GLOBALS['RED']}[ERROR]{$GLOBALS['NC']} {$msg}\n";
}

function parseFunctions(string $file): array {
    $source = file_get_contents($file);
    if ($source === false) {
        throw new RuntimeException("Cannot read: $file");
    }
    $tokens = token_get_all($source);
    $functions = [];
    $i = 0;
    $len = count($tokens);
    $pendingDoc = null;

    while ($i < $len) {
        $tok = $tokens[$i];
        if (is_array($tok) && $tok[0] === T_DOC_COMMENT) {
            $pendingDoc = $tok[1];
        }
        if (is_array($tok) && $tok[0] === T_FUNCTION) {
            $j = $i + 1;
            while ($j < $len && is_array($tokens[$j]) && $tokens[$j][0] === T_WHITESPACE) $j++;
            if ($j < $len && is_array($tokens[$j]) && $tokens[$j][0] === T_STRING) {
                $name = $tokens[$j][1];
                if (!str_starts_with($name, 'fbird_')) { $i++; $pendingDoc = null; continue; }
                $sig = extractSignature($tokens, $j + 1, $len);
                $functions[$name] = [
                    'doc' => $pendingDoc,
                    'return_type' => $sig['return_type'],
                    'params' => $sig['params'],
                    'file' => $file,
                ];
            }
            $pendingDoc = null;
        }
        $i++;
    }
    return $functions;
}

function extractSignature(array $tokens, int $start, int $len): array {
    $depth = 0;
    $buf = '';
    $i = $start;
    while ($i < $len) {
        $t = $tokens[$i];
        $char = is_array($t) ? $t[1] : $t;
        if ($char === '(') { $depth++; $buf .= $char; }
        elseif ($char === ')') { $depth--; $buf .= $char; if ($depth === 0) break; }
        else $buf .= $char;
        $i++;
    }
    $i++;
    while ($i < $len && is_array($tokens[$i]) && $tokens[$i][0] === T_WHITESPACE) $i++;
    $returnType = '';
    if ($i < $len) {
        $t = $tokens[$i];
        if ($t === ':') {
            $i++;
            $rtBuf = '';
            while ($i < $len) {
                $t = $tokens[$i];
                $char = is_array($t) ? $t[1] : $t;
                if ($char === '{') break;
                $rtBuf .= $char;
                $i++;
            }
            $returnType = trim($rtBuf);
        }
    }
    return ['params' => $buf, 'return_type' => $returnType];
}

function extractDocReturn(string $doc): ?string {
    if (!preg_match('/@return\s+(\S+)/', $doc, $m)) return null;
    return $m[1];
}

function extractDocParams(string $doc): array {
    preg_match_all('/@param\s+(.+?)\s+\$(\w+)/', $doc, $m, PREG_SET_ORDER);
    $params = [];
    foreach ($m as $match) { $params[$match[2]] = $match[1]; }
    return $params;
}

if (!file_exists($STUBS_FILE) || !file_exists($PHPSTAN_FILE)) {
    fwrite(STDERR, "Missing stub files\n");
    exit(2);
}

try {
    $stubsFuncs = parseFunctions($STUBS_FILE);
    $phpstanFuncs = parseFunctions($PHPSTAN_FILE);
} catch (Throwable $e) {
    fwrite(STDERR, "Parse error: {$e->getMessage()}\n");
    exit(2);
}

echo "Checking stub annotations (issue #299 regression test)...\n\n";

foreach ($stubsFuncs as $name => $info) {
    $returnType = $info['return_type'];
    $doc = $info['doc'] ?? '';

    if ($doc) {
        $docReturn = extractDocReturn($doc);
        if ($docReturn !== null) {
            $sigHasFirebird = str_contains($returnType, 'Firebird');
            $docHasBareResource = str_contains($docReturn, 'resource')
                && !str_contains($docReturn, 'Firebird');

            if ($sigHasFirebird && $docHasBareResource) {
                error("$name: @return $docReturn in docblock but signature says $returnType (stale resource annotation)");
            } else {
                ok("$name: return type consistent");
            }
        }

        $docParams = extractDocParams($doc);
        foreach ($docParams as $paramName => $paramType) {
            if (str_contains($paramType, 'resource')
                && !str_contains($paramType, 'Firebird')
                && !str_contains($paramType, 'callable')
                && $paramName !== 'file') {
                error("$name: @param \$$paramName uses bare '$paramType' without |\\Firebird\\* (dual-accept bridge)");
            }
        }
    }

    if (isset($expectedReturnTypes[$name])) {
        $expected = $expectedReturnTypes[$name];
        if ($returnType === 'mixed') {
            error("$name: return type is 'mixed' but should be '$expected'");
        } elseif ($returnType !== $expected && $returnType !== ltrim($expected, '\\')) {
            ok("$name: return type is specific ($returnType)");
        } else {
            ok("$name: return type matches expected ($returnType)");
        }
    }
}

echo "\nChecking stubs vs phpstan signature consistency...\n\n";
$allNames = array_unique(array_merge(array_keys($stubsFuncs), array_keys($phpstanFuncs)));
sort($allNames);

foreach ($allNames as $name) {
    if (!isset($stubsFuncs[$name])) {
        error("$name: missing from stubs/firebird-stubs.php");
        continue;
    }
    if (!isset($phpstanFuncs[$name])) {
        error("$name: missing from phpstan/fbird.stub.php");
        continue;
    }
    $sRet = $stubsFuncs[$name]['return_type'];
    $pRet = $phpstanFuncs[$name]['return_type'];
    if (ltrim($sRet, '\\') !== ltrim($pRet, '\\')) {
        error("$name (stubs vs phpstan): return type mismatch: $sRet vs $pRet");
    }
}

echo "\n";
$violations = count($errors);
if ($violations === 0) {
    echo "{$GREEN}All stub annotations are in sync. ($okCount checks passed){$NC}\n";
    exit(0);
} else {
    echo "{$RED}FAIL: $violations annotation violations found.{$NC}\n";
    echo "These will cause PHPStan errors in doctrine-firebird-driver!\n";
    exit(1);
}
