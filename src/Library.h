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
    Q_PROPERTY(QVariantList playlists READ playlists NOTIFY playlistsChanged)
    Q_PROPERTY(QVariantList sources READ sources NOTIFY sourcesChanged)
    Q_PROPERTY(QVariantList folders READ folders NOTIFY foldersChanged)
    Q_PROPERTY(int localCount READ localCount NOTIFY libraryChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanChanged)
    Q_PROPERTY(qreal scanProgress READ scanProgress NOTIFY scanChanged)
    Q_PROPERTY(QString scanStatus READ scanStatus NOTIFY scanChanged)
    Q_PROPERTY(int genresRevision READ genresRevision NOTIFY genresChanged)   // artist genres came in

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
    int genresRevision() const { return m_genresChanges; }

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
    // "ytartist:<browseId>" (top songs), "ytc:<playlist id>" (album / playlist from an artist page),
    // "genre:<name>", "recs" (recommended for you).
    QString playlistContext(int index) const;
    int playlistIndex(const QString &context) const;   // -1 when the context isn't a playlist
    QString playlistName(int index) const;
    int playlistCount() const { return int(m_playlists.size()); }
    QVariantList rowsForContext(const QString &context);   // current rows (YouTube playlists may still be growing)
    Q_INVOKABLE QVariantList libraryRows() const;           // local + liked on YouTube Music
    Q_INVOKABLE QVariantList onlineResults() const;         // latest YouTube Music search results
    Q_INVOKABLE QVariantList recommendedRows() const;       // "Recommended for you" (Recommendations)
    // Genres from the tags, most tracks first: [{ key, name, count, colorA, colorB, seed, cover, mosaic }].
    Q_INVOKABLE QVariantList genres() const;
    Q_INVOKABLE QVariantList tracksOfGenre(const QString &genre) const;
    Q_INVOKABLE int totalDuration(const QVariantList &rows) const;   // sum of durations (seconds)

    // Playlist covers: the user's own picture, otherwise the source's cover (YouTube Music, Spotify),
    // otherwise a 2×2 collage of four random covers of the playlist's tracks ("mosaic" in the map).
    Q_INVOKABLE bool setPlaylistCover(int index, const QUrl &image);
    Q_INVOKABLE void resetPlaylistCover(int index);
    Q_INVOKABLE void shufflePlaylistCover(int index);   // four other covers for the collage

    void ensurePalette(int row);

signals:
    void libraryChanged();
    void playlistsChanged();
    void foldersChanged();
    void scanChanged();
    void sourcesChanged();
    void genresChanged();
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
    QStringList genresOf(const Track &t) const;
    static QString coverKey(const QVariantMap &playlist);
    void applyPlaylistCover(QVariantMap &playlist) const;
    void refreshPlaylistCover(int index);
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
    mutable QVariantList m_genres;        // genres(), built when asked
    mutable int m_genresRevision = -1;
    mutable int m_genresArtistRevision = -1;
    int m_genresChanges = 0;

    QFutureWatcher<Scanner::Result> m_watcher;
    QNetworkAccessManager m_net;
    QSet<QString> m_paletteDone;
    bool m_rescanPending = false;
    bool m_manualScan = false;
};
