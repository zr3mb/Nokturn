#pragma once

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QUrl>

#include <utility>

class QNetworkAccessManager;
class QNetworkReply;

// Fetches a whole YouTube audio stream into memory, a few MB at a time with a Range header.
// Played straight from the URL, the connection is read at playback speed, and YouTube's server
// drops such a slow connection after ~30 s — the sound cut out mid-track. A song is a few MB and
// comes in well under a second when asked for in ranges.
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
    void next();

    QNetworkAccessManager *m_net;
    QUrl m_url;
    QByteArray m_data;
    qint64 m_total = -1;
    int m_attempts = 0;
    QPointer<QNetworkReply> m_reply;
};
