import QtQuick
import QtQuick.Controls
import Nokturn

// Developer tool: `Nokturn.exe --tour <folder>` walks through the screens
// and saves a screenshot of each one as PNG. Inactive in a normal run.
Item {
    id: root
    property Item target
    property string dir: ""
    property int step: -1

    readonly property bool withLogin: Qt.application.arguments.indexOf("--tour-login") >= 0
    readonly property bool withEngine: Qt.application.arguments.indexOf("--tour-engine") >= 0
    readonly property bool withBig: Qt.application.arguments.indexOf("--tour-big") >= 0
    readonly property bool withEq: Qt.application.arguments.indexOf("--tour-eq") >= 0
    readonly property bool withMatch: Qt.application.arguments.indexOf("--tour-match") >= 0
    // Correcting a Spotify track's YouTube Music match (needs a synced Spotify library).
    property var matchTrack: ({})
    property var matchResults: []
    function firstSpotifyTrack() {
        for (let i = 0; i < Library.tracks.count; ++i) {
            const t = Library.tracks.get(i)
            if (t.source === "spotify")
                return t
        }
        return {}
    }
    // The first visible track row of a Spotify track (TrackRow) — to open its menu.
    function findSpotifyRow(item) {
        if (item.openMenu && item.t && item.t.source === "spotify" && item.visible && item.opacity > 0.5)
            return item
        for (let i = 0; i < item.children.length; ++i) {
            const found = root.findSpotifyRow(item.children[i])
            if (found)
                return found
        }
        return null
    }
    readonly property var matchSteps: [
        { name: "M1-library", run: () => { Nav.finishOnboarding(); Nav.navigate("library"); root.matchTrack = root.firstSpotifyTrack()
                                            console.log("tour: track", root.matchTrack.artist, "-", root.matchTrack.title, root.matchTrack.durationText) } },
        { name: "M2-menu", run: () => { const row = root.findSpotifyRow(root.target); console.log("tour: menu row", row ? row.t.title : "none")
                                        if (row) row.openMenu(row.width - row.menuWidth, row.height) } },
        { name: "M2-auto", run: () => Spotify.rematch(root.matchTrack.id) },
        { name: "M3-dialog", run: () => { console.log("tour: auto match =", Spotify.matchedVideoId(root.matchTrack.id)); Nav.chooseMatch(root.matchTrack) } },
        { name: "M4-results", run: () => {} },
        { name: "M5-choose", run: () => { const r = root.matchResults[root.matchResults.length - 1]
                                          if (r) Spotify.chooseMatch(root.matchTrack.id, r.videoId)
                                          console.log("tour: chosen =", Spotify.matchedVideoId(root.matchTrack.id), "manual =", Spotify.manualMatch(root.matchTrack.id)) } },
        { name: "M6-dialog-again", run: () => Nav.chooseMatch(root.matchTrack) },
        { name: "M7-rematch", run: () => Spotify.rematch(root.matchTrack.id) },
        { name: "M8-result", run: () => console.log("tour: after rematch =", Spotify.matchedVideoId(root.matchTrack.id), "manual =", Spotify.manualMatch(root.matchTrack.id)) }
    ]
    readonly property bool withCovers: Qt.application.arguments.indexOf("--tour-covers") >= 0
    // Playlist covers: the collage of track covers, an uploaded picture (one of the tour's own
    // screenshots), back to the default. Likes a few tracks first, so "Liked" has covers to use.
    readonly property var coverSteps: [
        { name: "C1-liked", run: () => { Nav.finishOnboarding()
                                        let liked = 0
                                        for (let i = 0; i < Library.tracks.count && liked < 6; ++i) {
                                            const t = Library.tracks.get(i)
                                            if (t.cover && !t.liked) { Library.tracks.toggleLiked(i); ++liked }
                                        }
                                        Library.shufflePlaylistCover(0)
                                        console.log("tour: liked mosaic =", JSON.stringify(Library.playlists[0].mosaic))
                                        Nav.navigate("playlist", { kind: "playlist", index: 0 }) } },
        { name: "C2-shuffled", run: () => { Library.shufflePlaylistCover(0); console.log("tour: shuffled =", JSON.stringify(Library.playlists[0].mosaic)) } },
        { name: "C3-custom", run: () => { console.log("tour: upload =", Library.setPlaylistCover(1, "file:///" + root.dir + "/C1-liked.png"),
                                                      Library.playlists[1].cover, Library.playlists[1].customCover)
                                         Nav.navigate("playlist", { kind: "playlist", index: 1 }) } },
        { name: "C4-home", run: () => Nav.navigate("home") },
        { name: "C5-reset", run: () => { Library.resetPlaylistCover(1); console.log("tour: reset =", Library.playlists[1].cover, Library.playlists[1].customCover)
                                        Nav.navigate("playlist", { kind: "playlist", index: 1 }) } }
    ]
    readonly property var eqSteps: [
        { name: "Q1-play", run: () => { Equalizer.applyPreset("Flat"); Player.play(); Nav.navigate("equalizer") } },
        { name: "Q2-flat", run: () => console.log("tour: flat  peak", Equalizer.peak.toFixed(1), "dB, reduction", Equalizer.reduction.toFixed(1)) },
        { name: "Q3-bass", run: () => { Equalizer.applyPreset("Extreme bass"); console.log("tour: curve 30/60/250/1k/10k Hz:", [30, 60, 250, 1000, 10000].map(f => Equalizer.response(1000)[Math.round(Math.log(f / 20) / Math.log(1000) * 999)].toFixed(1)).join(" / ")) } },
        { name: "Q4-bass-meter", run: () => console.log("tour: bass  peak", Equalizer.peak.toFixed(1), "dB, reduction", Equalizer.reduction.toFixed(1)) },
        { name: "Q5-nolimit", run: () => { Equalizer.limiter = false; Equalizer.preamp = 12 } },
        { name: "Q6-clip", run: () => { console.log("tour: no limiter peak", Equalizer.peak.toFixed(1), "dB, clipping", Equalizer.clipping); Equalizer.limiter = true; Equalizer.preamp = 0 } }
    ]
    // Performance (--tour-perf, with NOKTURN_PERF_DEBUG=1): playback while searching and opening artists.
    readonly property bool withPerf: Qt.application.arguments.indexOf("--tour-perf") >= 0
    function findSearchPage(item) {
        if (item.onlineEnabled !== undefined && item.query !== undefined)
            return item
        for (let i = 0; i < item.children.length; ++i) {
            const found = findSearchPage(item.children[i])
            if (found)
                return found
        }
        return null
    }
    function typeQuery(text) {
        const page = findSearchPage(root.target)
        console.log("tour: step query", JSON.stringify(text), page ? "" : "(no search page)")
        if (page)
            page.query = text
    }
    readonly property var perfSteps: [
        { name: "P0-find", run: () => { Nav.finishOnboarding(); YouTube.search("Daft Punk Around the World") } },
        { name: "P1-play", run: () => { const r = Library.onlineResults(); console.log("tour: step play, tracks", Library.tracks.count, "results", r.length); if (r.length) Player.playFrom(r, r[0]); Nav.navigate("home") } },
        { name: "P2-wait", run: () => console.log("tour: step playing", Player.playing, Player.title) },
        { name: "P3-search", run: () => { console.log("tour: step open search"); Nav.navigate("search") } },
        { name: "P4-q1", run: () => root.typeQuery("d") },
        { name: "P5-q2", run: () => root.typeQuery("da") },
        { name: "P6-q3", run: () => root.typeQuery("daft") },
        { name: "P7-q4", run: () => root.typeQuery("daft punk") },
        { name: "P8-wait", run: () => console.log("tour: step wait") },
        { name: "P9-q5", run: () => root.typeQuery("dawid podsiadlo") },
        { name: "P10-wait", run: () => console.log("tour: step wait") },
        { name: "P11-artist", run: () => { console.log("tour: step artist Daft Punk"); Nav.openArtist("Daft Punk", "") } },
        { name: "P12-wait", run: () => console.log("tour: step wait") },
        { name: "P13-wait", run: () => console.log("tour: step wait") },
        { name: "P14-artist", run: () => { console.log("tour: step artist Taco Hemingway"); Nav.openArtist("Taco Hemingway", "") } },
        { name: "P15-wait", run: () => console.log("tour: step wait") },
        { name: "P16-wait", run: () => console.log("tour: step end, playing", Player.playing) }
    ]
    // Seeking back and forth in a YouTube stream, with lyrics (--tour-seek).
    readonly property bool withSeek: Qt.application.arguments.indexOf("--tour-seek") >= 0
    function seekLog(label) {
        console.log("tour:", label, "playing", Player.playing, "pos", Player.position.toFixed(2), "/", Player.duration,
                    "| lyrics", Lyrics.state, "synced", Lyrics.synced, "line", Lyrics.synced ? Lyrics.lineAt(Player.position) : -1)
    }
    readonly property var seekSteps: [
        { name: "K0-search", run: () => { Nav.finishOnboarding(); YouTube.search("Daft Punk Get Lucky") } },
        { name: "K1-play", run: () => { const r = Library.onlineResults(); console.log("tour: results", r.length); if (r.length) Player.playFrom(r, r[0]); Nav.queueOpen = true } },
        { name: "K2-wait", run: () => root.seekLog("start") },
        { name: "K3-wait", run: () => root.seekLog("playing") },
        { name: "K4-fwd", run: () => { Player.seek(60); root.seekLog("seek 60") } },
        { name: "K5-wait", run: () => root.seekLog("after 60") },
        { name: "K6-back", run: () => { Player.seek(10); root.seekLog("seek 10") } },
        { name: "K7-wait", run: () => root.seekLog("after 10") },
        { name: "K8-back2", run: () => { Player.seek(2); root.seekLog("seek 2") } },
        { name: "K9-wait", run: () => root.seekLog("after 2") }
    ]
    // Albums and singles on an artist profile open (--tour-albums).
    readonly property bool withAlbums: Qt.application.arguments.indexOf("--tour-albums") >= 0
    readonly property var albumSteps: [
        { name: "A1-artist", run: () => { Nav.finishOnboarding(); Nav.openArtist("Faydee", "") } },
        { name: "A2-wait", run: () => {} },
        { name: "A2b-wait", run: () => {} },
        { name: "A2c-wait", run: () => {} },
        { name: "A3-cards", run: () => {
            const secs = YouTube.artist("UC0ITc55ErLkHnGp26P-wmRA").sections || []
            secs.forEach(s => console.log("tour: section", s.title, s.kind, s.items.length, "first:", s.items.length ? s.items[0].kind + " " + s.items[0].title + " pl=" + s.items[0].playlistId + " id=" + s.items[0].browseId : ""))
            root.album = secs.length && secs[0].items.length ? secs[0].items[0] : null
            if (root.album) Nav.navigate("playlist", { kind: "ytcollection", playlistId: root.album.playlistId, title: root.album.title, cover: root.album.cover, label: "ALBUM" })
        } },
        { name: "A4-wait", run: () => {} },
        { name: "A5-rows", run: () => root.logCollection(root.album ? root.album.playlistId : "") },
        { name: "A6-page", run: () => { if (root.album) Nav.navigate("playlist", { kind: "ytcollection", playlistId: root.album.browseId, title: root.album.title, cover: root.album.cover, label: "ALBUM" }) } },
        { name: "A7-wait", run: () => {} },
        { name: "A8-rows", run: () => root.logCollection(root.album ? root.album.browseId : "") }
    ]
    property var album: null
    function logCollection(id) {
        const r = Library.collectionRows(id)
        console.log("tour: collection", id, "rows", r.length, r.length ? Library.tracks.get(r[0]).title + " / " + Library.tracks.get(r[0]).artist + " / cover " + (Library.tracks.get(r[0]).cover !== "") : "")
    }
    // Local playlists: create, add, rename, remove a track, the dialogs, delete (--tour-playlists).
    readonly property bool withPlaylists: Qt.application.arguments.indexOf("--tour-playlists") >= 0
    function userIndex() {
        return Library.playlists.findIndex(p => p.kind === "user")
    }
    function logUser(label) {
        const i = root.userIndex()
        const p = i >= 0 ? Library.playlists[i] : null
        console.log("tour:", label, p ? "index " + i + " '" + p.name + "' desc '" + p.description + "' rows " + Library.playlistRows(i).length
                                        + " page " + Nav.page + " " + JSON.stringify(Nav.args) : "no user playlist")
    }
    readonly property var playlistSteps: [
        { name: "L1-search", run: () => { Nav.finishOnboarding(); YouTube.search("Daft Punk") } },
        { name: "L2-create", run: () => { const r = Library.onlineResults(); console.log("tour: results", r.length, r.slice(0, 4).map(x => Library.tracks.get(x).title + " / " + Library.tracks.get(x).artist + " / " + Library.tracks.get(x).album).join(" | "))
                                          Playlists.createPlaylist("Moja testowa", "", "local", "PRIVATE", r.slice(0, 3)) } },
        { name: "L3-add", run: () => { root.logUser("created"); Playlists.addRows(root.userIndex(), Library.onlineResults().slice(2, 6)) } },
        { name: "L4-page", run: () => root.logUser("added") },
        { name: "L5-rename", run: () => { Playlists.edit(root.userIndex(), "Daft Punk na wieczór", "Na spokojnie"); root.logUser("renamed") } },   // i18n-ignore
        { name: "L6-remove", run: () => { const i = root.userIndex(); Playlists.removeRow(i, Library.playlistRows(i)[0]); root.logUser("removed") } },
        { name: "L7-edit-dialog", run: () => Nav.editPlaylist(root.userIndex()) },
        { name: "L8-create-dialog", run: () => Nav.createPlaylist(Library.onlineResults().slice(0, 2)) },
        { name: "L8b-wait", run: () => {} },
        { name: "L9-delete", run: () => { Playlists.deletePlaylist(root.userIndex()); root.logUser("deleted") } }
    ]
    // Where the lyrics of a few popular tracks come from (--tour-lyrics).
    readonly property bool withLyrics: Qt.application.arguments.indexOf("--tour-lyrics") >= 0
    readonly property var lyricQueries: ["Dua Lipa Levitating", "Ed Sheeran Shape of You", "Dawid Podsiadło Małomiasteczkowy", "Daft Punk Get Lucky"]   // i18n-ignore
    function lyricsLog(q) {
        console.log("tour: lyrics", q, "|", Player.title, "-", Player.artist, "| state", Lyrics.state, "synced", Lyrics.synced,
                    "source", Lyrics.source, "origin", Lyrics.origin, "lines", Lyrics.lines.length)
    }
    readonly property var lyricSteps: {
        const s = [{ name: "Y0", run: () => Nav.finishOnboarding() }]
        lyricQueries.forEach((q, i) => {
            s.push({ name: "Y" + i + "a", run: () => YouTube.search(q) })
            s.push({ name: "Y" + i + "b", run: () => { const r = Library.onlineResults(); if (r.length) Player.playFrom(r, r[0]) } })
            s.push({ name: "Y" + i + "c", run: () => {} })
            s.push({ name: "Y" + i + "d", run: () => root.lyricsLog(q) })
        })
        return s
    }
    // Recommendations on the home page, genres on the search page (--tour-home).
    readonly property bool withHome: Qt.application.arguments.indexOf("--tour-home") >= 0
    function recLog(label) {
        const r = Library.recommendedRows()
        console.log("tour:", label, "| loading", Recommendations.loading, "| based on", JSON.stringify(Recommendations.basedOn),
                    "| picks", r.length, "|", r.slice(0, 10).map(x => Library.tracks.get(x).artist + " - " + Library.tracks.get(x).title).join(" | "))
    }
    // The scrolled content of the current page (PageFlick)
    function findFlick(item) {
        if (item.visible && item.contentY !== undefined && item.contentHeight > item.height + 10 && item.width > 500)
            return item
        for (let i = 0; i < item.children.length; ++i) {
            const found = root.findFlick(item.children[i])
            if (found)
                return found
        }
        return null
    }
    function scrollPage(y) {
        const f = root.findFlick(root.target)
        console.log("tour: scroll", f ? "to " + y + " of " + Math.round(f.contentHeight) : "— nothing to scroll")
        if (f)
            f.contentY = y
    }
    readonly property var homeSteps: [
        { name: "H1-home", run: () => { Nav.finishOnboarding(); Nav.navigate("home"); root.recLog("start") } },
        { name: "H2-wait", run: () => { root.recLog("wait"); root.scrollPage(440) } },
        { name: "H3-wait", run: () => root.recLog("wait") },
        { name: "H4-refresh", run: () => { root.recLog("before refresh"); Recommendations.refresh() } },
        { name: "H5-wait", run: () => root.recLog("refreshing") },
        { name: "H6-refreshed", run: () => { root.recLog("refreshed"); root.scrollPage(440) } },
        { name: "H7-play", run: () => { const r = Library.recommendedRows(); if (r.length) Player.playFrom(r, r[1], "recs")
                                        console.log("tour: all tracks", Library.localTracks().length, "recently added", Library.recentlyAdded(50).length) } },
        { name: "H7b-playing", run: () => console.log("tour: playing", Player.title, "-", Player.artist) },
        { name: "H8-search", run: () => { const g = Library.genres()
                                          console.log("tour: genres", g.length, g.slice(0, 14).map(x => x.name + " (" + x.count + ")").join(", "))
                                          Nav.navigate("search") } },
        { name: "H8b-wait", run: () => console.log("tour: genres", Library.genres().length) },
        { name: "H8c-wait", run: () => console.log("tour: genres", Library.genres().length) },
        { name: "H8d-search", run: () => { const g = Library.genres()
                                           console.log("tour: genres", g.length, g.slice(0, 14).map(x => x.name + " (" + x.count + ")").join(", ")) } },
        { name: "H9-genre", run: () => { const g = Library.genres()
                                         if (g.length) {
                                             Nav.navigate("playlist", { kind: "genre", key: g[0].key, name: g[0].name })
                                             console.log("tour: genre", g[0].name, "rows", Library.tracksOfGenre(g[0].key).length, "mosaic", g[0].mosaic.length)
                                         } } },
        { name: "H9b-genre-loaded", run: () => {} },
        { name: "H10-home", run: () => { console.log("tour: playing", Player.title, "-", Player.artist); Nav.navigate("home") } },
        // Over 30 s of playback by now — the track counts as listened to (recommendations.json, "plays").
        { name: "H11-wait", run: () => {} },
        { name: "H12-wait", run: () => {} },
        { name: "H13-wait", run: () => {} },
        { name: "H13b-wait", run: () => {} },
        { name: "H13c-wait", run: () => {} },
        { name: "H13d-wait", run: () => {} },
        { name: "H14-wait", run: () => console.log("tour: position", Math.round(Player.position), "s of", Player.title) }
    ]
    // Artist profiles: a real artist found by name, then a YouTube channel that uploads remixes (--tour-artist).
    readonly property bool withArtist: Qt.application.arguments.indexOf("--tour-artist") >= 0
    function artistLog(id) {
        const a = YouTube.artist(id)
        console.log("tour: artist", id, a.name, "| photo", (a.photo || "") !== "", "| songs", Library.artistSongRows(id).length,
                    "| all", a.songsPlaylistId, "| sections", (a.sections || []).map(s => s.title + " " + s.kind + " " + s.items.length).join(", "),
                    "| error", a.error || "")
        // What the page shows (its bindings must follow the loaded data)
        const f = root.findFlick(root.target)
        if (f && f.parent.yt !== undefined)
            console.log("tour:   page has", (f.parent.yt.sections || []).length, "sections, height", Math.round(f.contentHeight))
    }
    readonly property var artistSteps: [
        { name: "R1-artist", run: () => { Nav.finishOnboarding(); Nav.openArtist("Kizo", "") } },
        { name: "R2-wait", run: () => {} },
        { name: "R3-loaded", run: () => root.artistLog(YouTube.knownArtistId("Kizo")) },
        { name: "R4-scroll", run: () => root.scrollPage(700) },
        { name: "R5-scroll", run: () => root.scrollPage(1400) },
        { name: "R6-scroll", run: () => root.scrollPage(2100) },
        { name: "R7-channel", run: () => Nav.openArtist("Rogal575", "UC-ijAH6jOi6945c3oytlCuQ") },
        { name: "R8-wait", run: () => {} },
        { name: "R9-loaded", run: () => root.artistLog("UC-ijAH6jOi6945c3oytlCuQ") },
        { name: "R10-scroll", run: () => root.scrollPage(700) }
    ]
    // Liked tracks: click the first row, then "next" must play the second (--tour-liked).
    readonly property bool withLiked: Qt.application.arguments.indexOf("--tour-liked") >= 0
    property var likedPage: null
    function findRows(item) {   // the visible TrackRows of the current page, top to bottom
        let out = []
        if (item.visible && item.trackIndex !== undefined && item.play !== undefined)
            out.push(item)
        for (let i = 0; i < item.children.length; ++i)
            out = out.concat(root.findRows(item.children[i]))
        return out
    }
    function queueLog(label) {
        console.log("tour:", label, "| playing", Player.title, "| shuffle", Player.shuffle, "| context", Player.queueContext,
                    "| up next", Player.upNext.slice(0, 4).map(r => Library.tracks.get(r).title).join(" / "))
    }
    readonly property var likedSteps: [
        { name: "V1-like", run: () => {
            Nav.finishOnboarding()
            const rows = Library.likedTracks().length > 0 ? [] : Library.libraryRows().slice(0, 6)
            rows.forEach(r => { Player.playFrom(rows, r, "test"); if (!Player.liked) Player.toggleLiked() })
            // A "shuffle play" button elsewhere first — it must not leave the shuffle mode on.
            Player.playShuffled(Library.libraryRows().slice(0, 30), "test")
            root.queueLog("shuffle play")
            Player.pause()
            console.log("tour: liked", Library.likedTracks().length)
            const i = Library.playlists.findIndex(p => p.kind === "liked")
            console.log("tour: liked playlist index", i, i >= 0 ? Library.playlists[i].name : "")
            Nav.navigate("playlist", { index: i })
        } },
        { name: "V2-page", run: () => {
            const rows = root.findRows(root.target).sort((a, b) => a.mapToItem(null, 0, 0).y - b.mapToItem(null, 0, 0).y)
            console.log("tour: rows on the page", rows.map(r => Library.tracks.get(r.trackIndex).title).join(" / "))
            console.log("tour: first row queue", rows.length ? (rows[0].queue || []).map(r => Library.tracks.get(r).title).join(" / ") : "-", "| context", rows.length ? rows[0].context : "")
            if (rows.length) rows[0].play()
        } },
        { name: "V3-playing", run: () => root.queueLog("after click") },
        { name: "V4-next", run: () => { Player.next(); root.queueLog("after next") } },
        { name: "V5-next", run: () => root.queueLog("later") }
    ]
    // The in-game overlay, driven like a pad (--tour-overlay).
    readonly property bool withOverlay: Qt.application.arguments.indexOf("--tour-overlay") >= 0
    function overlayLog(label) {
        const m = GameOverlay.view
        if (!m) { console.log("tour:", label, "| overlay closed, open =", GameOverlay.open); return }
        const it = m.items[m.selection] || {}
        console.log("tour:", label, "| category", m.categories[m.cat].key, "| sub", m.sub ? m.sub.title : "-", "| item", m.selection, "/", m.items.length,
                    it.title, "|", it.subtitle || "", "| adjusting", m.adjusting, "| playing", Player.title, "| volume", Math.round(Player.volume * 100))
    }
    function pad(actions) { actions.forEach(a => GameOverlay.simulate(a)) }
    readonly property var overlaySteps: [
        { name: "O1-open", run: () => { Nav.finishOnboarding(); const r = Library.libraryRows(); Player.playFrom(r, r[3], "library"); GameOverlay.show() } },
        { name: "O2-now", run: () => root.overlayLog("now playing") },
        { name: "O3-volume", run: () => { Player.volume = 0.5; root.pad(["down", "down", "down", "accept", "right", "right", "up"]); root.overlayLog("volume (50 + 15)") } },
        { name: "O4-queue", run: () => { root.pad(["back", "right"]); root.overlayLog("queue") } },
        { name: "O5-queue-down", run: () => { root.pad(["down", "down", "down", "down"]); root.overlayLog("queue down") } },
        { name: "O6-playlists", run: () => { root.pad(["right"]); root.overlayLog("playlists") } },
        { name: "O7-open", run: () => { root.pad(["down", "down", "down", "accept"]); root.overlayLog("opened") } },
        { name: "O8-track", run: () => { root.pad(["down", "down", "down"]); root.overlayLog("track") } },
        { name: "O9-play", run: () => { root.pad(["accept"]); root.overlayLog("played") } },
        { name: "O10-back", run: () => { root.pad(["back"]); root.overlayLog("back") } },
        { name: "O11-close", run: () => { root.pad(["back"]); root.overlayLog("closing") } },
        { name: "O12-closed", run: () => root.overlayLog("closed") },
        { name: "O13-reopen", run: () => { GameOverlay.toggle(); root.overlayLog("reopened") } },
        { name: "O14-shown", run: () => { root.overlayLog("shown"); GameOverlay.toggle() } },
        { name: "O15-gone", run: () => root.overlayLog("gone") }
    ]
    // The settings section, then waits for the real hotkey (WM_HOTKEY sent from outside) (--tour-hotkey).
    readonly property bool withHotkey: Qt.application.arguments.indexOf("--tour-hotkey") >= 0
    function hotkeyLog() {
        console.log("tour: hotkey", GameOverlay.hotkey, "| taken", GameOverlay.hotkeyTaken, "| open", GameOverlay.open, "| pad", GameOverlay.padConnected)
        if (GameOverlay.open)
            root.overlayLog("menu")
    }
    readonly property var hotkeySteps: [
        { name: "K1-settings", run: () => { Nav.finishOnboarding(); Nav.navigate("settings") } },
        { name: "K2-scroll", run: () => { root.scrollPage(900); root.hotkeyLog() } },
        { name: "K3-wait", run: () => root.hotkeyLog() },
        { name: "K4-wait", run: () => root.hotkeyLog() },
        { name: "K5-wait", run: () => root.hotkeyLog() },
        { name: "K6-wait", run: () => root.hotkeyLog() },
        { name: "K7-wait", run: () => root.hotkeyLog() },
        { name: "K8-wait", run: () => root.hotkeyLog() }
    ]
    // Open, browse, close — several times, with the menu fully gone in between and not (--tour-overlay-cycle).
    readonly property bool withOverlayCycle: Qt.application.arguments.indexOf("--tour-overlay-cycle") >= 0
    readonly property var overlayCycleSteps: {
        const s = [{ name: "C0-start", run: () => { Nav.finishOnboarding(); const r = Library.libraryRows(); Player.playFrom(r, r[3], "library") } }]
        for (let i = 1; i <= 4; ++i) {
            s.push({ name: "C" + i + "a-open", run: () => { GameOverlay.toggle(); root.overlayLog("cycle " + i + " open") } })
            s.push({ name: "C" + i + "b-browse", run: () => { root.pad(["right", "down", "down", "right", "down", "accept", "down"]); root.overlayLog("cycle " + i + " browse") } })
            s.push({ name: "C" + i + "c-close", run: () => { GameOverlay.toggle(); root.overlayLog("cycle " + i + " close") } })
            if (i % 2 === 0)
                s.push({ name: "C" + i + "d-gone", run: () => root.overlayLog("cycle " + i + " gone") })
        }
        s.push({ name: "C9-end", run: () => root.overlayLog("end") })
        return s
    }
    // Public YouTube Music playlists in the search results, opened and played (--tour-search-playlists).
    readonly property bool withSearchPlaylists: Qt.application.arguments.indexOf("--tour-search-playlists") >= 0
    readonly property var searchPlaylistSteps: [
        { name: "S1-search", run: () => { Nav.finishOnboarding(); Nav.navigate("search", { query: "vixa" }) } },
        { name: "S2-wait", run: () => {} },
        { name: "S3-results", run: () => {
            const p = YouTube.playlistResults
            console.log("tour: playlists", p.length, "|", p.slice(0, 6).map(x => x.title + " (" + x.subtitle + ")").join(" | "))
            root.scrollPage(2780)
        } },
        { name: "S4-open", run: () => {
            const p = YouTube.playlistResults[0]
            root.album = p ? { playlistId: p.id } : null
            if (p) Nav.navigate("playlist", { kind: "ytcollection", playlistId: p.id, title: p.title, subtitle: p.subtitle, cover: p.cover, label: "PLAYLIST" })
        } },
        { name: "S5-wait", run: () => {} },
        { name: "S6-play", run: () => {
            const r = root.album ? Library.collectionRows(root.album.playlistId) : []
            console.log("tour: playlist rows", r.length, r.length ? Library.tracks.get(r[0]).title : "")
            if (r.length) Player.playFrom(r, r[0], "ytc:" + root.album.playlistId)
        } },
        { name: "S7-playing", run: () => console.log("tour: playing", Player.title, "-", Player.artist, "| up next", Player.upNext.length) }
    ]
    readonly property var steps: withSearchPlaylists ? searchPlaylistSteps : withOverlayCycle ? overlayCycleSteps : withHotkey ? hotkeySteps : withOverlay ? overlaySteps : withLiked ? likedSteps : withArtist ? artistSteps : withHome ? homeSteps : withLyrics ? lyricSteps : withPlaylists ? playlistSteps : withAlbums ? albumSteps : withSeek ? seekSteps : withPerf ? perfSteps : withCovers ? coverSteps : withMatch ? matchSteps : withEq ? eqSteps : withBig ? bigSteps : withEngine ? engineSteps : withLogin ? loginSteps : baseSteps
    property double navStart: 0
    property bool measuring: false
    readonly property var bigSteps: [
        { name: "B1-home", run: () => console.log("tour: tracks in the model =", Library.tracks.count) },
        { name: "B2-liked", run: () => { root.navStart = Date.now(); root.measuring = true; Nav.navigate("playlist", { kind: "playlist", index: 2 }); console.log("tour: page build", Date.now() - root.navStart, "ms") } },
        { name: "B3-library", run: () => { root.navStart = Date.now(); root.measuring = true; Nav.navigate("library") } }
    ]

    Connections {
        target: root.target ? root.target.Window.window : null
        function onFrameSwapped() {
            if (root.measuring) {
                root.measuring = false
                console.log("tour: first frame after", Date.now() - root.navStart, "ms")
            }
        }
    }
    readonly property var engineSteps: [
        { name: "E1-download", run: () => { console.log("tour: engineReady before =", YouTube.engineReady); YouTube.downloadEngine() } },
        { name: "E2-wait", run: () => console.log("tour: downloading", Math.round(YouTube.engineProgress * 100) + "%") },
        { name: "E3-wait", run: () => {} },
        { name: "E4-wait", run: () => {} },
        { name: "E5-result", run: () => console.log("tour: engineReady after =", YouTube.engineReady, "version", YouTube.engineVersion) }
    ]
    readonly property var loginSteps: [
        { name: "L1-login-start", run: () => { Nav.navigate("sources"); YouTube.login() } },
        { name: "L2-login-wait", run: () => console.log("tour: loggingIn =", YouTube.loggingIn, "|", YouTube.loginStatus) },
        { name: "L3-wait", run: () => console.log("tour: loggingIn =", YouTube.loggingIn, "|", YouTube.loginStatus) },
        { name: "L4-wait", run: () => console.log("tour: loggingIn =", YouTube.loggingIn, "|", YouTube.loginStatus) },
        { name: "L5-login-cancel", run: () => { YouTube.cancelLogin(); console.log("tour: done, loggingIn =", YouTube.loggingIn) } }
    ]
    readonly property var baseSteps: [
        { name: "00a-welcome",      run: () => Nav.startOnboarding() },
        { name: "00b-layout",       run: () => Nav.onboardingStep = 1 },
        { name: "00c-layout-compact", run: () => { Nav.sidebarCompact = true; Nav.queueOpen = false } },
        { name: "00d-colors",       run: () => { Nav.sidebarCompact = false; Nav.queueOpen = true; Nav.onboardingStep = 2 } },
        { name: "00e-music",        run: () => Nav.onboardingStep = 3 },
        { name: "00f-playback",     run: () => { Player.crossfade = 6; Player.crossfadeStyle = "bass"; Nav.onboardingStep = 4 } },
        { name: "00g-integrations", run: () => { Discord.enabled = true; Nav.onboardingStep = 5 } },
        { name: "00h-done",         run: () => Nav.onboardingStep = 6 },
        { name: "01-home",      run: () => { Discord.enabled = false; Player.crossfade = 0; Nav.finishOnboarding(); Player.play(); Nav.navigate("home") } },
        { name: "02-search",    run: () => Nav.navigate("search") },
        { name: "03-library",   run: () => Nav.navigate("library") },
        { name: "04-albums",    run: () => Nav.navigate("library", { tab: 1 }) },
        { name: "05-playlist",  run: () => Nav.navigate("playlist", { kind: "playlist", index: 1 }) },
        { name: "06-sources",   run: () => Nav.navigate("sources") },
        { name: "07-settings",  run: () => Nav.navigate("settings") },
        { name: "08-compact",   run: () => { Nav.sidebarCompact = true; Nav.queueOpen = false; Nav.navigate("home") } },
        { name: "09-immersive", run: () => { Nav.sidebarCompact = false; Nav.queueOpen = true; Nav.immersive = true } },
        { name: "10-yt-search", run: () => { Nav.immersive = false; Nav.navigate("search", { query: "Daft Punk Get Lucky", filter: 2 }) } },
        { name: "11-yt-results", run: () => {} },
        { name: "12-yt-playing", run: () => { const r = Library.onlineResults(); if (r.length > 0) Player.playFrom(r, r[0]) } },
        { name: "13-yt-home", run: () => Nav.navigate("home") },
        { name: "14-yt-immersive", run: () => { Nav.immersive = true } }
    ]

    Connections {
        target: YouTube
        enabled: root.dir !== ""
        function onNotice(text, icon) { console.log("tour: notice:", text) }
    }
    Connections {
        target: Spotify
        enabled: root.dir !== ""
        function onNotice(text, icon) { console.log("tour: notice:", text) }
        function onMatchesFound(trackId, results, error) {
            root.matchResults = results
            console.log("tour:", results.length, "results", error)
            results.forEach(r => console.log("tour:  ", r.kind, r.current ? "*" : " ", r.durationText, r.artist, "-", r.title))
        }
        function onMatchChanged(trackId) { console.log("tour: matchChanged", trackId) }
    }

    Timer {
        id: timer
        interval: 2200
        running: root.dir !== ""
        repeat: true
        onTriggered: {
            if (root.step >= 0 && root.step < root.steps.length && !root.withPerf && !root.withSeek) {   // screenshots would skew the timings
                const name = root.steps[root.step].name
                // An open dialog lives in the overlay, outside the shell — its content is grabbed instead.
                let source = GameOverlay.open && GameOverlay.view ? GameOverlay.view.parent : root.target
                const overlay = root.target.Overlay.overlay
                if (overlay)
                    for (let i = 0; i < overlay.children.length; ++i) {
                        const popup = overlay.children[i]
                        if (popup.visible && popup.width > 300 && popup.children.length > 0)
                            source = Array.from(popup.children).reduce((a, b) => b.children.length > a.children.length ? b : a)
                    }
                const ok = source.grabToImage(r => console.log("tour: screenshot", name, r.saveToFile(root.dir + "/" + name + ".png")))
                if (!ok) console.log("tour: screenshot rejected", name)
            }
            root.step += 1
            if (root.step < root.steps.length)
                root.steps[root.step].run()
            else if (root.step > root.steps.length)
                Qt.quit()   // one extra tick so the last screenshot has time to save
        }
    }
}
