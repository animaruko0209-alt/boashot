param([string]$Archive = '', [switch]$Restore)
$ErrorActionPreference = 'Stop'
# MSBuild may inherit PSModulePath from PowerShell 7.
Import-Module (Join-Path $PSHOME 'Modules/Microsoft.PowerShell.Utility/Microsoft.PowerShell.Utility.psd1')
$repo = Split-Path $PSScriptRoot -Parent
$dependencies = Join-Path $repo 'dependencies'
$destination = Join-Path $dependencies 'DxLib'
$expectedHash = 'DB9E6138C1665C9D9081DD04019219F332A1185448162FDBBC6F8182B9E4A847'
$useOfficialArchive = !$Archive
if ($useOfficialArchive) { $Archive = Join-Path $dependencies 'DxLib_VC3_25a.zip' }

# Serialize setup for this checkout, including simultaneous Debug and Release builds.
$sha = [System.Security.Cryptography.SHA256]::Create()
try {
    $pathBytes = [System.Text.Encoding]::UTF8.GetBytes($destination.ToUpperInvariant())
    $lockName = 'BoashotDxLib-' + [BitConverter]::ToString($sha.ComputeHash($pathBytes))
} finally { $sha.Dispose() }
$mutex = New-Object System.Threading.Mutex($false, $lockName)
try {
    try { [void]$mutex.WaitOne() } catch [System.Threading.AbandonedMutexException] {}
    try {
        if ($Restore -and (Test-Path -LiteralPath (Join-Path $destination 'DxLib.h')) -and
            (Test-Path -LiteralPath (Join-Path $destination 'dxlicense.html'))) { return }

        $staging = Join-Path $dependencies ('DxLib-setup-' + [Guid]::NewGuid())
        New-Item -ItemType Directory -Force -Path $staging | Out-Null
        try {
            if ($useOfficialArchive -and (!(Test-Path -LiteralPath $Archive) -or
                (Get-FileHash -LiteralPath $Archive -Algorithm SHA256).Hash -ne $expectedHash)) {
                $download = Join-Path $staging 'DxLib.zip'
                [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
                Invoke-WebRequest -UseBasicParsing 'https://dxlib.xsrv.jp/DxLib/DxLib_VC3_25a.zip' -OutFile $download
                if ((Get-FileHash -LiteralPath $download -Algorithm SHA256).Hash -ne $expectedHash) {
                    throw 'The downloaded DxLib 3.25a archive failed SHA-256 verification.'
                }
                Move-Item -LiteralPath $download -Destination $Archive -Force
            }

            Add-Type -AssemblyName System.IO.Compression.FileSystem
            $zip = [System.IO.Compression.ZipFile]::OpenRead($Archive)
            try {
                $header = $zip.Entries | Where-Object { $_.Name -eq 'DxLib.h' } | Select-Object -First 1
                if (!$header) { throw 'DxLib.h was not found in the SDK archive.' }
                $prefix = $header.FullName.Substring(0, $header.FullName.Length - $header.Name.Length)
                foreach ($entry in $zip.Entries) {
                    if (($entry.FullName.StartsWith($prefix) -and $entry.Name -match '\.(h|lib)$') -or
                        $entry.FullName -match '/help/dxlicense.html$') {
                        [System.IO.Compression.ZipFileExtensions]::ExtractToFile($entry, (Join-Path $staging $entry.Name), $true)
                    }
                }
            } finally { $zip.Dispose() }
            if (!(Get-ChildItem -LiteralPath $staging -Filter '*.lib') -or
                !(Test-Path -LiteralPath (Join-Path $staging 'dxlicense.html'))) {
                throw 'The SDK archive is missing libraries or dxlicense.html.'
            }

            # Normalize the official CP932 headers for the project's /utf-8 option.
            $cp932 = [System.Text.Encoding]::GetEncoding(932)
            $utf8 = New-Object System.Text.UTF8Encoding($true)
            Get-ChildItem -LiteralPath $staging -Filter '*.h' | ForEach-Object {
                $source = [System.IO.File]::ReadAllText($_.FullName, $cp932)
                [System.IO.File]::WriteAllText($_.FullName, $source, $utf8)
            }
            New-Item -ItemType Directory -Force -Path $destination | Out-Null
            # Publish DxLib.h last so the next build can retry an interrupted setup.
            Get-ChildItem -LiteralPath $staging | Where-Object Name -ne 'DxLib.h' | ForEach-Object {
                Move-Item -LiteralPath $_.FullName -Destination $destination -Force
            }
            Move-Item -LiteralPath (Join-Path $staging 'DxLib.h') -Destination $destination -Force
            Write-Host "DxLib SDK ready: $destination"
        } finally {
            Get-ChildItem -LiteralPath $staging -File | Remove-Item
            Remove-Item -LiteralPath $staging
        }
    } finally { $mutex.ReleaseMutex() }
} finally { $mutex.Dispose() }
