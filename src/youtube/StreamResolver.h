#pragma once

#include "YtCookies.h"

#include <QDateTime>
#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSet>
#include <QUrl>

#include <functional>

class QProcess;

// Turns a videoId into a direct audio stream URL with yt-dlp
// (it handles signature deciphering and changes on YouTube's side — it's updated separately).
class StreamResolver : public QObject
{
    Q_OBJECT

public:
    explicit StreamResolver(QObject *parent = nullptr);

    bool ready() const { return m_remote || !m_enginePath.isEmpty(); }
    // Tracks are resolved by the Nokturn server instead of a local yt-dlp — always on Android,
    // where apps can't run downloaded programs; NOKTURN_SERVER_STREAMS=1 turns it on elsewhere.
    bool remote() const { return m_remote; }
    QString version() const { return m_version; }
    bool downloading() const { return m_downloading; }
    qreal downloadProgress() const { return m_progress; }

    void detect();
    void download();          // official release from GitHub, verified with SHA-256
    void setCookies(const YtCookies &cookies) { m_cookies = cookies; }
    // Ids with this prefix are resolved by another service (SoundCloud's own API) instead of yt-dlp;
    // they share the cache and the resolved / failed signals.
    using Done = std::function<void(const QUrl &url, const QString &error)>;
    using External = std::function<void(const QString &id, Done done)>;
    void setExternal(const QString &prefix, External resolve);

    void resolve(const QString &videoId);
    // A fresh URL through the account (another YouTube client) — when the anonymous one can't be opened.
    void resolveAgain(const QString &videoId);
    void prefetch(const QString &videoId) { if ((ready() || isExternal(videoId)) && !cachedUrl(videoId).isValid()) resolve(videoId); }
    bool isExternal(const QString &id) const { return !m_externalPrefix.isEmpty() && id.startsWith(m_externalPrefix); }
    QUrl cachedUrl(const QString &videoId) const;
    void forget(const QString &videoId) { m_cache.remove(videoId); }   // e.g. the URL expired mid-playback

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
    void runExternal(const QString &id);
    void readVersion();
    void finishDownload(const QByteArray &binary, const QByteArray &sums);
    void report(const QString &text, const QString &icon);

    QString m_enginePath;
    QString m_version;
    YtCookies m_cookies;
    QHash<QString, Entry> m_cache;
    QHash<QString, QProcess *> m_running;
    QString m_externalPrefix;
    External m_external;
    QSet<QString> m_externalRunning;
    QNetworkAccessManager m_net;
    bool m_remote = false;
    bool m_downloading = false;
    bool m_quiet = false;              // a quiet upgrade is in progress (no notifications)
    qreal m_progress = 0;
};
