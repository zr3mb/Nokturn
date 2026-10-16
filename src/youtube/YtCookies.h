#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QList>
#include <QString>
#include <QUrl>

struct YtCookie
{
    QString name;
    QString value;
    QString domain;     // ".youtube.com" (z poddomenami) albo "music.youtube.com"
    QString path = QStringLiteral("/");
    qint64 expires = 0; // sekundy od epoki, 0 = ciasteczko sesyjne
    bool secure = false;
    bool httpOnly = false;
};

// Ciasteczka konta YouTube. Na dysku są szyfrowane: Windows — DPAPI (tylko bieżący użytkownik
// może je odczytać), Linux — kluczem z systemowego magazynu haseł (Secret Service przez secret-tool).
// To pełny dostęp do konta, więc nie trzymamy ich jawnym tekstem.
class YtCookies
{
public:
    QList<YtCookie> cookies;

    bool isEmpty() const { return cookies.isEmpty(); }
    bool isLoggedIn() const;
    QString value(const QString &name) const;

    QByteArray headerFor(const QUrl &url) const;
    QByteArray authorization(const QString &origin) const;   // SAPISIDHASH …
    QString toNetscape() const;                               // format cookies.txt dla yt-dlp

    static YtCookies fromCdp(const QJsonArray &cdpCookies);   // Storage.getCookies
    static YtCookies fromHeader(const QString &header);       // "a=b; c=d" wklejone ręcznie

    bool save(const QString &file) const;
    static YtCookies load(const QString &file);
};
