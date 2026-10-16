#pragma once

#include "BrowserLogin.h"
#include "StreamResolver.h"
#include "TrackModel.h"
#include "YtMusicApi.h"

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

// YouTube Music: logowanie (przeglądarka + ciasteczka), synchronizacja biblioteki
// (polubione, playlisty), wyszukiwanie i silnik strumieni.
class YouTubeService : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(YouTube)
    QML_SINGLETON

    Q_PROPERTY(bool connected READ connected NOTIFY accountChanged)
    Q_PROPERTY(QString accountName READ accountName NOTIFY accountChanged)
    Q_PROPERTY(QString accountHandle READ accountHandle NOTIFY accountChanged)
    Q_PROPERTY(QString accountPhoto READ accountPhoto NOTIFY accountChanged)
    Q_PROPERTY(bool loggingIn READ loggingIn NOTIFY loggingInChanged)
    Q_PROPERTY(QString loginStatus READ loginStatus NOTIFY loggingInChanged)
    Q_PROPERTY(QString browserName READ browserName CONSTANT)
    Q_PROPERTY(bool syncing READ syncing NOTIFY syncingChanged)
    Q_PROPERTY(QString lastSync READ lastSync NOTIFY dataChanged)
    Q_PROPERTY(int likedCount READ likedCount NOTIFY dataChanged)
    Q_PROPERTY(int playlistCount READ playlistCount NOTIFY dataChanged)
    Q_PROPERTY(bool searching READ searching NOTIFY searchingChanged)
    Q_PROPERTY(bool engineReady READ engineReady NOTIFY engineChanged)
    Q_PROPERTY(QString engineVersion READ engineVersion NOTIFY engineChanged)
    Q_PROPERTY(bool engineDownloading READ engineDownloading NOTIFY engineChanged)
    Q_PROPERTY(qreal engineProgress READ engineProgress NOTIFY engineChanged)

public:
    static YouTubeService *instance();
    static YouTubeService *create(QQmlEngine *, QJSEngine *);

    bool connected() const { return m_cookies.isLoggedIn(); }
    QString accountName() const { return m_account.name; }
    QString accountHandle() const { return m_account.handle; }
    QString accountPhoto() const { return m_account.photo; }
    bool loggingIn() const { return m_login.running(); }
    QString loginStatus() const { return m_login.status(); }
    QString browserName() const { return m_browserName; }
    bool syncing() const { return m_pendingSync > 0; }
    QString lastSync() const;
    int likedCount() const { return int(m_liked.size()); }
    int playlistCount() const { return int(m_playlists.size()); }
    bool searching() const { return m_searching; }
    bool engineReady() const { return m_resolver.ready(); }
    QString engineVersion() const { return m_resolver.version(); }
    bool engineDownloading() const { return m_resolver.downloading(); }
    qreal engineProgress() const { return m_resolver.downloadProgress(); }

    Q_INVOKABLE void login();
    Q_INVOKABLE void cancelLogin();
    Q_INVOKABLE void loginWithCookies(const QString &cookieHeader);
    Q_INVOKABLE void logout();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void downloadEngine();
    Q_INVOKABLE void search(const QString &query);

    // Dla Library / Player
    StreamResolver *resolver() { return &m_resolver; }
    QList<Track> tracks() const;
    const QStringList &likedIds() const { return m_liked; }
    QVariantList playlists() const { return m_playlists; }
    QStringList playlistTrackIds(const QString &playlistId) const { return m_playlistTracks.value(playlistId); }
    bool playlistLoading(const QString &playlistId) const { return m_loadingPlaylists.contains(playlistId); }
    void ensurePlaylist(const QString &playlistId);
    const QStringList &searchIds() const { return m_search; }
    bool isLiked(const QString &trackId) const { return m_likedSet.contains(trackId); }
    void setTrackLiked(const QString &trackId, bool liked);
    void setTrackColors(const QString &trackId, const QColor &a, const QColor &b);

signals:
    void accountChanged();
    void loggingInChanged();
    void syncingChanged();
    void dataChanged();
    void searchingChanged();
    void engineChanged();
    void notice(const QString &text, const QString &icon);

private:
    explicit YouTubeService(QObject *parent = nullptr);

    void useCookies(const YtCookies &cookies, bool fresh);
    void syncStep(const QString &error);
    void storeTracks(const QList<Track> &tracks, bool transient);
    void setLikedList(const QStringList &ids);
    void loadCache();
    void saveCache() const;

    YtCookies m_cookies;
    BrowserLogin m_login;
    YtMusicApi m_api;
    StreamResolver m_resolver;
    QString m_browserName;

    YtMusicApi::Account m_account;
    QHash<QString, Track> m_tracks;
    QStringList m_liked;
    QSet<QString> m_likedSet;
    QVariantList m_playlists;
    QHash<QString, QStringList> m_playlistTracks;
    QSet<QString> m_loadingPlaylists;
    QSet<QString> m_refreshedPlaylists;
    QStringList m_search;
    quint64 m_searchSerial = 0;
    bool m_searching = false;
    int m_pendingSync = 0;
    QString m_syncError;
    QDateTime m_lastSync;
};
