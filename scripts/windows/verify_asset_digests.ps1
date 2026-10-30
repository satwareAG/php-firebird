# verify_asset_digests.ps1 - #688-P1 FR-003 sha256 asset join (smoke side).
#
# Verifies that downloaded release assets match the digests recorded by the
# build job's SHA256SUMS manifest (name join). Fails closed: empty manifest,
# missing file, missing entry, or digest mismatch all exit 1.
#
# Usage:
#   pwsh ./scripts/windows/verify_asset_digests.ps1 `
#     -ManifestPath SHA256SUMS `
#     -AssetsDir . `
#     -AssetNames @('php_firebird-v13.3.0-8.4-nts-vs17-x86_64.zip', ...)
#
# Exit codes: 0 = all assets verified; 1 = any mismatch/missing/unparseable.

param(
    [Parameter(Mandatory = $true)] [string] $ManifestPath,
    [Parameter(Mandatory = $true)] [string] $AssetsDir,
    [Parameter(Mandatory = $true)] [string[]] $AssetNames
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path $ManifestPath)) {
    Write-Error "FR-003 FAIL: digest manifest not found at '$ManifestPath'"
    exit 1
}

$manifest = @{}
foreach ($line in Get-Content $ManifestPath) {
    if ($line -match '^([0-9a-f]{64})\s\s(.+)$') { $manifest[$Matches[2]] = $Matches[1] }
}
if ($manifest.Count -eq 0) {
    Write-Error 'FR-003 FAIL: digest manifest empty/unparseable'
    exit 1
}

$failed = $false
foreach ($name in $AssetNames) {
    $assetPath = Join-Path $AssetsDir $name
    if (-not (Test-Path $assetPath)) {
        Write-Error "FR-003 FAIL: asset missing on disk: $name"
        $failed = $true
        continue
    }
    if (-not $manifest.ContainsKey($name)) {
        Write-Error "FR-003 FAIL: no manifest entry for $name"
        $failed = $true
        continue
    }
    $hash = (Get-FileHash -Algorithm SHA256 $assetPath).Hash.ToLower()
    if ($manifest[$name] -ne $hash) {
        Write-Error ("FR-003 FAIL: DIGEST MISMATCH for {0}: manifest={1} actual={2}" -f $name, $manifest[$name], $hash)
        $failed = $true
        continue
    }
    Write-Host "digest OK: $name"
}

if ($failed) { exit 1 }
Write-Host "FR-003 OK: all $($AssetNames.Count) asset(s) verified against the build manifest"
exit 0
