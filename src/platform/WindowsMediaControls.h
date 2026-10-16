#pragma once

#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QPointer>
#include <QTimer>

class PlayerController;

// Windows: System Media Transport Controls (SMTC) — Nokturn in the system media flyout
// (next to the volume control, in Quick Settings, on the lock screen) with the title, artist,
// cover and timeline; Play/Pause/Next/Previous/Stop buttons and seeking from the system.
// Hardware media keys go to the active SMTC session, so they work even with the window
// minimized. If SMTC were unavailable, the keys are registered as global
// hotkeys (RegisterHotKey).
class WindowsMediaControls : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    explicit WindowsMediaControls(PlayerController *player);
    ~WindowsMediaControls() override;

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

    // Called (on the GUI thread) from the SMTC event handlers.
    void handleButton(int button);
    void handleSeek(qint64 positionMs);

private:
    struct Impl;

    void tryAttach();
    void updateMetadata();
    void updateStatus();
    void updateTimeline();
    void registerHotkeys(quintptr hwnd);
    void unregisterHotkeys();

    QPointer<PlayerController> m_player;
    Impl *d;
    QTimer m_attachTimer;
    QTimer m_timelineTimer;
    quintptr m_hotkeyWindow = 0;
};
