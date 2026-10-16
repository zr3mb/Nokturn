#!/usr/bin/env bash
# Buduje Nokturn na Linuksie (Qt 6.8+, TagLib 2, CMake, Ninja).
#   ./build.sh                 — kompilacja (Release) do build/
#   ./build.sh --run           — kompilacja i uruchomienie
#   ./build.sh --install       — kompilacja i instalacja do ~/.local (program, ikona, plik .desktop)
#   ./build.sh --appimage      — kompilacja + samodzielny plik Nokturn-x86_64.AppImage (linuxdeploy)
# Zmienne: QT_DIR=/ścieżka/do/Qt/6.x/gcc_64 (gdy Qt nie jest systemowe), PREFIX=/usr/local (dla --install),
#          DISCORD_CLIENT_ID=<Application ID> (wbudowany identyfikator dla statusu na Discordzie).
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build="$root/build"
run=0; install=0; appimage=0
for arg in "$@"; do
    case "$arg" in
        --run) run=1 ;;
        --install) install=1 ;;
        --appimage) appimage=1 ;;
        -h|--help) sed -n '2,9p' "$0"; exit 0 ;;
        *) echo "Nieznana opcja: $arg" >&2; exit 1 ;;
    esac
done

for tool in cmake ninja; do
    if ! command -v "$tool" >/dev/null; then
        echo "Brak programu '$tool'. Zainstaluj zależności, np.:" >&2
        echo "  Debian/Ubuntu: sudo apt install cmake ninja-build g++ qt6-base-dev qt6-declarative-dev qt6-multimedia-dev qt6-websockets-dev libtag-dev" >&2
        echo "  Fedora:        sudo dnf install cmake ninja-build gcc-c++ qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtmultimedia-devel qt6-qtwebsockets-devel taglib-devel" >&2
        echo "  Arch:          sudo pacman -S cmake ninja qt6-base qt6-declarative qt6-multimedia qt6-websockets taglib" >&2
        exit 1
    fi
done

prefix_path=()
[[ -n "${QT_DIR:-}" ]] && prefix_path=(-DCMAKE_PREFIX_PATH="$QT_DIR")
install_prefix="${PREFIX:-$HOME/.local}"
[[ $appimage == 1 ]] && install_prefix=/usr

cmake -S "$root" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$install_prefix" \
    -DNOKTURN_DISCORD_CLIENT_ID="${DISCORD_CLIENT_ID:-}" "${prefix_path[@]}"
cmake --build "$build"

if [[ $install == 1 ]]; then
    cmake --install "$build"
    command -v update-desktop-database >/dev/null && update-desktop-database "$install_prefix/share/applications" || true
    echo "Zainstalowano w $install_prefix (uruchom: nokturn)"
fi

if [[ $appimage == 1 ]]; then
    # linuxdeploy + wtyczka Qt kopiują biblioteki Qt (z backendem FFmpeg) i zależności do AppDir.
    tools="$build/appimage-tools"
    mkdir -p "$tools"
    arch="$(uname -m)"
    for t in linuxdeploy linuxdeploy-plugin-qt; do
        if [[ ! -x "$tools/$t-$arch.AppImage" ]]; then
            curl -fL -o "$tools/$t-$arch.AppImage" \
                "https://github.com/linuxdeploy/$t/releases/download/continuous/$t-$arch.AppImage"
            chmod +x "$tools/$t-$arch.AppImage"
        fi
    done
    appdir="$build/AppDir"
    rm -rf "$appdir"
    DESTDIR="$appdir" cmake --install "$build"
    export QML_SOURCES_PATHS="$root/qml"
    export EXTRA_QT_MODULES="multimedia;websockets;dbus"
    [[ -n "${QT_DIR:-}" ]] && export QMAKE="$QT_DIR/bin/qmake"
    (cd "$build" && "$tools/linuxdeploy-$arch.AppImage" --appdir "$appdir" --plugin qt --output appimage \
        --desktop-file "$appdir/usr/share/applications/nokturn.desktop" \
        --icon-file "$appdir/usr/share/icons/hicolor/scalable/apps/nokturn.svg")
    mv "$build"/Nokturn*.AppImage "$root/" 2>/dev/null || true
    echo "Gotowe: $(ls "$root"/Nokturn*.AppImage 2>/dev/null)"
fi

if [[ $run == 1 ]]; then
    exec "$build/nokturn"
fi
