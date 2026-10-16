#pragma once

#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QPointer>
#include <QTimer>

class PlayerController;

// Windows: System Media Transport Controls (SMTC) — Nokturn w systemowym okienku multimediów
// (przy regulacji głośności, w Szybkich ustawieniach, na ekranie blokady) z tytułem, wykonawcą,
// okładką i osią czasu; przyciski Odtwórz/Pauza/Dalej/Wstecz/Stop i przewijanie z systemu.
// Sprzętowe klawisze multimedialne trafiają do aktywnej sesji SMTC, więc działają też przy
// zminimalizowanym oknie. Gdyby SMTC było niedostępne, klawisze są rejestrowane jako globalne
// skróty (RegisterHotKey).
class WindowsMediaControls : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    explicit WindowsMediaControls(PlayerController *player);
    ~WindowsMediaControls() override;

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

    // Wywoływane (w wątku GUI) z obsługi zdarzeń SMTC.
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
