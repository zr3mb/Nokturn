#pragma once

#include <QAbstractListModel>
#include <QColor>
#include <QHash>
#include <QtQml/qqmlregistration.h>

struct Track
{
    QString id;          // file path (local), "yt:<videoId>" or "demo:…"
    QString path;        // path to the audio file, videoId (YouTube) or empty (demo)
    QString title;
    QString artist;
    QString artistId;    // YouTube Music artist channel (browseId); empty for local files
    QString albumArtist;
    QString album;
    QString genre;
    int year = 0;
    int trackNo = 0;
    int duration = 0;    // seconds, 0 = live stream
    QString source;      // "local" | "youtube" | "radio"
    QString quality;
    QString cover;       // cover image URL (file:///…) or empty
    QColor colorA;
    QColor colorB;
    int seed = 0;        // generative cover variant
    bool liked = false;
    bool demo = false;   // sample track from a source that isn't connected yet
    bool transient = false; // only an online search result — not part of the library
    qint64 size = 0;     // to detect changes in the file
    qint64 mtime = 0;
    qint64 added = 0;    // when the file appeared in the library (ms since the epoch)
};

class TrackModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("TrackModel is provided by Library")
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        PathRole,
        TitleRole,
        ArtistRole,
        AlbumArtistRole,
        AlbumRole,
        AlbumKeyRole,
        GenreRole,
        YearRole,
        TrackNoRole,
        DurationRole,
        DurationTextRole,
        SourceRole,
        QualityRole,
        CoverRole,
        ColorARole,
        ColorBRole,
        SeedRole,
        LikedRole,
        DemoRole,
        RowRole,
        ArtistIdRole,
    };
    Q_ENUM(Role)

    explicit TrackModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return int(m_tracks.size()); }
    int revision() const { return m_revision; }

    void setTracks(QList<Track> tracks);
    void appendTracks(const QList<Track> &tracks);   // only tracks the model doesn't have yet
    const QList<Track> &tracks() const { return m_tracks; }
    const Track &at(int row) const { return m_tracks.at(row); }
    bool isValidRow(int row) const { return row >= 0 && row < m_tracks.size(); }
    int rowOf(const QString &id) const { return m_rowById.value(id, -1); }

    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE void toggleLiked(int row);
    void setColors(int row, const QColor &a, const QColor &b);

    static QString formatDuration(int seconds);
    static QString albumKey(const Track &t);
    static QString primaryArtist(const QString &artist);   // "A, B" / "A feat. B" -> "A"

signals:
    void countChanged();
    void revisionChanged();
    void likedChanged(int row, bool liked);

private:
    QList<Track> m_tracks;
    QHash<QString, int> m_rowById;
    int m_revision = 0;
};
