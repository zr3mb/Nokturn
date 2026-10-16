#pragma once

#include "TrackModel.h"
#include "YtCookies.h"
#include "lyrics/Lyrics.h"

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QUrlQuery>
#include <QVariantList>

#include <functional>

// Klient wewnętrznego API YouTube Music ("InnerTube", ten sam, którego używa music.youtube.com).
// Uwierzytelnienie: ciasteczka konta + nagłówek SAPISIDHASH — tak jak robi to przeglądarka.
class YtMusicApi : public QObject
{
    Q_OBJECT

public:
    struct Account { QString name; QString handle; QString photo; };

    using JsonCallback = std::function<void(const QJsonObject &response, const QString &error)>;
    using TracksCallback = std::function<void(const QList<Track> &tracks, const QString &error)>;
    using LyricsCallback = std::function<void(const Lyrics &lyrics, const QString &error)>;

    // Klient, w imieniu którego wysyłamy zapytanie (domyślnie strona WWW; aplikacja mobilna
    // dostaje teksty z czasami, których wersja WWW nie udostępnia).
    struct Client { QString name; QString version; QByteArray id; QByteArray userAgent; };
    static Client webClient();
    static Client androidClient();

    explicit YtMusicApi(QObject *parent = nullptr);

    void setCookies(const YtCookies &cookies) { m_cookies = cookies; }
    bool authenticated() const { return m_cookies.isLoggedIn(); }

    void call(const QString &endpoint, QJsonObject body, JsonCallback callback, const QUrlQuery &query = {});
    void callAs(const Client &client, const QString &endpoint, QJsonObject body, JsonCallback callback,
                const QUrlQuery &query = {});

    // Pobiera całą listę (z kolejnymi stronami) — np. "VLLM" (polubione) albo "VL<id playlisty>".
    void browseTracks(const QString &browseId, TracksCallback callback, int maxPages = 30);
    void fetchAccount(std::function<void(const Account &, const QString &error)> callback);
    void fetchLibraryPlaylists(std::function<void(const QVariantList &, const QString &error)> callback);
    void search(const QString &query, TracksCallback callback);
    void setLiked(const QString &videoId, bool liked);
    // Tekst utworu: najpierw wersja z czasami (klient mobilny), potem zwykły tekst.
    // Pusty wynik bez błędu = YouTube Music nie ma tekstu tego utworu.
    void fetchLyrics(const QString &videoId, LyricsCallback callback);

    static QList<Track> parseTracks(const QJsonValue &root);
    static QVariantList parsePlaylists(const QJsonValue &root);
    static Lyrics parseTimedLyrics(const QJsonValue &root);
    static Lyrics parsePlainLyrics(const QJsonValue &root);

private:
    void browsePage(const QString &browseId, const QString &continuation, bool commandStyle,
                    QList<Track> collected, int pagesLeft, TracksCallback callback);

    QNetworkAccessManager m_net;
    YtCookies m_cookies;
    Client m_web;
};
