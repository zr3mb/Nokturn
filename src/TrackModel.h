#pragma once

#include <QAbstractListModel>
#include <QColor>
#include <QHash>
#include <QtQml/qqmlregistration.h>

struct Track
{
    QString id;          // ścieżka pliku (lokalne), "yt:<videoId>" albo "demo:…"
    QString path;        // ścieżka do pliku audio, videoId (YouTube) albo puste (demo)
    QString title;
    QString artist;
    QString albumArtist;
    QString album;
    QString genre;
    int year = 0;
    int trackNo = 0;
    int duration = 0;    // sekundy, 0 = strumień na żywo
    QString source;      // "local" | "youtube" | "radio"
    QString quality;
    QString cover;       // URL obrazka okładki (file:///…) albo puste
    QColor colorA;
    QColor colorB;
    int seed = 0;        // wariant okładki generatywnej
    bool liked = false;
    bool demo = false;   // przykładowy utwór ze źródła, które nie jest jeszcze podłączone
    bool transient = false; // tylko wynik wyszukiwania online — nie należy do biblioteki
    qint64 size = 0;     // do wykrywania zmian w pliku
    qint64 mtime = 0;
    qint64 added = 0;    // kiedy plik pojawił się w bibliotece (ms od epoki)
};

class TrackModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("TrackModel jest udostępniany przez Library")
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
    };
    Q_ENUM(Role)

    explicit TrackModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return int(m_tracks.size()); }
    int revision() const { return m_revision; }

    void setTracks(QList<Track> tracks);
    const QList<Track> &tracks() const { return m_tracks; }
    const Track &at(int row) const { return m_tracks.at(row); }
    bool isValidRow(int row) const { return row >= 0 && row < m_tracks.size(); }
    int rowOf(const QString &id) const { return m_rowById.value(id, -1); }

    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE void toggleLiked(int row);
    void setColors(int row, const QColor &a, const QColor &b);

    static QString formatDuration(int seconds);
    static QString albumKey(const Track &t);

signals:
    void countChanged();
    void revisionChanged();
    void likedChanged(int row, bool liked);

private:
    QList<Track> m_tracks;
    QHash<QString, int> m_rowById;
    int m_revision = 0;
};
