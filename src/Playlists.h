#pragma once

#include "TrackModel.h"

#include <QHash>
#include <QObject>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <functional>

class QJSEngine;
class QQmlEngine;

// The user's own playlists: created here, kept on this computer (playlists.json), on the YouTube
// Music account, or on the Nokturn server as a shared playlist (OnlineService) that everyone with
// its link can add to. Local playlists can hold any track — local files, YouTube and Spotify
// tracks (their data is kept too, so they stay playable after the search results are gone).
// A YouTube Music playlist only takes YouTube videos: Spotify tracks go in as their YouTube Music
// match, local files are skipped.
//
// QML works with indexes into Library.playlists (as everywhere else); Library shows the local
// playlists next to the others (kind "user", "userId").
class Playlists : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    struct Local
    {
        QString id;
        QString name;
        QString description;
        QStringList trackIds;
        qint64 created = 0;
    };

    static Playlists *instance();
    static Playlists *create(QQmlEngine *, QJSEngine *);

    bool busy() const { return m_busy > 0; }

    // target: "local" | "youtube" | "shared"; privacy (YouTube): "PRIVATE" | "UNLISTED" | "PUBLIC"; rows: tracks to start with.
    Q_INVOKABLE void createPlaylist(const QString &name, const QString &description, const QString &target,
                                    const QString &privacy, const QVariantList &rows = {});
    Q_INVOKABLE void addRows(int index, const QVariantList &rows);
    Q_INVOKABLE void removeRow(int index, int row);
    Q_INVOKABLE void edit(int index, const QString &name, const QString &description);
    Q_INVOKABLE void setPrivacy(int index, const QString &privacy);   // YouTube Music playlists
    Q_INVOKABLE void deletePlaylist(int index);
    Q_INVOKABLE void copy(int index, const QString &target);           // a copy on this computer / on YouTube Music
    Q_INVOKABLE bool canEdit(int index) const;                        // the user's own: local, YouTube Music, shared
    Q_INVOKABLE QVariantList targets() const;                         // for "Add to playlist": { index, name, kind, cover }

    // For Library
    const QList<Local> &local() const { return m_local; }
    QStringList trackIds(const QString &id) const;
    QList<Track> tracks() const;   // kept data of the non-local tracks in local playlists

signals:
    void changed();
    void busyChanged();
    // A new playlist — QML opens it (kind "user" + id, or "ytplaylist" + YouTube playlist id).
    void created(const QString &kind, const QString &id);
    void notice(const QString &text, const QString &icon);

private:
    explicit Playlists(QObject *parent = nullptr);

    QVariantMap entry(int index) const;
    Local *findLocal(const QString &id);
    void keepTrack(const Track &t);
    // The YouTube videos for rows (Spotify tracks via their match); `skipped` counts local files etc.
    void videoTracks(const QVariantList &rows, std::function<void(const QList<Track> &tracks, int skipped)> done);
    void beginBusy();
    void endBusy();
    void load();
    void save() const;

    QList<Local> m_local;
    QHash<QString, Track> m_kept;   // id -> data of YouTube / Spotify tracks in local playlists
    int m_busy = 0;
};
