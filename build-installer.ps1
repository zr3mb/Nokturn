# Buduje instalator Nokturn dla Windows: kompilacja + dist\ + grafiki + Inno Setup.
#   .\build-installer.ps1    — wynik w installer\Nokturn-<wersja>-setup.exe
param(
    [string]$Msys = 'C:\msys64\ucrt64'
)
$root = $PSScriptRoot
$iscc = @(
    "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe",
    "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
    "$env:ProgramFiles\Inno Setup 6\ISCC.exe"
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $iscc) {
    Write-Error "Nie znaleziono Inno Setup 6 (zainstaluj: winget install JRSoftware.InnoSetup)"
    exit 1
}

& (Join-Path $root 'build.ps1') -Deploy -Msys $Msys
if ($LASTEXITCODE) { exit $LASTEXITCODE }

$bin = Join-Path $Msys 'bin'
& (Join-Path $bin 'python.exe') (Join-Path $root 'packaging\windows\make_assets.py') (Join-Path $bin 'rsvg-convert.exe')
if ($LASTEXITCODE) { exit $LASTEXITCODE }

$version = (Select-String -Path (Join-Path $root 'CMakeLists.txt') -Pattern 'project\(Nokturn VERSION ([\d.]+)').Matches[0].Groups[1].Value
& $iscc "/DAppVersion=$version" (Join-Path $root 'packaging\windows\nokturn.iss')
if ($LASTEXITCODE) { exit $LASTEXITCODE }
