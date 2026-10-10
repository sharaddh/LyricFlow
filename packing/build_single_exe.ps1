# Builds a single portable Capson.exe from the deployed payload in dist\Capson.
$ErrorActionPreference = "Stop"
$ProjRoot = Split-Path -Parent $PSScriptRoot
$distRoot = Join-Path $ProjRoot "dist\Capson"
$stub = Join-Path $PSScriptRoot "stub.exe"
$out = Join-Path $ProjRoot "dist\Capson-Portable.exe"

if (-not (Test-Path $distRoot)) { throw "dist\Capson not found - deploy first." }
if (-not (Test-Path $stub)) { throw "stub.exe not found - compile it first." }

$stubBytes = [System.IO.File]::ReadAllBytes($stub)

# Collect every file in payload order; empty dirs are recorded so they exist after
# extraction. Data bytes are appended to a single blob in the exact same order.
$records = New-Object System.Collections.Generic.List[object]
$dataMs = New-Object System.IO.MemoryStream

function Compress-Bytes([byte[]]$data) {
    $ms = New-Object System.IO.MemoryStream
    $ds = New-Object System.IO.Compression.DeflateStream($ms, [System.IO.Compression.CompressionLevel]::Optimal)
    $ds.Write($data, 0, $data.Length)
    $ds.Close()
    return $ms.ToArray()
}

foreach ($f in (Get-ChildItem -LiteralPath $distRoot -Recurse -File)) {
    $rel = $f.FullName.Substring($distRoot.Length + 1) -replace '\\', '/'
    $data = [System.IO.File]::ReadAllBytes($f.FullName)
    $comp = Compress-Bytes $data
    if ($comp.Length -lt $data.Length) {
        $records.Add(@{ Name = $rel; IsDir = $false; Compressed = $true; Data = $comp; UncompLen = $data.Length })
        $dataMs.Write($comp, 0, $comp.Length)
    } else {
        $records.Add(@{ Name = $rel; IsDir = $false; Compressed = $false; Data = $data; UncompLen = $data.Length })
        $dataMs.Write($data, 0, $data.Length)
    }
}

foreach ($d in (Get-ChildItem -LiteralPath $distRoot -Recurse -Directory)) {
    if ((Get-ChildItem -LiteralPath $d.FullName -Force | Measure-Object).Count -eq 0) {
        $rel = $d.FullName.Substring($distRoot.Length + 1) -replace '\\', '/'
        $records.Add(@{ Name = $rel; IsDir = $true })
    }
}

$dataBytes = $dataMs.ToArray()
$outMs = New-Object System.IO.MemoryStream
$bw = New-Object System.IO.BinaryWriter($outMs)
$bw.Write([byte[]]$stubBytes)
$bw.Write([byte[]]$dataBytes)

$tableOffset = [uint64]($stubBytes.Length + $dataBytes.Length)
$bw.Write([uint32]$records.Count)
foreach ($r in $records) {
    $bw.Write([byte]($(if ($r.IsDir) { 1 } else { 0 })))
    $bw.Write([byte]($(if ($r.IsDir -or -not $r.Compressed) { 0 } else { 1 })))
    $name = [System.Text.Encoding]::UTF8.GetBytes($r.Name)
    $bw.Write([uint32]$name.Length)
    $bw.Write([uint32]($(if ($r.IsDir) { 0 } else { $r.UncompLen })))
    $storedLen = $(if ($r.IsDir) { 0 } else { $r.Data.Length })
    $bw.Write([uint32]$storedLen)
    $bw.Write([byte[]]$name)
}
$bw.Write([uint64]$tableOffset)
$bw.Write([uint64]$dataBytes.Length)
$bw.Flush()

[System.IO.File]::WriteAllBytes($out, $outMs.ToArray())
$sizeMB = [math]::Round((Get-Item $out).Length / 1MB, 1)
Write-Output "Created $out ($sizeMB MB, $($records.Count) files)"