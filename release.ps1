# Wydaje nową wersję Nokturna: podbija numer, buduje instalator, publikuje GitHub Release.
# Aplikacje użytkowników zobaczą okienko „Dostępna nowa wersja” (src/Updater.cpp).
#   .\release.ps1 -Version 0.2.0 -Notes "- Szybszy start utworów z YouTube"
#   .\release.ps1 -Version 0.2.0 -NotesFile zmiany.md
#   .\release.ps1 -Version 0.2.0              — lista zmian z commitów (generuje GitHub)
param(
    [Parameter(Mandatory)][string]$Version,
    [string]$Notes = '',
    [string]$NotesFile = ''
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
Set-Location $root

if ($Version -notmatch '^\d+\.\d+\.\d+$') {
    Write-Error "Wersja musi mieć postać X.Y.Z (np. 0.2.0)"
}
$cmake = Join-Path $root 'CMakeLists.txt'
$current = (Select-String -Path $cmake -Pattern 'project\(Nokturn VERSION ([\d.]+)').Matches[0].Groups[1].Value
if ([version]$Version -le [version]$current) {
    Write-Error "Nowa wersja ($Version) musi być wyższa niż obecna ($current)"
}
if (git status --porcelain) {
    Write-Error "Masz niezacommitowane zmiany — najpierw je zacommituj"
}
if ((git rev-parse --abbrev-ref HEAD) -ne 'main') {
    Write-Error "Wydania robimy z gałęzi main"
}
if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
    Write-Error "Brak GitHub CLI (gh) — zainstaluj: winget install GitHub.cli"
}

# 1. Numer wersji (trafia do aplikacji przez NOKTURN_VERSION i do instalatora)
$text = [IO.File]::ReadAllText($cmake) -replace 'project\(Nokturn VERSION [\d.]+', "project(Nokturn VERSION $Version"
[IO.File]::WriteAllText($cmake, $text, (New-Object Text.UTF8Encoding $false))   # bez BOM

# 2. Instalator + suma kontrolna sprawdzana przez Updater przed instalacją
& (Join-Path $root 'build-installer.ps1')
if ($LASTEXITCODE) { git checkout -- $cmake; exit $LASTEXITCODE }
$setupName = "Nokturn-$Version-setup.exe"
$setup = Join-Path $root "installer\$setupName"
$hash = (Get-FileHash $setup -Algorithm SHA256).Hash.ToLower()
$sha = "$setup.sha256"
Set-Content $sha "$hash  $setupName" -Encoding ascii -NoNewline

# 3. Commit, tag, wypchnięcie
git add $cmake
git commit -m "Wersja $Version"
git tag "v$Version"
git push origin main "v$Version"

# 4. GitHub Release z instalatorem
$ghArgs = @("v$Version", $setup, $sha, '--title', "Nokturn $Version")
if ($NotesFile) { $ghArgs += @('--notes-file', $NotesFile) }
elseif ($Notes) { $ghArgs += @('--notes', $Notes) }
else { $ghArgs += '--generate-notes' }
gh release create @ghArgs
Write-Host "Opublikowano Nokturn $Version"
