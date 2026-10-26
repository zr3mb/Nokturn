#!/usr/bin/env bash
# Builds the Android app (arm64-v8a) as an installable APK.
#   scripts/build-android.sh            — build/android/Nokturn-<version>-arm64.apk
# Needs Qt 6.8 for Android plus the matching desktop Qt, the Android SDK and NDK 26.1:
#   QT_ANDROID=/path/to/Qt/6.8.3/android_arm64_v8a   QT_HOST=/path/to/Qt/6.8.3/gcc_64
#   ANDROID_SDK_ROOT=/path/to/sdk                    ANDROID_NDK_ROOT=$ANDROID_SDK_ROOT/ndk/26.1.10909125
# Signing: with NOKTURN_KEYSTORE (+ NOKTURN_KEYSTORE_PASS, NOKTURN_KEY_ALIAS) the APK is signed with that
# key; otherwise with a debug key made in build/android/ (fine for installing by hand, not for updates
# across machines).
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
out="$root/build/android"
: "${QT_ANDROID:?set QT_ANDROID to Qt for Android (…/6.8.3/android_arm64_v8a)}"
: "${QT_HOST:?set QT_HOST to the desktop Qt of the same version (…/6.8.3/gcc_64)}"
: "${ANDROID_SDK_ROOT:?set ANDROID_SDK_ROOT}"
ANDROID_NDK_ROOT="${ANDROID_NDK_ROOT:-$ANDROID_SDK_ROOT/ndk/26.1.10909125}"
TAGLIB_VERSION="${TAGLIB_VERSION:-v2.0.2}"
export ANDROID_SDK_ROOT ANDROID_NDK_ROOT

mkdir -p "$out"
deps="$out/deps"

# TagLib 2 as a static library for arm64 (built once).
if [[ ! -f "$deps/lib/libtag.a" ]]; then
    [[ -d "$out/taglib" ]] || git clone --depth 1 --branch "$TAGLIB_VERSION" --recurse-submodules \
        https://github.com/taglib/taglib.git "$out/taglib"
    cmake -S "$out/taglib" -B "$out/taglib/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-28 \
        -DBUILD_SHARED_LIBS=OFF -DWITH_ZLIB=ON -DBUILD_TESTING=OFF -DBUILD_EXAMPLES=OFF -DBUILD_BINDINGS=OFF \
        -DCMAKE_INSTALL_PREFIX="$deps"
    cmake --build "$out/taglib/build"
    cmake --install "$out/taglib/build"
fi

# OpenSSL for Qt's HTTPS: KDAB's prebuilt Android libraries (only the .so files are used).
[[ -d "$out/openssl" ]] || git clone --depth 1 https://github.com/KDAB/android_openssl.git "$out/openssl"

"$QT_ANDROID/bin/qt-cmake" -S "$root" -B "$out/app" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DQT_HOST_PATH="$QT_HOST" -DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" -DANDROID_NDK_ROOT="$ANDROID_NDK_ROOT" \
    -DCMAKE_FIND_ROOT_PATH="$deps" -DCMAKE_PREFIX_PATH="$deps" \
    -DQT_ANDROID_BUILD_ALL_ABIS=OFF -DQT_ANDROID_ABIS=arm64-v8a \
    -DNOKTURN_ANDROID_OPENSSL="$out/openssl/ssl_3/arm64-v8a"
cmake --build "$out/app" --target apk

version="$(sed -n 's/^project(Nokturn VERSION \([0-9.]*\).*/\1/p' "$root/CMakeLists.txt")"
unsigned="$(find "$out/app" -name '*-release-unsigned.apk' -o -name 'android-build-*-release.apk' | head -n1)"
[[ -n "$unsigned" ]] || unsigned="$(find "$out/app" -name '*.apk' | head -n1)"
[[ -n "$unsigned" ]] || { echo "No APK was built" >&2; exit 1; }

build_tools="$(ls -d "$ANDROID_SDK_ROOT"/build-tools/* | sort -V | tail -n1)"
keystore="${NOKTURN_KEYSTORE:-$out/debug.keystore}"
alias="${NOKTURN_KEY_ALIAS:-nokturn}"
pass="${NOKTURN_KEYSTORE_PASS:-android}"
if [[ ! -f "$keystore" ]]; then
    keytool -genkeypair -keystore "$keystore" -alias "$alias" -storepass "$pass" -keypass "$pass" \
        -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=Nokturn debug" >/dev/null
fi
apk="$out/Nokturn-$version-arm64.apk"
"$build_tools/zipalign" -f -p 4 "$unsigned" "$out/aligned.apk"
"$build_tools/apksigner" sign --ks "$keystore" --ks-key-alias "$alias" --ks-pass "pass:$pass" \
    --key-pass "pass:$pass" --out "$apk" "$out/aligned.apk"
rm -f "$out/aligned.apk"
echo "$apk"
