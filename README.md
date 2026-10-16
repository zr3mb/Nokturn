# Nokturn

Odtwarzacz muzyki z wielu źródeł (pliki lokalne, YouTube Music, radio internetowe, własne strumienie).

**Działa:** pliki lokalne (skanowanie folderów, tagi i okładki przez TagLib, odtwarzanie przez
Qt Multimedia / FFmpeg) oraz **YouTube Music** (logowanie w przeglądarce, polubione, playlisty,
wyszukiwanie, odtwarzanie przez yt-dlp). Wizualizer pokazuje prawdziwe widmo (FFT).
Do tego teksty utworów, miksowanie utworów płynnymi przejściami (jak „Miks” w Spotify) i status
„Słucha” na Discordzie. Działa na **Windows** i **Linuksie**.
Radio i własne strumienie są na razie tylko interfejsem.

C++20 · Qt 6.8+ (Qt Quick / QML, Qt Multimedia, Qt WebSockets, Qt DBus na Linuksie) · TagLib 2 · CMake

## Budowanie (Windows, MSYS2 UCRT64)

Wymagane pakiety:

```bash
pacman -S mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,qt6-base,qt6-declarative,qt6-multimedia,qt6-websockets,taglib}
```

```powershell
.\build.ps1 -Run       # kompilacja + uruchomienie
.\build.ps1 -Deploy    # samodzielny folder dist\ (działa bez MSYS2)
```

## Budowanie (Linux)

Wymagane: Qt **6.8+** (moduły base, declarative, multimedia z backendem FFmpeg, websockets, dbus), TagLib **2**,
CMake, Ninja, kompilator C++20.

```bash
# Arch / Manjaro
sudo pacman -S cmake ninja qt6-base qt6-declarative qt6-multimedia qt6-multimedia-ffmpeg qt6-websockets taglib
# Fedora 41+
sudo dnf install cmake ninja-build gcc-c++ qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtmultimedia-devel qt6-qtwebsockets-devel taglib-devel
# Debian 13 / Ubuntu 25.04+ (starsze wydania mają Qt < 6.8 albo TagLib 1 — użyj Qt z instalatora qt.io / aqtinstall)
sudo apt install cmake ninja-build g++ qt6-base-dev qt6-declarative-dev qt6-multimedia-dev qt6-websockets-dev libtag-dev
```

```bash
./build.sh --run        # kompilacja + uruchomienie
./build.sh --install    # instalacja do ~/.local (program "nokturn", ikona, plik .desktop) — PREFIX=… zmienia miejsce
./build.sh --appimage   # samodzielny Nokturn-x86_64.AppImage (pobiera linuxdeploy)
QT_DIR=~/Qt/6.8.3/gcc_64 ./build.sh   # Qt spoza systemu
```

Na Linuksie:
- program nazywa się `nokturn`, dane i ustawienia są w `~/.local/share/Nokturn/Nokturn/`;
- **MPRIS** — klawisze multimedialne, kontrolki w panelu GNOME/KDE i `playerctl` działają od razu;
- **logowanie do YouTube Music** używa Chrome / Chromium / Brave / Edge / Vivaldi z PATH (wersje ze Snapa
  tylko awaryjnie — izolacja Snapa nie pozwala na profil w ukrytym katalogu). Okno logowania trzeba zamknąć
  samemu po zalogowaniu (na Waylandzie nie da się odczytać tytułu cudzego okna);
- **ciasteczka** są szyfrowane kluczem z systemowego magazynu haseł (GNOME Keyring / KWallet przez `secret-tool`,
  pakiet `libsecret-tools` / `libsecret`); bez niego plik jest jawny, ale z prawami `0600`;
- **yt-dlp** jest pobierany jako samodzielny plik `yt-dlp_linux` (lub `_aarch64`), też z weryfikacją SHA-256;
- **whisper.cpp** (dopasowanie czasu tekstów) instaluje się z repozytorium dystrybucji — Nokturn szuka poleceń
  `whisper-cli` i `whisper-cpp`.

Narzędzie deweloperskie: `Nokturn.exe --tour <folder>` (Linux: `nokturn --tour <folder>`) przechodzi po wszystkich
ekranach i zapisuje ich zrzuty PNG.

## Pliki lokalne — jak to działa

- **Foldery** wybiera się w *Źródła → Pliki lokalne → Dodaj folder* (albo z Biblioteki). Przy pierwszym
  uruchomieniu dodawany jest systemowy folder *Muzyka*. Skan obejmuje podfoldery.
- **Formaty:** MP3, FLAC, M4A/AAC/ALAC, OGG, Opus, WAV, WMA, AIFF, APE, WavPack, MPC.
- **Tagi** (tytuł, wykonawca, wykonawca albumu, album, rok, numer, gatunek, długość, jakość) czyta TagLib.
  Brak tagów → nazwa pliku i nazwa folderu.
- **Okładki:** osadzone w pliku albo `cover/folder/front.jpg|png` w katalogu albumu. Są przycinane do kwadratu
  i zapisywane w cache; kolor akcentu interfejsu jest wyliczany z okładki.
- **Cache:** `%APPDATA%\Nokturn\Nokturn\library.json` + `covers\`. Kolejne skany są przyrostowe —
  niezmienione pliki (rozmiar + data modyfikacji) nie są czytane ponownie. Skan rusza w tle przy każdym starcie.
- **Kolejka:** kliknięcie utworu na liście (album, playlista, wyniki wyszukiwania) ustawia ją jako kolejkę.
  Zapamiętywane są głośność, tryby losowania/powtarzania oraz ostatni utwór i miejsce, w którym skończono.
- Zmienna `NOKTURN_PROFILE=<katalog>` przenosi ustawienia i cache w inne miejsce (przydatne do testów).

## YouTube Music — jak to działa

- **Logowanie (dwa etapy):** *Źródła → YouTube Music → Zaloguj się przez przeglądarkę*.
  1. Nokturn uruchamia zainstalowanego Chrome / Edge / Brave jako osobne okno z **własnym, czystym profilem**
     (`%APPDATA%\Nokturn\Nokturn\browser`) — **bez** DevTools, bo Google blokuje logowanie w przeglądarkach
     ze zdalnym debugowaniem („Ta przeglądarka lub aplikacja może nie być bezpieczna”). Logujesz się normalnie.
  2. Gdy okno pokaże YouTube Music, Nokturn je zamyka i uruchamia ten sam profil niewidocznie (headless)
     z DevTools Protocol tylko po to, by odczytać sesję (`Storage.getCookies`, wymagane `SAPISID` + `LOGIN_INFO`).

  Twoje zwykłe profile przeglądarki nie są dotykane. Awaryjnie można wkleić nagłówek `Cookie` ręcznie.
- **Przechowywanie:** ciasteczka (tylko domeny youtube.com) są szyfrowane DPAPI w `youtube.cookies` — odczyta je
  wyłącznie Twoje konto Windows. *Wyloguj* usuwa je razem z profilem przeglądarki.
- **API:** wewnętrzne API YouTube Music (InnerTube, klient `WEB_REMIX`) z nagłówkiem `SAPISIDHASH` — tak jak
  strona: konto, polubione (`VLLM`), playlisty z biblioteki, zawartość playlist (z kolejnymi stronami),
  wyszukiwanie. Serduszko w Nokturnie polubi / odlubi utwór także na koncie.
- **Odtwarzanie:** adres strumienia audio dostarcza **yt-dlp** (przycisk *Pobierz* ściąga oficjalne wydanie
  z GitHuba i sprawdza sumę SHA-256). Adresy są trzymane w cache do wygaśnięcia, a następny utwór w kolejce jest
  przygotowywany zawczasu. yt-dlp potrzebuje środowiska JavaScript — Nokturn używa Deno albo Node.js, jeśli są.
- **Uwaga:** to nieoficjalny sposób korzystania z YouTube Music, niezgodny z regulaminem YouTube. Zmiany po stronie
  YouTube mogą go psuć (wtedy pomaga *Aktualizuj* przy yt-dlp), a w skrajnym razie grożą ograniczeniem konta.

## Miksowanie utworów — jak to działa

- **Gdzie:** przycisk **Miks** na stronie playlisty, albumu lub wykonawcy (jak w Spotify) — ustawienia są osobne dla
  każdej playlisty: styl przejścia i długość (2–16 s). Globalny **crossfade** dla pozostałych kolejek jest
  w *Ustawienia → Odtwarzanie* (0 = wyłączony). Miks playlisty ma pierwszeństwo przed crossfade.
- **Style przejść:**
  - *Przenikanie* — klasyczne, o stałej mocy (sin/cos);
  - *Wzniesienie* — filtr górnoprzepustowy stopniowo wycina doły kończącego się utworu, następny wchodzi później;
  - *Zamiana basu* — góry przenikają przez całe przejście, a bas przełącza się szybko w połowie (jak u DJ-a: nigdy
    dwie stopy perkusji naraz);
  - *Echo* — kończący się utwór odchodzi w echo (375 ms), z którego wyłania się następny;
  - *Automatycznie* — energiczna końcówka dostaje zamianę basu, spokojna — przenikanie.
- **Inteligentne przejścia** (domyślnie włączone): cisza na końcu nagrania (< −50 dBFS przez 0,8 s) od razu
  uruchamia następny utwór, wyciszenie zapisane w nagraniu (≥ 14 dB poniżej średniej przez 1 s) zaczyna
  przejście wcześniej, a cisza na początku następnego utworu (do 4 s) jest pomijana.
- **Jak:** odtwarzacz ma dwa „decki” (dwa `QMediaPlayer`). Następny utwór jest ładowany na drugi deck kilkanaście
  sekund przed końcem (adres strumienia YouTube też), a w trakcie przejścia próbki obu decków są sumowane przez
  `AudioEngine` — przed korektorem, więc korektor, limiter i wizualizer działają na tym, co słychać. Obwiednia
  rusza dopiero, gdy nadejdą pierwsze próbki nowego utworu, więc opóźnienie startu nie robi dziury.
  Przewinięcie, „następny” albo pauza w trakcie przejścia są obsługiwane (przejście kończy się od razu).
- Diagnostyka: `NOKTURN_AUDIO_DEBUG=1` wypisuje co 3 s tempo próbek i liczbę niedoborów bufora.

## Status na Discordzie

- *Ustawienia → Discord*: status **„Słucha”** z tytułem, wykonawcą, okładką (adres z YouTube Music; dla plików
  lokalnych okładka dopasowanego utworu z YouTube Music) i paskiem postępu; przy utworach z YouTube — przycisk
  „Posłuchaj w YouTube Music”. Po wstrzymaniu status znika (opcjonalnie zostaje).
- Działa przez lokalne IPC aplikacji Discord (Windows: `\\.\pipe\discord-ipc-N`, Linux: `discord-ipc-N`
  w `$XDG_RUNTIME_DIR`, także Discord z Flatpaka/Snapa i Vesktop). Nic nie idzie przez internet z Nokturna.
- Discord wymaga **identyfikatora aplikacji**: utwórz aplikację na
  [discord.com/developers/applications](https://discord.com/developers/applications) (jej nazwa pojawi się jako
  „Słucha <nazwa>”), skopiuj *Application ID* i wklej w ustawieniach. Można go też wbudować przy kompilacji:
  `-DNOKTURN_DISCORD_CLIENT_ID=…` (Linux: `DISCORD_CLIENT_ID=… ./build.sh`). Opcjonalnie dodaj w *Rich Presence →
  Art Assets* obrazek `nokturn` — pokaże się zamiast okładki, gdy jej nie ma w sieci.
- Starsze klienty Discorda, które nie znają nowszych pól (typ statusu, przyciski), dostają wersję podstawową.

## Teksty utworów — jak to działa

- **Gdzie:** zakładka *Tekst* w panelu „Teraz odtwarzane” (albo przycisk z liniami tekstu na pasku odtwarzacza)
  oraz kolumna obok okładki w trybie pełnoekranowym. Przy tekście z czasami bieżąca linia jest podświetlona
  i przewijana na środek, a kliknięcie linii przewija utwór.
- **Skąd (po kolei):**
  1. **YouTube Music** — najpierw wersja z czasami (zapytanie jako klient mobilny `ANDROID_MUSIC`, bo strona WWW
     jej nie udostępnia), potem zwykły tekst z zakładki *Tekst*. Dla plików lokalnych utwór jest najpierw
     wyszukiwany po wykonawcy i tytule (z kontrolą długości ±12 s, żeby nie trafić na wersję koncertową).
  2. **Plik lokalny** — `.lrc` obok utworu (z czasami) albo tag `LYRICS` / `UNSYNCEDLYRICS`.
     Lokalny `.lrc` z czasami wygrywa ze zwykłym tekstem z YouTube.
- **Cache:** `lyrics\<sha1>.json` w katalogu danych. Brak tekstu jest zapamiętywany na 3 dni.

### Automatyczne dopasowanie czasu (alpha)

Dla tekstów bez znaczników czasu można włączyć w *Ustawienia → Teksty utworów* rozpoznawanie śpiewu:

- Działa **lokalnie** przez [whisper.cpp](https://github.com/ggml-org/whisper.cpp) (`whisper-cli`). Program
  (paczka `whisper-bin-x64.zip` z GitHuba, suma SHA-256 z pola `digest` wydania) i model ggml (Hugging Face,
  suma SHA-256 z `lfs.oid`) pobiera się z poziomu ustawień. Paczkę rozpakowuje wbudowany w Windows `tar`.
- Dostępne tylko na **mocniejszym sprzęcie** (min. 4 wątki CPU i 8 GB RAM). Zalecany model jest dobierany
  do sprzętu: `base` (142 MB), `small` (466 MB, od 8 wątków), `medium` (1,5 GB, od 16 wątków i 16 GB RAM).
- Przebieg: Qt Multimedia dekoduje utwór (plik albo strumień YouTube) do WAV 16 kHz mono →
  `whisper-cli -ml 1 -sow -oj` zwraca słowa z czasami (początek tekstu idzie jako `--prompt`) →
  `LyricsAligner` wyrównuje słowa tekstu ze słowami transkrypcji (Needleman–Wunsch, porównanie
  rozmyte Levenshteinem) i wyznacza czas każdej linii; linie bez rozpoznanych słów są interpolowane.
  Wynik z pokryciem poniżej 25% słów jest odrzucany.
- Wynik trafia do cache, więc każdy utwór jest rozpoznawany tylko raz. Zmiana utworu przerywa rozpoznawanie.

## Struktura

| Ścieżka | Zawartość |
|---|---|
| `src/TrackModel.*` | Model listy utworów (`QAbstractListModel`) |
| `src/LibraryScanner.*` | Skan folderów w wątku roboczym: TagLib, okładki, paleta kolorów, (de)serializacja cache |
| `src/Library.*` | Singleton `Library`: foldery, skanowanie, albumy, wykonawcy, playlisty, wyszukiwanie |
| `src/PlayerController.*` | Singleton `Player`: dwa decki QMediaPlayer, kolejka, historia, miksowanie, zapis stanu |
| `src/SpectrumAnalyzer.*` | FFT (2048, okno Hanna) z `QAudioBufferOutput` → 48 pasm logarytmicznych |
| `src/AppPaths.h` | Katalog danych i ustawienia (`settings.ini`) |
| `src/youtube/BrowserLogin.*` | Okno logowania w Chrome/Edge/Brave + DevTools Protocol (WebSocket) |
| `src/youtube/YtCookies.*` | Ciasteczka: szyfrowanie (DPAPI / Secret Service), `SAPISIDHASH`, eksport cookies.txt |
| `src/youtube/YtMusicApi.*` | Klient InnerTube i parser odpowiedzi (utwory, playlisty, konto) |
| `src/youtube/StreamResolver.*` | yt-dlp: pobieranie z weryfikacją SHA-256, adresy strumieni, cache |
| `src/youtube/YouTubeService.*` | Singleton `YouTube` dla QML: stan konta, synchronizacja, cache biblioteki |
| `src/audio/AudioEngine.*`, `src/audio/Equalizer.*` | Własny tor wyjściowy: mikser decków (style przejść), bufor + `QAudioSink`, 7-pasmowy korektor parametryczny |
| `src/lyrics/Lyrics.*` | Struktura tekstu, parser LRC, (de)serializacja cache |
| `src/lyrics/LyricsAligner.*` | Wyrównanie tekstu z transkrypcją (czasy linii) |
| `src/lyrics/SpeechEngine.*` | whisper.cpp: pobieranie z weryfikacją SHA-256, dekodowanie do 16 kHz, transkrypcja, wykrywanie sprzętu |
| `src/lyrics/LyricsService.*` | Singleton `Lyrics` dla QML: źródła tekstu, cache, bieżąca linia, dopasowanie (alpha) |
| `src/platform/DiscordPresence.*` | Singleton `Discord`: IPC Discorda (Rich Presence), status „Słucha” |
| `src/platform/Mpris.*` | MPRIS 2 na Linuksie (D-Bus): klawisze multimedialne, kontrolki w panelu systemu |
| `src/TagLibPath.h` | Ścieżka pliku dla TagLib (wchar_t na Windows, bajty systemu plików na Linuksie) |
| `packaging/linux/` | Plik `.desktop`, ikona SVG, metainfo (AppStream) |
| `qml/Theme.qml` | Paleta, czcionki, czasy animacji; akcent płynnie zmienia się wraz z okładką |
| `qml/Nav.qml` | Nawigacja (historia wstecz/dalej), stan paneli, powiadomienia |
| `qml/Icons.qml` | Wektorowe ikony (ścieżki SVG) |
| `qml/layout/` | Pasek tytułu, panel boczny, pasek odtwarzacza, panel „Teraz odtwarzane”, tryb pełnoekranowy |
| `qml/pages/` | Główna, Szukaj, Biblioteka, Playlista/Album/Wykonawca, Źródła, Ustawienia |
| `qml/components/` | Komponenty wielokrotnego użytku (okładki, przyciski, suwaki, wizualizer, karty…) |

## Skróty klawiszowe

| Skrót | Akcja |
|---|---|
| Spacja | Odtwórz / pauza |
| Ctrl + → / ← | Następny / poprzedni utwór |
| Ctrl + K, Ctrl + F | Szukaj |
| Alt + ← / → | Wstecz / dalej |
| F11, Esc | Tryb pełnoekranowy / wyjście |

## Kolejne etapy

1. Radio i strumienie URL (Qt Multimedia obsługuje HTTP/Icecast).
2. Własne playlisty, menu kontekstowe utworu, przeciąganie w kolejce.
3. Obserwowanie folderów (QFileSystemWatcher) zamiast skanu przy starcie.
4. YouTube Music: strona główna z miksami, radio utworu („podobne”), dodawanie do playlist.
"# apkadomuzy" 
"# apkadomuzy" 
