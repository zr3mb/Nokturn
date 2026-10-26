#include "LocalStreamProxy.h"

#include <QNetworkReply>
#include <QPointer>
#include <QRegularExpression>
#include <QTcpSocket>

namespace {

constexpr int kMaxTargets = 32;                  // the queue's prefetched tracks; older ones are dropped
constexpr qint64 kChunk = 1024 * 1024;           // asked for upstream one bounded range at a time
constexpr qint64 kAhead = 4 * kChunk;            // how far the download may run ahead of the player

bool debug()
{
    static const bool on = qEnvironmentVariableIsSet("NOKTURN_STREAM_DEBUG");
    return on;
}

// One player connection. FFmpeg asks for "bytes=N-" (open-ended); upstream gets bounded ranges one after
// another, like the desktop's in-memory fetch — the way YouTube serves audio quickly and reliably.
class Transfer : public QObject
{
public:
    Transfer(QNetworkAccessManager *net, QTcpSocket *socket, const QUrl &target, qint64 from, qint64 to, bool ranged)
        : QObject(socket), m_net(net), m_socket(socket), m_target(target), m_pos(from), m_start(from), m_end(to), m_ranged(ranged)
    {
        connect(socket, &QTcpSocket::bytesWritten, this, [this] { if (m_waiting) fetch(); });
        connect(socket, &QTcpSocket::disconnected, this, [this] {
            m_closed = true;
            if (m_reply)
                m_reply->abort();
        });
        fetch();
    }

private:
    void fetch()
    {
        m_waiting = false;
        if (m_closed || m_reply)
            return;
        if (m_socket->bytesToWrite() > kAhead) {   // the player reads at playback speed
            m_waiting = true;
            return;
        }
        qint64 last = m_pos + kChunk - 1;
        if (m_end >= 0)
            last = std::min(last, m_end);
        if (m_total > 0)
            last = std::min(last, m_total - 1);
        QNetworkRequest request(m_target);
        request.setRawHeader("User-Agent", "Mozilla/5.0");
        request.setRawHeader("Range", "bytes=" + QByteArray::number(m_pos) + '-' + QByteArray::number(last));
        request.setTransferTimeout(30'000);
        m_reply = m_net->get(request);
        connect(m_reply, &QNetworkReply::finished, this, [this] { onChunk(); });
    }

    void onChunk()
    {
        QNetworkReply *reply = std::exchange(m_reply, nullptr);
        reply->deleteLater();
        if (m_closed)
            return;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || (status != 206 && status != 200)) {
            if (debug())
                qInfo("stream proxy: upstream %d %s", status, qPrintable(reply->errorString()));
            if (++m_failures < 3 && m_headersSent) {   // a dropped piece is asked for again
                fetch();
                return;
            }
            if (!m_headersSent)
                m_socket->write("HTTP/1.1 " + QByteArray::number(status > 0 ? status : 502)
                                + " Upstream\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
            m_socket->disconnectFromHost();
            return;
        }
        m_failures = 0;
        const QByteArray body = reply->readAll();
        if (m_total < 0) {
            static const QRegularExpression totalRe(QStringLiteral("/(\\d+)$"));
            const auto match = totalRe.match(QString::fromLatin1(reply->rawHeader("Content-Range")));
            m_total = status == 200 ? body.size() : match.hasMatch() ? match.captured(1).toLongLong() : -1;
        }
        if (!m_headersSent) {
            const qint64 last = m_end >= 0 && m_total > 0 ? std::min(m_end, m_total - 1) : m_total - 1;
            QByteArray headers = m_ranged ? "HTTP/1.1 206 Partial Content\r\n" : "HTTP/1.1 200 OK\r\n";
            if (reply->hasRawHeader("Content-Type"))
                headers += "Content-Type: " + reply->rawHeader("Content-Type") + "\r\n";
            if (m_total > 0) {
                headers += "Content-Length: " + QByteArray::number(last - m_start + 1) + "\r\n";
                if (m_ranged)
                    headers += "Content-Range: bytes " + QByteArray::number(m_start) + '-' + QByteArray::number(last)
                               + '/' + QByteArray::number(m_total) + "\r\n";
            }
            headers += "Accept-Ranges: bytes\r\nConnection: close\r\n\r\n";
            m_socket->write(headers);
            m_headersSent = true;
            if (debug())
                qInfo("stream proxy: serving %lld-%lld of %lld", m_start, last, m_total);
        }
        m_socket->write(body);
        m_pos += body.size();
        const qint64 end = m_end >= 0 ? m_end + 1 : m_total;
        if (body.isEmpty() || status == 200 || (end > 0 && m_pos >= end)) {
            m_socket->disconnectFromHost();   // after the queued data is written
            return;
        }
        fetch();
    }

    QNetworkAccessManager *m_net;
    QTcpSocket *m_socket;
    QUrl m_target;
    QNetworkReply *m_reply = nullptr;
    qint64 m_pos;
    qint64 m_start;
    qint64 m_end;          // the player's last wanted byte, -1 = to the end
    qint64 m_total = -1;
    bool m_ranged;
    bool m_headersSent = false;
    bool m_waiting = false;
    bool m_closed = false;
    int m_failures = 0;
};

} // namespace

LocalStreamProxy *LocalStreamProxy::instance()
{
    static LocalStreamProxy *proxy = new LocalStreamProxy;
    return proxy;
}

LocalStreamProxy::LocalStreamProxy(QObject *parent)
    : QObject(parent)
{
    m_server.listen(QHostAddress::LocalHost, 0);
    connect(&m_server, &QTcpServer::newConnection, this, [this] {
        while (QTcpSocket *socket = m_server.nextPendingConnection())
            serve(socket);
    });
}

QUrl LocalStreamProxy::wrap(const QUrl &upstream)
{
    if (!m_server.isListening())
        return upstream;
    const QString key = QString::number(++m_next);
    m_targets.insert(key, upstream);
    m_targets.remove(QString::number(m_next - kMaxTargets));
    return QUrl(QStringLiteral("http://127.0.0.1:%1/%2").arg(m_server.serverPort()).arg(key));
}

void LocalStreamProxy::serve(QTcpSocket *socket)
{
    connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
        if (socket->property("answered").toBool()) {
            socket->readAll();
            return;
        }
        QByteArray head = socket->property("head").toByteArray() + socket->readAll();
        const qsizetype headEnd = head.indexOf("\r\n\r\n");
        if (headEnd < 0) {
            if (head.size() > 16 * 1024)
                socket->abort();
            else
                socket->setProperty("head", head);
            return;
        }
        socket->setProperty("answered", true);
        head.truncate(headEnd);
        const QList<QByteArray> lines = head.split('\n');
        const QList<QByteArray> request = lines.value(0).trimmed().split(' ');
        const QUrl target = m_targets.value(QString::fromLatin1(request.value(1)).mid(1));
        if (request.value(0) != "GET" || !target.isValid()) {
            socket->write("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
            socket->disconnectFromHost();
            return;
        }
        // "Range: bytes=N-" or "bytes=N-M"
        qint64 from = 0, to = -1;
        bool ranged = false;
        static const QRegularExpression rangeRe(QStringLiteral("^range:\\s*bytes=(\\d*)-(\\d*)"),
                                                QRegularExpression::CaseInsensitiveOption);
        for (const QByteArray &line : lines) {
            const auto m = rangeRe.match(QString::fromLatin1(line.trimmed()));
            if (m.hasMatch() && !m.captured(1).isEmpty()) {
                ranged = true;
                from = m.captured(1).toLongLong();
                to = m.captured(2).isEmpty() ? -1 : m.captured(2).toLongLong();
            }
        }
        if (debug())
            qInfo("stream proxy: player asks for %lld-%lld", from, to);
        new Transfer(&m_net, socket, target, from, to, ranged);
    });
}
