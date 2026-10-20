#include "Library.h"

#include "ListeningHistory.h"

#include "AppPaths.h"
#include "ArtistGenres.h"
#include "Language.h"
#include "Playlists.h"
#include "spotify/SpotifyService.h"
#include "youtube/YouTubeService.h"

#include <QCollator>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QPainter>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScopeGuard>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

namespace {

QString cacheFile() { return AppPaths::dataDir() + QStringLiteral("/library.json"); }

QVariantList rowsWhere(const TrackModel *model, auto &&predicate)
{
    QVariantList out;
    for (int i = 0; i < model->count(); ++i)
        if (predicate(model->at(i)))
            out << i;
    return out;
}

// "Pop; Rock", "Hip-Hop/Rap" -> one genre each
QStringList splitGenres(const QString &tag)
{
    static const QRegularExpression separators(QStringLiteral("\\s*[;/,|\\\\]\\s*"));
    QStringList out;
    for (const QString &g : tag.split(separators, Qt::SkipEmptyParts))
        if (const QString name = g.trimmed(); !name.isEmpty())
            out << name;
    return out;
}

// "Hip Hop", "hip-hop", "HipHop" -> "hiphop"; empty for tags that say nothing about the music.
QString genreKey(const QString &genre)
{
    QString key = genre.toLower();
    static const QRegularExpression other(QStringLiteral("[^\\p{L}\\p{N}&]+"));
    key.remove(other);
    static const QSet<QString> junk = { "other", "others", "unknown", "genre", "none", "misc", "various",
                                        "inne", "nieznany", "brak" };   // i18n-ignore
    static const QRegularExpression number(QStringLiteral("^\\d+$"));   // an unresolved ID3v1 code like "(17)"
    if (junk.contains(key) || number.match(key).hasMatch())
        return {};
    return key;
}

// The genre names the iTunes catalog gives artists (ArtistGenres): "Hip-Hop/Rap" is one genre.
QString artistGenreName(const QString &name)
{
    if (name.compare(QLatin1String("Singer/Songwriter"), Qt::CaseInsensitive) == 0)
        return name;
    return name.section(QLatin1Char('/'), 0, 0).trimmed();
}

// Common genre names in the interface language (both tags and the iTunes catalog use English ones).
QString genreDisplayName(const QString &name)
{
    static const QHash<QString, const char *> names = {
        { "alternative", QT_TR_NOOP("Alternative") }, { "electronic", QT_TR_NOOP("Electronic") },
        { "classical", QT_TR_NOOP("Classical") }, { "soundtrack", QT_TR_NOOP("Soundtrack") },
        { "singersongwriter", QT_TR_NOOP("Singer/Songwriter") }, { "childrensmusic", QT_TR_NOOP("Children's Music") },
        { "christian", QT_TR_NOOP("Christian") }, { "world", QT_TR_NOOP("World") },
        { "instrumental", QT_TR_NOOP("Instrumental") }, { "holiday", QT_TR_NOOP("Holiday") },
        { "vocal", QT_TR_NOOP("Vocal") }, { "comedy", QT_TR_NOOP("Comedy") },
    };
    QString key = name.toLower();
    static const QRegularExpression other(QStringLiteral("[^\\p{L}\\p{N}]+"));
    key.remove(other);
    const auto it = names.constFind(key);
    return it != names.cend() ? QCoreApplication::translate("Library", it.value()) : name;
}

} // namespace

Library *Library::instance()
{
    static Library *inst = new Library;
    return inst;
}

Library *Library::create(QQmlEngine *, QJSEngine *)
{
    Library *lib = instance();
    QJSEngine::setObjectOwnership(lib, QJSEngine::CppOwnership);
    return lib;
}

Library::Library(QObject *parent)
    : QObject(parent)
    , m_tracks(new TrackModel(this))
{
    buildSources();

    auto settings = AppPaths::settings();
    if (settings->contains("library/folders")) {
        m_folders = settings->value("library/folders").toStringList();
    } else {
        // First run: start with the system Music folder.
        const QString music = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
        if (!music.isEmpty() && QDir(music).exists())
            m_folders << QDir::cleanPath(music);
        saveFolders();
    }
    const QStringList liked = settings->value("library/liked").toStringList();
    m_liked = QSet<QString>(liked.cbegin(), liked.cend());

    connect(m_tracks, &TrackModel::likedChanged, this, [this](int row, bool liked) {
        const Track &track = m_tracks->at(row);
        const QString id = track.id;
        if (track.source == QLatin1String("youtube") && !track.demo) {
            YouTubeService::instance()->setTrackLiked(id, liked);   // the heart also goes to the account
            return;
        }
        if (liked)
            m_liked.insert(id);
        else
            m_liked.remove(id);
        if (!m_tracks->at(row).demo) {
            for (Track &t : m_local)
                if (t.id == id)
                    t.liked = liked;
        }
        AppPaths::settings()->setValue("library/liked", QStringList(m_liked.cbegin(), m_liked.cend()));
    });

    connect(&m_watcher, &QFutureWatcher<Scanner::Result>::progressValueChanged, this, &Library::scanChanged);
    connect(&m_watcher, &QFutureWatcher<Scanner::Result>::started, this, &Library::scanChanged);
    connect(&m_watcher, &QFutureWatcher<Scanner::Result>::finished, this, &Library::onScanFinished);

    connect(YouTubeService::instance(), &YouTubeService::dataChanged, this, &Library::rebuildModel);
    connect(ArtistGenres::instance(), &ArtistGenres::changed, this, [this] {
        ++m_genresChanges;
        emit genresChanged();
    });
    // The artists' genres are looked up early, so the search page has them by the time it's opened.
    QTimer::singleShot(20'000, this, [this] { genres(); });
    // Local playlist edits only change the playlist list — their tracks are already in the model.
    connect(Playlists::instance(), &Playlists::changed, this, [this] {
        rebuildPlaylists();
        emit playlistsChanged();
    });
    connect(YouTubeService::instance(), &YouTubeService::tracksAdded, m_tracks, &TrackModel::appendTracks);
    connect(YouTubeService::instance(), &YouTubeService::accountChanged, this, &Library::rebuildModel);
    connect(SpotifyService::instance(), &SpotifyService::dataChanged, this, &Library::rebuildModel);
    connect(SpotifyService::instance(), &SpotifyService::accountChanged, this, &Library::rebuildModel);
    // Source and playlist names, demo tracks — everything built here follows the interface language.
    connect(Language::instance(), &Language::changed, this, [this] {
        buildSources();
        emit sourcesChanged();
        applyLocalTracks(m_local);   // relabels "Unknown artist" and rebuilds the model
    });

    loadCache();
    // After startup pull in changes from disk (incremental scan — unchanged files come from the cache).
    QTimer::singleShot(600, this, [this] { rescan(false); });
}

void Library::buildSources()
{
    m_sources = {
        QVariantMap { { "id", "local" }, { "name", tr("Local files") },
                      { "description", tr("Music folders on this computer — FLAC, ALAC, MP3, OGG, OPUS, WAV.") },
                      { "icon", "folder" }, { "status", "connected" }, { "color", QColor("#9fbfa8") } },
        QVariantMap { { "id", "youtube" }, { "name", "YouTube Music" },
                      { "description", tr("Liked songs and playlists from your account — you sign in as usual in the browser.") },
                      { "icon", "playCircle" }, { "status", "disconnected" }, { "color", QColor("#cf9d9d") } },
        QVariantMap { { "id", "spotify" }, { "name", "Spotify" },
                      { "description", tr("Your Spotify playlists and liked songs — played from YouTube Music.") },
                      { "icon", "music" }, { "status", "disconnected" }, { "color", QColor("#8fc79a") } },
        QVariantMap { { "id", "radio" }, { "name", tr("Internet radio") },
                      { "description", tr("Icecast / SHOUTcast streaming stations and saved favorites.") },
                      { "icon", "radio" }, { "status", "available" }, { "color", QColor("#cdb68e") } },
        QVariantMap { { "id", "stream" }, { "name", tr("Custom stream") },
                      { "description", tr("Any URL of an audio file or an M3U / PLS playlist.") },
                      { "icon", "link" }, { "status", "available" }, { "color", QColor("#93a8c3") } },
    };
}

// ---- folders and scanning ------------------------------------------------------

QVariantList Library::folders() const
{
    QVariantList out;
    for (const QString &folder : m_folders) {
        int count = 0;
        for (const Track &t : m_local)
            count += Scanner::isUnder(t.path, folder);
        out << QVariantMap { { "path", QDir::toNativeSeparators(folder) }, { "count", count } };
    }
    return out;
}

qreal Library::scanProgress() const
{
    const int max = m_watcher.progressMaximum();
    return max > 0 ? qreal(m_watcher.progressValue()) / max : 0.0;
}

QString Library::scanStatus() const
{
    if (!scanning())
        return {};
    const int max = m_watcher.progressMaximum();
    if (max <= 0)
        return tr("Looking for files…");
    return tr("Scanning… %1 / %2").arg(m_watcher.progressValue()).arg(max);
}

void Library::addFolder(const QUrl &url)
{
    const QString path = QDir::cleanPath(url.isLocalFile() ? url.toLocalFile() : url.toString());
    if (path.isEmpty() || !QDir(path).exists())
        return;
    for (const QString &f : std::as_const(m_folders)) {
        if (Scanner::isUnder(path, f)) {
            emit notice(tr("This folder is already in the library"), QStringLiteral("folder"));
            return;
        }
    }
    // A new folder may contain existing subfolders — remove them so nothing is duplicated.
    m_folders.erase(std::remove_if(m_folders.begin(), m_folders.end(),
                                   [&](const QString &f) { return Scanner::isUnder(f, path); }),
                    m_folders.end());
    m_folders << path;
    saveFolders();
    emit foldersChanged();
    rescan(true);
}

void Library::removeFolder(const QString &nativePath)
{
    const QString path = QDir::cleanPath(QDir::fromNativeSeparators(nativePath));
    if (!m_folders.removeOne(path))
        return;
    saveFolders();

    QList<Track> remaining;
    for (const Track &t : std::as_const(m_local))
        if (!Scanner::isUnder(t.path, path))
            remaining << t;
    applyLocalTracks(remaining);
    saveCache();
    emit foldersChanged();
    emit notice(tr("Removed the folder from the library"), QStringLiteral("folder"));
}

void Library::rescan(bool manual)
{
    m_manualScan = m_manualScan || manual;
    if (m_watcher.isRunning()) {
        m_rescanPending = true;
        return;
    }

    Scanner::Input input;
    input.folders = m_folders;
    input.coverDir = AppPaths::coverDir();
    for (const Track &t : std::as_const(m_local))
        input.cache.insert(t.path, t);

    m_watcher.setFuture(QtConcurrent::run(&Scanner::scan, input));
    emit scanChanged();
}

void Library::onScanFinished()
{
    const QFuture<Scanner::Result> future = m_watcher.future();
    if (future.isValid() && future.resultCount() > 0) {
        const QList<Track> scanned = future.result().tracks;

        QSet<QString> before;
        for (const Track &t : std::as_const(m_local))
            before.insert(t.id);
        int added = 0;
        for (const Track &t : scanned)
            added += !before.contains(t.id);
        const int removed = int(before.size()) - (int(scanned.size()) - added);

        applyLocalTracks(scanned);
        saveCache();
        emit foldersChanged();

        if (m_manualScan || added > 0 || removed > 0) {
            QString text = added > 0
                ? tr("Added to the library: %1").arg(added)
                : tr("The library is up to date");
            if (removed > 0)
                text += QStringLiteral(" · ") + tr("removed: %1").arg(removed);
            emit notice(text, QStringLiteral("check"));
        }
    }
    m_manualScan = false;
    emit scanChanged();

    if (m_rescanPending) {
        m_rescanPending = false;
        rescan();
    }
}

// ---- cache ------------------------------------------------------------------

void Library::loadCache()
{
    QList<Track> local;
    QFile file(cacheFile());
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonArray arr = QJsonDocument::fromJson(file.readAll()).array();
        local.reserve(arr.size());
        for (const QJsonValue &v : arr) {
            Track t = Scanner::fromJson(v.toObject());
            if (!t.path.isEmpty())
                local << t;
        }
    }
    applyLocalTracks(local);
}

void Library::saveCache() const
{
    QJsonArray arr;
    for (const Track &t : m_local)
        arr.append(Scanner::toJson(t));
    QSaveFile file(cacheFile());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
        file.commit();
    }
}

void Library::saveFolders() const
{
    AppPaths::settings()->setValue("library/folders", m_folders);
}

// ---- building collections ---------------------------------------------------------

void Library::applyLocalTracks(QList<Track> local)
{
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(local.begin(), local.end(), [&](const Track &a, const Track &b) {
        const QString aa = a.albumArtist.isEmpty() ? a.artist : a.albumArtist;
        const QString ba = b.albumArtist.isEmpty() ? b.artist : b.albumArtist;
        if (int c = collator.compare(aa, ba)) return c < 0;
        if (int c = collator.compare(a.album, b.album)) return c < 0;
        if (a.trackNo != b.trackNo) return a.trackNo < b.trackNo;
        return collator.compare(a.title, b.title) < 0;
    });
    for (Track &t : local)
        t.liked = m_liked.contains(t.id);
    for (Track &t : local)
        if (Scanner::isUnknownArtist(t.artist))
            t.artist = Scanner::unknownArtist();

    // The tracks of an album repeat its artist, album, genre, format and cover — they share one copy of
    // each text instead of keeping their own (thousands of tracks add up).
    QSet<QString> texts;
    const auto share = [&texts](QString &s) {
        if (s.isEmpty())
            return;
        if (const auto it = texts.constFind(s); it != texts.cend())
            s = *it;
        else
            texts.insert(s);
    };
    for (Track &t : local) {
        share(t.artist);
        share(t.albumArtist);
        share(t.album);
        share(t.genre);
        share(t.quality);
        share(t.cover);
    }

    m_local = local;
    m_localCount = int(local.size());
    rebuildModel();
}

void Library::rebuildModel()
{
    static const bool perf = qEnvironmentVariableIsSet("NOKTURN_PERF_DEBUG");
    QElapsedTimer clock;
    clock.start();
    const auto log = qScopeGuard([&] {
        if (perf)
            qInfo("perf: Library::rebuildModel %lld ms (%d tracks)", clock.elapsed(), m_tracks->count());
    });
    YouTubeService *yt = YouTubeService::instance();
    QList<Track> all = m_local;
    all << yt->tracks();
    all << SpotifyService::instance()->tracks();
    // Tracks kept in local playlists (search results, Spotify) that no source provides anymore.
    {
        QSet<QString> present;
        for (const Track &t : std::as_const(all))
            present.insert(t.id);
        for (const Track &t : Playlists::instance()->tracks())
            if (!present.contains(t.id))
                all << t;
    }
    for (const Track &t : demoTracks()) {
        // Sample YouTube tracks disappear once the account is connected.
        if (t.source == QLatin1String("youtube") && yt->connected())
            continue;
        all << t;
    }
    m_tracks->setTracks(all);
    rebuildCollections();
    rebuildPlaylists();
    emit libraryChanged();
    emit playlistsChanged();
}

void Library::rebuildPlaylists()
{
    YouTubeService *yt = YouTubeService::instance();
    m_playlists = {
        QVariantMap { { "name", tr("Liked") }, { "kind", "liked" }, { "icon", "heart" },
                      { "colorA", QColor("#c9a0a8") }, { "colorB", QColor("#4d3440") }, { "seed", 1 } },
        QVariantMap { { "name", tr("Recently added") }, { "kind", "recent" }, { "icon", "clock" },
                      { "colorA", QColor("#9fb3c8") }, { "colorB", QColor("#34425a") }, { "seed", 2 } },
        QVariantMap { { "name", tr("Recently played") }, { "kind", "history" }, { "icon", "refresh" },
                      { "colorA", QColor("#b7a9cf") }, { "colorB", QColor("#3f3655") }, { "seed", 4 } },
        QVariantMap { { "name", yt->connected() ? tr("Liked on YouTube Music") : tr("Discovered on YouTube") },
                      { "kind", "youtube" }, { "icon", "playCircle" },
                      { "colorA", QColor("#d0b29a") }, { "colorB", QColor("#5c4334") }, { "seed", 3 } },
    };
    for (const Playlists::Local &p : Playlists::instance()->local()) {
        const auto [a, b] = Scanner::fallbackPalette(p.id);
        m_playlists << QVariantMap {
            { "name", p.name }, { "kind", "user" }, { "userId", p.id }, { "description", p.description },
            { "icon", "" }, { "cover", QString() }, { "count", p.trackIds.size() },
            { "colorA", a }, { "colorB", b }, { "seed", Scanner::stableSeed(p.id) } };
    }
    for (const QVariant &v : yt->playlists()) {
        const QVariantMap p = v.toMap();
        const auto [a, b] = Scanner::fallbackPalette(p.value("id").toString());
        m_playlists << QVariantMap {
            { "name", p.value("title") }, { "kind", "ytplaylist" }, { "ytId", p.value("id") },
            { "icon", "" }, { "cover", p.value("cover") }, { "count", p.value("count") },
            { "colorA", a }, { "colorB", b }, { "seed", Scanner::stableSeed(p.value("id").toString()) } };
    }
    SpotifyService *sp = SpotifyService::instance();
    if (sp->connected()) {
        m_playlists << QVariantMap { { "name", tr("Liked on Spotify") }, { "kind", "spotifyliked" }, { "icon", "heart" },
                                     { "colorA", QColor("#8fc79a") }, { "colorB", QColor("#2f4a36") }, { "seed", 4 } };
        for (const QVariant &v : sp->playlists()) {
            const QVariantMap p = v.toMap();
            const auto [a, b] = Scanner::fallbackPalette(p.value("id").toString());
            m_playlists << QVariantMap {
                { "name", p.value("name") }, { "kind", "spplaylist" }, { "spId", p.value("id") },
                { "icon", "" }, { "cover", p.value("cover") }, { "count", p.value("count") },
                { "colorA", a }, { "colorB", b }, { "seed", Scanner::stableSeed(p.value("id").toString()) } };
        }
    }
    for (QVariant &v : m_playlists) {
        QVariantMap p = v.toMap();
        p.insert("sourceCover", p.value("cover").toString());
        applyPlaylistCover(p);
        v = p;
    }
}

// ---- playlist covers -------------------------------------------------------------

namespace {

QString coversDir() { return AppPaths::dataDir() + QStringLiteral("/playlist-covers"); }

// Settings keys can't hold "/" — and file names shouldn't hold ":".
QString safeKey(const QString &key)
{
    QString out = key;
    for (QChar &c : out)
        if (!c.isLetterOrNumber() && c != QLatin1Char('-') && c != QLatin1Char('_'))
            c = QLatin1Char('_');
    return out;
}

QString customCoverFile(const QString &key)
{
    const QString name = AppPaths::settings()->value(QStringLiteral("playlistCover/") + safeKey(key)).toString();
    if (name.isEmpty())
        return {};
    const QString path = coversDir() + QLatin1Char('/') + name;
    return QFileInfo::exists(path) ? path : QString();
}

} // namespace

QString Library::coverKey(const QVariantMap &p)
{
    const QString kind = p.value("kind").toString();
    if (kind == QLatin1String("ytplaylist"))
        return QStringLiteral("yt:") + p.value("ytId").toString();
    if (kind == QLatin1String("spplaylist"))
        return QStringLiteral("sp:") + p.value("spId").toString();
    if (kind == QLatin1String("user"))
        return QStringLiteral("user:") + p.value("userId").toString();
    return kind;
}

void Library::applyPlaylistCover(QVariantMap &p) const
{
    const QString key = coverKey(p);
    p.insert("coverKey", key);
    p.remove("mosaic");
    const QString custom = customCoverFile(key);
    p.insert("customCover", !custom.isEmpty());
    if (!custom.isEmpty()) {
        p.insert("cover", QUrl::fromLocalFile(custom).toString());
        return;
    }
    p.insert("cover", p.value("sourceCover").toString());
    if (!p.value("cover").toString().isEmpty())
        return;

    // The collage: covers of different albums picked at random — the same four until the user
    // shuffles them (the seed is remembered).
    const QString kind = p.value("kind").toString();
    YouTubeService *yt = YouTubeService::instance();
    QVariantList rows;
    if (kind == QLatin1String("liked"))
        rows = likedTracks();
    else if (kind == QLatin1String("recent"))
        rows = recentlyAdded(50);
    else if (kind == QLatin1String("history"))
        rows = rowsOfIds(ListeningHistory::instance()->recentIds(200));
    else if (kind == QLatin1String("youtube"))
        rows = yt->connected() ? rowsOfIds(yt->likedIds())
                               : rowsWhere(m_tracks, [](const Track &t) { return t.source == QLatin1String("youtube") && t.demo; });
    else if (kind == QLatin1String("ytplaylist"))
        rows = rowsOfIds(yt->playlistTrackIds(p.value("ytId").toString()));   // only what's loaded — no fetching here
    else if (kind == QLatin1String("spotifyliked"))
        rows = rowsOfIds(SpotifyService::instance()->likedIds());
    else if (kind == QLatin1String("spplaylist"))
        rows = rowsOfIds(SpotifyService::instance()->playlistTrackIds(p.value("spId").toString()));
    else if (kind == QLatin1String("user"))
        rows = rowsOfIds(Playlists::instance()->trackIds(p.value("userId").toString()));

    QStringList covers;
    QSet<QString> seen;
    for (const QVariant &row : std::as_const(rows)) {
        const QString cover = m_tracks->at(row.toInt()).cover;
        if (!cover.isEmpty() && !seen.contains(cover)) {
            seen.insert(cover);
            covers << cover;
        }
    }
    if (covers.isEmpty())
        return;
    const quint32 seed = AppPaths::settings()->value(QStringLiteral("playlistMosaic/") + safeKey(key),
                                                     quint32(Scanner::stableSeed(key))).toUInt();
    QRandomGenerator rng(seed);
    std::shuffle(covers.begin(), covers.end(), rng);
    if (covers.size() == 1) {
        p.insert("cover", covers.first());
        return;
    }
    // Two or three different covers: repeated across the diagonal (A B / B A, A B / C A).
    QStringList mosaic = covers.mid(0, 4);
    if (mosaic.size() == 2)
        mosaic << mosaic.at(1) << mosaic.at(0);
    else if (mosaic.size() == 3)
        mosaic << mosaic.at(0);
    p.insert("mosaic", mosaic);
}

void Library::refreshPlaylistCover(int index)
{
    QVariantMap p = m_playlists.at(index).toMap();
    applyPlaylistCover(p);
    m_playlists[index] = p;
    emit playlistsChanged();
}

bool Library::setPlaylistCover(int index, const QUrl &image)
{
    if (index < 0 || index >= m_playlists.size())
        return false;
    QImageReader reader(image.isLocalFile() ? image.toLocalFile() : image.toString());
    reader.setAutoTransform(true);   // phone photos are rotated by their EXIF tag
    QImage img = reader.read();
    if (img.isNull()) {
        emit notice(tr("Couldn't open the image: %1").arg(reader.errorString()), QStringLiteral("close"));
        return false;
    }
    // A square from the middle, at most 640 px — the largest size the cover is shown at.
    const int side = std::min(img.width(), img.height());
    img = img.copy((img.width() - side) / 2, (img.height() - side) / 2, side, side);
    if (side > 640)
        img = img.scaled(640, 640, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    if (img.hasAlphaChannel()) {
        QImage flat(img.size(), QImage::Format_RGB32);
        flat.fill(QColor("#1a1c20"));
        QPainter(&flat).drawImage(0, 0, img);
        img = flat;
    }

    const QString key = coverKey(m_playlists.at(index).toMap());
    QDir().mkpath(coversDir());
    // A new name every time — the image cache would keep showing the old picture otherwise.
    const QString name = safeKey(key) + QLatin1Char('-') + QString::number(QDateTime::currentMSecsSinceEpoch())
                       + QStringLiteral(".jpg");
    if (!img.save(coversDir() + QLatin1Char('/') + name, "JPG", 90)) {
        emit notice(tr("Couldn't save the cover"), QStringLiteral("close"));
        return false;
    }
    if (const QString old = customCoverFile(key); !old.isEmpty())
        QFile::remove(old);
    AppPaths::settings()->setValue(QStringLiteral("playlistCover/") + safeKey(key), name);
    refreshPlaylistCover(index);
    return true;
}

void Library::resetPlaylistCover(int index)
{
    if (index < 0 || index >= m_playlists.size())
        return;
    const QString key = coverKey(m_playlists.at(index).toMap());
    if (const QString old = customCoverFile(key); !old.isEmpty())
        QFile::remove(old);
    AppPaths::settings()->remove(QStringLiteral("playlistCover/") + safeKey(key));
    refreshPlaylistCover(index);
}

void Library::shufflePlaylistCover(int index)
{
    if (index < 0 || index >= m_playlists.size())
        return;
    const QString key = coverKey(m_playlists.at(index).toMap());
    AppPaths::settings()->setValue(QStringLiteral("playlistMosaic/") + safeKey(key), QRandomGenerator::global()->generate());
    refreshPlaylistCover(index);
}

void Library::ensurePalette(int row)
{
    // Tracks from YouTube have covers from the web — the accent color is computed when they first play.
    if (!m_tracks->isValidRow(row))
        return;
    const Track &t = m_tracks->at(row);
    if ((t.source != QLatin1String("youtube") && t.source != QLatin1String("spotify")) || t.demo
        || !t.cover.startsWith(QLatin1String("http"))
        || m_paletteDone.contains(t.id))
        return;
    m_paletteDone.insert(t.id);

    const QString id = t.id;
    QNetworkReply *reply = m_net.get(QNetworkRequest(QUrl(t.cover)));
    connect(reply, &QNetworkReply::finished, this, [this, reply, id] {
        reply->deleteLater();
        QImage image;
        if (reply->error() != QNetworkReply::NoError || !image.loadFromData(reply->readAll()))
            return;
        const auto [a, b] = Scanner::paletteFromImage(image);
        YouTubeService::instance()->setTrackColors(id, a, b);
        m_tracks->setColors(m_tracks->rowOf(id), a, b);
    });
}

void Library::rebuildCollections()
{
    struct Album { QVariantMap data; qint64 added = 0; };
    QHash<QString, Album> albums;
    QStringList albumOrder;
    QHash<QString, QVariantMap> artists;
    QStringList artistOrder;

    for (int i = 0; i < m_tracks->count(); ++i) {
        const Track &t = m_tracks->at(i);
        if (t.source == QLatin1String("radio") || t.transient)
            continue;
        const QString key = TrackModel::albumKey(t);
        const QString artist = t.albumArtist.isEmpty() ? t.artist : t.albumArtist;
        // A track without an album (e.g. a music video from YouTube) doesn't create a separate "album".
        if (!t.album.isEmpty()) {
            if (!albums.contains(key)) {
                albumOrder << key;
                albums.insert(key, { QVariantMap {
                    { "key", key }, { "title", t.album }, { "artist", artist }, { "year", t.year },
                    { "colorA", t.colorA }, { "colorB", t.colorB }, { "seed", t.seed },
                    { "cover", t.cover }, { "source", t.source }, { "count", 0 } }, t.added });
            }
            Album &a = albums[key];
            a.data["count"] = a.data["count"].toInt() + 1;
            a.added = std::max(a.added, t.added);
            if (a.data["cover"].toString().isEmpty() && !t.cover.isEmpty())
                a.data["cover"] = t.cover;
            if (a.data["year"].toInt() == 0 && t.year > 0)
                a.data["year"] = t.year;
        }

        if (!artists.contains(artist)) {
            artistOrder << artist;
            artists.insert(artist, QVariantMap {
                { "name", artist }, { "colorA", t.colorA }, { "colorB", t.colorB },
                { "seed", (t.seed + 2) % 4 }, { "cover", t.cover }, { "count", 0 }, { "local", false } });
        }
        QVariantMap &ar = artists[artist];
        ar["count"] = ar["count"].toInt() + 1;
        if (!t.demo)
            ar["local"] = true;
        if (ar["cover"].toString().isEmpty() && !t.cover.isEmpty())
            ar["cover"] = t.cover;
    }

    m_albums.clear();
    for (const QString &key : std::as_const(albumOrder))
        m_albums << albums.value(key).data;

    QList<Album> byDate;
    for (const QString &key : std::as_const(albumOrder))
        byDate << albums.value(key);
    std::stable_sort(byDate.begin(), byDate.end(), [](const Album &a, const Album &b) {
        const bool al = a.data.value("source") == QLatin1String("local");
        const bool bl = b.data.value("source") == QLatin1String("local");
        if (al != bl) return al;   // own music first
        return a.added > b.added;
    });
    m_recentAlbums.clear();
    for (const Album &a : std::as_const(byDate))
        m_recentAlbums << a.data;

    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(artistOrder.begin(), artistOrder.end(),
              [&](const QString &a, const QString &b) { return collator.compare(a, b) < 0; });
    m_artists.clear();
    for (const QString &name : std::as_const(artistOrder))
        m_artists << artists.value(name);
}

QList<Track> Library::demoTracks() const
{
    struct D { const char *title; const char *artist; const char *album; int duration; const char *source; const char *color; const char *dark; int seed; };
    static const D defs[] = {
        { "Neon Rain",      "Kōsei",          "Afterglow Tapes",  258, "youtube", "#93a8c3", "#36445c", 1 },
        { "Tidal",          "Kōsei",          "Afterglow Tapes",  227, "youtube", "#93a8c3", "#36445c", 1 },
        { "Slow Orbit",     "Marlow & Vey",   "Low Light",        304, "youtube", "#ab9fc3", "#463d5c", 3 },
        { "Half Light",     "Marlow & Vey",   "Low Light",        238, "youtube", "#ab9fc3", "#463d5c", 3 },
        { "Velvet Static",  "Odessa Grey",    "Velvet Static",    219, "youtube", "#cda2a6", "#5c3a42", 2 },
        { "Paper Lanterns", "Mira Sol",       "Lanterns EP",      192, "youtube", "#c5bd95", "#545035", 0 },
        { QT_TR_NOOP("Night Jazz"), QT_TR_NOOP("Internet radio"), "Ambient Waves FM", 0,  "radio",   "#b9a889", "#4a4232", 0 },
    };

    QList<Track> out;
    for (const D &d : defs) {
        Track t;
        t.title = tr(d.title);
        t.artist = tr(d.artist);
        t.album = QString::fromUtf8(d.album);
        t.id = QStringLiteral("demo:") + QString::fromUtf8(d.title);   // stable regardless of the language
        t.duration = d.duration;
        t.source = QString::fromLatin1(d.source);
        t.quality = t.source == QLatin1String("radio") ? tr("MP3 · 128 kbps · live")
                                                       : QStringLiteral("Opus · 160 kbps");
        t.colorA = QColor(d.color);
        t.colorB = QColor(d.dark);
        t.seed = d.seed;
        t.demo = true;
        t.liked = m_liked.contains(t.id);
        out << t;
    }
    return out;
}

// ---- queries ------------------------------------------------------------------

QVariantList Library::search(const QString &query, const QString &source, int limit) const
{
    const QString q = query.trimmed();
    QVariantList out;
    for (int i = 0; i < m_tracks->count() && out.size() < limit; ++i) {
        const Track &t = m_tracks->at(i);
        if (t.transient || (!source.isEmpty() && t.source != source))
            continue;
        if (q.isEmpty()
            || t.title.contains(q, Qt::CaseInsensitive)
            || t.artist.contains(q, Qt::CaseInsensitive)
            || t.album.contains(q, Qt::CaseInsensitive)
            || t.genre.contains(q, Qt::CaseInsensitive))
            out << i;
    }
    return out;
}

QVariantList Library::tracksOfAlbum(const QString &albumKey) const
{
    return rowsWhere(m_tracks, [&](const Track &t) { return TrackModel::albumKey(t) == albumKey; });
}

QVariantList Library::tracksOfArtist(const QString &artist) const
{
    return rowsWhere(m_tracks, [&](const Track &t) {
        return t.artist == artist || t.albumArtist == artist;
    });
}

QVariantList Library::likedTracks() const
{
    return rowsWhere(m_tracks, [](const Track &t) { return t.liked && !t.transient; });
}

QVariantList Library::localTracks() const
{
    // Search results and recommendations aren't part of the library.
    return rowsWhere(m_tracks, [](const Track &t) { return !t.demo && !t.transient; });
}

QVariantList Library::recentlyAdded(int limit) const
{
    QList<int> rows;
    for (int i = 0; i < m_tracks->count(); ++i)
        if (!m_tracks->at(i).demo && !m_tracks->at(i).transient)
            rows << i;
    std::stable_sort(rows.begin(), rows.end(), [this](int a, int b) {
        return m_tracks->at(a).added > m_tracks->at(b).added;
    });
    QVariantList out;
    for (int i = 0; i < rows.size() && i < limit; ++i)
        out << rows.at(i);
    return out;
}

QVariantList Library::rowsOfIds(const QStringList &ids) const
{
    QVariantList out;
    for (const QString &id : ids)
        if (const int row = m_tracks->rowOf(id); row >= 0)
            out << row;
    return out;
}

QVariantList Library::playlistRows(int index)
{
    const QVariantMap p = m_playlists.value(index).toMap();
    const QString kind = p.value("kind").toString();
    YouTubeService *yt = YouTubeService::instance();
    if (kind == QLatin1String("liked"))
        return likedTracks();
    if (kind == QLatin1String("recent"))
        return recentlyAdded(50);
    if (kind == QLatin1String("history"))
        return rowsOfIds(ListeningHistory::instance()->recentIds(200));
    if (kind == QLatin1String("youtube")) {
        if (yt->connected())
            return rowsOfIds(yt->likedIds());
        return rowsWhere(m_tracks, [](const Track &t) { return t.source == QLatin1String("youtube") && t.demo; });
    }
    if (kind == QLatin1String("ytplaylist")) {
        const QString id = p.value("ytId").toString();
        yt->ensurePlaylist(id);
        return rowsOfIds(yt->playlistTrackIds(id));
    }
    if (kind == QLatin1String("user"))
        return rowsOfIds(Playlists::instance()->trackIds(p.value("userId").toString()));
    if (kind == QLatin1String("spotifyliked"))
        return rowsOfIds(SpotifyService::instance()->likedIds());
    if (kind == QLatin1String("spplaylist"))
        return rowsOfIds(SpotifyService::instance()->playlistTrackIds(p.value("spId").toString()));
    return {};
}

bool Library::playlistLoading(int index) const
{
    const QVariantMap p = m_playlists.value(index).toMap();
    return p.value("kind") == QLatin1String("ytplaylist")
           && YouTubeService::instance()->playlistLoading(p.value("ytId").toString());
}

// ---- queue contexts (the same strings PlaylistPage.qml builds) -------------------

QString Library::playlistContext(int index) const
{
    const QVariantMap p = m_playlists.value(index).toMap();
    if (p.isEmpty())
        return {};
    const QString kind = p.value("kind").toString();
    if (kind == QLatin1String("ytplaylist"))
        return QStringLiteral("yt:") + p.value("ytId").toString();
    if (kind == QLatin1String("spplaylist"))
        return QStringLiteral("sp:") + p.value("spId").toString();
    if (kind == QLatin1String("user"))
        return QStringLiteral("user:") + p.value("userId").toString();
    return QStringLiteral("playlist:") + kind;
}

int Library::playlistIndex(const QString &context) const
{
    for (int i = 0; i < m_playlists.size(); ++i)
        if (playlistContext(i) == context)
            return i;
    return -1;
}

QString Library::playlistName(int index) const
{
    return m_playlists.value(index).toMap().value("name").toString();
}

QVariantList Library::artistSongRows(const QString &browseId) const
{
    return rowsOfIds(YouTubeService::instance()->artistSongIds(browseId));
}

QVariantList Library::collectionRows(const QString &playlistId)
{
    YouTubeService *yt = YouTubeService::instance();
    yt->ensureCollection(playlistId);
    return rowsOfIds(yt->collectionTrackIds(playlistId));
}

QVariantList Library::rowsForContext(const QString &context)
{
    if (context == QLatin1String("library"))
        return libraryRows();
    if (context.startsWith(QLatin1String("ytartist:")))
        return artistSongRows(context.mid(9));
    if (context.startsWith(QLatin1String("ytc:")))
        return collectionRows(context.mid(4));
    if (context.startsWith(QLatin1String("album:")))
        return tracksOfAlbum(context.mid(6));
    if (context.startsWith(QLatin1String("artist:")))
        return tracksOfArtist(context.mid(7));
    if (context.startsWith(QLatin1String("genre:")))
        return tracksOfGenre(context.mid(6));
    if (context == QLatin1String("recs"))
        return recommendedRows();
    const int index = playlistIndex(context);
    return index >= 0 ? playlistRows(index) : QVariantList();
}

QVariantList Library::libraryRows() const
{
    QVariantList out = localTracks();
    out << rowsOfIds(YouTubeService::instance()->likedIds());
    out << rowsOfIds(SpotifyService::instance()->likedIds());
    return out;
}

int Library::totalDuration(const QVariantList &rows) const
{
    int total = 0;
    for (const QVariant &v : rows)
        if (const int row = v.toInt(); m_tracks->isValidRow(row))
            total += std::max(0, m_tracks->at(row).duration);
    return total;
}

QVariantList Library::onlineResults() const
{
    return rowsOfIds(YouTubeService::instance()->searchIds());
}

QVariantList Library::recommendedRows() const
{
    return rowsOfIds(YouTubeService::instance()->recommendedIds());
}

// ---- genres ----------------------------------------------------------------------

QStringList Library::genresOf(const Track &t) const
{
    // The file's own tag; for tracks without one (streaming) the genre of the artist.
    QStringList names;
    if (!t.genre.isEmpty()) {
        for (const QString &name : splitGenres(t.genre))
            names << genreDisplayName(name);
    } else if (const QString g = ArtistGenres::instance()->genreOf(TrackModel::primaryArtist(t.artist)); !g.isEmpty()) {
        names << genreDisplayName(artistGenreName(g));
    }
    return names;
}

QVariantList Library::genres() const
{
    // Built when asked (the search page), not on every model rebuild.
    ArtistGenres *artistGenres = ArtistGenres::instance();
    if (m_genresRevision == m_tracks->revision() && m_genresArtistRevision == artistGenres->revision())
        return m_genres;
    struct Genre { QHash<QString, int> spellings; QHash<QString, int> albums; int count = 0; };
    QHash<QString, Genre> found;
    QHash<QString, int> untagged;   // artist -> tracks without a genre tag
    for (const Track &t : m_tracks->tracks()) {
        if (t.transient || t.demo || t.source == QLatin1String("radio"))
            continue;
        if (t.genre.isEmpty() && !Scanner::isUnknownArtist(t.artist))
            ++untagged[TrackModel::primaryArtist(t.artist)];
        QSet<QString> seen;   // "Rock; rock"
        for (const QString &name : genresOf(t)) {
            const QString key = genreKey(name);
            if (key.isEmpty() || seen.contains(key))
                continue;
            seen.insert(key);
            Genre &g = found[key];
            ++g.spellings[name];
            ++g.count;
            if (!t.album.isEmpty())
                ++g.albums[TrackModel::albumKey(t)];
        }
    }
    QHash<QString, QVariantMap> albums;
    for (const QVariant &v : m_albums) {
        const QVariantMap a = v.toMap();
        albums.insert(a.value("key").toString(), a);
    }
    const auto most = [](const QHash<QString, int> &counts) {
        QString best;
        int n = 0;
        for (auto it = counts.cbegin(); it != counts.cend(); ++it)
            if (it.value() > n || (it.value() == n && it.key() < best)) {
                n = it.value();
                best = it.key();
            }
        return best;
    };

    QStringList keys = found.keys();
    std::sort(keys.begin(), keys.end(), [&](const QString &a, const QString &b) {
        const int ca = found.value(a).count, cb = found.value(b).count;
        return ca != cb ? ca > cb : a < b;
    });
    m_genres.clear();
    for (const QString &key : std::as_const(keys)) {
        const Genre &g = found[key];
        // The tile wears the colors and the cover of the album with the most tracks in the genre.
        const QVariantMap album = albums.value(most(g.albums));
        const auto [a, b] = Scanner::fallbackPalette(key);
        // Its page shows a collage of the four biggest albums.
        QStringList byCount = g.albums.keys();
        std::sort(byCount.begin(), byCount.end(), [&g](const QString &x, const QString &y) {
            return g.albums.value(x) != g.albums.value(y) ? g.albums.value(x) > g.albums.value(y) : x < y;
        });
        QStringList mosaic;
        for (const QString &albumKey : std::as_const(byCount)) {
            const QString cover = albums.value(albumKey).value("cover").toString();
            if (!cover.isEmpty() && !mosaic.contains(cover))
                mosaic << cover;
            if (mosaic.size() == 4)
                break;
        }
        m_genres << QVariantMap {
            { "key", key }, { "name", most(g.spellings) }, { "count", g.count },
            { "colorA", album.value("colorA", a) }, { "colorB", album.value("colorB", b) },
            { "seed", Scanner::stableSeed(key) }, { "cover", album.value("cover").toString() },
            { "mosaic", mosaic.size() == 4 ? mosaic : QStringList() } };
    }
    m_genresRevision = m_tracks->revision();
    m_genresArtistRevision = artistGenres->revision();

    // The genres of the artists with the most untagged tracks come from the iTunes catalog, in the
    // background — their tiles show up as the answers come in.
    QStringList artists = untagged.keys();
    std::sort(artists.begin(), artists.end(), [&untagged](const QString &a, const QString &b) {
        return untagged.value(a) != untagged.value(b) ? untagged.value(a) > untagged.value(b) : a < b;
    });
    if (artists.size() > 250)
        artists.resize(250);
    artistGenres->request(artists);
    return m_genres;
}

QVariantList Library::tracksOfGenre(const QString &genre) const
{
    const QString wanted = genreKey(genre);
    if (wanted.isEmpty())
        return {};
    return rowsWhere(m_tracks, [&](const Track &t) {
        if (t.transient || t.demo)
            return false;
        for (const QString &name : genresOf(t))
            if (genreKey(name) == wanted)
                return true;
        return false;
    });
}
