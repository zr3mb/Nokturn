# Buduje Nokturn przy użyciu toolchainu MSYS2 UCRT64 (GCC + Qt 6).
#   .\build.ps1          — kompilacja (Release)
#   .\build.ps1 -Run     — kompilacja i uruchomienie
#   .\build.ps1 -Deploy  — kompilacja + samodzielny folder dist\ z bibliotekami Qt
param(
    [switch]$Run,
    [switch]$Deploy,
    [string]$Msys = 'C:\msys64\ucrt64'
)
$bin = Join-Path $Msys 'bin'
if (-not (Test-Path (Join-Path $bin 'cmake.exe'))) {
    Write-Error "Nie znaleziono toolchainu MSYS2 w $Msys (zainstaluj: pacman -S mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,qt6-base,qt6-declarative})"
    exit 1
}
$env:PATH = "$bin;$env:PATH"

$root = $PSScriptRoot
$build = Join-Path $root 'build'

cmake -S $root -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE) { exit $LASTEXITCODE }
cmake --build $build
if ($LASTEXITCODE) { exit $LASTEXITCODE }

$exe = Join-Path $build 'Nokturn.exe'

if ($Deploy) {
    $dist = Join-Path $root 'dist'
    New-Item -ItemType Directory -Force $dist | Out-Null
    Copy-Item $exe $dist -Force
    & (Join-Path $bin 'windeployqt6.exe') --release --qmldir (Join-Path $root 'qml') --no-translations (Join-Path $dist 'Nokturn.exe')
    # windeployqt nie kopiuje bibliotek MSYS2 (runtime GCC, ICU, zlib, freetype…),
    # więc dobieramy je, śledząc zależności każdego pliku binarnego przez ldd.
    $ldd = Join-Path (Split-Path $Msys) 'usr\bin\ldd.exe'
    $done = @{}
    do {
        $added = 0
        foreach ($f in Get-ChildItem $dist -Recurse -Include *.exe, *.dll) {
            if ($done.ContainsKey($f.FullName)) { continue }
            $done[$f.FullName] = $true
            foreach ($line in & $ldd $f.FullName 2>$null) {
                if ($line -match '=>\s+/ucrt64/bin/(\S+\.dll)') {
                    $name = $Matches[1]
                    if (-not (Test-Path (Join-Path $dist $name))) {
                        Copy-Item (Join-Path $bin $name) $dist
                        $added++
                    }
                }
            }
        }
    } while ($added -gt 0)
    # Qt z MSYS2 domyślnie szuka modułów QML w share\qt6\qml — wskazujemy układ z windeployqt.
    Set-Content (Join-Path $dist 'qt.conf') "[Paths]`nPrefix = .`nPlugins = .`nQmlImports = qml`n" -Encoding ascii
    Write-Host "Gotowe: $dist"
}

if ($Run) {
    & $exe
}
