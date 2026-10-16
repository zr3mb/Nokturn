#pragma once

#include "SpectrumAnalyzer.h"
#include "audio/AudioEngine.h"

#include <QAudioBufferOutput>
#include <QAudioOutput>
#include <QColor>
#include <QElapsedTimer>
#include <QList>
#include <QMediaPlayer>
#include <QObject>
#include <QTimer>
#include <QHash>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <array>

class TrackModel;
struct Track;

// Odtwarzacz: QMediaPlayer (backend FFmpeg) + kolejka + widmo z prawdziwego dźwięku.
// Dwa "decki" (dwa QMediaPlayer) pozwalają miksować utwory: następny jest ładowany zawczasu
// i startuje w trakcie końcówki bieżącego (przejścia w AudioEngine). Ustawienia miksu są
// globalne (crossfade) albo osobne dla playlisty / albumu ("Miks" jak w Spotify).
class PlayerController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Player)
    QML_SINGLETON

    Q_PROPERTY(bool hasTrack READ hasTrack NOTIFY currentChanged)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentChanged)
    Q_PROPERTY(QString title READ title NOTIFY currentChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY currentChanged)
    Q_PROPERTY(QString album READ album NOTIFY currentChanged)
    Q_PROPERTY(QString albumKey READ albumKey NOTIFY currentChanged)
    Q_PROPERTY(QString source READ source NOTIFY currentChanged)
    Q_PROPERTY(QString quality READ quality NOTIFY currentChanged)
    Q_PROPERTY(QString cover READ cover NOTIFY currentChanged)
    Q_PROPERTY(QColor colorA READ colorA NOTIFY colorsChanged)
    Q_PROPERTY(QColor colorB READ colorB NOTIFY colorsChanged)
    Q_PROPERTY(int seed READ seed NOTIFY currentChanged)
    Q_PROPERTY(int duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(bool live READ live NOTIFY currentChanged)
    Q_PROPERTY(bool liked READ liked NOTIFY likedChanged)
    Q_PROPERTY(double position READ position NOTIFY positionChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(bool buffering READ buffering NOTIFY bufferingChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(bool shuffle READ shuffle WRITE setShuffle NOTIFY shuffleChanged)
    Q_PROPERTY(int repeatMode READ repeatMode WRITE setRepeatMode NOTIFY repeatModeChanged)
    Q_PROPERTY(QVariantList upNext READ upNext NOTIFY queueChanged)
    Q_PROPERTY(QList<qreal> spectrum READ spectrum NOTIFY spectrumChanged)
    Q_PROPERTY(qreal level READ level NOTIFY spectrumChanged)

    // ---- miksowanie ----
    Q_PROPERTY(int crossfade READ crossfade WRITE setCrossfade NOTIFY mixChanged)               // s, 0 = wyłączone
    Q_PROPERTY(QString crossfadeStyle READ crossfadeStyle WRITE setCrossfadeStyle NOTIFY mixChanged)
    Q_PROPERTY(bool smartTransitions READ smartTransitions WRITE setSmartTransitions NOTIFY mixChanged)
    Q_PROPERTY(QStringList mixStyles READ mixStyles CONSTANT)
    Q_PROPERTY(int mixRevision READ mixRevision NOTIFY mixChanged)
    Q_PROPERTY(QString queueContext READ queueContext NOTIFY queueChanged)
    Q_PROPERTY(bool mixActive READ mixActive NOTIFY mixChanged)          // bieżąca kolejka będzie miksowana
    Q_PROPERTY(bool transitioning READ transitioning NOTIFY transitionChanged)
    Q_PROPERTY(QString transitionLabel READ transitionLabel NOTIFY transitionChanged)

public:
    explicit PlayerController(QObject *parent = nullptr);
    ~PlayerController() override;

    bool hasTrack() const { return m_index >= 0; }
    int currentIndex() const { return m_index; }
    QString title() const;
    QString artist() const;
    QString album() const;
    QString albumKey() const;
    QString source() const;
    QString quality() const;
    QString cover() const;
    QColor colorA() const;
    QColor colorB() const;
    int seed() const;
    int duration() const;
    bool live() const;
    bool liked() const;

    double position() const;
    bool playing() const { return player().playbackState() == QMediaPlayer::PlayingState; }
    bool buffering() const;
    double volume() const { return m_volume; }
    bool muted() const { return m_muted; }
    bool shuffle() const { return m_shuffle; }
    int repeatMode() const { return m_repeat; }
    QVariantList upNext() const;
    QList<qreal> spectrum() const { return m_spectrum; }
    qreal level() const { return m_level; }

    int crossfade() const { return m_crossfade; }
    void setCrossfade(int seconds);
    QString crossfadeStyle() const { return m_crossfadeStyle; }
    void setCrossfadeStyle(const QString &style);
    bool smartTransitions() const { return m_smart; }
    void setSmartTransitions(bool on);
    QStringList mixStyles() const;
    int mixRevision() const { return m_mixRevision; }
    QString queueContext() const { return m_context; }
    bool mixActive() const { return effectiveMix().enabled; }
    bool transitioning() const { return m_engine.inTransition(); }
    QString transitionLabel() const { return m_transitionLabel; }

    void setVolume(double v);
    void setMuted(bool m);
    void setShuffle(bool s);
    void setRepeatMode(int r);

    // Odtwarza wiersz `row`; kolejką stają się `rows` (np. album, playlista, wyniki wyszukiwania).
    // `context` identyfikuje źródło kolejki ("playlist:…", "album:…") — od niego zależy miks.
    Q_INVOKABLE void playFrom(const QVariantList &rows, int row, const QString &context = {});
    // Odtwarza wiersz `row` z kolejką = cała lokalna biblioteka.
    Q_INVOKABLE void playIndex(int row);
    // Przeskakuje do utworu w bieżącej kolejce (np. kliknięcie w "Następne").
    Q_INVOKABLE void jumpTo(int row);
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void seek(double seconds);
    Q_INVOKABLE void cycleRepeat();
    Q_INVOKABLE void toggleLiked();

    // Miks playlisty / albumu: { enabled, style, length }.
    Q_INVOKABLE QVariantMap mixFor(const QString &context) const;
    Q_INVOKABLE void setMix(const QString &context, bool enabled, const QString &style, int length);
    Q_INVOKABLE QString mixStyleName(const QString &style) const;
    Q_INVOKABLE QString mixStyleDescription(const QString &style) const;

    // Dla integracji (Discord, MPRIS)
    QString currentTrackId() const { return m_currentId; }
    const Track *currentTrack() const { return current(); }

signals:
    void currentChanged();
    void colorsChanged();
    void durationChanged();
    void likedChanged();
    void positionChanged();
    void playingChanged();
    void bufferingChanged();
    void volumeChanged();
    void mutedChanged();
    void shuffleChanged();
    void repeatModeChanged();
    void queueChanged();
    void spectrumChanged();
    void mixChanged();
    void transitionChanged();
    void seeked(double seconds);
    void notice(const QString &text, const QString &icon);

private:
    static constexpr int kBars = 48;

    struct Deck
    {
        QMediaPlayer *player = nullptr;
        QAudioOutput *clock = nullptr;          // wyciszone — wyznacza tempo odtwarzania
        QAudioBufferOutput *buffers = nullptr;
        QString trackId;
        QString resolving;                      // videoId, na którego adres czekamy
    };

    struct MixConfig
    {
        bool enabled = false;
        QString style = QStringLiteral("auto");
        int length = 8;
    };

    QMediaPlayer &player() { return *m_decks[m_active].player; }
    const QMediaPlayer &player() const { return *m_decks[m_active].player; }
    int standby() const { return 1 - m_active; }
    void setupDeck(int d);
    void onDeckMediaStatus(int d, QMediaPlayer::MediaStatus status);
    MixConfig effectiveMix() const;
    void loadMixSettings();
    void saveMixSettings();
    int nextQueuePos() const;
    void preloadNext();
    void cancelPreload();
    bool standbyReady() const;
    void updateMix(double dt);
    void startTransition(AudioEngine::Style style, double seconds, const QString &label);
    void stopTransition();

    const Track *current() const;
    bool canPlay(int row, bool explain);
    void load(const QString &id, bool autoplay, bool flushAudio = true);
    void emitTrackChanged();
    void prefetchNext();
    void advance(bool userInitiated);
    void onModelReset();
    void onMediaStatus(QMediaPlayer::MediaStatus status);
    void ensureTicking();
    void tick();
    void saveState() const;
    void applyVolume();

    TrackModel *m_tracks = nullptr;
    AudioEngine m_engine;               // właściwe wyjście: mikser decków + korektor + QAudioSink
    std::array<Deck, AudioEngine::kDecks> m_decks;
    int m_active = 0;
    SpectrumAnalyzer m_analyzer;

    QString m_currentId;
    int m_index = -1;
    QStringList m_queue;
    int m_queuePos = -1;
    QStringList m_history;
    qint64 m_pendingSeek = -1;
    bool m_autoplayAfterResolve = false;
    QString m_context;

    // miksowanie
    int m_crossfade = 0;
    QString m_crossfadeStyle = QStringLiteral("fade");
    bool m_smart = true;
    QHash<QString, MixConfig> m_mixes;
    int m_mixRevision = 0;
    int m_plannedPos = -1;             // pozycja w kolejce utworu załadowanego na drugi deck
    QString m_plannedId;
    QString m_shuffleNextId;           // wylosowany z wyprzedzeniem następny utwór (prefetch adresu)
    double m_silence = 0;              // jak długo bieżący utwór milczy (s) — inteligentne przejścia
    double m_quiet = 0;                // jak długo bieżący utwór jest wyraźnie wyciszony (s)
    QString m_transitionLabel;

    double m_volume = 0.72;
    bool m_muted = false;
    bool m_shuffle = false;
    int m_repeat = 0; // 0 = brak, 1 = całość, 2 = jeden utwór

    QTimer m_timer;
    QElapsedTimer m_clock;
    QList<qreal> m_spectrum;
    QList<qreal> m_targets;
    std::vector<float> m_played;
    qreal m_level = 0.0;
};
