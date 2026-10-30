# assert-asset-manifest.ps1 - #688-P2 manifest oracle: set-equality join
# between the checked-in release-asset manifest and the actual asset names of
# a release (or a fixture set, for the PR-time negative controls).
#
# Rules (fail closed, no aggregate counts):
#   - every REQUIRED (expanded) asset present in the actuals    -> else FAIL
#   - every actual name matching a family namespace present in
#     the manifest expansion                                    -> else FAIL
#   - actuals NOT matching any family namespace are OTHER
#     PLATFORMS' assets (Linux/macOS pipelines own them)        -> ignored
#   - duplicate names in the expansion                          -> FAIL (manifest bug)
#
# Usage:
#   pwsh ./scripts/windows/assert-asset-manifest.ps1 `
#     -ManifestPath ./scripts/windows/release-asset-manifest.json `
#     -Version 13.3.0 `
#     -ActualNamesFile ./fixtures/actual.txt          # one name per line
#   pwsh ... -ActualNames @('php_firebird-...', ...)  # inline instead
#
# Exit codes: 0 = required set satisfied exactly; 1 = missing/extra/duplicate.

param(
    [Parameter(Mandatory = $true)] [string] $ManifestPath,
    [Parameter(Mandatory = $true)] [string] $Version,
    [string[]] $ActualNames,
    [string] $ActualNamesFile
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path $ManifestPath)) {
    Write-Error "MANIFEST FAIL: not found at '$ManifestPath'"
    exit 1
}

$mf = Get-Content $ManifestPath -Raw | ConvertFrom-Json

# Expand families into the required set
$required = New-Object 'System.Collections.Generic.HashSet[string]'
foreach ($family in $mf.families) {
    foreach ($php in $family.php) {
        $vs = $mf.vs_by_php.$php
        if (-not $vs) { Write-Error "MANIFEST FAIL: no vs mapping for php $php"; exit 1 }
        foreach ($ts in $family.ts) {
            $name = $family.pattern `
                -replace '\{version\}', $Version `
                -replace '\{php\}', $php `
                -replace '\{ts\}', $ts `
                -replace '\{vs\}', $vs
            [void]$required.Add($name)
        }
    }
}
if ($required.Count -eq 0) { Write-Error 'MANIFEST FAIL: expansion produced zero assets'; exit 1 }

# Actuals: inline array or file (one name per line)
if ($ActualNamesFile) {
    $ActualNames = Get-Content $ActualNamesFile | Where-Object { $_.Trim() -ne '' }
}
$actual = New-Object 'System.Collections.Generic.HashSet[string]'
foreach ($n in $ActualNames) {
    $t = $n.Trim()
    if ($t -eq '') { continue }
    if (-not $actual.Add($t)) {
        Write-Error "MANIFEST FAIL: duplicate actual asset name: $t"
        exit 1
    }
}

# Build the Windows namespace patterns (family patterns with version resolved)
$nsPatterns = foreach ($family in $mf.families) {
    $family.pattern -replace '\{version\}', [regex]::Escape($Version)
}

$missing = @()
$extra = @()
foreach ($req in $required) {
    if (-not $actual.Contains($req)) { $missing += $req }
}
foreach ($act in $actual) {
    if ($required.Contains($act)) { continue }
    $isWindowsFamily = $false
    foreach ($pat in $nsPatterns) {
        $rx = '^' + ($pat -replace '\{php\}', '\d+\.\d+' -replace '\{ts\}', '(nts|ts)' -replace '\{vs\}', 'vs\d+') + '$'
        if ($act -match $rx) { $isWindowsFamily = $true; break }
    }
    if ($isWindowsFamily) { $extra += $act }
}

$failed = $false
if ($missing.Count -gt 0) {
    $failed = $true
    Write-Error ("MANIFEST FAIL: {0} REQUIRED asset(s) missing from the release:" -f $missing.Count)
    foreach ($m in $missing) { Write-Error "  MISSING: $m" }
}
if ($extra.Count -gt 0) {
    $failed = $true
    Write-Error ("MANIFEST FAIL: {0} Windows-family asset(s) NOT in the manifest (unexpected/extra):" -f $extra.Count)
    foreach ($e in $extra) { Write-Error "  EXTRA: $e" }
}

if ($failed) { exit 1 }

Write-Host ("MANIFEST OK: {0} required assets satisfied exactly (set-equality join, #688-P2)" -f $required.Count)
exit 0
