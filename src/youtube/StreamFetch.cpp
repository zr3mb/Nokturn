#include "StreamFetch.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QUrlQuery>

#include <utility>

namespace {

constexpr qint64 kChunk = 4 * 1024 * 1024;
constexpr qint64 kMaxSize = 96 * 1024 * 1024;   // ~1.5 h of audio — longer ones stream from the URL

qint64 declaredLength(const QUrl &url)
{
    return QUrlQuery(url).queryItemValue(QStringLiteral("clen")).toLongLong();   // 0 when missing
}

} // namespace

StreamFetch::StreamFetch(QNetworkAccessManager *net, const QUrl &url, QObject *parent)
    : QObject(parent)
    , m_net(net)
    , m_url(url)
{
    if (const qint64 length = declaredLength(url); length > 0) {
        m_total = length;
        m_data.reserve(length);
    }
    next();
}

StreamFetch::~StreamFetch()
{
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
    }
}

bool StreamFetch::suits(const QUrl &url)
{
    // Only googlevideo.com directly. Audio relayed by the Nokturn server (the Android app) plays
    // from the URL as it arrives: the whole file first would add the relay's transfer to the wait.
    return url.host().endsWith(QLatin1String("googlevideo.com")) && declaredLength(url) <= kMaxSize;
}

void StreamFetch::next()
{
    const qint64 from = m_data.size();
    const qint64 to = (m_total > 0 ? std::min(from + kChunk, m_total) : from + kChunk) - 1;
    QNetworkRequest request(m_url);
    request.setRawHeader("User-Agent", "Mozilla/5.0");
    request.setRawHeader("Range", QByteArray("bytes=") + QByteArray::number(from) + '-' + QByteArray::number(to));
    request.setTransferTimeout(15000);
    m_reply = m_net->get(request);
    connect(m_reply, &QNetworkReply::finished, this, [this, reply = m_reply.data()] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || (status != 206 && status != 200)) {
            // A dropped piece is asked for again; after a few failures the URL is played as before.
            if (++m_attempts < 3)
                next();
            else
                emit finished(false);
            return;
        }
        m_attempts = 0;
        const QByteArray body = reply->readAll();
        if (status == 200) {   // no ranges: that's the whole file
            m_data = body;
            emit finished(!m_data.isEmpty());
            return;
        }
        // "Content-Range: bytes 0-4194303/7340032" — the total, when the URL didn't say.
        static const QRegularExpression totalRe(QStringLiteral("/(\\d+)$"));
        const auto match = totalRe.match(QString::fromLatin1(reply->rawHeader("Content-Range")));
        if (match.hasMatch())
            m_total = match.captured(1).toLongLong();
        m_data += body;
        if (body.isEmpty() || m_total <= 0 || m_data.size() > kMaxSize) {
            emit finished(false);
            return;
        }
        if (m_data.size() >= m_total)
            emit finished(true);
        else
            next();
    });
}
