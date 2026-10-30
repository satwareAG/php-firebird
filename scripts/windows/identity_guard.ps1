# identity_guard.ps1 - #688-P1 FR-002 interpreter/extension identity guard.
#
# Verifies that the LOADED extension self-reports the interpreter and Zend API
# identity this job expects (FR-001's FBIRD_BUILD_IDENTITY constant), failing
# closed on: wrong interpreter version, wrong Zend API, missing/unparseable
# identity, or a broken extension load. This is the durable fix for the
# v13.2.11/v13.2.12 wrong-interpreter smoke class (#686/#688-P1).
#
# Usage:
#   pwsh ./scripts/windows/identity_guard.ps1 `
#     -PhpExe C:\path\to\php.exe `
#     -ExtensionPath C:\path\to\php_firebird.dll `
#     -ExpectedPhpVersion '8.4' `
#     -ExpectedApiNo 20240924
#
# Exit codes: 0 = identity matches; 1 = mismatch/broken (fail closed).
# The error text always carries observed AND expected values (spec FR-002).

param(
    [Parameter(Mandatory = $true)] [string] $PhpExe,
    [Parameter(Mandatory = $true)] [string] $ExtensionPath,
    [Parameter(Mandatory = $true)] [string] $ExpectedPhpVersion,
    [Parameter(Mandatory = $true)] [int] $ExpectedApiNo
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path $PhpExe)) {
    Write-Error "IDENTITY GUARD FAIL: php binary not found at '$PhpExe'"
    exit 1
}
if (-not (Test-Path $ExtensionPath)) {
    Write-Error "IDENTITY GUARD FAIL: extension not found at '$ExtensionPath'"
    exit 1
}

# Read the identity the LOADED extension self-reports (FR-001 constant).
$out = & $PhpExe -n -d extension=$ExtensionPath -r "echo FBIRD_BUILD_IDENTITY;" 2>&1 | Out-String
if ($LASTEXITCODE -ne 0) {
    Write-Error "IDENTITY GUARD FAIL: php exited $LASTEXITCODE loading the extension: $out"
    exit 1
}
if ("$out" -notmatch 'php=(\d+)\s+api=(\d+)\s+ext=') {
    Write-Error "IDENTITY GUARD FAIL: FBIRD_BUILD_IDENTITY missing/unparseable in: $out"
    exit 1
}
$observedPhpId = [int]$Matches[1]
$observedApiNo = [int]$Matches[2]

# Expected interpreter: major.minor prefix match against PHP_VERSION_ID
# (id = major*10000 + minor*100 + patch - patch-exact comparison is brittle
# across provisioning; the smoke's interpreter-drift check covers the rest).
$expMajor = [int]($ExpectedPhpVersion.Split('.')[0])
$expMinor = [int]($ExpectedPhpVersion.Split('.')[1])
$obsMajor = [int][math]::Floor($observedPhpId / 10000)
$obsMinor = [int][math]::Floor(($observedPhpId % 10000) / 100)

if ($obsMajor -ne $expMajor -or $obsMinor -ne $expMinor) {
    Write-Error ("IDENTITY GUARD FAIL: interpreter mismatch - observed php id {0} (major {1}.{2}), expected major {3}.{4}" -f `
        $observedPhpId, $obsMajor, $obsMinor, $expMajor, $expMinor)
    exit 1
}
if ($observedApiNo -ne $ExpectedApiNo) {
    Write-Error ("IDENTITY GUARD FAIL: Zend API mismatch - observed {0}, expected {1}" -f `
        $observedApiNo, $ExpectedApiNo)
    exit 1
}

Write-Host ("IDENTITY GUARD OK: php {0}.{1}.x id {2}, Zend API {3}" -f `
    $obsMajor, $obsMinor, $observedPhpId, $observedApiNo)
exit 0
