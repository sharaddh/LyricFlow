# Recursively bundles every non-system (MSYS2 MinGW) DLL that the deployed
# Capson.exe, its Qt libraries and plugins depend on into dist\Capson.
#
# Qt's MSYS2/UCRT64 build links third-party shared libraries (brotli, zstd,
# zlib, ICU, freetype, harfbuzz, png, ...) that windeployqt does NOT copy. If
# they are missing the packaged app fails at launch with errors such as
# "libbrotlidec.dll was not found". This script walks the import table of every
# module in the payload, resolves transitive dependencies from the MinGW bin
# directory, and copies them next to the app. Any import that does not exist in
# the MinGW bin directory is a Windows system DLL and is intentionally skipped.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File packing\deploy_deps.ps1
# Optional overrides:
#   -TargetDir  <dir>   payload directory (default: dist\Capson)
#   -SearchDir  <dir>   MinGW bin directory (default: C:\msys64\ucrt64\bin)
param(
    [string]$TargetDir = "",
    [string]$SearchDir = "C:\msys64\ucrt64\bin"
)

$ErrorActionPreference = "Stop"
$ProjRoot = Split-Path -Parent $PSScriptRoot
if (-not $TargetDir) { $TargetDir = Join-Path $ProjRoot "dist\Capson" }

if (-not (Test-Path -LiteralPath $TargetDir)) { throw "Payload directory not found: $TargetDir" }
if (-not (Test-Path -LiteralPath $SearchDir)) { throw "MinGW bin directory not found: $SearchDir" }

$objdump = Join-Path $SearchDir "objdump.exe"
if (-not (Test-Path -LiteralPath $objdump)) { throw "objdump.exe not found in $SearchDir" }

# Cache of DLL name (lowercase) -> full path for candidates we can ship.
$available = @{}
foreach ($f in Get-ChildItem -LiteralPath $SearchDir -Filter *.dll -File) {
    $available[$f.Name.ToLower()] = $f.FullName
}

# Names already present anywhere in the payload.
$present = @{}
foreach ($f in Get-ChildItem -LiteralPath $TargetDir -Recurse -File) {
    $present[$f.Name.ToLower()] = $f.FullName
}

function Get-Imports([string]$path) {
    & $objdump -p $path 2>$null |
        Select-String "DLL Name:\s*(.+)" |
        ForEach-Object { $_.Matches[0].Groups[1].Value.Trim().ToLower() } |
        Sort-Object -Unique
}

$scanned = @{}
$queue = New-Object System.Collections.Generic.Queue[string]
foreach ($f in Get-ChildItem -LiteralPath $TargetDir -Recurse -File -Include *.exe,*.dll) {
    $queue.Enqueue($f.FullName)
}

$copied = New-Object System.Collections.Generic.List[string]

while ($queue.Count -gt 0) {
    $module = $queue.Dequeue()
    $key = $module.ToLower()
    if ($scanned.ContainsKey($key)) { continue }
    $scanned[$key] = $true

    foreach ($dep in Get-Imports $module) {
        if ($present.ContainsKey($dep)) {
            if (-not $scanned.ContainsKey($present[$dep].ToLower())) {
                $queue.Enqueue($present[$dep])
            }
            continue
        }
        if (-not $available.ContainsKey($dep)) { continue }   # Windows system DLL

        $dest = Join-Path $TargetDir (Split-Path -Leaf $available[$dep])
        Copy-Item -LiteralPath $available[$dep] -Destination $dest -Force
        $present[$dep] = $dest
        $copied.Add((Split-Path -Leaf $dest))
        $queue.Enqueue($dest)
    }
}

if ($copied.Count -eq 0) {
    Write-Output "All dependencies already bundled in $TargetDir"
} else {
    Write-Output ("Bundled {0} dependencies into {1}:" -f $copied.Count, $TargetDir)
    $copied | Sort-Object | ForEach-Object { Write-Output "  $_" }
}
