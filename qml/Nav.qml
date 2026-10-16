pragma Singleton
import QtQuick

// Globalny stan interfejsu: aktualna strona, historia, panele.
QtObject {
    id: nav

    property string page: "home"
    property var args: ({})
    property var backStack: []
    property var forwardStack: []

    property bool queueOpen: true
    property bool immersive: false
    property bool sidebarCompact: false

    readonly property bool canGoBack: backStack.length > 0
    readonly property bool canGoForward: forwardStack.length > 0

    signal pageRequested(string page, var args)
    signal searchFocusRequested()
    signal toastRequested(string text, string icon)
    signal addFolderRequested()

    function addFolder() {
        addFolderRequested()
    }

    function navigate(p, a) {
        a = a || {}
        if (p === page && JSON.stringify(a) === JSON.stringify(args))
            return
        backStack = backStack.concat([{ page: page, args: args }])
        forwardStack = []
        _show(p, a)
    }

    function back() {
        if (!canGoBack) return
        const e = backStack[backStack.length - 1]
        backStack = backStack.slice(0, -1)
        forwardStack = forwardStack.concat([{ page: page, args: args }])
        _show(e.page, e.args)
    }

    function forward() {
        if (!canGoForward) return
        const e = forwardStack[forwardStack.length - 1]
        forwardStack = forwardStack.slice(0, -1)
        backStack = backStack.concat([{ page: page, args: args }])
        _show(e.page, e.args)
    }

    function openSearch() {
        navigate("search")
        searchFocusRequested()
    }

    function toast(text, icon) {
        toastRequested(text, icon || "check")
    }

    function _show(p, a) {
        page = p
        args = a
        pageRequested(p, a)
    }
}
