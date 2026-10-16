#pragma once

#include "TrackModel.h"
#include "YtCookies.h"

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

    explicit YtMusicApi(QObject *parent = nullptr);

    void setCookies(const YtCookies &cookies) { m_cookies = cookies; }
    bool authenticated() const { return m_cookies.isLoggedIn(); }

    void call(const QString &endpoint, QJsonObject body, JsonCallback callback, const QUrlQuery &query = {});

    // Pobiera całą listę (z kolejnymi stronami) — np. "VLLM" (polubione) albo "VL<id playlisty>".
    void browseTracks(const QString &browseId, TracksCallback callback, int maxPages = 30);
    void fetchAccount(std::function<void(const Account &, const QString &error)> callback);
    void fetchLibraryPlaylists(std::function<void(const QVariantList &, const QString &error)> callback);
    void search(const QString &query, TracksCallback callback);
    void setLiked(const QString &videoId, bool liked);

    static QList<Track> parseTracks(const QJsonValue &root);
    static QVariantList parsePlaylists(const QJsonValue &root);

private:
    void browsePage(const QString &browseId, const QString &continuation, bool commandStyle,
                    QList<Track> collected, int pagesLeft, TracksCallback callback);

    QNetworkAccessManager m_net;
    YtCookies m_cookies;
    QString m_clientVersion;
};
