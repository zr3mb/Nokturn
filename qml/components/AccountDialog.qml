import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nokturn

// The Nokturn account: give this device's account a login and a password, sign in to one, set a new password
// with the recovery key, change the password, get a new recovery key. A new key is shown once, at the end.
Popup {
    id: root
    // "create" | "signin" | "recover" | "password" | "newkey" | "key" (showing the recovery key)
    property string mode: "signin"
    property string errorText: ""
    property bool waiting: false
    property string recoveryKey: ""
    property bool keySaved: false

    function show(m) {
        mode = m
        errorText = ""
        waiting = false
        loginField.text = m === "create" || m === "signin" || m === "recover" ? loginField.text : ""
        passwordField.text = ""
        secondField.text = ""
        keyField.text = ""
        open()
        Qt.callLater(() => (mode === "password" || mode === "newkey" ? passwordField : loginField).forceActiveFocus())
    }

    readonly property bool needsLogin: mode === "create" || mode === "signin" || mode === "recover"
    readonly property bool loginOk: /^[A-Za-z0-9][A-Za-z0-9._-]{2,31}$/.test(loginField.text.trim())
    readonly property bool newPasswordOk: secondField.text.length >= 8
    readonly property bool canSubmit: {
        if (waiting)
            return false
        switch (mode) {
        case "create": return loginOk && passwordField.text.length >= 8 && passwordField.text === secondField.text
        case "signin": return loginOk && passwordField.text.length > 0
        case "recover": return loginOk && keyField.text.replace(/[^A-Za-z0-9]/g, "").length === 20 && newPasswordOk
        case "password": return passwordField.text.length > 0 && newPasswordOk
        case "newkey": return passwordField.text.length > 0
        }
        return false
    }

    function submit() {
        if (!canSubmit)
            return
        errorText = ""
        waiting = true
        switch (mode) {
        case "create": Online.createLogin(loginField.text, passwordField.text); break
        case "signin": Online.signIn(loginField.text, passwordField.text); break
        case "recover": Online.recoverAccount(loginField.text, keyField.text, secondField.text); break
        case "password": Online.changePassword(passwordField.text, secondField.text); break
        case "newkey": Online.newRecoveryKey(passwordField.text); break
        }
    }

    function errorFor(code, message) {
        switch (code) {
        case "bad_login": return mode === "signin" ? qsTr("Wrong login or password.")
                                                   : qsTr("A login has 3–32 letters, digits, dots, dashes or underscores.")
        case "bad_password": return qsTr("The password needs at least 8 characters.")
        case "wrong_password": return qsTr("Wrong password.")
        case "login_taken": return qsTr("Someone already has this login — pick another.")
        case "has_login": return qsTr("This account already has a login.")
        case "bad_recovery": return qsTr("Wrong login or recovery key.")
        case "rate_limited": return qsTr("Too many attempts — wait a few minutes.")
        }
        return qsTr("Couldn't reach the server: %1").arg(message)
    }

    Connections {
        target: Online
        enabled: root.opened
        function onAccountFailed(code, message) {
            root.waiting = false
            root.errorText = root.errorFor(code, message)
        }
        function onRecoveryKeyShown(key) {
            root.recoveryKey = key
            root.keySaved = false
        }
        function onAccountDone(action) {
            root.waiting = false
            if (root.recoveryKey !== "" && action !== "signin" && action !== "password")
                root.mode = "key"
            else
                root.close()
        }
    }

    closePolicy: mode === "key" ? Popup.NoAutoClose : Popup.CloseOnEscape | Popup.CloseOnPressOutside
    onClosed: recoveryKey = ""
    anchors.centerIn: Overlay.overlay
    width: Math.min(480, (Overlay.overlay ? Overlay.overlay.width : 520) - 40)
    modal: true
    focus: true
    padding: 26
    background: Rectangle {
        radius: 22
        color: Theme.surfaceHi
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.07)
    }
    Overlay.modal: Rectangle { color: "#88000000" }
    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 200 }
        NumberAnimation { property: "scale"; from: 0.95; to: 1; duration: 320; easing.type: Easing.OutBack }
    }
    exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 160 } }

    contentItem: ColumnLayout {
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Icon { name: root.mode === "key" ? "key" : "person"; size: 22; color: Theme.accent }
            Text {
                Layout.fillWidth: true
                text: ({
                    create: Online.registered ? qsTr("Set a login and a password") : qsTr("Create a Nokturn account"),
                    signin: qsTr("Sign in to Nokturn"),
                    recover: qsTr("Forgot the password"),
                    password: qsTr("Change the password"),
                    newkey: qsTr("New recovery key"),
                    key: qsTr("Your recovery key")
                })[root.mode] || ""
                color: Theme.text
                font.pixelSize: 18
                font.weight: Font.DemiBold
                wrapMode: Text.WordWrap
            }
        }
        Text {
            Layout.fillWidth: true
            text: ({
                create: Online.registered
                        ? qsTr("Everything of this account stays — likes, shared playlists, folders. Then sign in with it on your other devices.")
                        : qsTr("Sign in with it on every device: the same library and playlists everywhere."),
                signin: Online.registered && Online.login === ""
                        ? qsTr("What this device has (likes, shared playlists) moves into that account — nothing is lost.")
                        : qsTr("The same library, playlists and folders as on your other devices."),
                recover: qsTr("Enter the recovery key you saved when you set the password. Your other devices will be signed out."),
                password: qsTr("Your other devices will be signed out; sign in on them with the new password."),
                newkey: qsTr("The old key stops working."),
                key: qsTr("It's the only way back in if you forget the password. Save it somewhere safe (a password manager, a note on paper) — it won't be shown again.")
            })[root.mode] || ""
            color: Theme.textDim
            font.pixelSize: 13
            wrapMode: Text.WordWrap
        }

        InputField {
            id: loginField
            visible: root.needsLogin
            Layout.fillWidth: true
            icon: "person"
            maximumLength: 32
            placeholderText: qsTr("Login")
            onAccepted: passwordField.visible ? passwordField.forceActiveFocus() : keyField.forceActiveFocus()
        }
        InputField {
            id: keyField
            visible: root.mode === "recover"
            Layout.fillWidth: true
            icon: "key"
            maximumLength: 40
            placeholderText: "XXXX-XXXX-XXXX-XXXX-XXXX"
            onAccepted: secondField.forceActiveFocus()
        }
        InputField {
            id: passwordField
            visible: root.mode !== "recover" && root.mode !== "key"
            Layout.fillWidth: true
            icon: "key"
            echoMode: TextInput.Password
            maximumLength: 200
            placeholderText: root.mode === "password" ? qsTr("Current password") : qsTr("Password")
            onAccepted: secondField.visible ? secondField.forceActiveFocus() : root.submit()
        }
        InputField {
            id: secondField
            visible: root.mode === "create" || root.mode === "recover" || root.mode === "password"
            Layout.fillWidth: true
            icon: "key"
            echoMode: TextInput.Password
            maximumLength: 200
            placeholderText: root.mode === "create" ? qsTr("The password again") : qsTr("New password (at least 8 characters)")
            onAccepted: root.submit()
        }

        // the recovery key
        Rectangle {
            visible: root.mode === "key"
            Layout.fillWidth: true
            implicitHeight: 64
            radius: 14
            color: Qt.rgba(1, 1, 1, 0.05)
            border.width: 1
            border.color: Theme.accent
            Text {
                anchors.centerIn: parent
                text: root.recoveryKey
                color: Theme.text
                font.pixelSize: 20
                font.weight: Font.DemiBold
                font.letterSpacing: 1.5
                font.family: "Consolas"
            }
        }
        RowLayout {
            visible: root.mode === "key"
            Layout.fillWidth: true
            Toggle { id: savedToggle; checked: root.keySaved; onToggled: c => root.keySaved = c }
            Text {
                Layout.fillWidth: true
                text: qsTr("I saved the key")
                color: Theme.text
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                MouseArea { anchors.fill: parent; onClicked: root.keySaved = !root.keySaved }
            }
        }

        Text {
            visible: root.errorText !== ""
            Layout.fillWidth: true
            text: root.errorText
            color: Theme.danger
            font.pixelSize: 13
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Text {
                visible: root.mode === "signin" || root.mode === "recover"
                text: root.mode === "signin" ? qsTr("Forgot the password?") : qsTr("Back to signing in")
                color: Theme.accent
                font.pixelSize: 13
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.show(root.mode === "signin" ? "recover" : "signin")
                }
            }
            Item { Layout.fillWidth: true }
            PillButton {
                visible: root.mode === "key"
                kind: "ghost"
                text: qsTr("Copy")
                fontSize: 13
                implicitHeight: 38
                onClicked: {
                    Online.copyText(root.recoveryKey)
                    root.keySaved = true
                }
            }
            PillButton {
                visible: root.mode !== "key"
                kind: "ghost"
                text: qsTr("Cancel")
                fontSize: 13
                implicitHeight: 38
                onClicked: root.close()
            }
            PillButton {
                text: root.mode === "key" ? qsTr("Done")
                      : ({ create: qsTr("Save"), signin: qsTr("Sign in"), recover: qsTr("Set the password"),
                           password: qsTr("Change"), newkey: qsTr("Show a new key") })[root.mode] || ""
                fontSize: 13
                implicitHeight: 38
                enabled: root.mode === "key" ? root.keySaved : root.canSubmit
                onClicked: root.mode === "key" ? root.close() : root.submit()
            }
        }
    }
}
