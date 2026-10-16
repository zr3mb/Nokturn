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
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class TrackModel;
struct Track;

// Odtwarzacz: QMediaPlayer (backend FFmpeg) + kolejka + widmo z prawdziwego dźwięku.
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
    bool playing() const { return m_player.playbackState() == QMediaPlayer::PlayingState; }
    bool buffering() const;
    double volume() const { return m_volume; }
    bool muted() const { return m_muted; }
    bool shuffle() const { return m_shuffle; }
    int repeatMode() const { return m_repeat; }
    QVariantList upNext() const;
    QList<qreal> spectrum() const { return m_spectrum; }
    qreal level() const { return m_level; }

    void setVolume(double v);
    void setMuted(bool m);
    void setShuffle(bool s);
    void setRepeatMode(int r);

    // Odtwarza wiersz `row`; kolejką stają się `rows` (np. album, playlista, wyniki wyszukiwania).
    Q_INVOKABLE void playFrom(const QVariantList &rows, int row);
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
    void notice(const QString &text, const QString &icon);

private:
    static constexpr int kBars = 48;

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
    QMediaPlayer m_player;
    QAudioOutput m_output;              // wyciszone — służy odtwarzaczowi tylko za zegar
    AudioEngine m_engine;               // właściwe wyjście: korektor + QAudioSink
    QAudioBufferOutput *m_bufferOutput = nullptr;
    SpectrumAnalyzer m_analyzer;

    QString m_currentId;
    int m_index = -1;
    QStringList m_queue;
    int m_queuePos = -1;
    QStringList m_history;
    qint64 m_pendingSeek = -1;
    QString m_resolvingId;            // videoId, dla którego czekamy na adres strumienia
    bool m_autoplayAfterResolve = false;

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
