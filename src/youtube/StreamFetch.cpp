#include "StreamFetch.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QUrlQuery>

#include <cstring>
#include <utility>

namespace {

constexpr qint64 kPart = 1024 * 1024;
constexpr int kParallel = 4;
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
        m_data = QByteArray(length, Qt::Uninitialized);
        plan(0);
        startMore();
    } else {
        request({ 0, kPart - 1 });   // the first part also tells the size (Content-Range)
    }
}

StreamFetch::~StreamFetch()
{
    for (const QPointer<QNetworkReply> &reply : std::as_const(m_replies)) {
        if (reply) {
            reply->disconnect(this);
            reply->abort();
            reply->deleteLater();
        }
    }
}

bool StreamFetch::suits(const QUrl &url)
{
    return url.host().endsWith(QLatin1String("googlevideo.com")) && declaredLength(url) <= kMaxSize;
}

void StreamFetch::plan(qint64 from)
{
    for (qint64 at = from; at < m_total; at += kPart)
        m_pending << Part{ at, std::min(at + kPart, m_total) - 1 };
}

void StreamFetch::startMore()
{
    while (m_replies.size() < kParallel && !m_pending.isEmpty())
        request(m_pending.takeFirst());
}

void StreamFetch::fail()
{
    m_done = true;
    const auto replies = std::exchange(m_replies, {});
    for (const QPointer<QNetworkReply> &reply : replies) {
        if (reply) {
            reply->disconnect(this);
            reply->abort();
            reply->deleteLater();
        }
    }
    emit finished(false);
}

void StreamFetch::request(Part part)
{
    QNetworkRequest req(m_url);
    req.setRawHeader("User-Agent", "Mozilla/5.0");
    req.setRawHeader("Range", QByteArray("bytes=") + QByteArray::number(part.from) + '-' + QByteArray::number(part.to));
    req.setTransferTimeout(15000);
    QNetworkReply *reply = m_net->get(req);
    m_replies << reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, part]() mutable {
        reply->deleteLater();
        m_replies.removeIf([reply](const QPointer<QNetworkReply> &r) { return r == reply; });
        if (m_done)
            return;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->error() == QNetworkReply::NoError ? reply->readAll() : QByteArray();
        if (status == 200 && !body.isEmpty()) {   // no ranges: that's the whole file
            m_data = body;
            m_done = true;
            const auto others = std::exchange(m_replies, {});
            for (const QPointer<QNetworkReply> &r : others) {
                if (r) {
                    r->disconnect(this);
                    r->abort();
                    r->deleteLater();
                }
            }
            emit finished(true);
            return;
        }
        if (status == 206 && m_total < 0) {
            // "Content-Range: bytes 0-1048575/7340032" — the size, when the URL didn't say.
            static const QRegularExpression totalRe(QStringLiteral("/(\\d+)$"));
            const auto match = totalRe.match(QString::fromLatin1(reply->rawHeader("Content-Range")));
            const qint64 total = match.hasMatch() ? match.captured(1).toLongLong() : 0;
            if (total <= 0 || total > kMaxSize) {
                fail();
                return;
            }
            m_total = total;
            m_data = QByteArray(total, Qt::Uninitialized);
            plan(std::min(part.to + 1, total));
            part.to = std::min(part.to, total - 1);
        }
        const qint64 expected = m_total > 0 ? std::min(part.to, m_total - 1) - part.from + 1 : -1;
        if (status != 206 || body.size() != expected) {
            // A dropped or short part is asked for again; after a few failures the URL is played as before.
            if (++part.attempts < 3)
                request(part);
            else
                fail();
            return;
        }
        std::memcpy(m_data.data() + part.from, body.constData(), size_t(body.size()));
        m_received += body.size();
        if (m_received >= m_total) {
            m_done = true;
            emit finished(true);
            return;
        }
        startMore();
    });
}
