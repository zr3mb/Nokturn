#pragma once

#include "YtCookies.h"

#include <QDateTime>
#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>

class QProcess;

// Zamienia videoId na bezpośredni adres strumienia audio przy pomocy yt-dlp
// (dba on o szyfrowanie podpisów i zmiany po stronie YouTube — aktualizuje się go osobno).
class StreamResolver : public QObject
{
    Q_OBJECT

public:
    explicit StreamResolver(QObject *parent = nullptr);

    bool ready() const { return !m_enginePath.isEmpty(); }
    QString version() const { return m_version; }
    bool downloading() const { return m_downloading; }
    qreal downloadProgress() const { return m_progress; }

    void detect();
    void download();          // oficjalne wydanie z GitHuba, weryfikowane sumą SHA-256
    void setCookies(const YtCookies &cookies) { m_cookies = cookies; }

    void resolve(const QString &videoId);
    void prefetch(const QString &videoId) { if (ready() && !cachedUrl(videoId).isValid()) resolve(videoId); }
    QUrl cachedUrl(const QString &videoId) const;

signals:
    void engineChanged();
    void resolved(const QString &videoId, const QUrl &url);
    void failed(const QString &videoId, const QString &error);
    void notice(const QString &text, const QString &icon);

private:
    struct Entry { QUrl url; QDateTime expires; };

    QString toolsDir() const;
    QStringList baseArguments() const;
    void run(const QString &videoId, bool withCookies);
    void readVersion();
    void finishDownload(const QByteArray &binary, const QByteArray &sums);
    void report(const QString &text, const QString &icon);

    QString m_enginePath;
    QString m_version;
    YtCookies m_cookies;
    QHash<QString, Entry> m_cache;
    QHash<QString, QProcess *> m_running;
    QNetworkAccessManager m_net;
    bool m_downloading = false;
    bool m_quiet = false;              // trwa ciche uaktualnienie (bez powiadomień)
    qreal m_progress = 0;
};
