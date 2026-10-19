#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QRect>
#include <QTimer>
#include <QQuickItem>
#include <QWindow>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;
class QLocalServer;
class ShortcutPortal;

// The in-game overlay: an XMB-style menu (qml/overlay) over the game, opened with a global shortcut,
// by holding View + Menu on a pad, or with "nokturn --overlay" (sent to the running app). It covers the
// game's monitor, takes the keyboard while open and gives the focus back when it closes.
// Windows: a low-level keyboard hook (games can switch RegisterHotKey off), XInput, and the focus moved
// by hand; a game in exclusive fullscreen is minimized while the menu is open.
// Linux (Wayland): the shortcut through the desktop portal (KDE, GNOME 48+, Hyprland) — or the command
// bound in the desktop's own shortcut settings; the pad from evdev (/dev/input); the window as a
// layer-shell overlay where the compositor has it (KDE, Hyprland, Sway; built with LayerShellQt),
// otherwise full screen.
class GameOverlay : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool supported READ supported CONSTANT)
    Q_PROPERTY(bool open READ isOpen NOTIFY openChanged)
    Q_PROPERTY(QRect area READ area NOTIFY openChanged)               // the game's monitor (logical pixels)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY settingsChanged)
    Q_PROPERTY(bool padEnabled READ padEnabled WRITE setPadEnabled NOTIFY settingsChanged)
    // How the shortcut works here: "native" (Windows, set in Nokturn), "portal" (the desktop owns it),
    // "command" (no portal: bind toggleCommand in the desktop's settings).
    Q_PROPERTY(QString hotkeyMode READ hotkeyMode NOTIFY settingsChanged)
    Q_PROPERTY(QString hotkey READ hotkey NOTIFY settingsChanged)     // "Ctrl+Shift+M"; "" while the desktop hasn't set one
    Q_PROPERTY(bool hotkeyTaken READ hotkeyTaken NOTIFY settingsChanged)   // another app has the hotkey (Windows)
    Q_PROPERTY(bool canConfigureHotkey READ canConfigureHotkey NOTIFY settingsChanged)   // the desktop's dialog
    Q_PROPERTY(QString toggleCommand READ toggleCommand CONSTANT)     // "nokturn --overlay"
    Q_PROPERTY(bool padConnected READ padConnected NOTIFY padConnectedChanged)
    Q_PROPERTY(QQuickItem *view READ view WRITE setView NOTIFY viewChanged)   // the open menu (DevTour)

public:
    static GameOverlay *instance();
    static GameOverlay *create(QQmlEngine *, QJSEngine *);
    ~GameOverlay() override;

    // "nokturn --overlay": hands the request to a running Nokturn; false when none is running.
    static bool sendToRunning();

    bool supported() const;
    bool isOpen() const { return m_open; }
    QRect area() const { return m_area; }
    bool enabled() const { return m_enabled; }
    void setEnabled(bool on);
    bool padEnabled() const { return m_padEnabled; }
    void setPadEnabled(bool on);
    QString hotkeyMode() const;
    QString hotkey() const;
    bool hotkeyTaken() const { return m_hotkeyTaken; }
    bool canConfigureHotkey() const;
    QString toggleCommand() const;
    bool padConnected() const { return m_padConnected; }
    QQuickItem *view() const { return m_view; }
    void setView(QQuickItem *view);

    Q_INVOKABLE void show();
    Q_INVOKABLE void hide();
    Q_INVOKABLE void toggle();
    // Before the window is first shown: the layer-shell role on Wayland.
    Q_INVOKABLE void prepare(QWindow *window);
    // The overlay window takes the keyboard (the game had it). Called once the window is visible.
    Q_INVOKABLE void activate(QWindow *window);
    // A key and modifiers from a QML key event; false if it can't be a global hotkey (Windows).
    Q_INVOKABLE bool setHotkey(int key, int modifiers);
    Q_INVOKABLE void resetHotkey();
    Q_INVOKABLE void configureHotkey();
    Q_INVOKABLE void copyToClipboard(const QString &text);
    // Sends a pad action as if it came from the pad (DevTour).
    Q_INVOKABLE void simulate(const QString &action) { emit padInput(action); }

signals:
    void openChanged();
    void settingsChanged();
    void padConnectedChanged();
    void viewChanged();
    // "left", "right", "up", "down", "accept", "back", "playPause", "previousTrack", "nextTrack"
    void padInput(const QString &action);

private:
    explicit GameOverlay(QObject *parent = nullptr);
    void registerHotkey();
    void unregisterHotkey();
    void listen();
    void pollPad();
    bool readPad(unsigned &buttons, int &x, int &y);   // the platform's pad, false = none
    void closePad();
    void saveSettings();

    bool m_open = false;
    bool m_enabled = true;
    bool m_padEnabled = true;
    bool m_hotkeyTaken = false;
    bool m_hotkeyRegistered = false;
    bool m_padConnected = false;
    int m_key = 0;          // Qt::Key
    int m_modifiers = 0;    // Qt::KeyboardModifiers
    QRect m_area;
    quintptr m_previous = 0;   // the window that had the focus (the game, Windows)
    QPointer<QQuickItem> m_view;
    QLocalServer *m_server = nullptr;
    ShortcutPortal *m_portal = nullptr;   // Linux
    QString m_activationToken;            // from the portal: lets the window take the focus (Wayland)
    bool m_layerShell = false;

    // Pad
    QTimer m_padTimer;
    int m_padSlot = -1;            // Windows: the XInput user index; -1 = none
    int m_padFd = -1;              // Linux: the evdev device
    unsigned m_evButtons = 0, m_evHat = 0;
    int m_evX = 0, m_evY = 0;      // the left stick, ±32767, up positive
    int m_absMin[2] = { -32768, -32768 }, m_absMax[2] = { 32767, 32767 };
    qint64 m_nextPadScan = 0;      // looking for a pad is slow — not every tick
    unsigned m_lastButtons = 0;
    QString m_held;                // the direction being held (repeats)
    qint64 m_heldNext = 0;
    qint64 m_comboSince = -1;      // View + Menu held since (ms)
    bool m_comboFired = false;
    QElapsedTimer m_clock;
};
