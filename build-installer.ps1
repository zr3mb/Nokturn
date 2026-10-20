# Builds the Nokturn installer for Windows: build + dist\ + the package the setup downloads + the setup.
#   .\build-installer.ps1              — installer\Nokturn-<version>-setup.exe and Nokturn-<version>-win64.tar.zst
#   .\build-installer.ps1 -SetupOnly   — just the setup again, for the package built last time
param(
    [string]$Msys = 'C:\msys64\ucrt64',
    [switch]$SetupOnly
)
$root = $PSScriptRoot
$bin = Join-Path $Msys 'bin'
$env:PATH = "$bin;$env:PATH"   # g++ and windres need their own tools
$version = (Select-String -Path (Join-Path $root 'CMakeLists.txt') -Pattern 'project\(Nokturn VERSION ([\d.]+)').Matches[0].Groups[1].Value
$out = Join-Path $root 'installer'
$work = Join-Path $root 'build\setup'
$config = Join-Path $work 'setup_config.h'
New-Item -ItemType Directory -Force $out, $work | Out-Null

if (-not $SetupOnly) {
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

    & (Join-Path $bin 'python.exe') (Join-Path $root 'packaging\windows\make_assets.py') (Join-Path $bin 'rsvg-convert.exe')
    if ($LASTEXITCODE) { exit $LASTEXITCODE }

    # The package (dist\ + icon) and setup_config.h with its size and SHA-256 for the setup.
    & (Join-Path $bin 'python.exe') (Join-Path $root 'packaging\windows\make_payload.py') (Join-Path $root 'dist') `
        (Join-Path $root 'packaging\windows\assets\nokturn.ico') $version (Join-Path $out "Nokturn-$version-win64.tar.zst") `
        $config (Join-Path $bin 'zstd.exe')
    if ($LASTEXITCODE) { exit $LASTEXITCODE }
}
if (-not (Test-Path $config)) {
    Write-Error 'No package built yet: run without -SetupOnly first'
    exit 1
}

# The setup: one static .exe (Win32, Direct2D, WinHTTP, zstd).
$src = Join-Path $root 'packaging\windows\setup'
$res = Join-Path $work 'setup.res.o'
& (Join-Path $bin 'windres.exe') -I $src -I $work -I (Join-Path $root 'packaging\windows\assets') `
    -i (Join-Path $src 'setup.rc') -o $res
if ($LASTEXITCODE) { exit $LASTEXITCODE }
& (Join-Path $bin 'g++.exe') -std=c++20 -O2 -s -static -municode -mwindows -Wall "-I$work" (Join-Path $src 'setup.cpp') $res `
    -o (Join-Path $out "Nokturn-$version-setup.exe") `
    -ld2d1 -ldwrite -ldwmapi -lwindowscodecs -lwinhttp -lbcrypt -lole32 -luuid -lshell32 -lzstd
if ($LASTEXITCODE) { exit $LASTEXITCODE }
Write-Host "Built installer\Nokturn-$version-setup.exe"
