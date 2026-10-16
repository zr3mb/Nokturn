# Nokturn

Odtwarzacz muzyki z wielu źródeł (pliki lokalne, YouTube Music, radio internetowe, własne strumienie).

**Działa:** pliki lokalne (skanowanie folderów, tagi i okładki przez TagLib, odtwarzanie przez
Qt Multimedia / FFmpeg) oraz **YouTube Music** (logowanie w przeglądarce, polubione, playlisty,
wyszukiwanie, odtwarzanie przez yt-dlp). Wizualizer pokazuje prawdziwe widmo (FFT).
Radio i własne strumienie są na razie tylko interfejsem.

C++20 · Qt 6.8+ (Qt Quick / QML, Qt Multimedia, Qt WebSockets) · TagLib 2 · CMake

## Budowanie (Windows, MSYS2 UCRT64)

Wymagane pakiety:

```bash
pacman -S mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,qt6-base,qt6-declarative,qt6-multimedia,qt6-websockets,taglib}
```

```powershell
.\build.ps1 -Run       # kompilacja + uruchomienie
.\build.ps1 -Deploy    # samodzielny folder dist\ (działa bez MSYS2)
```

Narzędzie deweloperskie: `Nokturn.exe --tour <folder>` przechodzi po wszystkich ekranach i zapisuje ich zrzuty PNG.

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

## Struktura

| Ścieżka | Zawartość |
|---|---|
| `src/TrackModel.*` | Model listy utworów (`QAbstractListModel`) |
| `src/LibraryScanner.*` | Skan folderów w wątku roboczym: TagLib, okładki, paleta kolorów, (de)serializacja cache |
| `src/Library.*` | Singleton `Library`: foldery, skanowanie, albumy, wykonawcy, playlisty, wyszukiwanie |
| `src/PlayerController.*` | Singleton `Player`: QMediaPlayer, kolejka, historia, zapis stanu |
| `src/SpectrumAnalyzer.*` | FFT (2048, okno Hanna) z `QAudioBufferOutput` → 48 pasm logarytmicznych |
| `src/AppPaths.h` | Katalog danych i ustawienia (`settings.ini`) |
| `src/youtube/BrowserLogin.*` | Okno logowania w Chrome/Edge/Brave + DevTools Protocol (WebSocket) |
| `src/youtube/YtCookies.*` | Ciasteczka: szyfrowanie DPAPI, `SAPISIDHASH`, eksport cookies.txt |
| `src/youtube/YtMusicApi.*` | Klient InnerTube i parser odpowiedzi (utwory, playlisty, konto) |
| `src/youtube/StreamResolver.*` | yt-dlp: pobieranie z weryfikacją SHA-256, adresy strumieni, cache |
| `src/youtube/YouTubeService.*` | Singleton `YouTube` dla QML: stan konta, synchronizacja, cache biblioteki |
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
