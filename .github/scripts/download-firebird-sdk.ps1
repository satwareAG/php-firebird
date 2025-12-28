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
# All URLs verified to return HTTP 302 (redirect to download)
$downloads = @{
    "3.0" = @{
        "x64" = "https://github.com/FirebirdSQL/firebird/releases/download/v3.0.12/Firebird-3.0.12.33787-0-x64.zip"
        "x86" = "https://github.com/FirebirdSQL/firebird/releases/download/v3.0.12/Firebird-3.0.12.33787-0-win32.zip"
    }
    "4.0" = @{
        # Build number corrected: 3221 (was incorrectly 3602)
        "x64" = "https://github.com/FirebirdSQL/firebird/releases/download/v4.0.6/Firebird-4.0.6.3221-0-x64.zip"
        "x86" = $null  # FB 4.0 x86 not available
    }
    "5.0" = @{
        # Use ZIP files instead of .exe (Inno Setup) for reliable extraction
        "x64" = "https://github.com/FirebirdSQL/firebird/releases/download/v5.0.3/Firebird-5.0.3.1683-0-windows-x64.zip"
        "x86" = "https://github.com/FirebirdSQL/firebird/releases/download/v5.0.3/Firebird-5.0.3.1683-0-windows-x86.zip"
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

    # Firebird 5.0 has a different structure - look for files in multiple locations
    New-Item -ItemType Directory -Force -Path "$DestinationPath\include" | Out-Null
    New-Item -ItemType Directory -Force -Path "$DestinationPath\lib" | Out-Null
    New-Item -ItemType Directory -Force -Path "$DestinationPath\bin" | Out-Null
    
    # Try different known paths for Inno Setup extraction
    $sdkPaths = @("$DestinationPath\extracted\sdk", "$DestinationPath\extracted\{app}\sdk", "$DestinationPath\extracted")
    $foundSdk = $false
    foreach ($sdkPath in $sdkPaths) {
        if (Test-Path "$sdkPath\include\ibase.h") {
            Write-Host "Found SDK at: $sdkPath"
            Copy-Item "$sdkPath\include\*" -Destination "$DestinationPath\include\" -Recurse -Force
            Copy-Item "$sdkPath\lib\*" -Destination "$DestinationPath\lib\" -Recurse -Force -ErrorAction SilentlyContinue
            $foundSdk = $true
            break
        }
    }
    
    # Find fbclient.dll in various locations
    $fbclientPaths = @(
        "$DestinationPath\extracted\fbclient.dll",
        "$DestinationPath\extracted\{app}\fbclient.dll",
        "$DestinationPath\extracted\WOW64\fbclient.dll"
    )
    foreach ($fbPath in $fbclientPaths) {
        if (Test-Path $fbPath) {
            Copy-Item $fbPath -Destination "$DestinationPath\bin\" -Force
            break
        }
    }
    
    # Also copy lib files
    $libPaths = @("$DestinationPath\extracted\lib", "$DestinationPath\extracted\{app}\lib")
    foreach ($libPath in $libPaths) {
        if (Test-Path $libPath) {
            Copy-Item "$libPath\*" -Destination "$DestinationPath\lib\" -Recurse -Force -ErrorAction SilentlyContinue
            break
        }
    }
} else {
    # ZIP file
    Write-Host "Extracting ZIP archive..."
    Expand-Archive -Path $tempFile -DestinationPath "$DestinationPath\extracted" -Force

    # Create standard directory structure
    New-Item -ItemType Directory -Force -Path "$DestinationPath\include" | Out-Null
    New-Item -ItemType Directory -Force -Path "$DestinationPath\lib" | Out-Null
    New-Item -ItemType Directory -Force -Path "$DestinationPath\bin" | Out-Null
    
    # Firebird ZIPs can have two structures:
    # 1. Flat: extracted/include/ibase.h (FB 4.0, 5.0 ZIPs)
    # 2. Nested: extracted/Firebird-X.Y.Z/sdk/include/ibase.h (some older archives)
    
    $extractedBase = "$DestinationPath\extracted"
    
    # Debug: list top-level contents
    Write-Host "Contents of extracted folder:"
    Get-ChildItem $extractedBase | ForEach-Object { Write-Host "  $($_.Name)" }
    
    # Check for flat structure first (headers directly in extracted/include)
    if (Test-Path "$extractedBase\include\ibase.h") {
        Write-Host "Found flat structure (include/ at root)"
        Copy-Item "$extractedBase\include\*" -Destination "$DestinationPath\include\" -Recurse -Force
        if (Test-Path "$extractedBase\lib") {
            Copy-Item "$extractedBase\lib\*" -Destination "$DestinationPath\lib\" -Recurse -Force -ErrorAction SilentlyContinue
        }
        if (Test-Path "$extractedBase\bin") {
            Copy-Item "$extractedBase\bin\*" -Destination "$DestinationPath\bin\" -Recurse -Force -ErrorAction SilentlyContinue
        }
        # Copy fbclient.dll from root to multiple locations (build system looks in various places)
        if (Test-Path "$extractedBase\fbclient.dll") {
            Write-Host "Copying fbclient.dll to bin/, lib/, and root"
            Copy-Item "$extractedBase\fbclient.dll" -Destination "$DestinationPath\bin\" -Force
            Copy-Item "$extractedBase\fbclient.dll" -Destination "$DestinationPath\lib\" -Force
            Copy-Item "$extractedBase\fbclient.dll" -Destination "$DestinationPath\" -Force
        }
    }
    # Check for flat structure with sdk subdirectory
    elseif (Test-Path "$extractedBase\sdk\include\ibase.h") {
        Write-Host "Found flat structure with sdk/ subdirectory"
        Copy-Item "$extractedBase\sdk\include\*" -Destination "$DestinationPath\include\" -Recurse -Force
        Copy-Item "$extractedBase\sdk\lib\*" -Destination "$DestinationPath\lib\" -Recurse -Force -ErrorAction SilentlyContinue
        # Copy fbclient.dll from root
        if (Test-Path "$extractedBase\fbclient.dll") {
            Copy-Item "$extractedBase\fbclient.dll" -Destination "$DestinationPath\bin\" -Force
        }
    }
    # Check for nested structure (named directory containing install)
    else {
        $extractedDirInfo = Get-ChildItem $extractedBase -Directory | Where-Object { $_.Name -like "Firebird*" } | Select-Object -First 1
        if (-not $extractedDirInfo) {
            $extractedDirInfo = Get-ChildItem $extractedBase -Directory | Select-Object -First 1
        }
        
        if ($extractedDirInfo) {
            $extractedDir = $extractedDirInfo.FullName
            Write-Host "Checking nested directory: $extractedDir"
            
            if (Test-Path "$extractedDir\include\ibase.h") {
                Write-Host "Found SDK at nested root level"
                Copy-Item "$extractedDir\include\*" -Destination "$DestinationPath\include\" -Recurse -Force
                Copy-Item "$extractedDir\lib\*" -Destination "$DestinationPath\lib\" -Recurse -Force -ErrorAction SilentlyContinue
                if (Test-Path "$extractedDir\fbclient.dll") {
                    Copy-Item "$extractedDir\fbclient.dll" -Destination "$DestinationPath\bin\" -Force
                }
            } elseif (Test-Path "$extractedDir\sdk\include\ibase.h") {
                Write-Host "Found SDK in nested sdk\ subdirectory"
                Copy-Item "$extractedDir\sdk\include\*" -Destination "$DestinationPath\include\" -Recurse -Force
                Copy-Item "$extractedDir\sdk\lib\*" -Destination "$DestinationPath\lib\" -Recurse -Force -ErrorAction SilentlyContinue
                if (Test-Path "$extractedDir\fbclient.dll") {
                    Copy-Item "$extractedDir\fbclient.dll" -Destination "$DestinationPath\bin\" -Force
                }
            }
        }
    }
    
    # Final fallback: search for ibase.h and copy from found location
    if (-not (Test-Path "$DestinationPath\include\ibase.h")) {
        Write-Host "Fallback: searching for ibase.h..."
        $ibaseFile = Get-ChildItem $extractedBase -Recurse -Filter "ibase.h" | Select-Object -First 1
        if ($ibaseFile) {
            $includeDir = $ibaseFile.Directory.FullName
            Write-Host "Found ibase.h at: $includeDir"
            Copy-Item "$includeDir\*" -Destination "$DestinationPath\include\" -Recurse -Force
            
            # Try to find lib directory nearby
            $parentDir = Split-Path $includeDir -Parent
            if (Test-Path "$parentDir\lib") {
                Copy-Item "$parentDir\lib\*" -Destination "$DestinationPath\lib\" -Recurse -Force -ErrorAction SilentlyContinue
            }
        }
    }
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
