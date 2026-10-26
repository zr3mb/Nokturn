#include "LocalStreamProxy.h"

#include <QNetworkReply>
#include <QPointer>
#include <QTcpSocket>

namespace {

constexpr int kMaxTargets = 32;   // the queue's prefetched tracks; older ones are dropped

QByteArray statusLine(int status)
{
    switch (status) {
    case 200: return "HTTP/1.1 200 OK\r\n";
    case 206: return "HTTP/1.1 206 Partial Content\r\n";
    case 404: return "HTTP/1.1 404 Not Found\r\n";
    case 416: return "HTTP/1.1 416 Range Not Satisfiable\r\n";
    default:  return "HTTP/1.1 " + QByteArray::number(status) + " Upstream\r\n";
    }
}

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
        if (socket->property("answered").toBool())
            return;
        QByteArray head = socket->property("head").toByteArray() + socket->readAll();
        const qsizetype end = head.indexOf("\r\n\r\n");
        if (end < 0) {
            if (head.size() > 16 * 1024)
                socket->abort();
            else
                socket->setProperty("head", head);
            return;
        }
        socket->setProperty("answered", true);
        head.truncate(end);
        const QList<QByteArray> lines = head.split('\n');
        const QList<QByteArray> request = lines.value(0).trimmed().split(' ');
        const QString key = QString::fromLatin1(request.value(1)).mid(1);
        const QUrl target = m_targets.value(key);
        if (request.value(0) != "GET" || !target.isValid()) {
            socket->write(statusLine(404) + "Content-Length: 0\r\nConnection: close\r\n\r\n");
            socket->disconnectFromHost();
            return;
        }

        QNetworkRequest upstream(target);
        upstream.setRawHeader("User-Agent", "Mozilla/5.0");
        for (const QByteArray &line : lines) {
            if (line.toLower().startsWith("range:"))
                upstream.setRawHeader("Range", line.mid(6).trimmed());
        }
        upstream.setTransferTimeout(30'000);
        QNetworkReply *reply = m_net.get(upstream);
        QPointer<QTcpSocket> client(socket);

        connect(reply, &QNetworkReply::metaDataChanged, socket, [reply, client] {
            if (!client || client->property("headersSent").toBool())
                return;
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (status == 0)
                return;
            QByteArray headers = statusLine(status);
            for (const char *name : { "Content-Type", "Content-Length", "Content-Range", "Accept-Ranges" }) {
                if (reply->hasRawHeader(name))
                    headers += QByteArray(name) + ": " + reply->rawHeader(name) + "\r\n";
            }
            headers += "Connection: close\r\n\r\n";
            client->write(headers);
            client->setProperty("headersSent", true);
        });
        connect(reply, &QNetworkReply::readyRead, socket, [reply, client] {
            if (client && client->property("headersSent").toBool())
                client->write(reply->readAll());
        });
        connect(reply, &QNetworkReply::finished, socket, [reply, client] {
            reply->deleteLater();
            if (!client)
                return;
            if (!client->property("headersSent").toBool()) {
                const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                client->write(statusLine(status > 0 ? status : 502) + "Content-Length: 0\r\nConnection: close\r\n\r\n");
            } else {
                client->write(reply->readAll());
            }
            client->disconnectFromHost();
        });
        // FFmpeg drops the connection on a seek or a track change: stop the download too.
        connect(socket, &QTcpSocket::disconnected, reply, [reply] { if (reply->isRunning()) reply->abort(); });
    });
}
