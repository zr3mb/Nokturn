#pragma once

#include "Lyrics.h"

#include <QNetworkAccessManager>
#include <QObject>

#include <functional>

// LRCLIB (lrclib.net): a free, open database of lyrics — many with timestamps. A fallback when YouTube
// Music has no lyrics (or only plain text) for a track: an exact lookup by artist, title and duration
// first, then a search where the closest duration (±12 s) wins.
class LrcLib : public QObject
{
    Q_OBJECT

public:
    explicit LrcLib(QObject *parent = nullptr);

    // Empty lyrics = nothing found (or a network error — LRCLIB is only a fallback).
    void fetch(const QString &artist, const QString &title, const QString &album, int durationSec,
               std::function<void(const Lyrics &)> done);

    // "Title (Official Video) [Remastered 2011] feat. X" -> "Title"; the first of several artists.
    static QString cleanTitle(const QString &title);
    static QString firstArtist(const QString &artist);

private:
    void get(const QUrl &url, std::function<void(const QByteArray &body, int status)> done);
    static Lyrics toLyrics(const QJsonObject &record);

    QNetworkAccessManager m_net;
};
