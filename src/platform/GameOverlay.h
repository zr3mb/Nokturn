#pragma once

#include <QAbstractNativeEventFilter>
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


// The in-game overlay: an XMB-style menu (qml/overlay) over the game, opened with a global hotkey
// (Ctrl+Shift+M by default) or by holding View + Menu on an Xbox pad. It covers the monitor the game
// is on, takes the keyboard while open and gives the focus back to the game when it closes.
// Windows only: the hotkey (RegisterHotKey), the pad (XInput) and the focus juggling are Win32.
// A game in exclusive fullscreen can't be drawn over — it's minimized while the menu is open.
class GameOverlay : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool supported READ supported CONSTANT)
    Q_PROPERTY(bool open READ isOpen NOTIFY openChanged)
    Q_PROPERTY(QRect area READ area NOTIFY openChanged)               // the game's monitor (logical pixels)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY settingsChanged)
    Q_PROPERTY(bool padEnabled READ padEnabled WRITE setPadEnabled NOTIFY settingsChanged)
    Q_PROPERTY(QString hotkey READ hotkey NOTIFY settingsChanged)     // "Ctrl+Shift+M" (native text)
    Q_PROPERTY(bool hotkeyTaken READ hotkeyTaken NOTIFY settingsChanged)   // another app has the hotkey
    Q_PROPERTY(bool padConnected READ padConnected NOTIFY padConnectedChanged)
    Q_PROPERTY(QQuickItem *view READ view WRITE setView NOTIFY viewChanged)   // the open menu (DevTour)

public:
    static GameOverlay *instance();
    static GameOverlay *create(QQmlEngine *, QJSEngine *);
    ~GameOverlay() override;

    bool supported() const;
    bool isOpen() const { return m_open; }
    QRect area() const { return m_area; }
    bool enabled() const { return m_enabled; }
    void setEnabled(bool on);
    bool padEnabled() const { return m_padEnabled; }
    void setPadEnabled(bool on);
    QString hotkey() const;
    bool hotkeyTaken() const { return m_hotkeyTaken; }
    bool padConnected() const { return m_padConnected; }
    QQuickItem *view() const { return m_view; }
    void setView(QQuickItem *view);

    Q_INVOKABLE void show();
    Q_INVOKABLE void hide();
    Q_INVOKABLE void toggle();
    // The overlay window takes the keyboard (the game had it). Called once the window is visible.
    Q_INVOKABLE void activate(QWindow *window);
    // A key and modifiers from a QML key event; false if it can't be a global hotkey.
    Q_INVOKABLE bool setHotkey(int key, int modifiers);
    Q_INVOKABLE void resetHotkey();
    // Sends a pad action as if it came from the pad (DevTour).
    Q_INVOKABLE void simulate(const QString &action) { emit padInput(action); }

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

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
    void pollPad();
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
    quintptr m_previous = 0;   // the window that had the focus (the game)
    QPointer<QQuickItem> m_view;

    // Pad
    QTimer m_padTimer;
    int m_padSlot = -1;            // the connected pad (XInput user index), -1 = none
    qint64 m_nextPadScan = 0;      // looking for a pad on disconnected slots is slow — not every tick
    unsigned m_lastButtons = 0;
    QString m_held;                // the direction being held (repeats)
    qint64 m_heldNext = 0;
    qint64 m_comboSince = -1;      // View + Menu held since (ms)
    bool m_comboFired = false;
    QElapsedTimer m_clock;
};
