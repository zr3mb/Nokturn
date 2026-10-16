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

# Deno — środowisko JavaScript, którego yt-dlp potrzebuje do pełnej obsługi YouTube
# (zwłaszcza po zalogowaniu). Pobierane raz do build\deno i dołączane obok Nokturn.exe.
$denoDir = Join-Path $root 'build\deno'
$denoExe = Join-Path $denoDir 'deno.exe'
if (-not (Test-Path $denoExe)) {
    New-Item -ItemType Directory -Force $denoDir | Out-Null
    $zip = Join-Path $denoDir 'deno.zip'
    $base = 'https://github.com/denoland/deno/releases/latest/download/deno-x86_64-pc-windows-msvc.zip'
    Invoke-WebRequest $base -OutFile $zip -UseBasicParsing
    # Plik .sha256sum to bajty z tabelą Get-FileHash — wyłuskujemy 64-znakowy skrót.
    $sums = [Text.Encoding]::UTF8.GetString((Invoke-WebRequest "$base.sha256sum" -UseBasicParsing).Content)
    $expected = ([regex]::Match($sums, '[0-9A-Fa-f]{64}')).Value.ToLower()
    if ((Get-FileHash $zip -Algorithm SHA256).Hash.ToLower() -ne $expected) {
        Write-Error 'Pobrany Deno nie zgadza się z sumą kontrolną'
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
