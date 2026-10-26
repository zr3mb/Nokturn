import QtQuick
import QtWebView
import Nokturn

// Android: signing in to YouTube Music or SoundCloud inside the app. The page runs in the system
// WebView; once the account's cookies are there, they're read from Android's CookieManager and
// handed to the service the same way as pasted ones.
Rectangle {
    id: root
    property string service: "youtube"   // "youtube" | "soundcloud"
    signal closed()

    readonly property var config: service === "soundcloud"
        ? { title: "SoundCloud", start: "https://soundcloud.com/signin", cookieUrl: "https://soundcloud.com" }
        : { title: "YouTube Music", start: "https://accounts.google.com/ServiceLogin?service=youtube&continue=https%3A%2F%2Fmusic.youtube.com%2F",
            cookieUrl: "https://music.youtube.com" }
    property bool done: false

    color: Theme.bg
    Component.onCompleted: YouTube.clearBrowserCookies()   // a fresh session, as with the desktop's clean profile

    function check() {
        if (done)
            return
        const header = YouTube.browserCookies(config.cookieUrl)
        if (service === "soundcloud") {
            const m = header.match(/(?:^|;\s*)oauth_token=([^;]+)/)
            if (m) {
                done = true
                SoundCloud.loginWithToken(m[1])
                root.closed()
            }
        } else if (header.indexOf("SAPISID=") >= 0 && web.url.toString().indexOf("music.youtube.com") >= 0) {
            done = true
            YouTube.loginWithCookies(header)
            root.closed()
        }
    }

    Item {
        id: bar
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 56
        IconButton {
            x: 8
            anchors.verticalCenter: parent.verticalCenter
            icon: "close"
            onClicked: root.closed()
        }
        Text {
            anchors.centerIn: parent
            text: qsTr("Sign in to %1").arg(root.config.title)
            color: Theme.text
            font.pixelSize: 16
            font.weight: Font.DemiBold
        }
        Spinner {
            anchors { right: parent.right; rightMargin: 18; verticalCenter: parent.verticalCenter }
            visible: web.loading
            size: 18
            color: Theme.textDim
        }
    }

    WebView {
        id: web
        anchors { left: parent.left; right: parent.right; top: bar.bottom; bottom: parent.bottom }
        url: root.config.start
        onLoadingChanged: request => { if (request.status === WebView.LoadSucceededStatus) root.check() }
    }

    // Single-page sign-in flows don't always finish with a page load.
    Timer {
        interval: 1500
        repeat: true
        running: !root.done
        onTriggered: root.check()
    }
}
