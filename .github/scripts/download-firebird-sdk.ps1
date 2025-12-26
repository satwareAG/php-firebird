# .github/scripts/download-firebird-sdk.ps1
param(
    [Parameter(Mandatory=$true)]
    [ValidateSet("3.0", "4.0", "5.0")]
    [string]$Version,

    [Parameter(Mandatory=$true)]
    [ValidateSet("x64", "x86")]
    [string]$Architecture,

    [Parameter(Mandatory=$true)]
    [string]$DestinationPath
)

$ErrorActionPreference = "Stop"

# Define download URLs for each version
$downloads = @{
    "3.0" = @{
        "x64" = "https://github.com/FirebirdSQL/firebird/releases/download/v3.0.12/Firebird-3.0.12.33787-0-x64.zip"
        "x86" = "https://github.com/FirebirdSQL/firebird/releases/download/v3.0.12/Firebird-3.0.12.33787-0-win32.zip"
    }
    "4.0" = @{
        "x64" = "https://github.com/FirebirdSQL/firebird/releases/download/v4.0.6/Firebird-4.0.6.3602-0-x64.zip"
        "x86" = $null  # FB 4.0 x86 not available
    }
    "5.0" = @{
        "x64" = "https://github.com/FirebirdSQL/firebird/releases/download/v5.0.3/Firebird-5.0.3.1683-0-windows-x64.exe"
        "x86" = $null  # FB 5.0 x86 not available
    }
}

$url = $downloads[$Version][$Architecture]
if (-not $url) {
    throw "Firebird $Version is not available for $Architecture"
}

# Create destination directory
New-Item -ItemType Directory -Force -Path $DestinationPath | Out-Null

$tempFile = Join-Path $env:TEMP "firebird-sdk-$Version-$Architecture.download"

Write-Host "Downloading Firebird $Version SDK ($Architecture)..."
Invoke-WebRequest -Uri $url -OutFile $tempFile -UseBasicParsing

# Extract based on file type
if ($url.EndsWith(".exe")) {
    # Inno Setup installer - use 7-zip to extract
    Write-Host "Extracting from Inno Setup installer..."
    & 7z x $tempFile -o"$DestinationPath\extracted" -y

    # Copy SDK files to standard locations
    Copy-Item "$DestinationPath\extracted\sdk\*" -Destination $DestinationPath -Recurse -Force
    Copy-Item "$DestinationPath\extracted\fbclient.dll" -Destination "$DestinationPath\bin\" -Force
} else {
    # ZIP file
    Write-Host "Extracting ZIP archive..."
    Expand-Archive -Path $tempFile -DestinationPath "$DestinationPath\extracted" -Force

    # Find and copy SDK structure
    $extractedDir = Get-ChildItem "$DestinationPath\extracted" -Directory | Select-Object -First 1
    Copy-Item "$extractedDir\*" -Destination $DestinationPath -Recurse -Force
}

# Clean up
Remove-Item $tempFile -Force
Remove-Item "$DestinationPath\extracted" -Recurse -Force -ErrorAction SilentlyContinue

# Verify required files exist
$requiredFiles = @(
    "include\ibase.h",
    "lib\fbclient_ms.lib"
)

foreach ($file in $requiredFiles) {
    $path = Join-Path $DestinationPath $file
    if (-not (Test-Path $path)) {
        throw "Required file not found: $path"
    }
}

Write-Host "Firebird $Version SDK installed successfully to $DestinationPath"
Write-Host "Contents:"
Get-ChildItem $DestinationPath -Recurse | ForEach-Object { Write-Host "  $($_.FullName)" }
