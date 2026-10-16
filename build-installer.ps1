# Builds the Nokturn installer for Windows: build + dist\ + artwork + Inno Setup.
#   .\build-installer.ps1    — result in installer\Nokturn-<version>-setup.exe
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
    Write-Error "Inno Setup 6 not found (install: winget install JRSoftware.InnoSetup)"
    exit 1
}

& (Join-Path $root 'build.ps1') -Deploy -Msys $Msys
if ($LASTEXITCODE) { exit $LASTEXITCODE }

# Deno — the JavaScript runtime yt-dlp needs for full YouTube support
# (especially when signed in). Downloaded once to build\deno and shipped next to Nokturn.exe.
$denoDir = Join-Path $root 'build\deno'
$denoExe = Join-Path $denoDir 'deno.exe'
if (-not (Test-Path $denoExe)) {
    New-Item -ItemType Directory -Force $denoDir | Out-Null
    $zip = Join-Path $denoDir 'deno.zip'
    $base = 'https://github.com/denoland/deno/releases/latest/download/deno-x86_64-pc-windows-msvc.zip'
    Invoke-WebRequest $base -OutFile $zip -UseBasicParsing
    # The .sha256sum file is bytes with a Get-FileHash table — we extract the 64-character hash.
    $sums = [Text.Encoding]::UTF8.GetString((Invoke-WebRequest "$base.sha256sum" -UseBasicParsing).Content)
    $expected = ([regex]::Match($sums, '[0-9A-Fa-f]{64}')).Value.ToLower()
    if ((Get-FileHash $zip -Algorithm SHA256).Hash.ToLower() -ne $expected) {
        Write-Error 'The downloaded Deno does not match its checksum'
        exit 1
    }
    Expand-Archive $zip $denoDir -Force
    Remove-Item $zip
}
Copy-Item $denoExe (Join-Path $root 'dist') -Force

$bin = Join-Path $Msys 'bin'
& (Join-Path $bin 'python.exe') (Join-Path $root 'packaging\windows\make_assets.py') (Join-Path $bin 'rsvg-convert.exe')
if ($LASTEXITCODE) { exit $LASTEXITCODE }

$version = (Select-String -Path (Join-Path $root 'CMakeLists.txt') -Pattern 'project\(Nokturn VERSION ([\d.]+)').Matches[0].Groups[1].Value
& $iscc "/DAppVersion=$version" (Join-Path $root 'packaging\windows\nokturn.iss')
if ($LASTEXITCODE) { exit $LASTEXITCODE }
