#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QList>
#include <QString>
#include <QUrl>

// Encryption for account secrets on disk (the same as for the cookies below) — also used for the
// Spotify token. Windows: DPAPI; Linux: a key from the system password store.
QByteArray protectSecret(const QByteArray &plain);
QByteArray unprotectSecret(const QByteArray &data);

struct YtCookie
{
    QString name;
    QString value;
    QString domain;     // ".youtube.com" (with subdomains) or "music.youtube.com"
    QString path = QStringLiteral("/");
    qint64 expires = 0; // seconds since the epoch, 0 = session cookie
    bool secure = false;
    bool httpOnly = false;
};

// YouTube account cookies. On disk they're encrypted: Windows — DPAPI (only the current user
// can read them), Linux — with a key from the system password store (Secret Service via secret-tool).
// This is full access to the account, so we don't keep them in plain text.
class YtCookies
{
public:
    QList<YtCookie> cookies;

    bool isEmpty() const { return cookies.isEmpty(); }
    bool isLoggedIn() const;
    QString value(const QString &name) const;

    QByteArray headerFor(const QUrl &url) const;
    QByteArray authorization(const QString &origin) const;   // SAPISIDHASH …
    QString toNetscape() const;                               // cookies.txt format for yt-dlp

    static YtCookies fromCdp(const QJsonArray &cdpCookies,    // Storage.getCookies
                             const QString &domain = QStringLiteral("youtube.com"));
    static YtCookies fromHeader(const QString &header);       // "a=b; c=d" pasted manually

    bool save(const QString &file) const;
    static YtCookies load(const QString &file);
};
