import QtQuick
import QtQuick.Layouts

// Section wrapper: slides up and fades in when it appears (with a delay).
Item {
    id: root
    property int delay: 0
    property real distance: 26
    default property alias content: inner.data

    Layout.fillWidth: true
    implicitHeight: inner.childrenRect.height

    Item {
        id: inner
        width: root.width
        height: root.height
        opacity: 0
        transform: Translate { id: shift; y: root.distance }
    }

    SequentialAnimation {
        id: anim
        PauseAnimation { duration: Theme.ms(root.delay) }
        ParallelAnimation {
            NumberAnimation { target: inner; property: "opacity"; to: 1; duration: Theme.ms(600); easing.type: Easing.OutCubic }
            NumberAnimation { target: shift; property: "y"; to: 0; duration: Theme.ms(850); easing.type: Easing.OutQuint }
        }
    }

    Component.onCompleted: anim.start()
}
