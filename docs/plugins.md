# Writing plugins for Nokturn

*[Po polsku](plugins.pl.md)*

A plugin adds something to Nokturn without changing the app itself: an animated wallpaper that
moves with the music, a colour theme, a small card that pops up when a track changes, or logic
that restyles the interface. Plugins are written in **QML** (with JavaScript), the same language
Nokturn's interface is made of, so a plugin can use everything the app's own screens use.

Users install plugins on the **Plugins** page: from the catalog with one click, or from a `.zip`
file. This guide shows how to write one.

> **A plugin is code.** It runs inside Nokturn with the app's rights: it can see what is playing
> and use the internet. Nokturn warns about this before every install. Don't publish plugins that
> collect data, and don't install plugins you don't trust.

## Contents

1. [Your first plugin in five minutes](#your-first-plugin-in-five-minutes)
2. [plugin.json](#pluginjson)
3. [Where a plugin draws: slots](#where-a-plugin-draws-slots)
4. [The `plugin` object: settings and files](#the-plugin-object-settings-and-files)
5. [What a plugin can read and do](#what-a-plugin-can-read-and-do)
6. [Themes](#themes)
7. [Restyling the interface from QML](#restyling-the-interface-from-qml)
8. [A settings screen](#a-settings-screen)
9. [Packing and sharing](#packing-and-sharing)
10. [Debugging](#debugging)
11. [Good manners: speed and safety](#good-manners-speed-and-safety)

## Your first plugin in five minutes

1. In Nokturn open **Plugins → Plugins folder**. Every plugin is one folder in there.
   The folder is in Nokturn's data directory:
   - Windows: `%APPDATA%\Nokturn\Nokturn\plugins`
   - Linux: `~/.local/share/Nokturn/Nokturn/plugins`
   - macOS: `~/Library/Application Support/Nokturn/Nokturn/plugins`
2. Make a folder `hello-wallpaper` with two files.

   `plugin.json`:

   ```json
   {
       "id": "hello-wallpaper",
       "name": "Hello wallpaper",
       "version": "1.0.0",
       "author": "You",
       "description": "A circle that pulses with the bass.",
       "api": 1,
       "wallpaper": "Wallpaper.qml"
   }
   ```

   `Wallpaper.qml`:

   ```qml
   import QtQuick
   import Nokturn

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

3. Back in Nokturn press **Reload**. The plugin shows up under **Installed**, switched on. Play
   something: a circle in the colour of the cover breathes behind the panels.

Edit the files and press **Reload** again to see the change; there's no need to restart Nokturn.
If something is wrong in the QML, the error appears in red on the plugin's card.

The [`plugins/`](../plugins) folder of this repository has three complete examples:
[spectrum-wallpaper](../plugins/spectrum-wallpaper) (a wallpaper with settings),
[night-themes](../plugins/night-themes) (themes only, no code) and
[now-playing-badge](../plugins/now-playing-badge) (an overlay).

## plugin.json

| Field | Required | What it is |
|---|---|---|
| `id` | yes | A unique name: lowercase letters, digits, `.`, `-`, `_` (2–64 characters). It names the plugin's folder and keeps its settings. Never change it after publishing. |
| `name` | yes | The name shown on the card. |
| `version` | yes | `1.0.0`-style. The catalog offers an update when its version is higher. |
| `author` | no | Who made it. |
| `description` | no | One or two sentences about what it does. |
| `homepage` | no | A link (source code, issues); the card shows a link button. |
| `api` | no | The plugin API version it was written for; today `1`. A plugin asking for a newer API than the app has isn't loaded. |
| `wallpaper` | no | A QML file drawn behind the interface. |
| `overlay` | no | A QML file drawn above the interface. |
| `main` | no | A QML file with logic and no look of its own. |
| `settings` | no | A QML file with the plugin's settings screen. |
| `themes` | no | A list of colour themes ([Themes](#themes)). |
| `preview` | no | A square picture (PNG, 256×256 is plenty) for the card and the catalog. |

`name` and `description` (and a theme's `name`) can be translated. Give an object with language
codes instead of a string; Nokturn picks the interface language, then English:

```json
"name": { "en": "Spectrum wallpaper", "pl": "Tapeta spektrum" }
```

All paths are relative to the plugin's folder and can't leave it.

## Where a plugin draws: slots

Each QML file named in `plugin.json` is built once when Nokturn starts (or when the plugin is
switched on), in one of these places:

| Slot | Where | Size | Mouse |
|---|---|---|---|
| `wallpaper` | Behind the sidebar, the pages and the panels; above the theme's background and the "living background" | The whole window | Not useful (the interface is on top) |
| `overlay` | Above everything, including the full-screen player | The whole window | Never: clicks go through to the app |
| `main` | Nowhere visible | 0 × 0 | — |

The root item of a `wallpaper` or `overlay` file is stretched over the window for you. The
panels are translucent, so a wallpaper shows through them, softened. Several plugins can use the
same slot; they are stacked in the order of their folder names.

When the plugin is switched off or removed, its items are destroyed.

## The `plugin` object: settings and files

Declare `property var plugin` in the root item of any of your files and Nokturn fills it in
before your bindings run:

```qml
Item {
    property var plugin
    readonly property real strength: plugin.setting("strength", 0.5)
}
```

| Member | What it does |
|---|---|
| `plugin.id` | The plugin's id. |
| `plugin.version` | Its version from `plugin.json`. |
| `plugin.dir` | The plugin's folder as a URL (ends with `/`). |
| `plugin.file("img/star.png")` | A file of the plugin as a URL, for `Image.source`, `Qt.createComponent` and so on. |
| `plugin.setting(key, fallback)` | A stored setting, or `fallback` when it was never set. |
| `plugin.setSetting(key, value)` | Stores a setting (numbers, text, booleans, lists, objects). It's saved at once and survives updates of the plugin. |
| `plugin.revision` | Grows with every `setSetting`. |
| `plugin.settingChanged(key)` | A signal sent after a setting changes. |

A binding that calls `plugin.setting()` is not re-evaluated by itself when the setting changes.
Read `plugin.revision` in the same binding so it follows changes, the same way the examples do:

```qml
readonly property string style: plugin.revision >= 0 ? plugin.setting("style", "both") : "both"
```

Settings live in `plugin-data/<id>.json` next to the plugins folder. Removing a plugin removes
them too.

## What a plugin can read and do

`import Nokturn` gives a plugin the same singletons the app's own screens use. The ones below
are what plugins can rely on in API version 1; anything else in the module may change between
versions.

**`Player`**: the playing track and the music itself

| Property | |
|---|---|
| `hasTrack`, `playing`, `buffering` | Whether a track is loaded, plays, is loading. |
| `title`, `artist`, `album`, `cover` | The track's details; `cover` is an image URL (it can be empty). |
| `colorA`, `colorB` | Two colours taken from the cover. |
| `position`, `duration` | Seconds. |
| `volume`, `muted` | |
| `level` | The overall loudness right now, 0–1, updated many times a second. |
| `spectrum` | A list of levels (0–1) per frequency band, from bass to treble. |
| `upNext` | Rows of the next tracks in the queue. |

Signals: `currentChanged()` (another track), `spectrumChanged()`.
Methods: `play()`, `pause()`, `toggle()`, `next()`, `previous()`, `seek(seconds)`.

**`Theme`**: colours and type of the interface

`bg`, `surface`, `surfaceHi` (backgrounds, darkest to lightest), `text`, `textDim`, `textFaint`,
`accent`, `accent2` (they follow the cover unless the theme fixes them), `onAccent` (text on the
accent), `success`, `danger`, `warning`, `fontDisplay`, `fontText`, `themeId`.
Helpers: `Theme.alpha(color, opacity)` and `Theme.ms(milliseconds)` (a duration scaled like the
app's own animations).

**`Nav`**: moving around the app

`Nav.navigate("home" | "search" | "library" | "settings" | "plugins" | …)`,
`Nav.toast(text, icon)` (a short message at the bottom), `Nav.page` (the open page),
`Nav.immersive` (the full-screen player is open), `Nav.queueOpen` (the right panel is open).

**`Language.effective`**: `"en"` or `"pl"`, the interface language, for translating your own texts.

**Components**: `ArtCover` (a cover with a gradient fallback), `Icon` (`name`, `size`, `color`;
the names are in `qml/Icons.qml`), `Visualizer` (spectrum bars), `PillButton`, `IconButton`,
`Toggle`, `SegmentedTabs`, `ProgressSlider`. Using them keeps a plugin looking like part of
Nokturn.

Plugins can also use everything Qt Quick offers: `QtQuick.Shapes`, `QtQuick.Effects`
(`MultiEffect`), `ShaderEffect`, `Canvas`, `XMLHttpRequest`, timers and animations.

## Themes

A theme is a set of colours. A plugin can bring any number of them, with no QML at all:

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

The themes appear in **Settings → Appearance** next to the built-in ones. The fields:

| Field | |
|---|---|
| `id`, `name` | Required. The id only has to be unique within the plugin. |
| `bg` | Required. The window's background. |
| `surface`, `surfaceHi` | Panels and raised elements (menus, hovered rows). |
| `text`, `textDim`, `textFaint` | Main text, secondary text, hints. |
| `accent`, `accent2` | The accent colour and its darker partner (gradients, the vinyl label). |
| `onAccent` | Text on accent-coloured buttons. |
| `fixedAccent` | `true` (the default for plugin themes): the theme keeps its accent. `false`: the accent follows the cover like in the Nokturn theme. |
| `fontDisplay`, `fontText` | Optional font families for headings and body text, for example `"Georgia"`. The font has to be installed on the computer. |

A missing colour is taken from the Nokturn theme. Keep themes dark with light text: covers,
lyrics and overlays are drawn assuming that. Check that `textDim` stays readable on `surface`.

## Restyling the interface from QML

A `main` file can change the look while the plugin is on, with `Binding` on `Theme`'s
properties. When the plugin is switched off, the bindings let go and the app's own values come
back.

```qml
import QtQuick
import Nokturn

// The accent pulses with the bass, whatever theme is chosen.
Item {
    Binding {
        target: Theme
        property: "accent"
        value: Player.playing ? Qt.lighter(Player.colorA, 1 + Player.level * 0.6) : Player.colorA
    }
}
```

The colours `bg`, `surface`, `surfaceHi`, `text`, `textDim`, `textFaint`, `accent`, `accent2`
and `onAccent` can be bound this way. Leave the user's own choices alone (`themeId`, `wallpaper`,
`wallpaperDim`): they are saved in the user's settings. If you only want different colours, a [theme](#themes) is simpler and the user can
pick it like any other.

## A settings screen

Name a QML file in `"settings"` and the plugin's card gets a settings button that opens it in a
window. Its root gets `plugin` like any other file. Nokturn's components make it look native:

```qml
import QtQuick
import QtQuick.Layouts
import Nokturn

ColumnLayout {
    property var plugin
    spacing: 12

    Text { text: "Strength"; color: Theme.textDim; font.pixelSize: 13 }
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

The other files see the change at once through `plugin.revision`.

## Packing and sharing

**A file.** Pack the plugin's folder into a `.zip`. The files can be at the archive's root, or
inside one folder. Others install it with **Plugins → Install from file**. Selecting a
`plugin.json` on disk works too: the folder is copied in.

**The catalog.** The catalog on the Nokturn server is what the **Catalog** tab shows. To get a
plugin in, send it to the maintainers: open an issue or a pull request that adds its folder under
[`plugins/`](../plugins). Plugins in the catalog are read before they're published.

For maintainers: `python plugins/build-catalog.py` packs every folder in `plugins/` into
`dist/plugins` (`<id>-<version>.zip`, `<id>.png`, `index.json` with sizes and SHA-256 sums). Upload
those files to `/var/lib/nokturn/plugins` on the server; Nokturn reads
`https://<server>/plugins/index.json` and checks every download against the sum in it.

## Debugging

- **Errors** from loading a plugin's QML appear on its card, with the file and line.
- **`console.log()`** output goes to the terminal when Nokturn is started from one with its output
  piped (on Windows, from Git Bash or a similar shell):

  ```bash
  QT_FORCE_STDERR_LOGGING=1 "C:/Program Files/Nokturn/Nokturn.exe" 2>&1 | cat
  ```

- **Reload** builds every plugin again from its files.
- **A separate profile** keeps experiments away from your library: start Nokturn with
  `NOKTURN_PROFILE=<an empty folder>`; plugins then live in `<that folder>/plugins`.
- **A test catalog**: `NOKTURN_PLUGIN_CATALOG=file:///path/to/index.json` makes the Catalog tab
  read your own `index.json` (build it with `build-catalog.py <folder>`).

## Good manners: speed and safety

A wallpaper is drawn all the time, behind everything, so it costs on every frame.

- Prefer `Rectangle`, `Shape` and `Image` to `Canvas` (a Canvas repaints on the CPU).
- Keep the number of items small: a hundred animated rectangles is fine, thousands are not.
- Slow down or stop when nothing plays: `running: Player.playing`.
- Use `Behavior` with short durations to smooth `Player.level`, instead of reacting to every
  value.
- Load big pictures with `sourceSize` so they aren't decoded at full size.
- Don't block: no long loops in JavaScript, no synchronous `XMLHttpRequest`.

And for safety:

- Only talk to the network when the plugin's purpose needs it, and say so in its description.
- Never read or send the user's accounts, tokens, history or files.
- Keep what you store in `plugin.setSetting`, not in files of your own.
