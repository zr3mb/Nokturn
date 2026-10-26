#pragma once

#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QTcpServer>
#include <QUrl>

// A plain-HTTP proxy on 127.0.0.1 for streams the player should start before they're fully downloaded.
// FFmpeg reads http://127.0.0.1:<port>/<n> (Range requests included) and the proxy fetches the real URL
// through Qt Network — on Android FFmpeg's own HTTPS isn't dependable, Qt's is.
class LocalStreamProxy : public QObject
{
    Q_OBJECT

public:
    static LocalStreamProxy *instance();

    // The local address that serves `upstream`; `upstream` itself when the proxy can't listen.
    QUrl wrap(const QUrl &upstream);

private:
    explicit LocalStreamProxy(QObject *parent = nullptr);
    void serve(class QTcpSocket *socket);

    QTcpServer m_server;
    QNetworkAccessManager m_net;
    QHash<QString, QUrl> m_targets;
    int m_next = 0;
};
