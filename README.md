# Nokturn

A music player for many sources (local files, YouTube Music, internet radio, custom streams).
The interface is available in **English** and **Polish**.

**Working:** local files (folder scanning, tags and covers via TagLib, playback via Qt Multimedia / FFmpeg)
and **YouTube Music** (sign-in through the browser, liked songs, playlists, search, playback via yt-dlp).
The visualizer shows the real spectrum (FFT). On top of that: lyrics, mixing tracks with smooth transitions
(like Spotify's "Mix") and a "Listening" status on Discord. Runs on **Windows** and **Linux**.
Online, through a small server of its own: **share links** for tracks, **shared playlists** everyone with the link
adds to, and **listening rooms** where friends hear the same queue at the same moment.
Radio and custom streams are only an interface for now.

C++20 · Qt 6.8+ (Qt Quick / QML, Qt Multimedia, Qt WebSockets, Qt DBus on Linux) · TagLib 2 · CMake

## Download

Grab the latest version from [Releases](https://github.com/zr3mb/Nokturn/releases/latest):

- **Windows** — `Nokturn-<version>-setup.exe` (a small installer that downloads the app from the release).
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
.\build-installer.ps1                                       # installer\Nokturn-<version>-setup.exe and -win64.tar.zst
.\release.ps1 -Version 0.3.0 -Notes "- What has changed"   # a new version for users
```

The installer is our own (`packaging/windows/setup/setup.cpp`, Win32 + Direct2D in the app's colors, under 1 MB).
`build-installer.ps1` packs `dist\` (with Deno) into `Nokturn-<version>-win64.tar.zst` and builds the setup with
that file's size and SHA-256 inside; the setup downloads it from the same GitHub release, checks it and installs
per user to `%LOCALAPPDATA%\Programs\Nokturn` (no admin rights). It takes over an install made by the old Inno
Setup installer, removes files the previous version had and this one doesn't, and copies itself as
`uninstall.exe` for *Settings → Apps*. Switches: `/SILENT` (progress window only), `/VERYSILENT`, `/DIR="…"`; for
testing `--payload <file>`, `--url <url>`, `--test` (separate registry entry and shortcuts), `--no-launch` and
`--preview <screen> <file.png>` (renders a screen without opening the window).

`release.ps1` bumps the number in `CMakeLists.txt`, builds the setup and the package, computes SHA-256, commits,
tags `v0.3.0`, pushes and creates a **GitHub Release** with both (requires `gh` and a public repository). Without
`-Notes` the change list is generated from the commits.

Publishing the release starts the **Linux AppImage** workflow (`.github/workflows/linux-appimage.yml`, GitHub
Actions on Ubuntu 22.04 with Qt 6.8 and TagLib 2): it builds `Nokturn-<version>-x86_64.AppImage` with Deno
inside, checks that it starts and attaches it with its `.sha256` to the same release a few minutes later.
It can also be run by hand from the Actions tab (without a tag it only builds an artifact).
The **Windows installer** workflow (`.github/workflows/windows-installer.yml`, MSYS2 UCRT64 on GitHub Actions) does
the same for the setup and its package, so a release can also be made without Windows: bump the version in
`CMakeLists.txt` on `main`, then `gh release create vX.Y.Z --target main --generate-notes`.

The app (`src/Updater.*`) checks `releases/latest` 5 s after startup and every 6 hours. When the release is newer,
it shows a dialog with the change list: *Update now* downloads the installer, verifies SHA-256 (the `digest` field
from the API or the `.sha256` file), runs it with `/SILENT` and closes Nokturn — the setup downloads the new version, replaces
the previous one and starts it. *Skip this version* silences the dialog until the next release; manually:
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
- **Sign-in with the browser (default, a free account is enough):** *Sources → Spotify → Sign in with the
  browser*. Like for YouTube, Nokturn opens Chrome / Edge / Brave as a separate window with its own clean profile
  (`%APPDATA%\Nokturn\Nokturn\browser-spotify`) and no DevTools; you sign in to Spotify as usual. When the web
  player appears, the window closes, and the same profile is started invisibly (headless, DevTools Protocol) on
  `open.spotify.com` to pick up the access token the web player gets for itself. The public Web API rate-limits
  that token ("API rate limit exceeded"), so the library is read with the web player's own GraphQL queries
  (`profileAttributes`, `libraryV3`, `fetchPlaylist`, `fetchLibraryTracks` on `api-partner.spotify.com`):
  their names, hashes and headers are taken from the web player's requests, and the ones it doesn't send while
  we watch (liked songs come from its local cache) are looked up in its JavaScript. The token lasts about an hour
  and is renewed the same invisible way (~15 s) when needed; it's kept encrypted in `spotify.token` until it
  expires. *Sign out* removes it together with the browser profile. This is an unofficial way of using Spotify
  (like the YouTube Music integration) — changes on Spotify's side can break it.
- **Your own Spotify app (optional):** since February 2026 Spotify's developer mode needs **Premium**. With
  Premium you can use an official app instead (*Use my own Spotify app*):
  1. Open <https://developer.spotify.com/dashboard> and create an app.
  2. Add the redirect URI `http://127.0.0.1:43117/callback` and tick **Web API**.
  3. Copy the app's **Client ID** into *Sources → Spotify* and click *Sign in to Spotify*.

  This uses OAuth 2.0 Authorization Code with PKCE (no client secret) in your regular browser; Nokturn listens on
  `127.0.0.1:43117` for the redirect. Read-only scopes: `playlist-read-private playlist-read-collaborative
  user-library-read`. The refresh token is encrypted (DPAPI / Secret Service) in `spotify.token`. A build can ship
  a built-in ID: `-DNOKTURN_SPOTIFY_CLIENT_ID=<id>` in CMake.
- **Sync:** a few seconds after start and with the refresh button; the data is cached in `spotify.json`. With your
  own app, playlists whose `snapshot_id` didn't change aren't downloaded again.
- **Covers:** Spotify serves playlist covers as WebP — the build needs the Qt image formats plugins
  (MSYS2: `mingw-w64-ucrt-x86_64-qt6-imageformats`; the Linux AppImage gets them from `qtimageformats`).

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
- The sound never waits for the GUI thread: the decoders' samples are mixed and equalized on the decoders' own
  threads (connected directly), and the audio sink pulls from the ring buffer (~200 ms cushion) on a thread of its
  own — dragging the window's edge, which re-lays the interface out for every step, used to cut the sound. The UI
  still stays light: search results, artist songs and opened collections are appended to the track model instead
  of rebuilding it, and lists create their rows' hover-only controls and cover motifs on demand. The playback
  position (lyrics, progress bar) is corrected by what still waits in the buffers.
- Diagnostics: `NOKTURN_AUDIO_DEBUG=1` prints the sample rate and the number of buffer underruns every 3 s;
  `NOKTURN_PERF_DEBUG=1` reports every GUI thread stall over 60 ms and how long model rebuilds take
  (`Nokturn.exe --tour <dir> --tour-perf` plays a track while searching and opening artists;
  `--tour-resize "<query>"` plays one while the window's width changes every frame and the GUI thread stalls).

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

## Playlists — how it works

- **New playlist:** the **+** next to *Playlists* in the sidebar, *New playlist…* in a track's menu (⋯ or right-click),
  or *Save as a new playlist…* in the ⋯ menu of any playlist, album or artist page (a copy with its tracks).
  The dialog asks where to save it:
  - **This computer** — kept in `playlists.json` in the data directory. It can hold any track: local files,
    YouTube Music and Spotify tracks. Their data is stored with the playlist, so search results stay playable.
  - **YouTube Music** (signed in) — created on the account (InnerTube `playlist/create`), private / unlisted /
    public; it shows up in YouTube Music on the phone too. Spotify tracks go in as their YouTube Music match,
    local files are skipped.
- **Adding and removing:** *Add to playlist* in the track menu lists your own playlists (local and YouTube Music);
  *Remove from this playlist* on the playlist's page. YouTube Music edits go through `browse/edit_playlist`
  (removing needs each entry's `setVideoId`, read from the playlist).
- **Playlist settings** (⋯ next to Play): rename / description, who can see it (YouTube Music), sort order
  (playlist order, reversed, title, artist, album, duration — remembered per playlist), save as a new playlist,
  delete (YouTube Music playlists are deleted on the account too — it asks for a second click). The cover and the
  Mix settings work as for any playlist.
- YouTube Music playlists you only saved (someone else's) can't be edited — YouTube Music refuses the change.

## Online features — how it works

Share links, shared playlists and listening rooms go through the Nokturn server (`server/`, see
[Server](#server-vps)), by default `https://nokturn.404piwko.wtf`. Nothing is sent there until one of these
features is used.

- **Account:** anonymous. The first time you share something, Nokturn registers with the name from
  *Settings → Online → Your name* and gets a token (kept encrypted like the other tokens, in
  `online-<server>.token`). No e-mail, no password. *Delete the online account* removes the name, the links and
  the shared playlists you own from the server.
- **What travels:** only YouTube Music and SoundCloud tracks, the sources everyone can play. A Spotify track goes
  as its YouTube Music match, a local file as the YouTube Music track that matches its artist, title and duration;
  a track without a match is skipped with a notice.
- **Share links:** *Copy a share link* in a track's menu (⋯ or right-click) or the share button on the player bar
  copies `…/t/<code>`. Opened in a browser, the page shows the track (with a preview in chat apps), an *Open in
  Nokturn* button (`nokturn://t/<code>`) and a *Listen on YouTube Music / SoundCloud* link; opened in Nokturn, the
  track plays.
- **Shared playlists:** *New playlist → Save to: Shared*. The invite link (`…/p/<id>`) is copied right away and
  can be copied again from the playlist's ⋯ menu. Everyone who opens it joins: the playlist shows up in their
  sidebar, and what anyone adds or removes reaches the others at once (WebSocket). The owner renames and deletes
  it (for everyone); the others can leave. The owner can set a **password** (playlist ⋯ menu): others then enter it
  to join through the link, and the link's page shows nothing but that it has one. The password is kept as a
  salted scrypt hash; those already in stay; wrong guesses are limited to 10 per 15 minutes. Kept in `shared-playlists.json` too, so they show up offline.
- **Listening rooms:** *Listening room* in the sidebar — start one or paste a link / a 6-character code.
  The invite link is `…/r/<code>`. While you're in a room, the player plays the room's queue: clicking a track
  adds it to the room (or jumps to it when it's already queued), and play / pause / next / previous / seeking
  (including the media keys) go to the room, so everybody follows. Two modes:
  - *Everyone adds* — everyone adds tracks, skips and pauses;
  - *The host picks the music* — only the host adds and controls, the others listen along. The host can hand
    the room over or remove someone.

  The server keeps the clock: the position is "*x* seconds at server time *t*", and each player corrects itself
  when it drifts more than 3 s (the clock offset comes from ping round trips). Players report when a track ends;
  in *host* mode only the host's report counts, in *open* mode anyone's near the end of the track, and the server
  moves on by itself when a track is overdue. Transitions (crossfade, Mix) keep working inside a room. Shuffle and
  repeat are off there — the room's order applies. Someone who loses the connection keeps their place for 45 s;
  a room with nobody in it closes after 15 minutes.
- **The same library on every device:** once there is an account, the YouTube Music likes (the heart) and the
  tracks played go to it as well (*Settings → Online → Same library on every device*, on by default). Every
  device signed in to the account shows the same *Liked* and *Recently played*; a change reaches the others at
  once over the WebSocket. Plays from other devices show in *Recently played* but not in the statistics, which
  count only what was heard on this one. Likes waiting for a connection are kept in `library-sync-<server>.json`.
- **YouTube Music, Spotify and SoundCloud playlists on the account:** a device signed in to one of them keeps a
  copy of each of its playlists on the account — a playlist with the origin `ytmusic:<id>` / `spotify:<id>` /
  `soundcloud:<id>` (`spotify:liked`, `soundcloud:liked` for the liked tracks), replaced whenever the original changes and deleted with it — so devices
  without those sign-ins have them too. Spotify tracks go as their YouTube Music matches, found in the background
  one every 1.5 s; an unfinished copy goes up once it has a track and grows as they come. The app that made the
  copies shows the originals and hides the copies. Likes keep YouTube Music's order on the account: each one goes
  up with its place in time (`at`), and one out of place is moved.
- **Playlist folders and hidden playlists:** the sidebar (and the home page, which is where the phone layout lists
  them) groups the playlists: the built-in lists, the user's folders (the folder button next to *New playlist*;
  a playlist's ⋯ menu → *Move to a folder…*), then one group per source — your playlists, shared, YouTube Music,
  Spotify, SoundCloud. **Ctrl + drag** a playlist in the sidebar puts it in your order (a line shows where it
  lands); dropped in a folder it joins the folder, back in its source's group it leaves it. *Hide the playlist* in the same menu takes it out of the list; *Show hidden playlists* at
  the end brings them back. The layout (`{ folders, assign, hidden, order, updated }`, keyed like the playlist covers:
  `yt:<id>`, `sp:<id>`, `shared:<id>`, `user:<id>`…) is kept on the account, the newer one winning, so every
  device — the browser version too — shows the same folders.
- **More devices on one account:** *Link another device* shows an 8-character code (valid 10 minutes, single
  use); *I have a code* on the other device (the desktop app, the Android app or the browser version) switches it
  to that account, with its library, shared playlists and rooms. Its own likes are added to the account.
- **Links open Nokturn:** `nokturn://` is registered for the current user on startup (Windows: `HKCU\Software\Classes\nokturn`,
  removed by the uninstaller; Linux AppImage: a hidden `nokturn-links.desktop` + `xdg-mime`; installed from
  source: `MimeType` in `nokturn.desktop`). A link opened while Nokturn runs goes to the running window.
- `NOKTURN_SERVER=<url>` points at another server (testing), *Settings → Online → Server* permanently.
  `NOKTURN_ONLINE_DEBUG=1` logs room states, what the player is told and what it reports.
  `--tour <dir> --tour-online "<room>;<playlist>;<share>"` walks through it against a server.
  `--tour <dir> --tour-library "<pair code>"` links a test profile with a code and checks the synced library.
  `--tour <dir> --tour-folders "<pair code or ->"` makes a folder, hides a playlist and shows the hidden ones.

## Server (VPS)

The server is Python 3 + aiohttp with SQLite (accounts, share links, shared playlists); listening rooms live in
memory. Behind nginx with a Let's Encrypt certificate it takes a few MB of RAM.

1. Point the subdomain at the VPS: an `A` record (and `AAAA` for IPv6) `nokturn.404piwko.wtf → <VPS address>`.
2. On the VPS (Ubuntu 22.04 / 24.04):

   ```bash
   git clone https://github.com/zr3mb/Nokturn.git && cd Nokturn
   sudo ./server/install.sh --email you@example.com      # e-mail for Let's Encrypt expiry notices (optional)
   ```

   It installs nginx, certbot and Python, creates the `nokturn` system user, puts the code in
   `/opt/nokturn-server` (with its own venv), the database in `/var/lib/nokturn/nokturn.db`, the settings in
   `/etc/nokturn-server.env`, a hardened `nokturn-server` systemd service on `127.0.0.1:8710`, the nginx site
   (with WebSocket proxying for `/api/v1/ws`) and the certificate; ports 80/443 are opened in ufw if it's active.
   `--domain <name>` uses another domain, `--no-tls` stays on plain HTTP, `--uninstall` removes the service and
   the site (the data stays).
3. Update: `git pull && sudo ./server/install.sh` (the database is kept).

Logs: `journalctl -u nokturn-server -f`. Backup: `sqlite3 /var/lib/nokturn/nokturn.db ".backup /root/nokturn.db"`.
Locally: `cd server && pip install -r requirements.txt && python3 -m nokturn_server --db /tmp/n.db --public-url http://127.0.0.1:8710`
(then `NOKTURN_SERVER=http://127.0.0.1:8710` for the app). Tests: `cd server && python3 -m unittest discover -s tests`.

API (`/api/v1`, JSON, `Authorization: Bearer <token>`): `POST /register`, `GET|PATCH|DELETE /me`,
`POST /shares`, `GET /shares/<code>`, `GET|POST /playlists`, `GET|PATCH|DELETE /playlists/<id>`,
`POST /playlists/<id>/join|leave|tracks`, `DELETE /playlists/<id>/tracks/<entry>`,
`POST /playlists/<id>/tracks/<entry>/move`, `GET /rooms/<code>` (preview), `GET /health`. Rooms run over the
WebSocket `/api/v1/ws`: `room.create`, `room.join`, `room.leave`, `room.add`, `room.remove`, `room.move`,
`room.play`, `room.pause`, `room.resume`, `room.seek`, `room.next`, `room.previous`, `room.ended`, `room.mode`,
`room.host`, `room.rename`, `room.kick`, `ping`; the server answers with the whole room (`room`), `room.left`,
`playlist` / `playlist.removed` (a shared playlist changed) and `error`. The pages `/t/<code>`, `/r/<code>`,
`/p/<id>` are in Polish or English depending on the browser. Requests are rate-limited per address and account.

## Lyrics — how it works

- **Where:** the *Lyrics* tab in the "Now playing" panel (or the button with text lines on the player bar)
  and a column next to the cover in full-screen mode. With timed lyrics the current line is highlighted
  and scrolled to the center, and clicking a line seeks the track. When the timed lyrics belong to a different
  version (e.g. a music video with an intro), *Earlier* / *Later* under them shift the timing by 0.5 s —
  remembered per track.
- **Where from (in order):**
  1. **YouTube Music** — first the timed version (requested as the `ANDROID_MUSIC` mobile client, because the
     website doesn't provide it — without the account's website cookies, with them it gets no timing), then plain
     text from the *Lyrics* tab. `NOKTURN_LYRICS_DEBUG=1` prints what the timed request returned. For local files the track is first
     looked up by artist and title (with a ±12 s duration check, so a live version isn't picked).
  2. **Local file** — an `.lrc` next to the track (timed) or the `LYRICS` / `UNSYNCEDLYRICS` tag.
     A local timed `.lrc` wins over plain text from YouTube.
  3. **[LRCLIB](https://lrclib.net)** — when there's still no timing (no lyrics, or plain text only): an exact
     lookup by artist, cleaned title, album and duration (±2 s), then a search where timed lyrics and the closest
     duration (±12 s) win. Its timed lyrics replace plain text; its plain text is used only when nothing else was
     found. Works for YouTube, Spotify and local tracks.
- **Cache:** `lyrics\<sha1>.json` in the data directory. Missing lyrics are remembered for 3 days. Cache version 3:
  older "missing" entries and untimed YouTube lyrics are fetched again once.

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
| `src/Playlists.*` | `Playlists` singleton: the user's playlists — local (`playlists.json`) or on YouTube Music |
| `src/spotify/SpotifyService.*` | `Spotify` singleton: sign-in, playlist / liked songs sync, YouTube Music matching |
| `src/spotify/SpotifyWebLogin.*` | Spotify sign-in window + web player token through the DevTools Protocol |
| `src/youtube/YouTubeService.*` | `YouTube` singleton for QML: account state, sync, library cache |
| `src/audio/AudioEngine.*`, `src/audio/Equalizer.*` | Custom output path: deck mixer (transition styles), buffer + `QAudioSink`, 7-band parametric equalizer |
| `src/lyrics/Lyrics.*` | Lyrics structure, LRC parser, cache (de)serialization |
| `src/lyrics/LrcLib.*` | LRCLIB client: exact lookup, then a search by duration |
| `src/lyrics/LyricsAligner.*` | Aligning lyrics with the transcription (line times) |
| `src/lyrics/SpeechEngine.*` | whisper.cpp: download with SHA-256 verification, decoding to 16 kHz, transcription, hardware detection |
| `src/lyrics/LyricsService.*` | `Lyrics` singleton for QML: lyric sources, cache, current line, alignment (alpha) |
| `src/online/OnlineService.*` | `Online` singleton: the server account, share links, shared playlists, listening rooms (WebSocket, driving the player), `nokturn://` links |
| `src/platform/DiscordPresence.*` | `Discord` singleton: Discord IPC (Rich Presence), "Listening" status |
| `src/platform/WindowsMediaControls.*`, `WinRtMedia.h` | Windows: SMTC (media flyout, lock screen) and media keys |
| `src/platform/Mpris.*` | MPRIS 2 on Linux (D-Bus): media keys, system panel controls |
| `src/TagLibPath.h` | File path for TagLib (wchar_t on Windows, file system bytes on Linux) |
| `src/Prefs.*` | `Prefs` singleton for QML: typed reading/writing of `settings.ini` (appearance, wizard) |
| `src/MemoryTrim.*` | `Memory` singleton: gives freed memory back to the system after a page closes, the library is rebuilt or the window is minimized |
| `i18n/` | Translations: `pl.json` (Polish), `en.json` (English plural forms) |
| `scripts/check_translations.py` | Translation completeness check |
| `server/` | The online server: `nokturn_server/` (aiohttp app, SQLite, rooms, link pages), `install.sh` for Ubuntu, tests |
| `packaging/linux/` | `.desktop` file, SVG icon, metainfo (AppStream) |
| `packaging/windows/` | The setup (`setup/`), the package it downloads (`make_payload.py`), the icon |
| `qml/onboarding/` | First-run wizard: steps, animated layout thumbnails (`LayoutPreview`), transition preview |
| `qml/Theme.qml` | Palette, fonts, animation timings; the accent smoothly follows the cover |
| `qml/Nav.qml` | Navigation (back/forward history), panel state, notifications |
| `qml/Icons.qml` | Vector icons (SVG paths) |
| `qml/layout/` | Title bar, sidebar, player bar, "Now playing" panel, full-screen mode |
| `qml/pages/` | Home, Search, Library, Playlist/Album/Artist, Sources, Settings, Equalizer, Stats, Listening room |
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
