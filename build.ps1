# Builds Nokturn with the MSYS2 UCRT64 toolchain (GCC + Qt 6).
#   .\build.ps1          — build (Release)
#   .\build.ps1 -Run     — build and run
#   .\build.ps1 -Deploy  — build + a standalone dist\ folder with the Qt libraries
param(
    [switch]$Run,
    [switch]$Deploy,
    [string]$Msys = 'C:\msys64\ucrt64'
)
$bin = Join-Path $Msys 'bin'
if (-not (Test-Path (Join-Path $bin 'cmake.exe'))) {
    Write-Error "MSYS2 toolchain not found in $Msys (install: pacman -S mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,qt6-base,qt6-declarative})"
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
    # windeployqt doesn't copy the MSYS2 libraries (GCC runtime, ICU, zlib, freetype…),
    # so we pick them up by following each binary's dependencies with ldd.
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
    # Qt from MSYS2 looks for QML modules in share\qt6\qml by default — we point it to the windeployqt layout.
    Set-Content (Join-Path $dist 'qt.conf') "[Paths]`nPrefix = .`nPlugins = .`nQmlImports = qml`n" -Encoding ascii
    Write-Host "Done: $dist"
}

if ($Run) {
    & $exe
}
