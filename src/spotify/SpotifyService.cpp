#include "SpotifyService.h"

#include "AppPaths.h"
#include "LibraryScanner.h"
#include "youtube/YouTubeService.h"
#include "youtube/YtCookies.h"
#include "youtube/YtMusicApi.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
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

#include <climits>
#include <cstdlib>
#include <utility>

namespace {

constexpr quint16 kRedirectPort = 43117;
const char *kScopes = "playlist-read-private playlist-read-collaborative user-library-read";

QString tokenFile() { return AppPaths::dataDir() + QStringLiteral("/spotify.token"); }
QString cacheFile() { return AppPaths::dataDir() + QStringLiteral("/spotify.json"); }
QString matchesFile() { return AppPaths::dataDir() + QStringLiteral("/spotify-matches.json"); }
QString browserProfile() { return AppPaths::dataDir() + QStringLiteral("/browser-spotify"); }

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

// Cover from the web player's image sources ({ url, width, height }) — the one closest to 300 px.
QString webImage(const QJsonArray &sources)
{
    QString best;
    int bestDiff = INT_MAX;
    for (const QJsonValue &v : sources) {
        const QJsonObject s = v.toObject();
        const int width = s.value("width").toInt(300);
        if (std::abs(width - 300) < bestDiff) {
            bestDiff = std::abs(width - 300);
            best = s.value("url").toString();
        }
    }
    return best;
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
    connect(&m_webLogin, &SpotifyWebLogin::statusChanged, this, &SpotifyService::loggingInChanged);
    connect(&m_webLogin, &SpotifyWebLogin::succeeded, this, &SpotifyService::onWebToken);
    connect(&m_webLogin, &SpotifyWebLogin::failed, this, &SpotifyService::onWebFailed);
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

// ---- sign-in through the browser (web player token) ---------------------------------

void SpotifyService::loginWithBrowser()
{
    if (loggingIn())
        return;
    m_browserLogin = true;
    emit loggingInChanged();
    m_webLogin.start(browserProfile());
}

void SpotifyService::onWebToken(const QString &token, const QDateTime &expiresUtc, const QJsonObject &web)
{
    qInfo().noquote() << "spotify: browser token received, valid until" << expiresUtc.toLocalTime().toString(Qt::ISODate)
                      << "| queries" << web.value("queries").toObject().keys().join(QLatin1Char(' '));
    m_accessToken = token;
    m_accessExpires = expiresUtc.addSecs(-60);
    m_webApi = web;
    m_webCaptured = true;
    if (!m_browserLogin) {   // an invisible renewal
        saveToken();
        deliverToken({});
        return;
    }
    m_browserLogin = false;
    m_web = true;
    m_refreshToken.clear();
    saveToken();
    emit loggingInChanged();
    emit accountChanged();
    emit notice(tr("Signed in to Spotify"), QStringLiteral("check"));
    refresh();
}

void SpotifyService::onWebFailed(const QString &reason)
{
    qWarning().noquote() << "spotify: browser token failed:" << reason;
    if (!m_browserLogin) {
        deliverToken(tr("Spotify session expired — sign in again (%1)").arg(reason));
        return;
    }
    m_browserLogin = false;
    emit loggingInChanged();
    emit notice(tr("Couldn't sign in to Spotify: %1").arg(reason), QStringLiteral("close"));
}

void SpotifyService::deliverToken(const QString &error)
{
    m_refreshing = false;
    const auto waiting = std::exchange(m_waitingForToken, {});
    for (const auto &w : waiting)
        w(error);
}

// ---- sign-in with an own Spotify app (OAuth 2.0 + PKCE) -----------------------------

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
    if (m_browserLogin)
        m_webLogin.cancel();   // -> onWebFailed
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
        m_web = false;
        QDir(browserProfile()).removeRecursively();
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
    // With the browser sign-in the captured queries matter too: if the library ones are missing,
    // capture again (once per run — the web player may simply not use them).
    const QJsonObject queries = m_webApi.value("queries").toObject();
    const bool webReady = !m_web || m_webCaptured
                       || (queries.contains("libraryV3") && queries.contains("fetchPlaylist") && queries.contains("fetchLibraryTracks"));
    if (!m_accessToken.isEmpty() && QDateTime::currentDateTimeUtc() < m_accessExpires && webReady) {
        next({});
        return;
    }
    m_waitingForToken << next;
    if (m_refreshing)
        return;
    m_refreshing = true;
    if (m_web) {
        // A new web player token from the signed-in browser profile (a few seconds, invisible).
        if (m_webLogin.running())
            deliverToken(tr("Spotify sign-in is in progress"));
        else
            m_webLogin.refresh(browserProfile());   // -> onWebToken / onWebFailed
        return;
    }
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
    form.addQueryItem(QStringLiteral("refresh_token"), m_refreshToken);
    form.addQueryItem(QStringLiteral("client_id"), m_clientId);
    tokenRequest(form, [this](bool ok, const QString &error) {
        deliverToken(ok ? QString() : tr("Spotify session expired — sign in again (%1)").arg(error));
    });
}

void SpotifyService::saveToken() const
{
    QJsonObject token { { "mode", m_web ? "web" : "app" } };
    if (m_web) {
        // The web player token is short-lived; kept so a restart within the hour needs no browser.
        token.insert("access_token", m_accessToken);
        token.insert("expires", m_accessExpires.toString(Qt::ISODate));
        token.insert("web", m_webApi);
    } else {
        token.insert("refresh_token", m_refreshToken);
        token.insert("client_id", m_clientId);
    }
    const QByteArray json = QJsonDocument(token).toJson(QJsonDocument::Compact);
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
    if (json.value("mode").toString() == QLatin1String("web")) {
        m_web = true;
        m_accessToken = json.value("access_token").toString();
        m_accessExpires = QDateTime::fromString(json.value("expires").toString(), Qt::ISODate);
        m_webApi = json.value("web").toObject();
        return;
    }
    m_refreshToken = json.value("refresh_token").toString();
    // The token belongs to the Spotify app it was issued for.
    const QString issuedFor = json.value("client_id").toString();
    if (!issuedFor.isEmpty() && issuedFor != m_clientId)
        m_clientId = issuedFor;
}

void SpotifyService::logout()
{
    if (m_webLogin.running()) {
        m_browserLogin = false;
        m_webLogin.cancel();
    }
    m_web = false;
    QDir(browserProfile()).removeRecursively();   // the session in the browser profile goes too
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
            const QByteArray body = reply->readAll();
            const QJsonObject json = QJsonDocument::fromJson(body).object();
            if (status != 200)
                qWarning().noquote() << "spotify:" << status << url.path() << "retry-after" << reply->rawHeader("Retry-After")
                                     << body.left(300).simplified();
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
    if (m_web) {
        syncWeb();
        return;
    }
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

// ---- sync through the web player's own queries (browser sign-in) ---------------------
//
// The public Web API answers the web player's token with "rate limit exceeded", so with the browser
// sign-in Nokturn asks the same GraphQL queries the web player asks (SpotifyWebLogin captured their
// names, hashes and headers). Their answers are internal and may change — parsing is lenient.

void SpotifyService::query(const QString &name, const QJsonObject &variables, JsonCallback callback, int attempt)
{
    withAccessToken([this, name, variables, callback, attempt](const QString &tokenError) {
        if (!tokenError.isEmpty()) {
            callback({}, tokenError);
            return;
        }
        const QJsonObject captured = m_webApi.value("queries").toObject().value(name).toObject();
        if (captured.isEmpty()) {
            callback({}, tr("the web player didn't use the %1 query").arg(name));
            return;
        }
        QJsonObject vars = captured.value("variables").toObject();
        for (auto it = variables.begin(); it != variables.end(); ++it)
            vars.insert(it.key(), it.value());
        const QJsonObject body {
            { "variables", vars }, { "operationName", name },
            { "extensions", QJsonObject { { "persistedQuery", QJsonObject {
                { "version", 1 }, { "sha256Hash", captured.value("hash") } } } } } };
        QNetworkRequest request(QUrl(captured.value("url").toString()));
        request.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());
        request.setRawHeader("Content-Type", "application/json;charset=UTF-8");
        request.setRawHeader("Accept", "application/json");
        const QJsonObject headers = m_webApi.value("headers").toObject();
        for (auto it = headers.begin(); it != headers.end(); ++it)
            request.setRawHeader(it.key().toUtf8(), it.value().toString().toUtf8());
        QNetworkReply *reply = m_net.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
        connect(reply, &QNetworkReply::finished, this, [this, reply, name, variables, callback, attempt] {
            reply->deleteLater();
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray data = reply->readAll();
            const QJsonObject json = QJsonDocument::fromJson(data).object();
            if (status != 200)
                qWarning().noquote() << "spotify:" << status << name << data.left(300).simplified();
            if ((status == 401 || status == 400) && attempt == 0) {
                m_accessToken.clear();   // expired, or the web player changed its queries — capture anew
                query(name, variables, callback, attempt + 1);
                return;
            }
            if (status == 429 && attempt < 4) {
                const int wait = std::clamp(reply->rawHeader("Retry-After").toInt(), 1, 30);
                QTimer::singleShot(wait * 1000, this, [this, name, variables, callback, attempt] {
                    query(name, variables, callback, attempt + 1);
                });
                return;
            }
            if (reply->error() != QNetworkReply::NoError) {
                callback(json, reply->errorString());
                return;
            }
            const QJsonArray errors = json.value("errors").toArray();
            if (!json.contains("data") && !errors.isEmpty()) {
                callback(json, errors.first().toObject().value("message").toString());
                return;
            }
            callback(json.value("data").toObject(), {});
        });
    });
}

void SpotifyService::syncWeb()
{
    query(QStringLiteral("profileAttributes"), {}, [this](const QJsonObject &data, const QString &error) {
        if (!error.isEmpty()) {
            finishSync(error);
            return;
        }
        const QJsonObject profile = data.value("me").toObject().value("profile").toObject();
        const QString name = profile.value("name").toString(profile.value("username").toString());
        if (!name.isEmpty() && name != m_accountName) {
            m_accountName = name;
            emit accountChanged();
        }
        syncWebLibrary(0, {});
    });
}

void SpotifyService::syncWebLibrary(int offset, QVariantList playlists)
{
    const QJsonObject vars { { "offset", offset }, { "limit", 50 }, { "flatten", true }, { "expandedFolders", QJsonArray() } };
    query(QStringLiteral("libraryV3"), vars, [this, offset, playlists](const QJsonObject &data, const QString &error) mutable {
        if (!error.isEmpty()) {
            finishSync(error);
            return;
        }
        const QJsonObject library = data.value("me").toObject().value("libraryV3").toObject();
        const QJsonArray items = library.value("items").toArray();
        if (items.isEmpty() && offset == 0)
            qWarning().noquote() << "spotify: libraryV3 without items:" << QJsonDocument(data).toJson(QJsonDocument::Compact).left(600);
        for (const QJsonValue &v : items) {
            const QJsonObject item = v.toObject().value("item").toObject();
            const QJsonObject p = item.value("data").toObject();
            const QString uri = item.value("_uri").toString(p.value("uri").toString());
            const QString type = p.value("__typename").toString();
            if (!uri.startsWith(QLatin1String("spotify:playlist:")) || (!type.isEmpty() && type != QLatin1String("Playlist")))
                continue;   // albums, artists, podcasts, folders, Liked Songs (synced separately)
            const QString id = uri.mid(17);
            const QJsonArray images = p.value("images").toObject().value("items").toArray();
            const QString cover = images.isEmpty() ? QString()
                                : webImage(images.first().toObject().value("sources").toArray());
            const QString name = p.value("name").toString();
            playlists << QVariantMap { { "id", id }, { "name", name.isEmpty() ? tr("Untitled playlist") : name },
                                       { "count", m_playlistTracks.value(id).size() }, { "cover", cover } };
        }
        const int total = library.value("totalCount").toInt();
        if (!items.isEmpty() && offset + items.size() < total) {
            syncWebLibrary(offset + int(items.size()), playlists);
            return;
        }
        QStringList queue;
        QSet<QString> present;
        for (const QVariant &p : std::as_const(playlists)) {
            queue << p.toMap().value("id").toString();
            present.insert(queue.last());
        }
        for (auto it = m_playlistTracks.begin(); it != m_playlistTracks.end();)
            it = present.contains(it.key()) ? std::next(it) : m_playlistTracks.erase(it);
        m_playlists = playlists;
        emit dataChanged();
        syncWebPlaylist(queue, 0, {});
    });
}

void SpotifyService::syncWebPlaylist(QStringList queue, int offset, QStringList ids)
{
    if (queue.isEmpty()) {
        syncWebLiked(0, {});
        return;
    }
    const QString id = queue.first();
    const QJsonObject vars { { "uri", QStringLiteral("spotify:playlist:") + id }, { "offset", offset }, { "limit", 100 } };
    query(QStringLiteral("fetchPlaylist"), vars, [this, queue, offset, ids, id](const QJsonObject &data, const QString &error) mutable {
        const QJsonObject content = data.value("playlistV2").toObject().value("content").toObject();
        const QJsonArray items = content.value("items").toArray();
        if (!error.isEmpty())
            qWarning().noquote() << "spotify: playlist" << id << error;
        for (const QJsonValue &v : items) {
            const QJsonObject track = v.toObject().value("itemV2").toObject().value("data").toObject();
            const QString trackId = storeWebTrack(track, track.value("uri").toString());
            if (!trackId.isEmpty() && !ids.contains(trackId))
                ids << trackId;
        }
        const int total = content.value("totalCount").toInt();
        if (error.isEmpty() && !items.isEmpty() && offset + items.size() < total && offset < 10000) {
            syncWebPlaylist(queue, offset + int(items.size()), ids);
            return;
        }
        if (error.isEmpty() || !m_playlistTracks.contains(id)) {
            m_playlistTracks.insert(id, ids);
            for (QVariant &p : m_playlists) {
                QVariantMap map = p.toMap();
                if (map.value("id").toString() == id) {
                    map.insert("count", ids.size());
                    p = map;
                }
            }
            emit dataChanged();
        }
        queue.removeFirst();
        syncWebPlaylist(queue, 0, {});
    });
}

void SpotifyService::syncWebLiked(int offset, QStringList ids)
{
    if (!m_webApi.value("queries").toObject().contains(QStringLiteral("fetchLibraryTracks"))) {
        qWarning() << "spotify: the web player didn't show the liked songs query";
        finishSync({});
        return;
    }
    const QJsonObject vars { { "offset", offset }, { "limit", 50 } };
    query(QStringLiteral("fetchLibraryTracks"), vars, [this, offset, ids](const QJsonObject &data, const QString &error) mutable {
        if (!error.isEmpty()) {
            finishSync(error);
            return;
        }
        const QJsonObject tracks = data.value("me").toObject().value("library").toObject().value("tracks").toObject();
        const QJsonArray items = tracks.value("items").toArray();
        if (items.isEmpty() && offset == 0)
            qWarning().noquote() << "spotify: liked songs without items:" << QJsonDocument(data).toJson(QJsonDocument::Compact).left(600);
        for (const QJsonValue &v : items) {
            const QJsonObject track = v.toObject().value("track").toObject();
            const QString trackId = storeWebTrack(track.value("data").toObject(), track.value("_uri").toString());
            if (!trackId.isEmpty() && !ids.contains(trackId))
                ids << trackId;
        }
        const int total = tracks.value("totalCount").toInt();
        if (!items.isEmpty() && offset + items.size() < total && offset < 10000) {
            syncWebLiked(offset + int(items.size()), ids);
            return;
        }
        m_liked = ids;
        finishSync({});
    });
}

QString SpotifyService::storeWebTrack(const QJsonObject &data, const QString &uri)
{
    const QString type = data.value("__typename").toString();
    if (!uri.startsWith(QLatin1String("spotify:track:")) || (!type.isEmpty() && type != QLatin1String("Track")))
        return {};   // podcast episodes, local files, unavailable tracks
    QStringList artists;
    for (const QJsonValue &a : data.value("artists").toObject().value("items").toArray())
        artists << a.toObject().value("profile").toObject().value("name").toString();
    const QJsonObject album = data.value("albumOfTrack").toObject();
    QJsonObject duration = data.value("trackDuration").toObject();
    if (duration.isEmpty())
        duration = data.value("duration").toObject();
    const Track track = makeTrack(uri.mid(14), data.value("name").toString(), artists.join(QStringLiteral(", ")),
                                  album.value("name").toString(),
                                  int(duration.value("totalMilliseconds").toDouble() / 1000),
                                  webImage(album.value("coverArt").toObject().value("sources").toArray()));
    m_tracks.insert(track.id, track);
    return track.id;
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
    qInfo().noquote() << "spotify: sync finished —" << (error.isEmpty() ? QStringLiteral("ok") : error)
                      << "| liked" << m_liked.size() << "playlists" << m_playlists.size();
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
