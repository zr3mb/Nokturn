#include "Playlists.h"

#include "AppPaths.h"
#include "Library.h"
#include "LibraryScanner.h"
#include "spotify/SpotifyService.h"
#include "youtube/YouTubeService.h"

#include <QDateTime>
#include <QFile>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QUuid>

#include <memory>
#include <optional>
#include <vector>

namespace {

QString storeFile() { return AppPaths::dataDir() + QStringLiteral("/playlists.json"); }

QJsonObject trackToJson(const Track &t)
{
    return { { "id", t.id }, { "path", t.path }, { "source", t.source }, { "title", t.title },
             { "artist", t.artist }, { "artistId", t.artistId }, { "album", t.album },
             { "duration", t.duration }, { "cover", t.cover }, { "quality", t.quality } };
}

Track trackFromJson(const QJsonObject &o)
{
    Track t;
    t.id = o.value("id").toString();
    t.path = o.value("path").toString();
    t.source = o.value("source").toString();
    t.title = o.value("title").toString();
    t.artist = o.value("artist").toString();
    t.artistId = o.value("artistId").toString();
    t.album = o.value("album").toString();
    t.duration = o.value("duration").toInt();
    t.cover = o.value("cover").toString();
    t.quality = o.value("quality").toString();
    const QString key = TrackModel::albumKey(t);
    const auto [a, b] = Scanner::fallbackPalette(key);
    t.colorA = a;
    t.colorB = b;
    t.seed = Scanner::stableSeed(key);
    return t;
}

} // namespace

Playlists *Playlists::instance()
{
    static Playlists *inst = new Playlists;
    return inst;
}

Playlists *Playlists::create(QQmlEngine *, QJSEngine *)
{
    Playlists *p = instance();
    QJSEngine::setObjectOwnership(p, QJSEngine::CppOwnership);
    return p;
}

Playlists::Playlists(QObject *parent)
    : QObject(parent)
{
    load();
}

QVariantMap Playlists::entry(int index) const
{
    return Library::instance()->playlists().value(index).toMap();
}

Playlists::Local *Playlists::findLocal(const QString &id)
{
    for (Local &p : m_local)
        if (p.id == id)
            return &p;
    return nullptr;
}

QStringList Playlists::trackIds(const QString &id) const
{
    for (const Local &p : m_local)
        if (p.id == id)
            return p.trackIds;
    return {};
}

QList<Track> Playlists::tracks() const
{
    return m_kept.values();
}

void Playlists::keepTrack(const Track &t)
{
    if (t.source == QLatin1String("local") || t.id.isEmpty())
        return;
    Track kept = t;
    kept.transient = false;
    kept.liked = false;
    m_kept.insert(t.id, kept);
}

bool Playlists::canEdit(int index) const
{
    const QString kind = entry(index).value("kind").toString();
    return kind == QLatin1String("user") || kind == QLatin1String("ytplaylist");
}

QVariantList Playlists::targets() const
{
    QVariantList out;
    const QVariantList all = Library::instance()->playlists();
    for (int i = 0; i < all.size(); ++i) {
        const QVariantMap p = all.at(i).toMap();
        const QString kind = p.value("kind").toString();
        if (kind == QLatin1String("user") || kind == QLatin1String("ytplaylist"))
            out << QVariantMap { { "index", i }, { "name", p.value("name") }, { "kind", kind }, { "cover", p.value("cover") } };
    }
    return out;
}

// ---- creating and editing ---------------------------------------------------------

void Playlists::createPlaylist(const QString &name, const QString &description, const QString &target,
                               const QString &privacy, const QVariantList &rows)
{
    const QString title = name.trimmed().isEmpty() ? tr("New playlist") : name.trimmed();
    if (target == QLatin1String("youtube")) {
        if (!YouTubeService::instance()->connected()) {
            emit notice(tr("Sign in to YouTube Music first"), QStringLiteral("close"));
            return;
        }
        beginBusy();
        videoTracks(rows, [this, title, description, privacy](const QList<Track> &tracks, int skipped) {
            YouTubeService::instance()->createPlaylist(title, description.trimmed(), privacy, tracks,
                                                       [this, title, skipped](const QString &id, const QString &error) {
                endBusy();
                if (!error.isEmpty()) {
                    emit notice(tr("Couldn't create the playlist: %1").arg(error), QStringLiteral("close"));
                    return;
                }
                emit notice(skipped > 0 ? tr("Created %1 on YouTube Music — local files were skipped").arg(title)
                                        : tr("Created %1 on YouTube Music").arg(title), QStringLiteral("check"));
                emit created(QStringLiteral("ytplaylist"), id);
            });
        });
        return;
    }

    Local p;
    p.id = QUuid::createUuid().toString(QUuid::Id128).left(12);
    p.name = title;
    p.description = description.trimmed();
    p.created = QDateTime::currentMSecsSinceEpoch();
    const TrackModel *model = Library::instance()->tracks();
    for (const QVariant &v : rows) {
        if (!model->isValidRow(v.toInt()) || model->at(v.toInt()).demo)
            continue;
        const Track &t = model->at(v.toInt());
        keepTrack(t);
        if (!p.trackIds.contains(t.id))
            p.trackIds << t.id;
    }
    m_local << p;
    save();
    emit changed();
    emit notice(tr("Created %1").arg(title), QStringLiteral("check"));
    emit created(QStringLiteral("user"), p.id);
}

void Playlists::addRows(int index, const QVariantList &rows)
{
    const QVariantMap p = entry(index);
    const QString kind = p.value("kind").toString();
    const QString name = p.value("name").toString();
    if (kind == QLatin1String("user")) {
        Local *local = findLocal(p.value("userId").toString());
        if (!local)
            return;
        const TrackModel *model = Library::instance()->tracks();
        int added = 0;
        for (const QVariant &v : rows) {
            if (!model->isValidRow(v.toInt()) || model->at(v.toInt()).demo)
                continue;
            const Track &t = model->at(v.toInt());
            if (local->trackIds.contains(t.id))
                continue;
            keepTrack(t);
            local->trackIds << t.id;
            ++added;
        }
        save();
        emit changed();
        emit notice(added > 0 ? tr("Added to %1").arg(name) : tr("Already in %1").arg(name), QStringLiteral("check"));
        return;
    }
    if (kind != QLatin1String("ytplaylist"))
        return;
    beginBusy();
    const QString playlistId = p.value("ytId").toString();
    videoTracks(rows, [this, playlistId, name](const QList<Track> &tracks, int skipped) {
        if (tracks.isEmpty()) {
            endBusy();
            emit notice(tr("Local files can't be added to a YouTube Music playlist"), QStringLiteral("close"));
            return;
        }
        YouTubeService::instance()->addToPlaylist(playlistId, tracks, [this, name, skipped](const QString &error) {
            endBusy();
            if (!error.isEmpty())
                emit notice(tr("Couldn't add to %1: %2").arg(name, error), QStringLiteral("close"));
            else
                emit notice(skipped > 0 ? tr("Added to %1 — local files were skipped").arg(name)
                                        : tr("Added to %1").arg(name), QStringLiteral("check"));
        });
    });
}

void Playlists::removeRow(int index, int row)
{
    const QVariantMap p = entry(index);
    const QString kind = p.value("kind").toString();
    const TrackModel *model = Library::instance()->tracks();
    if (!model->isValidRow(row))
        return;
    const Track &t = model->at(row);
    if (kind == QLatin1String("user")) {
        if (Local *local = findLocal(p.value("userId").toString())) {
            local->trackIds.removeAll(t.id);
            save();
            emit changed();
            emit notice(tr("Removed from %1").arg(p.value("name").toString()), QStringLiteral("check"));
        }
        return;
    }
    if (kind != QLatin1String("ytplaylist") || t.source != QLatin1String("youtube"))
        return;
    beginBusy();
    const QString name = p.value("name").toString();
    YouTubeService::instance()->removeFromPlaylist(p.value("ytId").toString(), t.path, [this, name](const QString &error) {
        endBusy();
        if (!error.isEmpty())
            emit notice(tr("Couldn't remove from %1: %2").arg(name, error), QStringLiteral("close"));
        else
            emit notice(tr("Removed from %1").arg(name), QStringLiteral("check"));
    });
}

void Playlists::edit(int index, const QString &name, const QString &description)
{
    const QVariantMap p = entry(index);
    const QString kind = p.value("kind").toString();
    const QString title = name.trimmed();
    if (title.isEmpty())
        return;
    if (kind == QLatin1String("user")) {
        if (Local *local = findLocal(p.value("userId").toString())) {
            local->name = title;
            local->description = description.trimmed();
            save();
            emit changed();
        }
        return;
    }
    if (kind != QLatin1String("ytplaylist") || title == p.value("name").toString())
        return;
    beginBusy();
    YouTubeService::instance()->renamePlaylist(p.value("ytId").toString(), title, [this](const QString &error) {
        endBusy();
        if (!error.isEmpty())
            emit notice(tr("Couldn't rename the playlist: %1").arg(error), QStringLiteral("close"));
    });
}

void Playlists::setPrivacy(int index, const QString &privacy)
{
    const QVariantMap p = entry(index);
    if (p.value("kind").toString() != QLatin1String("ytplaylist"))
        return;
    beginBusy();
    YouTubeService::instance()->setPlaylistPrivacy(p.value("ytId").toString(), privacy, [this](const QString &error) {
        endBusy();
        if (!error.isEmpty())
            emit notice(tr("Couldn't change who can see the playlist: %1").arg(error), QStringLiteral("close"));
        else
            emit notice(tr("Saved on YouTube Music"), QStringLiteral("check"));
    });
}

void Playlists::deletePlaylist(int index)
{
    const QVariantMap p = entry(index);
    const QString kind = p.value("kind").toString();
    const QString name = p.value("name").toString();
    if (kind == QLatin1String("user")) {
        const QString id = p.value("userId").toString();
        for (int i = 0; i < m_local.size(); ++i)
            if (m_local.at(i).id == id)
                m_local.removeAt(i--);
        save();
        emit changed();
        emit notice(tr("Deleted %1").arg(name), QStringLiteral("check"));
        return;
    }
    if (kind != QLatin1String("ytplaylist"))
        return;
    beginBusy();
    YouTubeService::instance()->deletePlaylist(p.value("ytId").toString(), [this, name](const QString &error) {
        endBusy();
        if (!error.isEmpty())
            emit notice(tr("Couldn't delete %1: %2").arg(name, error), QStringLiteral("close"));
        else
            emit notice(tr("Deleted %1 from YouTube Music").arg(name), QStringLiteral("check"));
    });
}

void Playlists::copy(int index, const QString &target)
{
    const QVariantMap p = entry(index);
    createPlaylist(p.value("name").toString(), p.value("description").toString(), target, QStringLiteral("PRIVATE"),
                   Library::instance()->playlistRows(index));
}

// ---- YouTube videos for rows ---------------------------------------------------------

void Playlists::videoTracks(const QVariantList &rows, std::function<void(const QList<Track> &, int)> done)
{
    struct State
    {
        std::vector<std::optional<Track>> entries;
        int pending = 0;
        int skipped = 0;
        std::function<void(const QList<Track> &, int)> done;
        void finish()
        {
            QList<Track> out;
            for (const auto &s : entries)
                if (s)
                    out << *s;
            done(out, skipped);
        }
    };
    auto state = std::make_shared<State>();
    state->done = std::move(done);
    const TrackModel *model = Library::instance()->tracks();
    for (const QVariant &v : rows) {
        if (!model->isValidRow(v.toInt()))
            continue;
        const Track t = model->at(v.toInt());
        const size_t slot = state->entries.size();
        state->entries.emplace_back();
        if (t.source == QLatin1String("youtube") && !t.demo) {
            state->entries[slot] = t;
        } else if (t.source == QLatin1String("spotify")) {
            ++state->pending;
            SpotifyService::instance()->match(t.id, [state, slot, t](const QString &videoId) {
                if (videoId.isEmpty()) {
                    ++state->skipped;
                } else {
                    Track yt = t;
                    yt.id = QStringLiteral("yt:") + videoId;
                    yt.path = videoId;
                    yt.source = QStringLiteral("youtube");
                    yt.quality = QStringLiteral("YouTube Music");
                    state->entries[slot] = yt;
                }
                if (--state->pending == 0)
                    state->finish();
            });
        } else {
            ++state->skipped;   // local files, radio, SoundCloud
        }
    }
    if (state->pending == 0)
        state->finish();
}

void Playlists::beginBusy()
{
    if (++m_busy == 1)
        emit busyChanged();
}

void Playlists::endBusy()
{
    if (--m_busy == 0)
        emit busyChanged();
}

// ---- storage ------------------------------------------------------------------------

void Playlists::load()
{
    QFile f(storeFile());
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    for (const QJsonValue &v : root.value("playlists").toArray()) {
        const QJsonObject o = v.toObject();
        Local p;
        p.id = o.value("id").toString();
        p.name = o.value("name").toString();
        p.description = o.value("description").toString();
        p.created = qint64(o.value("created").toDouble());
        for (const QJsonValue &t : o.value("tracks").toArray())
            p.trackIds << t.toString();
        if (!p.id.isEmpty())
            m_local << p;
    }
    for (const QJsonValue &v : root.value("trackData").toArray()) {
        const Track t = trackFromJson(v.toObject());
        if (!t.id.isEmpty())
            m_kept.insert(t.id, t);
    }
}

void Playlists::save() const
{
    QJsonArray playlists;
    QSet<QString> used;
    for (const Local &p : m_local) {
        playlists.append(QJsonObject { { "id", p.id }, { "name", p.name }, { "description", p.description },
                                       { "created", double(p.created) }, { "tracks", QJsonArray::fromStringList(p.trackIds) } });
        for (const QString &id : p.trackIds)
            used.insert(id);
    }
    QJsonArray trackData;
    for (const Track &t : m_kept)
        if (used.contains(t.id))
            trackData.append(trackToJson(t));
    QSaveFile f(storeFile());
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(QJsonObject { { "playlists", playlists }, { "trackData", trackData } }).toJson());
        f.commit();
    }
}
