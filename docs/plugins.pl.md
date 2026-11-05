# Jak napisać plugin do Everplay

*[In English](plugins.md)*

Plugin dodaje coś do Everplay bez zmieniania samej aplikacji. Może to być animowana tapeta, która
rusza się z muzyką, motyw kolorów, mała karta pokazująca się przy zmianie utworu albo logika, która
zmienia wygląd interfejsu. Pluginy pisze się w **QML** (z JavaScriptem), czyli w tym samym języku,
w którym zrobiony jest interfejs Everplay. Plugin może więc korzystać ze wszystkiego, czego używają
ekrany aplikacji.

Użytkownicy instalują pluginy na stronie **Pluginy**: jednym kliknięciem z katalogu albo z pliku
`.zip`. Ten poradnik pokazuje, jak napisać własny.

> **Plugin to kod.** Działa w Everplay z uprawnieniami aplikacji: widzi, co gra, i może korzystać
> z internetu. Everplay ostrzega o tym przed każdą instalacją. Nie publikuj pluginów, które zbierają
> dane, i nie instaluj takich, którym nie ufasz.

## Spis treści

1. [Pierwszy plugin w pięć minut](#pierwszy-plugin-w-pięć-minut)
2. [plugin.json](#pluginjson)
3. [Gdzie plugin rysuje: miejsca](#gdzie-plugin-rysuje-miejsca)
4. [Obiekt `plugin`: ustawienia i pliki](#obiekt-plugin-ustawienia-i-pliki)
5. [Co plugin może odczytać i zrobić](#co-plugin-może-odczytać-i-zrobić)
6. [Motywy](#motywy)
7. [Zmiana wyglądu z poziomu QML](#zmiana-wyglądu-z-poziomu-qml)
8. [Ekran ustawień](#ekran-ustawień)
9. [Pakowanie i udostępnianie](#pakowanie-i-udostępnianie)
10. [Szukanie błędów](#szukanie-błędów)
11. [Dobre obyczaje: wydajność i bezpieczeństwo](#dobre-obyczaje-wydajność-i-bezpieczeństwo)

## Pierwszy plugin w pięć minut

1. W Everplay otwórz **Pluginy → Folder pluginów**. Każdy plugin to jeden folder w środku.
   Folder leży w katalogu danych Everplay:
   - Windows: `%APPDATA%\Everplay\Everplay\plugins`
   - Linux: `~/.local/share/Everplay/Everplay/plugins`
   - macOS: `~/Library/Application Support/Everplay/Everplay/plugins`
2. Utwórz folder `hello-wallpaper` z dwoma plikami.

   `plugin.json`:

   ```json
   {
       "id": "hello-wallpaper",
       "name": "Hello wallpaper",
       "version": "1.0.0",
       "author": "Ty",
       "description": "Koło, które pulsuje z basem.",
       "api": 1,
       "wallpaper": "Wallpaper.qml"
   }
   ```

   `Wallpaper.qml`:

   ```qml
   import QtQuick
   import Everplay

   Item {
       Rectangle {
           anchors.centerIn: parent
           width: 200 + Player.level * 400
           height: width
           radius: width / 2
           color: Theme.accent
           opacity: 0.25
           Behavior on width { NumberAnimation { duration: 120 } }
       }
   }
   ```

3. Wróć do Everplay i naciśnij **Przeładuj**. Plugin pojawi się w zakładce **Zainstalowane**, od razu
   włączony. Puść coś: koło w kolorze okładki oddycha za panelami.

Po zmianie plików znowu naciśnij **Przeładuj**. Nie trzeba restartować Everplay. Jeśli w QML jest
błąd, pojawi się na czerwono na karcie pluginu.

W folderze [`plugins/`](../plugins) tego repozytorium są trzy kompletne przykłady:
[spectrum-wallpaper](../plugins/spectrum-wallpaper) (tapeta z ustawieniami),
[night-themes](../plugins/night-themes) (same motywy, bez kodu) i
[now-playing-badge](../plugins/now-playing-badge) (nakładka).

## plugin.json

| Pole | Wymagane | Co to jest |
|---|---|---|
| `id` | tak | Unikalna nazwa: małe litery, cyfry, `.`, `-`, `_` (2–64 znaki). Tak nazywa się folder pluginu i pod nią trzymane są jego ustawienia. Nie zmieniaj jej po publikacji. |
| `name` | tak | Nazwa na karcie. |
| `version` | tak | W stylu `1.0.0`. Katalog proponuje aktualizację, gdy jego wersja jest wyższa. |
| `author` | nie | Kto zrobił plugin. |
| `description` | nie | Jedno lub dwa zdania o tym, co robi. |
| `homepage` | nie | Link (kod, zgłoszenia błędów); karta pokaże przycisk z linkiem. |
| `api` | nie | Wersja API, dla której plugin powstał; dziś `1`. Plugin wymagający nowszego API niż ma aplikacja nie zostanie wczytany. |
| `wallpaper` | nie | Plik QML rysowany za interfejsem. |
| `overlay` | nie | Plik QML rysowany nad interfejsem. |
| `main` | nie | Plik QML z logiką, bez własnego wyglądu. |
| `settings` | nie | Plik QML z ekranem ustawień pluginu. |
| `themes` | nie | Lista motywów kolorów ([Motywy](#motywy)). |
| `preview` | nie | Kwadratowy obrazek (PNG, 256×256 wystarczy) na kartę i do katalogu. |

`name` i `description` (oraz `name` motywu) można przetłumaczyć. Zamiast tekstu podaj obiekt
z kodami języków. Everplay wybierze język interfejsu, a gdy go brak, angielski:

```json
"name": { "en": "Spectrum wallpaper", "pl": "Tapeta spektrum" }
```

Wszystkie ścieżki są względne wobec folderu pluginu i nie mogą z niego wychodzić.

## Gdzie plugin rysuje: miejsca

Każdy plik QML wskazany w `plugin.json` jest budowany raz, gdy Everplay startuje albo gdy plugin
zostanie włączony. Trafia w jedno z tych miejsc:

| Miejsce | Gdzie | Rozmiar | Mysz |
|---|---|---|---|
| `wallpaper` | Za paskiem bocznym, stronami i panelami; nad tłem motywu i „żywym tłem” | Całe okno | Bez znaczenia (interfejs jest na wierzchu) |
| `overlay` | Nad wszystkim, także nad odtwarzaczem pełnoekranowym | Całe okno | Nigdy: kliknięcia przechodzą do aplikacji |
| `main` | Niewidoczne | 0 × 0 | — |

Główny element pliku `wallpaper` lub `overlay` jest rozciągany na całe okno automatycznie. Panele
są półprzezroczyste, więc tapeta prześwituje przez nie, trochę przytłumiona. Kilka pluginów może
używać tego samego miejsca; układają się w kolejności nazw folderów.

Gdy plugin zostanie wyłączony albo usunięty, jego elementy znikają.

## Obiekt `plugin`: ustawienia i pliki

Zadeklaruj `property var plugin` w głównym elemencie dowolnego swojego pliku, a Everplay wypełni
je, zanim zadziałają twoje wiązania:

```qml
Item {
    property var plugin
    readonly property real strength: plugin.setting("strength", 0.5)
}
```

| Element | Co robi |
|---|---|
| `plugin.id` | Id pluginu. |
| `plugin.version` | Wersja z `plugin.json`. |
| `plugin.dir` | Folder pluginu jako URL (kończy się na `/`). |
| `plugin.file("img/star.png")` | Plik pluginu jako URL, do `Image.source`, `Qt.createComponent` itd. |
| `plugin.setting(klucz, domyślna)` | Zapisane ustawienie albo `domyślna`, gdy nigdy go nie ustawiono. |
| `plugin.setSetting(klucz, wartość)` | Zapisuje ustawienie (liczby, tekst, wartości logiczne, listy, obiekty). Zapis jest natychmiastowy i przetrwa aktualizacje pluginu. |
| `plugin.revision` | Rośnie przy każdym `setSetting`. |
| `plugin.settingChanged(klucz)` | Sygnał wysyłany po zmianie ustawienia. |

Wiązanie wywołujące `plugin.setting()` samo nie przeliczy się po zmianie ustawienia. Odczytaj
w nim też `plugin.revision`, tak jak robią to przykłady:

```qml
readonly property string style: plugin.revision >= 0 ? plugin.setting("style", "both") : "both"
```

Ustawienia leżą w `plugin-data/<id>.json` obok folderu pluginów. Usunięcie pluginu usuwa też je.

## Co plugin może odczytać i zrobić

`import Everplay` daje pluginowi te same singletony, z których korzystają ekrany aplikacji. Na te
poniżej plugin może liczyć w API w wersji 1. Reszta modułu może się zmieniać między wersjami.

**`Player`**: grający utwór i sama muzyka

| Właściwość | |
|---|---|
| `hasTrack`, `playing`, `buffering` | Czy utwór jest wczytany, gra, ładuje się. |
| `title`, `artist`, `album`, `cover` | Dane utworu; `cover` to adres obrazka (może być pusty). |
| `colorA`, `colorB` | Dwa kolory wzięte z okładki. |
| `position`, `duration` | Sekundy. |
| `volume`, `muted` | |
| `level` | Ogólna głośność w tej chwili, 0–1, odświeżana wiele razy na sekundę. |
| `spectrum` | Lista poziomów (0–1) dla pasm częstotliwości, od basu do sopranów. |
| `upNext` | Wiersze następnych utworów w kolejce. |

Sygnały: `currentChanged()` (inny utwór), `spectrumChanged()`.
Metody: `play()`, `pause()`, `toggle()`, `next()`, `previous()`, `seek(sekundy)`.

**`Theme`**: kolory i pisma interfejsu

`bg`, `surface`, `surfaceHi` (tła, od najciemniejszego), `text`, `textDim`, `textFaint`,
`accent`, `accent2` (podążają za okładką, chyba że motyw je ustala), `onAccent` (tekst na
akcencie), `success`, `danger`, `warning`, `fontDisplay`, `fontText`, `themeId`.
Pomocnicze: `Theme.alpha(kolor, krycie)` i `Theme.ms(milisekundy)` (czas przeskalowany tak jak
animacje aplikacji).

**`Nav`**: poruszanie się po aplikacji

`Nav.navigate("home" | "search" | "library" | "settings" | "plugins" | …)`,
`Nav.toast(tekst, ikona)` (krótki komunikat na dole), `Nav.page` (otwarta strona),
`Nav.immersive` (otwarty odtwarzacz pełnoekranowy), `Nav.queueOpen` (otwarty prawy panel).

**`Language.effective`**: `"en"` albo `"pl"`, czyli język interfejsu, do tłumaczenia własnych tekstów.

**Komponenty**: `ArtCover` (okładka z gradientem zastępczym), `Icon` (`name`, `size`, `color`;
nazwy są w `qml/Icons.qml`), `Visualizer` (słupki spektrum), `PillButton`, `IconButton`, `Toggle`,
`SegmentedTabs`, `ProgressSlider`. Dzięki nim plugin wygląda jak część Everplay.

Pluginy mogą też korzystać ze wszystkiego, co daje Qt Quick: `QtQuick.Shapes`, `QtQuick.Effects`
(`MultiEffect`), `ShaderEffect`, `Canvas`, `XMLHttpRequest`, timerów i animacji.

## Motywy

Motyw to zestaw kolorów. Plugin może mieć ich dowolnie dużo, nawet bez linijki QML:

```json
"themes": [
    {
        "id": "deep-sea",
        "name": { "en": "Deep Sea", "pl": "Głębia" },
        "bg": "#070d14", "surface": "#0e1823", "surfaceHi": "#16232f",
        "text": "#e3eef5", "textDim": "#93a9b8", "textFaint": "#5b7080",
        "accent": "#5cc8d6", "accent2": "#1f4f7a", "onAccent": "#04121a",
        "fixedAccent": true
    }
]
```

Motywy pojawiają się w **Ustawienia → Wygląd** obok wbudowanych. Pola:

| Pole | |
|---|---|
| `id`, `name` | Wymagane. Id musi być unikalne tylko w obrębie pluginu. |
| `bg` | Wymagane. Tło okna. |
| `surface`, `surfaceHi` | Panele i elementy wyniesione (menu, wskazany wiersz). |
| `text`, `textDim`, `textFaint` | Tekst główny, drugorzędny, podpowiedzi. |
| `accent`, `accent2` | Kolor akcentu i jego ciemniejszy partner (gradienty, naklejka płyty). |
| `onAccent` | Tekst na przyciskach w kolorze akcentu. |
| `fixedAccent` | `true` (domyślne dla motywów z pluginów): motyw trzyma swój akcent. `false`: akcent podąża za okładką jak w motywie Everplay. |
| `fontDisplay`, `fontText` | Opcjonalne czcionki nagłówków i tekstu, np. `"Georgia"`. Czcionka musi być zainstalowana w systemie. |

Brakujący kolor zostanie wzięty z motywu Everplay. Trzymaj motywy ciemne, z jasnym tekstem: okładki,
teksty piosenek i nakładki są rysowane z takim założeniem. Sprawdź, czy `textDim` jest czytelny na
`surface`.

## Zmiana wyglądu z poziomu QML

Plik `main` może zmieniać wygląd, póki plugin jest włączony, przez `Binding` na właściwościach
`Theme`. Po wyłączeniu pluginu wiązania puszczają i wracają wartości aplikacji.

```qml
import QtQuick
import Everplay

// Akcent pulsuje z basem, niezależnie od wybranego motywu.
Item {
    Binding {
        target: Theme
        property: "accent"
        value: Player.playing ? Qt.lighter(Player.colorA, 1 + Player.level * 0.6) : Player.colorA
    }
}
```

W ten sposób można wiązać kolory `bg`, `surface`, `surfaceHi`, `text`, `textDim`, `textFaint`,
`accent`, `accent2` i `onAccent`. Nie ruszaj wyborów użytkownika (`themeId`, `wallpaper`,
`wallpaperDim`): zapisują się w jego ustawieniach.
Jeśli chcesz tylko innych kolorów, prostszy jest [motyw](#motywy), który użytkownik wybierze jak
każdy inny.

## Ekran ustawień

Wskaż plik QML w polu `"settings"`, a na karcie pluginu pojawi się przycisk ustawień, który otworzy
go w okienku. Jego główny element dostaje `plugin` jak każdy inny plik. Komponenty Everplay sprawią,
że będzie wyglądał natywnie:

```qml
import QtQuick
import QtQuick.Layouts
import Everplay

ColumnLayout {
    property var plugin
    spacing: 12

    Text { text: "Siła"; color: Theme.textDim; font.pixelSize: 13 }
    ProgressSlider {
        Layout.fillWidth: true
        value: plugin.setting("strength", 0.5)
        onMoved: v => plugin.setSetting("strength", v)
    }
    Toggle {
        checked: plugin.setting("bars", true)
        onToggled: on => plugin.setSetting("bars", on)
    }
}
```

Pozostałe pliki widzą zmianę od razu dzięki `plugin.revision`.

## Pakowanie i udostępnianie

**Plik.** Spakuj folder pluginu do `.zip`. Pliki mogą leżeć bezpośrednio w archiwum albo w jednym
folderze w środku. Inni instalują go przez **Pluginy → Zainstaluj z pliku**. Działa też wskazanie
pliku `plugin.json` na dysku: cały folder zostanie skopiowany.

**Katalog.** Zakładka **Katalog** pokazuje katalog na serwerze Everplay. Żeby plugin się w nim
znalazł, wyślij go opiekunom: otwórz zgłoszenie albo pull request, który dodaje jego folder do
[`plugins/`](../plugins). Pluginy są czytane przed publikacją w katalogu.

Dla opiekunów: `python plugins/build-catalog.py` pakuje każdy folder z `plugins/` do `dist/plugins`
(`<id>-<wersja>.zip`, `<id>.png` i `index.json` z rozmiarami i sumami SHA-256). Te pliki trafiają do
`/var/lib/nokturn/plugins` na serwerze. Everplay czyta `https://<serwer>/plugins/index.json`
i sprawdza każde pobranie z sumą zapisaną w katalogu.

## Szukanie błędów

- **Błędy** wczytywania QML pluginu pojawiają się na jego karcie, z plikiem i numerem linii.
- **`console.log()`** trafia do terminala, gdy Everplay zostanie z niego uruchomiony z przekierowanym
  wyjściem (na Windowsie np. z Git Bash):

  ```bash
  QT_FORCE_STDERR_LOGGING=1 "C:/Program Files/Everplay/Everplay.exe" 2>&1 | cat
  ```

- **Przeładuj** buduje wszystkie pluginy od nowa z plików.
- **Osobny profil** trzyma eksperymenty z dala od twojej biblioteki: uruchom Everplay
  z `EVERPLAY_PROFILE=<pusty folder>`, a pluginy będą w `<ten folder>/plugins`.
- **Testowy katalog**: `EVERPLAY_PLUGIN_CATALOG=file:///ścieżka/do/index.json` sprawia, że zakładka
  Katalog czyta twój własny `index.json` (zbuduj go poleceniem `build-catalog.py <folder>`).

## Dobre obyczaje: wydajność i bezpieczeństwo

Tapeta jest rysowana cały czas, za wszystkim, więc kosztuje w każdej klatce.

- Wybieraj `Rectangle`, `Shape` i `Image` zamiast `Canvas` (Canvas rysuje na procesorze).
- Trzymaj liczbę elementów nisko: sto animowanych prostokątów jest w porządku, tysiące już nie.
- Zwolnij albo zatrzymaj się, gdy nic nie gra: `running: Player.playing`.
- Wygładzaj `Player.level` przez `Behavior` z krótkim czasem, zamiast reagować na każdą wartość.
- Duże obrazki wczytuj z `sourceSize`, żeby nie były dekodowane w pełnym rozmiarze.
- Nie blokuj: żadnych długich pętli w JavaScripcie ani synchronicznego `XMLHttpRequest`.

I dla bezpieczeństwa:

- Łącz się z siecią tylko wtedy, gdy cel pluginu tego wymaga, i napisz o tym w opisie.
- Nigdy nie odczytuj ani nie wysyłaj kont użytkownika, tokenów, historii ani plików.
- To, co zapisujesz, trzymaj w `plugin.setSetting`, a nie we własnych plikach.
