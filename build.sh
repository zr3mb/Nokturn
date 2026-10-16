#!/usr/bin/env bash
# Builds Nokturn on Linux (Qt 6.8+, TagLib 2, CMake, Ninja).
#   ./build.sh                 — build (Release) into build/
#   ./build.sh --run           — build and run
#   ./build.sh --install       — build and install into ~/.local (program, icon, .desktop file)
#   ./build.sh --appimage      — build + a standalone Nokturn-x86_64.AppImage file (linuxdeploy)
# Variables: QT_DIR=/path/to/Qt/6.x/gcc_64 (when Qt isn't the system one), PREFIX=/usr/local (for --install),
#            DISCORD_CLIENT_ID=<Application ID> (built-in ID for the Discord status).
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
        *) echo "Unknown option: $arg" >&2; exit 1 ;;
    esac
done

for tool in cmake ninja; do
    if ! command -v "$tool" >/dev/null; then
        echo "The '$tool' program is missing. Install the dependencies, e.g.:" >&2
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
    echo "Installed into $install_prefix (run: nokturn)"
fi

if [[ $appimage == 1 ]]; then
    # linuxdeploy + the Qt plugin copy the Qt libraries (with the FFmpeg backend) and dependencies into AppDir.
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
    # DENO_BIN=/path/to/deno — ship Deno next to the program (yt-dlp's JavaScript runtime), like the Windows installer.
    if [[ -n "${DENO_BIN:-}" ]]; then
        install -m 755 "$DENO_BIN" "$appdir/usr/bin/deno"
    fi
    export QML_SOURCES_PATHS="$root/qml"
    export EXTRA_QT_MODULES="multimedia;websockets;dbus"
    # Native Wayland support when the Qt build has it (otherwise the app runs through XWayland).
    qt_plugins="${QT_DIR:+$QT_DIR/plugins}"
    if [[ -n "$qt_plugins" && -e "$qt_plugins/platforms/libqwayland-egl.so" ]]; then
        export EXTRA_PLATFORM_PLUGINS="libqwayland-egl.so;libqwayland-generic.so"
    fi
    [[ -n "${QT_DIR:-}" ]] && export QMAKE="$QT_DIR/bin/qmake"
    (cd "$build" && "$tools/linuxdeploy-$arch.AppImage" --appdir "$appdir" --plugin qt --output appimage \
        --desktop-file "$appdir/usr/share/applications/nokturn.desktop" \
        --icon-file "$appdir/usr/share/icons/hicolor/scalable/apps/nokturn.svg")
    mv "$build"/Nokturn*.AppImage "$root/" 2>/dev/null || true
    echo "Done: $(ls "$root"/Nokturn*.AppImage 2>/dev/null)"
fi

if [[ $run == 1 ]]; then
    exec "$build/nokturn"
fi
