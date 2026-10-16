#include "SpotifyService.h"

#include "AppPaths.h"
#include "LibraryScanner.h"
#include "youtube/YouTubeService.h"
#include "youtube/YtCookies.h"
#include "youtube/YtMusicApi.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QFile>
#include <QHostAddress>
#include <QJSEngine>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QSet>
#include <QTcpSocket>
#include <QUrl>

#include <utility>

namespace {

constexpr quint16 kRedirectPort = 43117;
const char *kScopes = "playlist-read-private playlist-read-collaborative user-library-read";

QString tokenFile() { return AppPaths::dataDir() + QStringLiteral("/spotify.token"); }
QString cacheFile() { return AppPaths::dataDir() + QStringLiteral("/spotify.json"); }
QString matchesFile() { return AppPaths::dataDir() + QStringLiteral("/spotify-matches.json"); }

QString randomString(int length)
{
    static const QByteArray chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
    QString s;
    for (int i = 0; i < length; ++i)
        s += QChar(chars.at(QRandomGenerator::system()->bounded(int(chars.size()))));
    return s;
}

QUrl apiUrl(const QString &path)
{
    return QUrl(QStringLiteral("https://api.spotify.com/v1") + path);
}

QJsonObject trackToJson(const Track &t)
{
    return { { "id", t.path }, { "title", t.title }, { "artist", t.artist }, { "album", t.album },
             { "duration", t.duration }, { "cover", t.cover } };
}

Track makeTrack(const QString &spotifyId, const QString &title, const QString &artist, const QString &album,
                int duration, const QString &cover)
{
    Track t;
    t.id = QStringLiteral("sp:") + spotifyId;
    t.path = spotifyId;
    t.source = QStringLiteral("spotify");
    t.title = title;
    t.artist = artist;
    t.album = album;
    t.duration = duration;
    t.cover = cover;
    t.quality = QCoreApplication::translate("SpotifyService", "Spotify · played from YouTube Music");
    const QString key = TrackModel::albumKey(t);
    const auto [a, b] = Scanner::fallbackPalette(key);
    t.colorA = a;
    t.colorB = b;
    t.seed = Scanner::stableSeed(key);
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

SpotifyService *SpotifyService::instance()
{
    static SpotifyService *inst = new SpotifyService;
    return inst;
}

SpotifyService *SpotifyService::create(QQmlEngine *, QJSEngine *)
{
    SpotifyService *s = instance();
    QJSEngine::setObjectOwnership(s, QJSEngine::CppOwnership);
    return s;
}

SpotifyService::SpotifyService(QObject *parent)
    : QObject(parent)
{
    m_clientId = AppPaths::settings()->value("spotify/clientId").toString();
    if (m_clientId.isEmpty())
        m_clientId = QStringLiteral(NOKTURN_SPOTIFY_CLIENT_ID);

    connect(&m_server, &QTcpServer::newConnection, this, &SpotifyService::onCallback);
    m_matchesSave.setSingleShot(true);
    m_matchesSave.setInterval(3000);
    connect(&m_matchesSave, &QTimer::timeout, this, &SpotifyService::saveMatches);

    loadCache();
    loadToken();
    if (connected())
        QTimer::singleShot(4000, this, &SpotifyService::refresh);   // after the UI has loaded
}

void SpotifyService::setClientId(const QString &id)
{
    const QString trimmed = id.trimmed();
    if (trimmed == m_clientId)
        return;
    m_clientId = trimmed;
    AppPaths::settings()->setValue("spotify/clientId", trimmed);
    emit settingsChanged();
}

bool SpotifyService::builtInClientId() const
{
    return !QStringLiteral(NOKTURN_SPOTIFY_CLIENT_ID).isEmpty();
}

QString SpotifyService::redirectUri() const
{
    return QStringLiteral("http://127.0.0.1:%1/callback").arg(kRedirectPort);
}

QString SpotifyService::lastSync() const
{
    if (!m_lastSync.isValid())
        return {};
    const qint64 mins = m_lastSync.secsTo(QDateTime::currentDateTime()) / 60;
    if (mins < 1)
        return tr("just now");
    if (mins < 60)
        return tr("%1 min ago").arg(mins);
    return m_lastSync.toString(QStringLiteral("d.MM, HH:mm"));
}

// ---- sign-in (OAuth 2.0 + PKCE) -----------------------------------------------------

void SpotifyService::login()
{
    if (m_clientId.isEmpty()) {
        emit notice(tr("Enter your Spotify Client ID first"), QStringLiteral("key"));
        return;
    }
    if (!m_server.isListening() && !m_server.listen(QHostAddress::LocalHost, kRedirectPort)) {
        emit notice(tr("Couldn't start the sign-in (port %1 is busy)").arg(kRedirectPort), QStringLiteral("close"));
        return;
    }
    emit loggingInChanged();

    m_verifier = randomString(96);
    m_state = randomString(24);
    const QByteArray challenge = QCryptographicHash::hash(m_verifier.toLatin1(), QCryptographicHash::Sha256)
                                     .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("client_id"), m_clientId);
    q.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    q.addQueryItem(QStringLiteral("redirect_uri"), redirectUri());
    q.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
    q.addQueryItem(QStringLiteral("code_challenge"), QString::fromLatin1(challenge));
    q.addQueryItem(QStringLiteral("state"), m_state);
    q.addQueryItem(QStringLiteral("scope"), QString::fromLatin1(kScopes));
    QUrl url(QStringLiteral("https://accounts.spotify.com/authorize"));
    url.setQuery(q);
    QDesktopServices::openUrl(url);
}

void SpotifyService::cancelLogin()
{
    m_server.close();
    emit loggingInChanged();
}

void SpotifyService::onCallback()
{
    while (QTcpSocket *socket = m_server.nextPendingConnection()) {
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
            const QByteArray request = socket->readAll();
            const QList<QByteArray> line = request.left(request.indexOf('\r')).split(' ');
            if (line.size() < 2 || !line.at(1).startsWith("/callback")) {
                socket->write("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                socket->disconnectFromHost();
                return;
            }
            const QUrlQuery q(QUrl(QString::fromLatin1(line.at(1))).query());
            const QString code = q.queryItemValue(QStringLiteral("code"));
            const bool ok = !code.isEmpty() && q.queryItemValue(QStringLiteral("state")) == m_state;
            const QString message = ok ? tr("Nokturn is connected to Spotify — you can close this tab.")
                                       : tr("Signing in to Spotify didn't work — go back to Nokturn and try again.");
            const QByteArray body = "<!doctype html><meta charset=utf-8><title>Nokturn</title>"
                                    "<body style=\"font:16px system-ui;background:#101114;color:#e8e4dd;"
                                    "display:grid;place-items:center;height:90vh\"><p>"
                                    + message.toHtmlEscaped().toUtf8() + "</p>";
            socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n"
                          "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body);
            socket->disconnectFromHost();

            m_server.close();
            emit loggingInChanged();
            if (ok)
                exchangeCode(code);
            else
                emit notice(tr("Spotify sign-in was cancelled"), QStringLiteral("close"));
        });
    }
}

void SpotifyService::exchangeCode(const QString &code)
{
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("authorization_code"));
    form.addQueryItem(QStringLiteral("code"), code);
    form.addQueryItem(QStringLiteral("redirect_uri"), redirectUri());
    form.addQueryItem(QStringLiteral("client_id"), m_clientId);
    form.addQueryItem(QStringLiteral("code_verifier"), m_verifier);
    tokenRequest(form, [this](bool ok, const QString &error) {
        if (!ok) {
            emit notice(tr("Couldn't sign in to Spotify: %1").arg(error), QStringLiteral("close"));
            return;
        }
        saveToken();
        emit accountChanged();
        emit notice(tr("Signed in to Spotify"), QStringLiteral("check"));
        refresh();
    });
}

void SpotifyService::tokenRequest(const QUrlQuery &form, std::function<void(bool, const QString &)> done)
{
    QNetworkRequest request(QUrl(QStringLiteral("https://accounts.spotify.com/api/token")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/x-www-form-urlencoded"));
    QNetworkReply *reply = m_net.post(request, form.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply, done] {
        reply->deleteLater();
        const QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();
        const QString access = json.value("access_token").toString();
        if (reply->error() != QNetworkReply::NoError || access.isEmpty()) {
            QString error = json.value("error_description").toString();
            if (error.isEmpty())
                error = json.value("error").toString(reply->errorString());
            done(false, error);
            return;
        }
        m_accessToken = access;
        m_accessExpires = QDateTime::currentDateTimeUtc().addSecs(json.value("expires_in").toInt(3600) - 60);
        const QString refresh = json.value("refresh_token").toString();
        if (!refresh.isEmpty() && refresh != m_refreshToken) {
            m_refreshToken = refresh;   // Spotify may rotate it
            saveToken();
        }
        done(true, {});
    });
}

void SpotifyService::withAccessToken(std::function<void(const QString &)> next)
{
    if (!connected()) {
        next(tr("Not signed in to Spotify"));
        return;
    }
    if (!m_accessToken.isEmpty() && QDateTime::currentDateTimeUtc() < m_accessExpires) {
        next({});
        return;
    }
    m_waitingForToken << next;
    if (m_refreshing)
        return;
    m_refreshing = true;
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
    form.addQueryItem(QStringLiteral("refresh_token"), m_refreshToken);
    form.addQueryItem(QStringLiteral("client_id"), m_clientId);
    tokenRequest(form, [this](bool ok, const QString &error) {
        m_refreshing = false;
        const QString message = ok ? QString() : tr("Spotify session expired — sign in again (%1)").arg(error);
        const auto waiting = std::exchange(m_waitingForToken, {});
        for (const auto &w : waiting)
            w(message);
    });
}

void SpotifyService::saveToken() const
{
    const QByteArray json = QJsonDocument(QJsonObject {
        { "refresh_token", m_refreshToken }, { "client_id", m_clientId } }).toJson(QJsonDocument::Compact);
    QSaveFile f(tokenFile());
    if (f.open(QIODevice::WriteOnly)) {
        f.write(protectSecret(json));
        f.commit();
    }
#ifndef Q_OS_WIN
    QFile::setPermissions(tokenFile(), QFileDevice::ReadOwner | QFileDevice::WriteOwner);
#endif
}

void SpotifyService::loadToken()
{
    QFile f(tokenFile());
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonObject json = QJsonDocument::fromJson(unprotectSecret(f.readAll())).object();
    m_refreshToken = json.value("refresh_token").toString();
    // The token belongs to the Spotify app it was issued for.
    const QString issuedFor = json.value("client_id").toString();
    if (!issuedFor.isEmpty() && issuedFor != m_clientId)
        m_clientId = issuedFor;
}

void SpotifyService::logout()
{
    m_refreshToken.clear();
    m_accessToken.clear();
    m_accountName.clear();
    m_tracks.clear();
    m_liked.clear();
    m_playlists.clear();
    m_playlistTracks.clear();
    m_snapshots.clear();
    QFile::remove(tokenFile());
    QFile::remove(cacheFile());
    emit accountChanged();
    emit dataChanged();
    emit notice(tr("Signed out of Spotify"), QStringLiteral("person"));
}

// ---- Web API ------------------------------------------------------------------------

void SpotifyService::get(const QUrl &url, JsonCallback callback, int attempt)
{
    withAccessToken([this, url, callback, attempt](const QString &tokenError) {
        if (!tokenError.isEmpty()) {
            callback({}, tokenError);
            return;
        }
        QNetworkRequest request(url);
        request.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());
        QNetworkReply *reply = m_net.get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply, url, callback, attempt] {
            reply->deleteLater();
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();
            if (status == 401 && attempt == 0) {
                m_accessToken.clear();   // expired early — refresh and try once more
                get(url, callback, attempt + 1);
                return;
            }
            if (status == 429 && attempt < 4) {
                // Rate limit: wait as long as Spotify asks.
                const int wait = std::clamp(reply->rawHeader("Retry-After").toInt(), 1, 30);
                QTimer::singleShot(wait * 1000, this, [this, url, callback, attempt] { get(url, callback, attempt + 1); });
                return;
            }
            if (reply->error() != QNetworkReply::NoError) {
                QString error = json.value("error").toObject().value("message").toString();
                callback(json, error.isEmpty() ? reply->errorString() : error);
                return;
            }
            callback(json, {});
        });
    });
}

void SpotifyService::getAll(const QUrl &first, QJsonArray collected,
                            std::function<void(const QJsonArray &, const QString &)> done)
{
    get(first, [this, collected, done](const QJsonObject &json, const QString &error) mutable {
        if (!error.isEmpty()) {
            done(collected, error);
            return;
        }
        for (const QJsonValue &v : json.value("items").toArray())
            collected.append(v);
        const QString next = json.value("next").toString();
        if (next.isEmpty() || collected.size() >= 10000)
            done(collected, {});
        else
            getAll(QUrl(next), collected, done);
    });
}

// ---- sync ---------------------------------------------------------------------------

void SpotifyService::refresh()
{
    if (!connected() || m_syncing)
        return;
    m_syncing = true;
    m_syncError.clear();
    emit syncingChanged();
    get(apiUrl(QStringLiteral("/me")), [this](const QJsonObject &me, const QString &error) {
        if (!error.isEmpty()) {
            finishSync(error);
            return;
        }
        const QString name = me.value("display_name").toString(me.value("id").toString());
        if (name != m_accountName) {
            m_accountName = name;
            emit accountChanged();
        }
        syncPlaylists();
    });
}

void SpotifyService::syncPlaylists()
{
    getAll(apiUrl(QStringLiteral("/me/playlists?limit=50")), {}, [this](const QJsonArray &items, const QString &error) {
        if (!error.isEmpty()) {
            finishSync(error);
            return;
        }
        QVariantList playlists;
        QStringList changed;
        QSet<QString> present;
        for (const QJsonValue &v : items) {
            const QJsonObject p = v.toObject();
            const QString id = p.value("id").toString();
            if (id.isEmpty())
                continue;
            present.insert(id);
            const QJsonArray images = p.value("images").toArray();
            const int count = p.value("tracks").toObject().value("total").toInt(p.value("items").toObject().value("total").toInt());
            playlists << QVariantMap {
                { "id", id }, { "name", p.value("name").toString() }, { "count", count },
                { "cover", images.isEmpty() ? QString() : images.first().toObject().value("url").toString() } };
            const QString snapshot = p.value("snapshot_id").toString();
            if (snapshot != m_snapshots.value(id) || !m_playlistTracks.contains(id)) {
                changed << id;
                m_snapshots.insert(id, snapshot);
            }
        }
        // Playlists removed on Spotify disappear here too.
        for (auto it = m_playlistTracks.begin(); it != m_playlistTracks.end();) {
            if (!present.contains(it.key())) {
                m_snapshots.remove(it.key());
                it = m_playlistTracks.erase(it);
            } else {
                ++it;
            }
        }
        m_playlists = playlists;
        emit dataChanged();
        syncNextPlaylist(changed);
    });
}

void SpotifyService::syncNextPlaylist(QStringList queue)
{
    if (queue.isEmpty()) {
        syncLiked();
        return;
    }
    const QString id = queue.takeFirst();
    const QUrl url = apiUrl(QStringLiteral("/playlists/%1/tracks?limit=100&additional_types=track"
                                           "&fields=next,items(track(id,name,duration_ms,type,is_local,"
                                           "artists(name),album(name,images)))").arg(id));
    getAll(url, {}, [this, id, queue](const QJsonArray &items, const QString &error) {
        if (error.isEmpty()) {
            m_playlistTracks.insert(id, storeItems(items, true));
            emit dataChanged();
        } else {
            // e.g. Spotify's own editorial / algorithmic playlists aren't available to third-party apps.
            m_snapshots.remove(id);
            if (!m_playlistTracks.contains(id))
                m_playlistTracks.insert(id, {});
        }
        syncNextPlaylist(queue);
    });
}

void SpotifyService::syncLiked()
{
    getAll(apiUrl(QStringLiteral("/me/tracks?limit=50")), {}, [this](const QJsonArray &items, const QString &error) {
        if (error.isEmpty())
            m_liked = storeItems(items, true);
        finishSync(error);
    });
}

QStringList SpotifyService::storeItems(const QJsonArray &items, bool wrapped)
{
    QStringList ids;
    for (const QJsonValue &v : items) {
        const QJsonObject t = wrapped ? v.toObject().value("track").toObject() : v.toObject();
        const QString spotifyId = t.value("id").toString();
        if (spotifyId.isEmpty() || t.value("is_local").toBool() || (t.contains("type") && t.value("type").toString() != QLatin1String("track")))
            continue;   // podcast episodes, local files on someone's Spotify, removed tracks
        QStringList artists;
        for (const QJsonValue &a : t.value("artists").toArray())
            artists << a.toObject().value("name").toString();
        const QJsonObject album = t.value("album").toObject();
        const QJsonArray images = album.value("images").toArray();
        // Images come largest first — the middle size (~300 px) is plenty for covers.
        const QString cover = images.isEmpty() ? QString()
                            : images.at(std::min<int>(1, int(images.size()) - 1)).toObject().value("url").toString();
        const Track track = makeTrack(spotifyId, t.value("name").toString(), artists.join(QStringLiteral(", ")),
                                      album.value("name").toString(), t.value("duration_ms").toInt() / 1000, cover);
        m_tracks.insert(track.id, track);
        if (!ids.contains(track.id))
            ids << track.id;
    }
    return ids;
}

void SpotifyService::finishSync(const QString &error)
{
    m_syncing = false;
    m_syncError = error;
    if (error.isEmpty())
        m_lastSync = QDateTime::currentDateTime();
    // Drop tracks no longer in any playlist or the liked songs.
    QSet<QString> used(m_liked.cbegin(), m_liked.cend());
    for (const QStringList &ids : std::as_const(m_playlistTracks))
        for (const QString &id : ids)
            used.insert(id);
    for (auto it = m_tracks.begin(); it != m_tracks.end();)
        it = used.contains(it.key()) ? std::next(it) : m_tracks.erase(it);
    saveCache();
    emit syncingChanged();
    emit dataChanged();
    if (!error.isEmpty())
        emit notice(QStringLiteral("Spotify: ") + error, QStringLiteral("close"));
}

QList<Track> SpotifyService::tracks() const
{
    QList<Track> out;
    out.reserve(m_tracks.size());
    for (const Track &t : m_tracks)
        out << t;
    return out;
}

// ---- matching to YouTube Music --------------------------------------------------------

void SpotifyService::match(const QString &trackId, std::function<void(const QString &)> callback)
{
    if (m_matches.contains(trackId)) {
        const QString videoId = m_matches.value(trackId);
        QTimer::singleShot(0, this, [callback, videoId] { callback(videoId); });
        return;
    }
    auto &waiting = m_matching[trackId];
    waiting << callback;
    if (waiting.size() > 1)
        return;   // already searching

    const Track t = m_tracks.value(trackId);
    const QString query = t.artist.section(QStringLiteral(", "), 0, 0) + QLatin1Char(' ') + t.title;
    YouTubeService::instance()->api()->search(query, [this, trackId, t](const QList<Track> &results, const QString &error) {
        QString videoId;
        if (error.isEmpty()) {
            const int best = YtMusicApi::bestMatch(t, results);
            if (best >= 0)
                videoId = results.at(best).path;
            m_matches.insert(trackId, videoId);   // also "not found", so we don't search again
            m_matchesSave.start();
        }
        const auto callbacks = m_matching.take(trackId);
        for (const auto &cb : callbacks)
            cb(videoId);
    });
}

// ---- cache --------------------------------------------------------------------------

void SpotifyService::loadCache()
{
    QFile f(cacheFile());
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
        m_accountName = root.value("account").toString();
        for (const QJsonValue &v : root.value("tracks").toArray()) {
            const QJsonObject o = v.toObject();
            const Track t = makeTrack(o.value("id").toString(), o.value("title").toString(), o.value("artist").toString(),
                                      o.value("album").toString(), o.value("duration").toInt(), o.value("cover").toString());
            m_tracks.insert(t.id, t);
        }
        m_liked = fromArray(root.value("liked").toArray());
        m_playlists = root.value("playlists").toArray().toVariantList();
        const QJsonObject lists = root.value("playlistTracks").toObject();
        for (auto it = lists.begin(); it != lists.end(); ++it)
            m_playlistTracks.insert(it.key(), fromArray(it.value().toArray()));
        const QJsonObject snapshots = root.value("snapshots").toObject();
        for (auto it = snapshots.begin(); it != snapshots.end(); ++it)
            m_snapshots.insert(it.key(), it.value().toString());
        m_lastSync = QDateTime::fromString(root.value("lastSync").toString(), Qt::ISODate);
    }
    QFile m(matchesFile());
    if (m.open(QIODevice::ReadOnly)) {
        const QJsonObject root = QJsonDocument::fromJson(m.readAll()).object();
        for (auto it = root.begin(); it != root.end(); ++it)
            m_matches.insert(it.key(), it.value().toString());
    }
}

void SpotifyService::saveCache() const
{
    QJsonArray tracks;
    for (const Track &t : m_tracks)
        tracks.append(trackToJson(t));
    QJsonObject lists;
    for (auto it = m_playlistTracks.cbegin(); it != m_playlistTracks.cend(); ++it)
        lists.insert(it.key(), toArray(it.value()));
    QJsonObject snapshots;
    for (auto it = m_snapshots.cbegin(); it != m_snapshots.cend(); ++it)
        snapshots.insert(it.key(), it.value());
    const QJsonObject root {
        { "account", m_accountName }, { "tracks", tracks }, { "liked", toArray(m_liked) },
        { "playlists", QJsonArray::fromVariantList(m_playlists) }, { "playlistTracks", lists },
        { "snapshots", snapshots }, { "lastSync", m_lastSync.toString(Qt::ISODate) } };
    QSaveFile f(cacheFile());
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        f.commit();
    }
}

void SpotifyService::saveMatches() const
{
    QJsonObject root;
    for (auto it = m_matches.cbegin(); it != m_matches.cend(); ++it)
        root.insert(it.key(), it.value());
    QSaveFile f(matchesFile());
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        f.commit();
    }
}
