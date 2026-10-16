#include "YouTubeService.h"

#include "AppPaths.h"

#include <QDir>
#include <QFile>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTimer>

namespace {

QString cookieFile() { return AppPaths::dataDir() + QStringLiteral("/youtube.cookies"); }
QString cacheFile() { return AppPaths::dataDir() + QStringLiteral("/youtube.json"); }
QString browserProfile() { return AppPaths::dataDir() + QStringLiteral("/browser"); }

QJsonObject trackToJson(const Track &t)
{
    return {
        { "id", t.path }, { "title", t.title }, { "artist", t.artist }, { "album", t.album },
        { "duration", t.duration }, { "cover", t.cover },
        { "colorA", t.colorA.name() }, { "colorB", t.colorB.name() }, { "seed", t.seed },
    };
}

Track trackFromJson(const QJsonObject &o)
{
    Track t;
    t.path = o.value("id").toString();
    t.id = QStringLiteral("yt:") + t.path;
    t.title = o.value("title").toString();
    t.artist = o.value("artist").toString();
    t.album = o.value("album").toString();
    t.duration = o.value("duration").toInt();
    t.cover = o.value("cover").toString();
    t.colorA = QColor(o.value("colorA").toString());
    t.colorB = QColor(o.value("colorB").toString());
    t.seed = o.value("seed").toInt();
    t.source = QStringLiteral("youtube");
    t.quality = QStringLiteral("YouTube Music · strumień");
    return t;
}

QJsonArray toArray(const QStringList &list)
{
    QJsonArray arr;
    for (const QString &s : list)
        arr.append(s);
    return arr;
}

QStringList fromArray(const QJsonArray &arr)
{
    QStringList out;
    for (const QJsonValue &v : arr)
        out << v.toString();
    return out;
}

} // namespace

YouTubeService *YouTubeService::instance()
{
    static YouTubeService *inst = new YouTubeService;
    return inst;
}

YouTubeService *YouTubeService::create(QQmlEngine *, QJSEngine *)
{
    YouTubeService *s = instance();
    QJSEngine::setObjectOwnership(s, QJSEngine::CppOwnership);
    return s;
}

YouTubeService::YouTubeService(QObject *parent)
    : QObject(parent)
{
    BrowserLogin::findBrowser(&m_browserName);

    connect(&m_login, &BrowserLogin::runningChanged, this, &YouTubeService::loggingInChanged);
    connect(&m_login, &BrowserLogin::statusChanged, this, &YouTubeService::loggingInChanged);
    connect(&m_login, &BrowserLogin::succeeded, this, [this](const YtCookies &cookies) {
        useCookies(cookies, true);
    });
    connect(&m_login, &BrowserLogin::failed, this, [this](const QString &reason) {
        emit notice(reason, QStringLiteral("close"));
    });

    connect(&m_resolver, &StreamResolver::engineChanged, this, &YouTubeService::engineChanged);
    connect(&m_resolver, &StreamResolver::notice, this, &YouTubeService::notice);
    m_resolver.detect();

    const YtCookies saved = YtCookies::load(cookieFile());
    if (saved.isLoggedIn()) {
        loadCache();
        useCookies(saved, false);
    }
}

// ---- logowanie ----------------------------------------------------------------

void YouTubeService::login()
{
    if (m_browserName.isEmpty()) {
        emit notice(QStringLiteral("Nie znaleziono Chrome, Edge ani Brave — użyj ręcznego wklejenia ciasteczek"),
                    QStringLiteral("close"));
        return;
    }
    m_login.start(browserProfile());
}

void YouTubeService::cancelLogin()
{
    m_login.cancel();
}

void YouTubeService::loginWithCookies(const QString &cookieHeader)
{
    const YtCookies cookies = YtCookies::fromHeader(cookieHeader);
    if (!cookies.isLoggedIn()) {
        emit notice(QStringLiteral("W ciasteczkach brakuje SAPISID lub LOGIN_INFO"), QStringLiteral("key"));
        return;
    }
    useCookies(cookies, true);
}

void YouTubeService::useCookies(const YtCookies &cookies, bool fresh)
{
    m_cookies = cookies;
    m_api.setCookies(cookies);
    m_resolver.setCookies(cookies);
    if (fresh) {
        if (!cookies.save(cookieFile()))
            emit notice(QStringLiteral("Nie udało się zapisać sesji na dysku"), QStringLiteral("close"));
        emit notice(QStringLiteral("Zalogowano do YouTube Music"), QStringLiteral("check"));
    }
    emit accountChanged();
    emit dataChanged();
    // Przy starcie aplikacji daj chwilę na wczytanie interfejsu, po logowaniu synchronizuj od razu.
    QTimer::singleShot(fresh ? 0 : 1500, this, &YouTubeService::refresh);
}

void YouTubeService::logout()
{
    m_login.cancel();
    m_cookies = {};
    m_api.setCookies({});
    m_resolver.setCookies({});
    QFile::remove(cookieFile());
    QFile::remove(cacheFile());
    QDir(browserProfile()).removeRecursively();   // sesja w profilu przeglądarki też znika

    m_account = {};
    m_tracks.clear();
    m_liked.clear();
    m_likedSet.clear();
    m_playlists.clear();
    m_playlistTracks.clear();
    m_refreshedPlaylists.clear();
    m_search.clear();
    emit accountChanged();
    emit dataChanged();
    emit notice(QStringLiteral("Wylogowano z YouTube Music"), QStringLiteral("person"));
}

// ---- synchronizacja -------------------------------------------------------------

void YouTubeService::refresh()
{
    if (!connected() || syncing())
        return;
    m_pendingSync = 3;
    m_syncError.clear();
    emit syncingChanged();

    m_api.fetchAccount([this](const YtMusicApi::Account &account, const QString &error) {
        if (error.isEmpty()) {
            m_account = account;
            emit accountChanged();
        }
        syncStep(error);
    });

    m_api.browseTracks(QStringLiteral("VLLM"), [this](const QList<Track> &tracks, const QString &error) {
        // Pusta odpowiedź przy niepustej liście to zwykle wygasła sesja, a nie odlubienie wszystkiego.
        if (!tracks.isEmpty() || (error.isEmpty() && m_liked.isEmpty())) {
            storeTracks(tracks, false);
            QStringList ids;
            for (const Track &t : tracks)
                ids << t.id;
            setLikedList(ids);
        }
        syncStep(error);
    });

    m_api.fetchLibraryPlaylists([this](const QVariantList &playlists, const QString &error) {
        if (error.isEmpty()) {
            m_playlists = playlists;
            QSet<QString> ids;
            for (const QVariant &p : playlists)
                ids.insert(p.toMap().value("id").toString());
            for (auto it = m_playlistTracks.begin(); it != m_playlistTracks.end();)
                it = ids.contains(it.key()) ? std::next(it) : m_playlistTracks.erase(it);
            m_refreshedPlaylists.clear();
        }
        syncStep(error);
    });
}

void YouTubeService::syncStep(const QString &error)
{
    if (!error.isEmpty() && m_syncError.isEmpty())
        m_syncError = error;
    if (--m_pendingSync > 0)
        return;

    m_lastSync = QDateTime::currentDateTime();
    saveCache();
    emit syncingChanged();
    emit dataChanged();
    if (!m_syncError.isEmpty())
        emit notice(QStringLiteral("YouTube Music: ") + m_syncError, QStringLiteral("close"));
}

QString YouTubeService::lastSync() const
{
    if (!m_lastSync.isValid())
        return {};
    const qint64 mins = m_lastSync.secsTo(QDateTime::currentDateTime()) / 60;
    if (mins < 1)
        return QStringLiteral("przed chwilą");
    if (mins < 60)
        return QStringLiteral("%1 min temu").arg(mins);
    return m_lastSync.toString(QStringLiteral("d.MM, HH:mm"));
}

void YouTubeService::ensurePlaylist(const QString &playlistId)
{
    // Każdą playlistę odświeżamy raz na sesję, w tle — do tego czasu widać wersję z cache.
    if (m_loadingPlaylists.contains(playlistId) || m_refreshedPlaylists.contains(playlistId))
        return;
    m_loadingPlaylists.insert(playlistId);
    m_refreshedPlaylists.insert(playlistId);
    // Bywa wołane z wiązania QML — powiadamiamy dopiero w następnym obiegu pętli zdarzeń.
    QTimer::singleShot(0, this, &YouTubeService::dataChanged);

    m_api.browseTracks(QStringLiteral("VL") + playlistId, [this, playlistId](const QList<Track> &tracks, const QString &error) {
        m_loadingPlaylists.remove(playlistId);
        if (!error.isEmpty() && tracks.isEmpty()) {
            emit notice(QStringLiteral("Nie udało się wczytać playlisty: ") + error, QStringLiteral("close"));
            emit dataChanged();
            return;
        }
        storeTracks(tracks, false);
        QStringList ids;
        for (const Track &t : tracks)
            ids << t.id;
        m_playlistTracks.insert(playlistId, ids);
        saveCache();
        emit dataChanged();
    });
}

void YouTubeService::search(const QString &query)
{
    const QString q = query.trimmed();
    const quint64 serial = ++m_searchSerial;
    if (q.isEmpty()) {
        m_search.clear();
        m_searching = false;
        emit searchingChanged();
        emit dataChanged();
        return;
    }
    m_searching = true;
    emit searchingChanged();
    m_api.search(q, [this, serial](const QList<Track> &tracks, const QString &error) {
        if (serial != m_searchSerial)
            return;   // przyszło już nowsze zapytanie
        m_searching = false;
        emit searchingChanged();
        if (!error.isEmpty()) {
            emit notice(QStringLiteral("Wyszukiwanie w YouTube Music: ") + error, QStringLiteral("close"));
            return;
        }
        storeTracks(tracks, true);
        m_search.clear();
        for (const Track &t : tracks)
            m_search << t.id;
        emit dataChanged();
    });
}

// ---- dane -----------------------------------------------------------------------

void YouTubeService::storeTracks(const QList<Track> &tracks, bool transient)
{
    for (Track t : tracks) {
        const auto existing = m_tracks.constFind(t.id);
        if (existing != m_tracks.cend()) {
            // Zachowaj kolory wyliczone z okładki i "stałość" utworu w bibliotece.
            t.colorA = existing->colorA;
            t.colorB = existing->colorB;
            t.transient = existing->transient && transient;
        } else {
            t.transient = transient;
        }
        m_tracks.insert(t.id, t);
    }
}

void YouTubeService::setLikedList(const QStringList &ids)
{
    m_liked = ids;
    m_likedSet = QSet<QString>(ids.cbegin(), ids.cend());
}

QList<Track> YouTubeService::tracks() const
{
    // Kolejność: polubione, utwory z playlist, wyniki wyszukiwania.
    QList<Track> out;
    QSet<QString> seen;
    auto add = [&](const QString &id) {
        if (seen.contains(id))
            return;
        const auto it = m_tracks.constFind(id);
        if (it == m_tracks.cend())
            return;
        seen.insert(id);
        Track t = *it;
        t.liked = m_likedSet.contains(id);
        out << t;
    };
    for (const QString &id : m_liked)
        add(id);
    for (const QStringList &ids : m_playlistTracks)
        for (const QString &id : ids)
            add(id);
    for (const QString &id : m_search)
        add(id);
    return out;
}

void YouTubeService::setTrackLiked(const QString &trackId, bool liked)
{
    if (liked == m_likedSet.contains(trackId))
        return;
    if (liked) {
        m_liked.prepend(trackId);
        m_likedSet.insert(trackId);
        if (m_tracks.contains(trackId))
            m_tracks[trackId].transient = false;
    } else {
        m_liked.removeAll(trackId);
        m_likedSet.remove(trackId);
    }
    const auto it = m_tracks.constFind(trackId);
    if (it != m_tracks.cend())
        m_api.setLiked(it->path, liked);   // synchronizacja z kontem
    saveCache();
}

void YouTubeService::setTrackColors(const QString &trackId, const QColor &a, const QColor &b)
{
    auto it = m_tracks.find(trackId);
    if (it == m_tracks.end())
        return;
    it->colorA = a;
    it->colorB = b;
}

void YouTubeService::downloadEngine()
{
    m_resolver.download();
}

// ---- cache ------------------------------------------------------------------------

void YouTubeService::loadCache()
{
    QFile file(cacheFile());
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();

    const QJsonObject account = root.value("account").toObject();
    m_account = { account.value("name").toString(), account.value("handle").toString(), account.value("photo").toString() };

    for (const QJsonValue &v : root.value("tracks").toArray()) {
        const Track t = trackFromJson(v.toObject());
        m_tracks.insert(t.id, t);
    }
    setLikedList(fromArray(root.value("liked").toArray()));
    m_playlists = root.value("playlists").toArray().toVariantList();
    const QJsonObject lists = root.value("playlistTracks").toObject();
    for (auto it = lists.begin(); it != lists.end(); ++it)
        m_playlistTracks.insert(it.key(), fromArray(it.value().toArray()));
    m_lastSync = QDateTime::fromString(root.value("lastSync").toString(), Qt::ISODate);
}

void YouTubeService::saveCache() const
{
    QJsonArray tracks;
    for (const Track &t : m_tracks)
        if (!t.transient)
            tracks.append(trackToJson(t));
    QJsonObject lists;
    for (auto it = m_playlistTracks.cbegin(); it != m_playlistTracks.cend(); ++it)
        lists.insert(it.key(), toArray(it.value()));

    const QJsonObject root {
        { "account", QJsonObject { { "name", m_account.name }, { "handle", m_account.handle }, { "photo", m_account.photo } } },
        { "tracks", tracks },
        { "liked", toArray(m_liked) },
        { "playlists", QJsonArray::fromVariantList(m_playlists) },
        { "playlistTracks", lists },
        { "lastSync", m_lastSync.toString(Qt::ISODate) },
    };
    QSaveFile file(cacheFile());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        file.commit();
    }
}
