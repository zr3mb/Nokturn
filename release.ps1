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
# Bez $ErrorActionPreference = 'Stop': w PowerShellu 5.1 ostrzeżenia narzędzi na stderr
# (np. windeployqt) przerwałyby wtedy wydanie. Błędy sprawdzamy jawnie.
$root = $PSScriptRoot
Set-Location $root

if ($Version -notmatch '^\d+\.\d+\.\d+$') {
    throw "Wersja musi mieć postać X.Y.Z (np. 0.2.0)"
}
$cmake = Join-Path $root 'CMakeLists.txt'
$current = (Select-String -Path $cmake -Pattern 'project\(Nokturn VERSION ([\d.]+)').Matches[0].Groups[1].Value
if ([version]$Version -le [version]$current) {
    throw "Nowa wersja ($Version) musi być wyższa niż obecna ($current)"
}
if (git status --porcelain --untracked-files=no) {
    throw "Masz niezacommitowane zmiany — najpierw je zacommituj"
}
if ((git rev-parse --abbrev-ref HEAD) -ne 'main') {
    throw "Wydania robimy z gałęzi main"
}
if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
    throw "Brak GitHub CLI (gh) — zainstaluj: winget install GitHub.cli"
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
if ($LASTEXITCODE) { throw "Nie udało się zacommitować wersji" }
git tag "v$Version"
git push origin main "v$Version"
if ($LASTEXITCODE) { throw "Nie udało się wypchnąć wersji $Version" }

# 4. GitHub Release z instalatorem
$ghArgs = @("v$Version", $setup, $sha, '--title', "Nokturn $Version")
if ($NotesFile) { $ghArgs += @('--notes-file', $NotesFile) }
elseif ($Notes) { $ghArgs += @('--notes', $Notes) }
else { $ghArgs += '--generate-notes' }
gh release create @ghArgs
if ($LASTEXITCODE) { throw "Nie udało się utworzyć wydania na GitHubie" }
Write-Host "Opublikowano Nokturn $Version"
