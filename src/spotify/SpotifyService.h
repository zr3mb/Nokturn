#pragma once

#include "TrackModel.h"

#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QTcpServer>
#include <QTimer>
#include <QUrlQuery>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <functional>

class QJSEngine;
class QQmlEngine;

// Spotify: playlists and liked songs synced through the official Web API. Spotify doesn't let other
// apps play its audio, so each track is matched to YouTube Music (title, artist, duration) when it
// plays and streamed from there; matches are remembered.
//
// Sign-in: OAuth 2.0 with PKCE in the browser — no client secret, the redirect lands on a tiny local
// server at http://127.0.0.1:43117/callback. Every user can bring their own Spotify app ("Client ID")
// so the 25-user limit of Spotify's development mode doesn't apply; a build can also ship a default
// one (-DNOKTURN_SPOTIFY_CLIENT_ID=…). The refresh token is stored encrypted, like the YouTube cookies.
class SpotifyService : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Spotify)
    QML_SINGLETON

    Q_PROPERTY(bool connected READ connected NOTIFY accountChanged)
    Q_PROPERTY(QString accountName READ accountName NOTIFY accountChanged)
    Q_PROPERTY(bool loggingIn READ loggingIn NOTIFY loggingInChanged)
    Q_PROPERTY(bool syncing READ syncing NOTIFY syncingChanged)
    Q_PROPERTY(QString clientId READ clientId WRITE setClientId NOTIFY settingsChanged)
    Q_PROPERTY(bool builtInClientId READ builtInClientId CONSTANT)
    Q_PROPERTY(QString redirectUri READ redirectUri CONSTANT)
    Q_PROPERTY(int likedCount READ likedCount NOTIFY dataChanged)
    Q_PROPERTY(int playlistCount READ playlistCount NOTIFY dataChanged)
    Q_PROPERTY(QString lastSync READ lastSync NOTIFY dataChanged)

public:
    static SpotifyService *instance();
    static SpotifyService *create(QQmlEngine *, QJSEngine *);

    bool connected() const { return !m_refreshToken.isEmpty(); }
    QString accountName() const { return m_accountName; }
    bool loggingIn() const { return m_server.isListening(); }
    bool syncing() const { return m_syncing; }
    QString clientId() const { return m_clientId; }
    void setClientId(const QString &id);
    bool builtInClientId() const;
    QString redirectUri() const;
    int likedCount() const { return int(m_liked.size()); }
    int playlistCount() const { return int(m_playlists.size()); }
    QString lastSync() const;

    Q_INVOKABLE void login();
    Q_INVOKABLE void cancelLogin();
    Q_INVOKABLE void logout();
    Q_INVOKABLE void refresh();

    // For Library / Player
    QList<Track> tracks() const;
    QVariantList playlists() const { return m_playlists; }   // [{ id, name, cover, count }]
    const QStringList &likedIds() const { return m_liked; }
    QStringList playlistTrackIds(const QString &playlistId) const { return m_playlistTracks.value(playlistId); }

    // YouTube Music match of a Spotify track ("sp:<id>"): videoId, "" = not found.
    bool matchKnown(const QString &trackId) const { return m_matches.contains(trackId); }
    QString matchedVideoId(const QString &trackId) const { return m_matches.value(trackId); }
    void match(const QString &trackId, std::function<void(const QString &videoId)> callback);

signals:
    void accountChanged();
    void loggingInChanged();
    void syncingChanged();
    void dataChanged();
    void settingsChanged();
    void notice(const QString &text, const QString &icon);

private:
    using JsonCallback = std::function<void(const QJsonObject &json, const QString &error)>;

    explicit SpotifyService(QObject *parent = nullptr);

    // sign-in
    void onCallback();
    void exchangeCode(const QString &code);
    void tokenRequest(const QUrlQuery &form, std::function<void(bool ok, const QString &error)> done);
    void withAccessToken(std::function<void(const QString &error)> next);
    void saveToken() const;
    void loadToken();

    // API
    void get(const QUrl &url, JsonCallback callback, int attempt = 0);
    void getAll(const QUrl &first, QJsonArray collected, std::function<void(const QJsonArray &items, const QString &error)> done);

    // sync
    void syncPlaylists();
    void syncNextPlaylist(QStringList queue);
    void syncLiked();
    void finishSync(const QString &error);
    QStringList storeItems(const QJsonArray &items, bool wrapped);

    void loadCache();
    void saveCache() const;
    void saveMatches() const;

    QNetworkAccessManager m_net;
    QTcpServer m_server;
    QString m_verifier;
    QString m_state;

    QString m_clientId;
    QString m_refreshToken;
    QString m_accessToken;
    QDateTime m_accessExpires;
    QList<std::function<void(const QString &)>> m_waitingForToken;
    bool m_refreshing = false;

    QString m_accountName;
    QHash<QString, Track> m_tracks;                 // "sp:<id>" -> track
    QStringList m_liked;
    QVariantList m_playlists;
    QHash<QString, QStringList> m_playlistTracks;
    QHash<QString, QString> m_snapshots;            // playlist id -> snapshot_id (skip unchanged playlists)
    bool m_syncing = false;
    QString m_syncError;
    QDateTime m_lastSync;

    QHash<QString, QString> m_matches;              // "sp:<id>" -> YouTube videoId ("" = not found)
    QHash<QString, QList<std::function<void(const QString &)>>> m_matching;
    QTimer m_matchesSave;
};
