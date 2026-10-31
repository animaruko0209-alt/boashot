param([ValidateSet('Debug','Release')][string]$Configuration = 'Debug', [switch]$Tests)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$installation) { throw 'Visual Studio C++ build tools are required.' }
$msbuild = Join-Path $installation 'MSBuild/Current/Bin/MSBuild.exe'
if ($Tests) {
    & $msbuild (Join-Path $repo 'tests/boashot-tests.vcxproj') /m /nologo /v:minimal "/p:Configuration=$Configuration" /p:Platform=x64
    if ($LASTEXITCODE -ne 0) { throw 'Test build failed.' }
    & (Join-Path $repo "tests/bin/$Configuration/boashot-tests.exe")
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }
} else {
    if (!(Test-Path (Join-Path $repo 'dependencies/DxLib/DxLib.h'))) { & "$PSScriptRoot/setup-dxlib.ps1" }
    & $msbuild (Join-Path $repo 'boashot.sln') /m /nologo /v:minimal "/p:Configuration=$Configuration" /p:Platform=x64
    if ($LASTEXITCODE -ne 0) { throw 'Game build failed.' }
}
