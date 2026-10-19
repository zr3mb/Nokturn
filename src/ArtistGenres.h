#pragma once

#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QTimer>

// Genres of artists whose tracks have no genre tag (YouTube Music, Spotify): the primary genre the iTunes
// Search API gives the artist. Asked in the background, one artist every few seconds (the API allows
// about 20 requests a minute), and remembered on disk (artist-genres.json).
class ArtistGenres : public QObject
{
    Q_OBJECT

public:
    static ArtistGenres *instance();

    QString genreOf(const QString &artist) const;   // empty = not known (yet)
    // Artists to look up, the most important first; known and already queued ones are skipped.
    void request(const QStringList &artists);
    int revision() const { return m_revision; }

signals:
    void changed();   // new genres came in (batched)

private:
    explicit ArtistGenres(QObject *parent = nullptr);

    struct Entry { QString genre; qint64 checked = 0; };
    static QString keyOf(const QString &artist);
    bool fresh(const QString &key) const;
    void next();
    void load();
    void save() const;

    QHash<QString, Entry> m_known;   // lower-case name -> genre ("" = the API doesn't know the artist)
    QStringList m_queue;
    QSet<QString> m_queued;
    QNetworkAccessManager m_net;
    QTimer m_timer;    // paces the requests
    QTimer m_notify;   // batches changed() and saving
    bool m_busy = false;
    int m_revision = 0;
};
