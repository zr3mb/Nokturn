#pragma once

#include "TrackModel.h"
#include "YtCookies.h"
#include "lyrics/Lyrics.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QUrlQuery>
#include <QVariantList>

#include <functional>

// Client for YouTube Music's internal API ("InnerTube", the same one music.youtube.com uses).
// Authentication: account cookies + the SAPISIDHASH header — just like the browser does.
class YtMusicApi : public QObject
{
    Q_OBJECT

public:
    struct Account { QString name; QString handle; QString photo; };
    // An artist's YouTube Music page: header, top songs and the carousels (albums, singles,
    // "featured on" playlists, related artists) as QML-ready maps.
    struct Artist {
        QString browseId, name, description, subscribers, photo;
        QList<Track> songs;          // "Top songs"
        QString songsPlaylistId;     // the full song list ("See all")
        QVariantList sections;       // [{ title, kind: "albums"|"playlists"|"artists", items: [...] }]
    };

    using JsonCallback = std::function<void(const QJsonObject &response, const QString &error)>;
    using TracksCallback = std::function<void(const QList<Track> &tracks, const QString &error)>;
    using LyricsCallback = std::function<void(const Lyrics &lyrics, const QString &error)>;

    // The client we send the request as (the website by default; the mobile app
    // gets timed lyrics that the web version doesn't provide).
    struct Client { QString name; QString version; QByteArray id; QByteArray userAgent; };
    static Client webClient();
    static Client androidClient();

    explicit YtMusicApi(QObject *parent = nullptr);

    void setCookies(const YtCookies &cookies) { m_cookies = cookies; }
    bool authenticated() const { return m_cookies.isLoggedIn(); }

    void call(const QString &endpoint, QJsonObject body, JsonCallback callback, const QUrlQuery &query = {});
    void callAs(const Client &client, const QString &endpoint, QJsonObject body, JsonCallback callback,
                const QUrlQuery &query = {});

    // Fetches the whole list (with further pages) — e.g. "VLLM" (liked) or "VL<playlist id>".
    void browseTracks(const QString &browseId, TracksCallback callback, int maxPages = 30);
    void fetchAccount(std::function<void(const Account &, const QString &error)> callback);
    void fetchLibraryPlaylists(std::function<void(const QVariantList &, const QString &error)> callback);
    // filter: "" (everything), "songs" (official audio tracks only) or "videos".
    void search(const QString &query, TracksCallback callback, const QString &filter = {});
    void fetchArtist(const QString &browseId, std::function<void(const Artist &, const QString &error)> callback);
    // Finds the artist's channel by name (exact, case-insensitive match in the search results);
    // an empty id without an error = not found.
    void findArtistId(const QString &name, std::function<void(const QString &browseId, const QString &error)> callback);
    static Artist parseArtist(const QJsonObject &root);
    void setLiked(const QString &videoId, bool liked);

    // The account's own playlists (needs the signed-in account). privacy: "PRIVATE" | "UNLISTED" | "PUBLIC".
    void createPlaylist(const QString &title, const QString &description, const QString &privacy,
                        const QStringList &videoIds, std::function<void(const QString &playlistId, const QString &error)> callback);
    // actions: { action: "ACTION_ADD_VIDEO", addedVideoId } / ACTION_REMOVE_VIDEO / ACTION_SET_PLAYLIST_NAME …
    void editPlaylist(const QString &playlistId, const QJsonArray &actions, std::function<void(const QString &error)> callback);
    void deletePlaylist(const QString &playlistId, std::function<void(const QString &error)> callback);
    // Removing needs each entry's "setVideoId": videoId -> setVideoId for the whole playlist.
    void fetchSetVideoIds(const QString &playlistId, std::function<void(const QHash<QString, QString> &, const QString &error)> callback);
    // Lyrics: the timed version first (mobile client), then plain text.
    // An empty result without an error = YouTube Music has no lyrics for this track.
    void fetchLyrics(const QString &videoId, LyricsCallback callback);

    static QList<Track> parseTracks(const QJsonValue &root);
    static QVariantList parsePlaylists(const QJsonValue &root);
    // How well a search result fits a local file (title, artist, duration ±12 s): up to 4, below
    // kMatchThreshold = a different track. Covers, live, remixes, sped up / slowed versions lose
    // unless the local title is one too.
    static constexpr double kMatchThreshold = 2.0;
    static double matchScore(const Track &local, const Track &result);
    // The best result above the threshold; -1 = none.
    static int bestMatch(const Track &local, const QList<Track> &results);
    static Lyrics parseTimedLyrics(const QJsonValue &root);
    static Lyrics parsePlainLyrics(const QJsonValue &root);

private:
    void browsePage(const QString &browseId, const QString &continuation, bool commandStyle,
                    QList<Track> collected, int pagesLeft, TracksCallback callback);

    QNetworkAccessManager m_net;
    YtCookies m_cookies;
    Client m_web;
};
