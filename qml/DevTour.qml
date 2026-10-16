import QtQuick
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
    readonly property var steps: withCovers ? coverSteps : withMatch ? matchSteps : withEq ? eqSteps : withBig ? bigSteps : withEngine ? engineSteps : withLogin ? loginSteps : baseSteps
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
            if (root.step >= 0 && root.step < root.steps.length) {
                const name = root.steps[root.step].name
                const ok = root.target.grabToImage(r => console.log("tour: screenshot", name, r.saveToFile(root.dir + "/" + name + ".png")))
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
