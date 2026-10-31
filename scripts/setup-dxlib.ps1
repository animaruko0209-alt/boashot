param([string]$Archive = '')
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$destination = Join-Path $repo 'dependencies/DxLib'
if (!$Archive) { $Archive = Join-Path $repo 'dependencies/DxLib_VC3_25a.zip' }
if (!(Test-Path -LiteralPath $Archive)) {
    New-Item -ItemType Directory -Force (Split-Path $Archive -Parent) | Out-Null
    Invoke-WebRequest 'https://dxlib.xsrv.jp/DxLib/DxLib_VC3_25a.zip' -OutFile $Archive
}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::OpenRead($Archive)
try {
    $header = $zip.Entries | Where-Object { $_.Name -eq 'DxLib.h' } | Select-Object -First 1
    if (!$header) { throw 'DxLib.h was not found in the SDK archive.' }
    $prefix = $header.FullName.Substring(0, $header.FullName.Length - $header.Name.Length)
    New-Item -ItemType Directory -Force $destination | Out-Null
    foreach ($entry in $zip.Entries) {
        if (($entry.FullName.StartsWith($prefix) -and $entry.Name -match '\.(h|lib)$') -or $entry.FullName -match '/help/dxlicense.html$') {
            [System.IO.Compression.ZipFileExtensions]::ExtractToFile($entry, (Join-Path $destination $entry.Name), $true)
        }
    }
} finally { $zip.Dispose() }
# The official SDK headers use CP932. Normalize a local copy for /utf-8 builds.
$cp932 = [System.Text.Encoding]::GetEncoding(932)
$utf8 = New-Object System.Text.UTF8Encoding($true)
Get-ChildItem -LiteralPath $destination -Filter '*.h' | ForEach-Object {
    $source = [System.IO.File]::ReadAllText($_.FullName, $cp932)
    [System.IO.File]::WriteAllText($_.FullName, $source, $utf8)
}
Write-Host "DxLib SDK ready: $destination"
Write-Host "Archive SHA256: $((Get-FileHash -LiteralPath $Archive -Algorithm SHA256).Hash)"
