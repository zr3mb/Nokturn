#pragma once

#include "LibraryScanner.h"
#include "TrackModel.h"

#include <QFutureWatcher>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSet>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

// Music library: local files from the scanned folders + sample
// tracks from sources that aren't connected yet (YouTube Music, radio).
class Library : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(TrackModel *tracks READ tracks CONSTANT)
    Q_PROPERTY(QVariantList albums READ albums NOTIFY libraryChanged)
    Q_PROPERTY(QVariantList recentAlbums READ recentAlbums NOTIFY libraryChanged)
    Q_PROPERTY(QVariantList artists READ artists NOTIFY libraryChanged)
    Q_PROPERTY(QVariantList playlists READ playlists NOTIFY libraryChanged)
    Q_PROPERTY(QVariantList sources READ sources NOTIFY sourcesChanged)
    Q_PROPERTY(QVariantList folders READ folders NOTIFY foldersChanged)
    Q_PROPERTY(int localCount READ localCount NOTIFY libraryChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanChanged)
    Q_PROPERTY(qreal scanProgress READ scanProgress NOTIFY scanChanged)
    Q_PROPERTY(QString scanStatus READ scanStatus NOTIFY scanChanged)

public:
    static Library *instance();
    static Library *create(QQmlEngine *, QJSEngine *);

    TrackModel *tracks() const { return m_tracks; }
    QVariantList albums() const { return m_albums; }
    QVariantList recentAlbums() const { return m_recentAlbums; }
    QVariantList artists() const { return m_artists; }
    QVariantList playlists() const { return m_playlists; }
    QVariantList sources() const { return m_sources; }
    QVariantList folders() const;
    int localCount() const { return m_localCount; }
    bool scanning() const { return m_watcher.isRunning(); }
    qreal scanProgress() const;
    QString scanStatus() const;

    Q_INVOKABLE void addFolder(const QUrl &url);
    Q_INVOKABLE void removeFolder(const QString &path);
    Q_INVOKABLE void rescan(bool manual = true);

    // They return lists of row indices in `tracks`.
    Q_INVOKABLE QVariantList search(const QString &query, const QString &source = {}, int limit = 200) const;
    Q_INVOKABLE QVariantList tracksOfAlbum(const QString &albumKey) const;
    Q_INVOKABLE QVariantList tracksOfArtist(const QString &artist) const;
    Q_INVOKABLE QVariantList likedTracks() const;
    Q_INVOKABLE QVariantList localTracks() const;
    Q_INVOKABLE QVariantList recentlyAdded(int limit = 50) const;
    Q_INVOKABLE QVariantList playlistRows(int index);       // YouTube playlists are loaded in the background
    Q_INVOKABLE bool playlistLoading(int index) const;
    Q_INVOKABLE QVariantList artistSongRows(const QString &browseId) const;   // top songs from YouTube Music
    Q_INVOKABLE QVariantList collectionRows(const QString &playlistId);       // album / playlist opened from an artist

    // Queue contexts: "playlist:<kind>", "yt:<playlist id>", "album:<key>", "artist:<name>", "library",
    // "ytartist:<browseId>" (top songs), "ytc:<playlist id>" (album / playlist from an artist page).
    QString playlistContext(int index) const;
    int playlistIndex(const QString &context) const;   // -1 when the context isn't a playlist
    QString playlistName(int index) const;
    int playlistCount() const { return int(m_playlists.size()); }
    QVariantList rowsForContext(const QString &context);   // current rows (YouTube playlists may still be growing)
    Q_INVOKABLE QVariantList libraryRows() const;           // local + liked on YouTube Music
    Q_INVOKABLE QVariantList onlineResults() const;         // latest YouTube Music search results
    Q_INVOKABLE int totalDuration(const QVariantList &rows) const;   // sum of durations (seconds)

    void ensurePalette(int row);

signals:
    void libraryChanged();
    void foldersChanged();
    void scanChanged();
    void sourcesChanged();
    void notice(const QString &text, const QString &icon);

private:
    explicit Library(QObject *parent = nullptr);

    void loadCache();
    void saveCache() const;
    void saveFolders() const;
    void applyLocalTracks(QList<Track> local);
    void rebuildModel();
    void buildSources();
    void rebuildCollections();
    void rebuildPlaylists();
    QVariantList rowsOfIds(const QStringList &ids) const;
    void onScanFinished();
    QList<Track> demoTracks() const;

    TrackModel *m_tracks;
    QList<Track> m_local;
    QStringList m_folders;
    QSet<QString> m_liked;
    int m_localCount = 0;

    QVariantList m_albums;
    QVariantList m_recentAlbums;
    QVariantList m_artists;
    QVariantList m_playlists;
    QVariantList m_sources;

    QFutureWatcher<Scanner::Result> m_watcher;
    QNetworkAccessManager m_net;
    QSet<QString> m_paletteDone;
    bool m_rescanPending = false;
    bool m_manualScan = false;
};
