#pragma once

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QUrl>

#include <utility>

class QNetworkAccessManager;
class QNetworkReply;

// Fetches a whole YouTube audio stream into memory with Range requests, several parts at once.
// Played straight from the URL, the connection is read at playback speed, and YouTube's server
// drops such a slow connection after ~30 s — the sound cut out mid-track. A song is a few MB and
// comes in well under a second when its parts are asked for in parallel.
class StreamFetch : public QObject
{
    Q_OBJECT

public:
    StreamFetch(QNetworkAccessManager *net, const QUrl &url, QObject *parent = nullptr);
    ~StreamFetch() override;

    // A googlevideo.com stream that isn't huge (a very long mix keeps streaming from the URL).
    static bool suits(const QUrl &url);

    QByteArray takeData() { return std::exchange(m_data, {}); }

signals:
    void finished(bool ok);

private:
    struct Part { qint64 from; qint64 to; int attempts = 0; };

    void request(Part part);
    void plan(qint64 from);
    void startMore();
    void fail();

    QNetworkAccessManager *m_net;
    QUrl m_url;
    QByteArray m_data;
    qint64 m_total = -1;
    QList<Part> m_pending;                     // parts not asked for yet
    QList<QPointer<QNetworkReply>> m_replies;  // parts on their way
    qint64 m_received = 0;
    bool m_done = false;
};
