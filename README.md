# Nokturn

A music player for many sources (local files, YouTube Music, internet radio, custom streams).
The interface is available in **English** and **Polish**.

**Working:** local files (folder scanning, tags and covers via TagLib, playback via Qt Multimedia / FFmpeg)
and **YouTube Music** (sign-in through the browser, liked songs, playlists, search, playback via yt-dlp).
The visualizer shows the real spectrum (FFT). On top of that: lyrics, mixing tracks with smooth transitions
(like Spotify's "Mix") and a "Listening" status on Discord. Runs on **Windows** and **Linux**.
Radio and custom streams are only an interface for now.

C++20 · Qt 6.8+ (Qt Quick / QML, Qt Multimedia, Qt WebSockets, Qt DBus on Linux) · TagLib 2 · CMake

## Download

Grab the latest version from [Releases](https://github.com/zr3mb/Nokturn/releases/latest):

- **Windows** — `Nokturn-<version>-setup.exe` (installer).
- **Linux** — `Nokturn-<version>-x86_64.AppImage`: `chmod +x Nokturn-*.AppImage` and run it (put it somewhere
  you can write to, e.g. `~/Applications`, so it can update itself). Needs `fuse2` (Arch: `sudo pacman -S fuse2`).

Both update themselves — see [Installer and updates](#installer-and-updates).

## Building (Windows, MSYS2 UCRT64)

Required packages:

```bash
pacman -S mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,qt6-base,qt6-declarative,qt6-multimedia,qt6-websockets,taglib}
```

```powershell
.\build.ps1 -Run       # build + run
.\build.ps1 -Deploy    # standalone dist\ folder (works without MSYS2)
```

## Installer and updates

```powershell
.\build-installer.ps1                                       # installer\Nokturn-<version>-setup.exe (Inno Setup 6, ships Deno)
.\release.ps1 -Version 0.3.0 -Notes "- What has changed"   # a new version for users
```

`release.ps1` bumps the number in `CMakeLists.txt`, builds the installer, computes SHA-256, commits, tags `v0.3.0`,
pushes and creates a **GitHub Release** with the installer (requires `gh` and a public repository). Without
`-Notes` the change list is generated from the commits.

Publishing the release starts the **Linux AppImage** workflow (`.github/workflows/linux-appimage.yml`, GitHub
Actions on Ubuntu 22.04 with Qt 6.8 and TagLib 2): it builds `Nokturn-<version>-x86_64.AppImage` with Deno
inside, checks that it starts and attaches it with its `.sha256` to the same release a few minutes later.
It can also be run by hand from the Actions tab (without a tag it only builds an artifact).

The app (`src/Updater.*`) checks `releases/latest` 5 s after startup and every 6 hours. When the release is newer,
it shows a dialog with the change list: *Update now* downloads the installer, verifies SHA-256 (the `digest` field
from the API or the `.sha256` file), runs it with `/SILENT` and closes Nokturn — the installer overwrites the
previous version and starts the new one. *Skip this version* silences the dialog until the next release; manually:
*Settings → Check for updates*. On Linux the AppImage downloads the new `…-x86_64.AppImage` next to itself
(SHA-256 verified), renames it over the running file and restarts; a build installed from source opens the release
page instead. `NOKTURN_UPDATE_API=<url>` overrides
the API URL (testing).

## Languages

The source language is **English**: user-visible text in QML goes through `qsTr()`, in C++ through `tr()`.
Translations live in `i18n/` as JSON (embedded in the app as a resource):

- `i18n/pl.json` — English text → Polish. Plural strings (`"%n tracks"`) map to an array of Polish forms
  `[one, few, many]`, e.g. `["%n utwór", "%n utwory", "%n utworów"]`.
- `i18n/en.json` — only the English plural forms `[one, other]`.

The language is picked in *Settings → Appearance → Language* (and on the first wizard screen): *System* follows
the operating system (Polish system → Polish, otherwise English). Switching takes effect immediately.

```bash
python scripts/check_translations.py           # missing / unused translations, Polish text left in sources
python scripts/check_translations.py --strict  # exit code 1 when something is missing (for CI)
```

Adding a language: create `i18n/<code>.json` (same keys as `pl.json`), add it to `qt_add_resources` in
`CMakeLists.txt`, add the plural rule in `JsonTranslator::pluralIndex` (`src/Language.cpp`) and the option in
the language switch (`qml/pages/SettingsPage.qml`).

## System integration

- **Windows — System Media Transport Controls:** Nokturn shows up in the system media flyout (next to the volume
  control, in Quick Settings, on the lock screen) with the title, artist, album, cover and timeline. The Play /
  Pause / Next / Previous / Stop buttons and seeking from the system control the player. **Media keys** on the
  keyboard go to the active SMTC session, so they also work with the window minimized. If SMTC is unavailable,
  the keys are registered as global hotkeys (`RegisterHotKey`). The implementation uses the WinRT ABI directly
  (`src/platform/WinRtMedia.h`), so it works with MinGW without C++/WinRT; only the system `runtimeobject`,
  `shlwapi` and `shcore` libraries are linked.
- **Linux — MPRIS:** the same over D-Bus (GNOME/KDE panel, media keys, `playerctl`).

## Building (Linux)

Required: Qt **6.8+** (base, declarative, multimedia with the FFmpeg backend, websockets, dbus modules),
TagLib **2**, CMake, Ninja, a C++20 compiler.

```bash
# Arch / Manjaro
sudo pacman -S cmake ninja qt6-base qt6-declarative qt6-multimedia qt6-multimedia-ffmpeg qt6-websockets taglib
# Fedora 41+
sudo dnf install cmake ninja-build gcc-c++ qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtmultimedia-devel qt6-qtwebsockets-devel taglib-devel
# Debian 13 / Ubuntu 25.04+ (older releases have Qt < 6.8 or TagLib 1 — use Qt from the qt.io installer / aqtinstall)
sudo apt install cmake ninja-build g++ qt6-base-dev qt6-declarative-dev qt6-multimedia-dev qt6-websockets-dev libtag-dev
```

```bash
./build.sh --run        # build + run
./build.sh --install    # install into ~/.local (the "nokturn" program, icon, .desktop file) — PREFIX=… changes the location
./build.sh --appimage   # standalone Nokturn-x86_64.AppImage (downloads linuxdeploy)
QT_DIR=~/Qt/6.8.3/gcc_64 ./build.sh   # Qt outside the system
```

On Linux:
- the program is called `nokturn`, data and settings are in `~/.local/share/Nokturn/Nokturn/`;
- **MPRIS** — media keys, GNOME/KDE panel controls and `playerctl` work right away;
- **signing in to YouTube Music** uses Chrome / Chromium / Brave / Edge / Vivaldi from PATH (Snap versions only as
  a fallback — Snap's sandbox doesn't allow a profile in a hidden directory). You have to close the sign-in window
  yourself after signing in (on Wayland another app's window title can't be read);
- **cookies** are encrypted with a key from the system password store (GNOME Keyring / KWallet via `secret-tool`,
  the `libsecret-tools` / `libsecret` package); without it the file is plain text, but with `0600` permissions;
- **yt-dlp** is downloaded as the standalone `yt-dlp_linux` (or `_aarch64`) file, also with SHA-256 verification;
- **whisper.cpp** (lyric timing) is installed from the distribution's repositories — Nokturn looks for the
  `whisper-cli` and `whisper-cpp` commands.

Developer tool: `Nokturn.exe --tour <folder>` (Linux: `nokturn --tour <folder>`) walks through every screen and
saves PNG screenshots of them.

## First run

On first start Nokturn shows a wizard (it can be skipped and started again in *Settings → Appearance*):

1. **Welcome** — including the interface language.
2. **Layout** — full / compact sidebar and the "Now playing" panel visible / hidden. Each variant has an animated
   window thumbnail (moving highlight, a playing track, the visualizer), and the choice changes the app right away.
3. **Colors** — accent from the cover (the thumbnail cycles through "covers") or fixed; live or calm background.
4. **Music** — folders with scan progress, signing in to YouTube Music (account, number of liked songs and
   playlists) and downloading the yt-dlp playback engine.
5. **Playback** — crossfade with a preview of the tracks overlapping, transition style, smart transitions,
   lyric timing (alpha, depending on the hardware).
6. **Integrations** — Discord status (with the application ID) and controls (shortcuts, MPRIS on Linux).
7. **Summary** of the choices.

Appearance and layout settings (`[ui]` in `settings.ini`) are remembered between launches.

## Local files — how it works

- **Folders** are chosen in *Sources → Local files → Add folder* (or from the Library). On the first run the
  system *Music* folder is added. The scan includes subfolders.
- **Formats:** MP3, FLAC, M4A/AAC/ALAC, OGG, Opus, WAV, WMA, AIFF, APE, WavPack, MPC.
- **Tags** (title, artist, album artist, album, year, number, genre, duration, quality) are read by TagLib.
  No tags → the file name and the folder name.
- **Covers:** embedded in the file or `cover/folder/front.jpg|png` in the album folder. They're cropped to a square
  and saved in the cache; the interface accent color is computed from the cover.
- **Cache:** `%APPDATA%\Nokturn\Nokturn\library.json` + `covers\`. Later scans are incremental — unchanged files
  (size + modification date) aren't read again. A scan starts in the background on every launch.
- **Queue:** clicking a track in a list (album, playlist, search results) makes that list the queue.
  Volume, shuffle/repeat modes and the last track and position are remembered.
- The `NOKTURN_PROFILE=<directory>` variable moves the settings and cache elsewhere (handy for testing).

## YouTube Music — how it works

- **Signing in (two stages):** *Sources → YouTube Music → Sign in with the browser*.
  1. Nokturn starts the installed Chrome / Edge / Brave as a separate window with its **own clean profile**
     (`%APPDATA%\Nokturn\Nokturn\browser`) — **without** DevTools, because Google blocks signing in in browsers
     with remote debugging ("This browser or app may not be secure"). You sign in as usual.
  2. When the window shows YouTube Music, Nokturn closes it and starts the same profile invisibly (headless)
     with the DevTools Protocol only to read the session (`Storage.getCookies`, `SAPISID` + `LOGIN_INFO` required).

  Your regular browser profiles aren't touched. As a fallback you can paste the `Cookie` header manually.
- **Storage:** the cookies (youtube.com domains only) are encrypted with DPAPI in `youtube.cookies` — only your
  Windows account can read them. *Sign out* removes them together with the browser profile.
- **API:** YouTube Music's internal API (InnerTube, the `WEB_REMIX` client) with the `SAPISIDHASH` header — just
  like the website: account, liked songs (`VLLM`), library playlists, playlist contents (with further pages),
  search. The content language follows the interface language. The heart in Nokturn likes / unlikes the track
  on the account too.
- **Artist profiles:** click an artist name anywhere (track lists, player bar, "Now playing", full screen) or an
  artist in the Library. The page shows the YouTube Music profile — photo, description, subscribers, popular songs,
  albums, singles, playlists and related artists (sections named as YouTube Music names them) — plus the artist's
  tracks from your library. Artists of local files are looked up on YouTube Music by name. Albums and playlists
  opened from a profile are loaded on demand and don't become part of the library.
- **Playback:** the audio stream URL comes from **yt-dlp** (the *Download* button fetches the official release
  from GitHub and verifies SHA-256; on Windows the faster unpacked build `yt-dlp_win.zip`). URLs are cached until
  they expire; the next track in the queue (also with shuffle) and a track you hover over are prepared in advance.
  yt-dlp needs a JavaScript runtime — the Windows installer ships **Deno**; otherwise Nokturn uses Deno or
  Node.js if present. URLs are resolved **anonymously first** (~2 s); the account cookies are only a fallback
  (age-restricted tracks, bot checks), because with them yt-dlp needs ~5 s per track.
  If a stream URL can't be opened, Nokturn asks for a fresh one once (through the account when signed in)
  before skipping the track. `NOKTURN_STREAM_DEBUG=1` prints how long each yt-dlp call took.
- **Note:** this is an unofficial way of using YouTube Music and goes against YouTube's terms of service. Changes
  on YouTube's side can break it (then *Update* next to yt-dlp helps), and in the extreme case they may lead to
  account restrictions.

## Spotify — how it works

- **What it does:** syncs your Spotify playlists (your own and followed ones) and liked songs into Nokturn. They
  show up in the sidebar ("Liked on Spotify" + one entry per playlist) and in the library.
- **Playback:** Spotify doesn't let third-party apps stream its audio, so each Spotify track is matched to
  YouTube Music when you play it (search "artist title", then the best match by title, artist and duration) and
  played through the same yt-dlp path as YouTube tracks. Matches are cached in `spotify-matches.json`; a track
  with no match is skipped with a notice. This needs the yt-dlp engine (see above), not a YouTube account.
- **Your own Client ID:** Spotify requires every app to be registered, and apps in development mode are limited
  to a few users — so each user signs in with their own (free) Spotify app:
  1. Open <https://developer.spotify.com/dashboard> and create an app.
  2. Add the redirect URI `http://127.0.0.1:43117/callback` and tick **Web API**.
  3. Copy the app's **Client ID** into *Sources → Spotify* and click *Sign in to Spotify*.

  A build can ship a built-in ID instead: `-DNOKTURN_SPOTIFY_CLIENT_ID=<id>` in CMake.
- **Sign-in:** OAuth 2.0 Authorization Code with PKCE (no client secret) in your regular browser; Nokturn listens
  on `127.0.0.1:43117` for the redirect. Read-only scopes: `playlist-read-private playlist-read-collaborative
  user-library-read`. The refresh token is encrypted (DPAPI / Secret Service) in `spotify.token`.
- **Sync:** a few seconds after start and with the refresh button. Playlists whose `snapshot_id` didn't change
  aren't downloaded again; the data is cached in `spotify.json`.

## Mixing tracks — how it works

- **Where:** the **Mix** button on a playlist, album or artist page (like in Spotify) — settings are separate for
  each playlist: transition style and length (2–16 s). The global **crossfade** for other queues is in
  *Settings → Playback* (0 = off). A playlist's Mix takes precedence over the crossfade.
- **Transition styles:**
  - *Crossfade* — classic, constant power (sin/cos);
  - *Rise* — a high-pass filter gradually cuts the lows of the ending track, the next one comes in later;
  - *Bass swap* — the highs crossfade over the whole transition while the bass switches quickly halfway (like a
    DJ: never two kick drums at once);
  - *Echo* — the ending track fades away into an echo (375 ms) from which the next one emerges;
  - *Automatic* — an energetic ending gets a bass swap, a calm one a crossfade.
- **Smart transitions** (on by default): silence at the end of the recording (< −50 dBFS for 0.8 s) starts the
  next track right away, a fade-out recorded in the track (≥ 14 dB below the average for 1 s) starts the
  transition earlier, and silence at the start of the next track (up to 4 s) is skipped.
- **How:** the player has two "decks" (two `QMediaPlayer`s). The next track is loaded on the second deck a dozen
  or so seconds before the end (YouTube stream URL included), and during the transition the samples of both decks
  are summed by `AudioEngine` — before the equalizer, so the equalizer, limiter and visualizer work on what you
  hear. The envelope starts only when the first samples of the new track arrive, so a delayed start leaves no gap.
  Seeking, "next" or pause during a transition are handled (the transition ends at once).
- Diagnostics: `NOKTURN_AUDIO_DEBUG=1` prints the sample rate and the number of buffer underruns every 3 s.

## Discord status

- *Settings → Discord*: a **"Listening"** status with the title, artist, cover (URL from YouTube Music; for local
  files the cover of the matched YouTube Music track) and a progress bar; for YouTube tracks — a "Listen on
  YouTube Music" button. When paused the status disappears (optionally it stays).
- Works through the Discord app's local IPC (Windows: `\\.\pipe\discord-ipc-N`, Linux: `discord-ipc-N`
  in `$XDG_RUNTIME_DIR`, also Discord from Flatpak/Snap and Vesktop). Nothing goes over the internet from Nokturn.
- Discord requires an **application ID**: create an application at
  [discord.com/developers/applications](https://discord.com/developers/applications) (its name shows up as
  "Listening to <name>"), copy the *Application ID* and paste it in the settings. It can also be built in at
  compile time: `-DNOKTURN_DISCORD_CLIENT_ID=…` (Linux: `DISCORD_CLIENT_ID=… ./build.sh`). Optionally add a
  `nokturn` image under *Rich Presence → Art Assets* — it shows instead of the cover when there's none online.
- Older Discord clients that don't know the newer fields (status type, buttons) get the basic version.

## Lyrics — how it works

- **Where:** the *Lyrics* tab in the "Now playing" panel (or the button with text lines on the player bar)
  and a column next to the cover in full-screen mode. With timed lyrics the current line is highlighted
  and scrolled to the center, and clicking a line seeks the track.
- **Where from (in order):**
  1. **YouTube Music** — first the timed version (requested as the `ANDROID_MUSIC` mobile client, because the
     website doesn't provide it), then plain text from the *Lyrics* tab. For local files the track is first
     looked up by artist and title (with a ±12 s duration check, so a live version isn't picked).
  2. **Local file** — an `.lrc` next to the track (timed) or the `LYRICS` / `UNSYNCEDLYRICS` tag.
     A local timed `.lrc` wins over plain text from YouTube.
- **Cache:** `lyrics\<sha1>.json` in the data directory. Missing lyrics are remembered for 3 days.

### Automatic timing (alpha)

For lyrics without timestamps you can turn on singing recognition in *Settings → Lyrics*:

- Runs **locally** with [whisper.cpp](https://github.com/ggml-org/whisper.cpp) (`whisper-cli`). The program
  (the `whisper-bin-x64.zip` package from GitHub, SHA-256 from the release's `digest` field) and a ggml model
  (Hugging Face, SHA-256 from `lfs.oid`) are downloaded from the settings. The package is unpacked with the `tar`
  built into Windows.
- Only available on **stronger hardware** (at least 4 CPU threads and 8 GB of RAM). The recommended model is
  picked for the hardware: `base` (142 MB), `small` (466 MB, from 8 threads), `medium` (1.5 GB, from 16 threads
  and 16 GB of RAM).
- Flow: Qt Multimedia decodes the track (a file or a YouTube stream) to 16 kHz mono WAV →
  `whisper-cli -ml 1 -sow -oj` returns words with timestamps (the start of the lyrics goes in as `--prompt`) →
  `LyricsAligner` aligns the lyric words with the transcription words (Needleman–Wunsch, fuzzy comparison with
  Levenshtein) and computes the time of each line; lines without recognized words are interpolated.
  A result covering less than 25% of the words is rejected.
- The result goes to the cache, so each track is recognized only once. Changing the track stops recognition.

## Structure

| Path | Contents |
|---|---|
| `src/TrackModel.*` | Track list model (`QAbstractListModel`) |
| `src/LibraryScanner.*` | Folder scanning on a worker thread: TagLib, covers, color palette, cache (de)serialization |
| `src/Library.*` | `Library` singleton: folders, scanning, albums, artists, playlists, search |
| `src/PlayerController.*` | `Player` singleton: two QMediaPlayer decks, queue, history, mixing, saving state |
| `src/SpectrumAnalyzer.*` | FFT (2048, Hann window) from `QAudioBufferOutput` → 48 logarithmic bands |
| `src/AppPaths.h` | Data directory and settings (`settings.ini`) |
| `src/Language.*` | `Language` singleton and the JSON translator (`i18n/*.json`), live language switching |
| `src/Updater.*` | `Updater` singleton: updates from GitHub Releases |
| `src/youtube/BrowserLogin.*` | Sign-in window in Chrome/Edge/Brave + DevTools Protocol (WebSocket) |
| `src/youtube/YtCookies.*` | Cookies: encryption (DPAPI / Secret Service), `SAPISIDHASH`, cookies.txt export |
| `src/youtube/YtMusicApi.*` | InnerTube client and response parser (tracks, playlists, account) |
| `src/youtube/StreamResolver.*` | yt-dlp: download with SHA-256 verification, stream URLs, cache |
| `src/spotify/SpotifyService.*` | `Spotify` singleton: PKCE sign-in, playlist / liked songs sync, YouTube Music matching |
| `src/youtube/YouTubeService.*` | `YouTube` singleton for QML: account state, sync, library cache |
| `src/audio/AudioEngine.*`, `src/audio/Equalizer.*` | Custom output path: deck mixer (transition styles), buffer + `QAudioSink`, 7-band parametric equalizer |
| `src/lyrics/Lyrics.*` | Lyrics structure, LRC parser, cache (de)serialization |
| `src/lyrics/LyricsAligner.*` | Aligning lyrics with the transcription (line times) |
| `src/lyrics/SpeechEngine.*` | whisper.cpp: download with SHA-256 verification, decoding to 16 kHz, transcription, hardware detection |
| `src/lyrics/LyricsService.*` | `Lyrics` singleton for QML: lyric sources, cache, current line, alignment (alpha) |
| `src/platform/DiscordPresence.*` | `Discord` singleton: Discord IPC (Rich Presence), "Listening" status |
| `src/platform/WindowsMediaControls.*`, `WinRtMedia.h` | Windows: SMTC (media flyout, lock screen) and media keys |
| `src/platform/Mpris.*` | MPRIS 2 on Linux (D-Bus): media keys, system panel controls |
| `src/TagLibPath.h` | File path for TagLib (wchar_t on Windows, file system bytes on Linux) |
| `src/Prefs.*` | `Prefs` singleton for QML: typed reading/writing of `settings.ini` (appearance, wizard) |
| `i18n/` | Translations: `pl.json` (Polish), `en.json` (English plural forms) |
| `scripts/check_translations.py` | Translation completeness check |
| `packaging/linux/` | `.desktop` file, SVG icon, metainfo (AppStream) |
| `packaging/windows/` | Inno Setup script and installer artwork |
| `qml/onboarding/` | First-run wizard: steps, animated layout thumbnails (`LayoutPreview`), transition preview |
| `qml/Theme.qml` | Palette, fonts, animation timings; the accent smoothly follows the cover |
| `qml/Nav.qml` | Navigation (back/forward history), panel state, notifications |
| `qml/Icons.qml` | Vector icons (SVG paths) |
| `qml/layout/` | Title bar, sidebar, player bar, "Now playing" panel, full-screen mode |
| `qml/pages/` | Home, Search, Library, Playlist/Album/Artist, Sources, Settings, Equalizer |
| `qml/components/` | Reusable components (covers, buttons, sliders, visualizer, cards…) |

## Keyboard shortcuts

| Shortcut | Action |
|---|---|
| Space | Play / pause |
| Ctrl + → / ← | Next / previous track |
| Ctrl + K, Ctrl + F | Search |
| Alt + ← / → | Back / forward |
| F11, Esc | Full-screen mode / exit |

## Next steps

1. Radio and URL streams (Qt Multimedia supports HTTP/Icecast).
2. Custom playlists, a track context menu, drag and drop in the queue.
3. Watching folders (QFileSystemWatcher) instead of scanning at startup.
4. YouTube Music: home page with mixes, track radio ("similar"), adding to playlists.
