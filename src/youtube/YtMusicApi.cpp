#include "YtMusicApi.h"

#include "Language.h"
#include "LibraryScanner.h"
#include "lyrics/LyricsAligner.h"

#include <QCoreApplication>
#include <QDate>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSet>
#include <QLocale>

namespace {

const QString kOrigin = QStringLiteral("https://music.youtube.com");
const QByteArray kUserAgent =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/139.0.0.0 Safari/537.36";


// Content language follows the interface language (playlist names, lyric footers);
// the region comes from the system settings (what's available where).
QString hl() { return Language::instance()->effective(); }
QString gl()
{
    const QString code = QLocale::territoryToCode(QLocale::system().territory());
    return code.size() == 2 ? code : QStringLiteral("US");
}
// Safe descent into JSON along a path like "a.b.0.c".
QJsonValue dig(QJsonValue v, const QString &path)
{
    for (const QString &key : path.split('.')) {
        bool isIndex = false;
        const int idx = key.toInt(&isIndex);
        if (isIndex && v.isArray())
            v = v.toArray().at(idx);
        else if (v.isObject())
            v = v.toObject().value(key);
        else
            return {};
    }
    return v;
}

QString runsText(const QJsonValue &text)
{
    if (text.toObject().contains("simpleText"))
        return text.toObject().value("simpleText").toString();
    QString out;
    for (const QJsonValue &run : text.toObject().value("runs").toArray())
        out += run.toObject().value("text").toString();
    return out;
}

// Collects every object with the given key anywhere in the tree (document order).
void collect(const QJsonValue &v, const QString &key, QList<QJsonObject> &out)
{
    if (v.isObject()) {
        const QJsonObject o = v.toObject();
        for (auto it = o.begin(); it != o.end(); ++it) {
            if (it.key() == key && it.value().isObject())
                out << it.value().toObject();
            else
                collect(it.value(), key, out);
        }
    } else if (v.isArray()) {
        for (const QJsonValue &item : v.toArray())
            collect(item, key, out);
    }
}

int parseDuration(const QString &text)
{
    static const QRegularExpression re(QStringLiteral("^(?:(\\d+):)?(\\d{1,2}):(\\d{2})$"));
    const auto m = re.match(text.trimmed());
    if (!m.hasMatch())
        return -1;
    return m.captured(1).toInt() * 3600 + m.captured(2).toInt() * 60 + m.captured(3).toInt();
}

QString bigThumbnail(const QJsonArray &thumbnails)
{
    QString url = thumbnails.isEmpty() ? QString() : thumbnails.last().toObject().value("url").toString();
    // Thumbnails from googleusercontent have the size in the URL — we ask for a bigger one.
    static const QRegularExpression size(QStringLiteral("=w\\d+-h\\d+"));
    url.replace(size, QStringLiteral("=w544-h544"));
    return url;
}

bool isNoiseRun(const QString &t)
{
    static const QRegularExpression punctuation(QStringLiteral("^[\\s•·,&|\\-–]*$"));
    if (punctuation.match(t).hasMatch() || t.trimmed() == QLatin1String("oraz") || t.trimmed() == QLatin1String("and"))
        return true;
    // Labels of non-title runs in both interface languages YouTube may answer in.
    static const QSet<QString> words = {
        "Utwór", "Song", "Teledysk", "Video", "Film", "Odcinek", "Episode", "Singiel", "Single", "EP", "Album",   // i18n-ignore
    };
    const QString s = t.trimmed();
    return s.isEmpty() || s == QLatin1String("•") || s == QLatin1String("&") || s == QLatin1String(",")
           || words.contains(s) || parseDuration(s) >= 0
           || s.contains(QLatin1String("wyświetl")) || s.contains(QLatin1String("views"))   // i18n-ignore
           || s.contains(QLatin1String("odtworze")) || s.contains(QLatin1String("plays"));
}

std::optional<Track> parseItem(const QJsonObject &r)
{
    QString videoId = dig(r, "playlistItemData.videoId").toString();
    if (videoId.isEmpty())
        videoId = dig(r, "overlay.musicItemThumbnailOverlayRenderer.content.musicPlayButtonRenderer"
                         ".playNavigationEndpoint.watchEndpoint.videoId").toString();
    const QJsonArray flex = r.value("flexColumns").toArray();
    if (videoId.isEmpty())
        videoId = dig(flex, "0.musicResponsiveListItemFlexColumnRenderer.text.runs.0.navigationEndpoint"
                            ".watchEndpoint.videoId").toString();
    if (videoId.isEmpty() || flex.isEmpty())
        return std::nullopt;

    // Results that aren't tracks (artist, album, playlist, profile, podcast).
    static const QSet<QString> nonSongTypes = {
        "Wykonawca", "Artist", "Album", "Playlista", "Playlist", "Profil", "Profile", "Podcast", "Odcinek", "Episode",
    };
    const QString kind = dig(flex, "1.musicResponsiveListItemFlexColumnRenderer.text.runs.0.text").toString().trimmed();
    if (nonSongTypes.contains(kind))
        return std::nullopt;

    Track t;
    t.title = runsText(dig(flex, "0.musicResponsiveListItemFlexColumnRenderer.text")).trimmed();
    QStringList artists;
    QStringList fallbackArtists;
    for (int c = 1; c < flex.size(); ++c) {
        const QJsonArray runs = dig(flex, QStringLiteral("%1.musicResponsiveListItemFlexColumnRenderer.text.runs").arg(c)).toArray();
        for (const QJsonValue &rv : runs) {
            const QJsonObject run = rv.toObject();
            const QString text = run.value("text").toString();
            const QString pageType = dig(run, "navigationEndpoint.browseEndpoint.browseEndpointContextSupportedConfigs"
                                              ".browseEndpointContextMusicConfig.pageType").toString();
            if (pageType == QLatin1String("MUSIC_PAGE_TYPE_ARTIST") || pageType == QLatin1String("MUSIC_PAGE_TYPE_USER_CHANNEL")) {
                artists << text;
                if (t.artistId.isEmpty())
                    t.artistId = dig(run, "navigationEndpoint.browseEndpoint.browseId").toString();
            }
            else if (pageType == QLatin1String("MUSIC_PAGE_TYPE_ALBUM"))
                t.album = text;
            else if (const int d = parseDuration(text); d >= 0)
                t.duration = d;
            else if (c == 1 && !isNoiseRun(text) && fallbackArtists.isEmpty())
                fallbackArtists << text;
        }
    }
    const QJsonArray fixed = r.value("fixedColumns").toArray();
    if (!fixed.isEmpty()) {
        const int d = parseDuration(runsText(dig(fixed, "0.musicResponsiveListItemFixedColumnRenderer.text")));
        if (d >= 0)
            t.duration = d;
    }

    t.artist = !artists.isEmpty() ? artists.join(QStringLiteral(", "))
             : !fallbackArtists.isEmpty() ? fallbackArtists.first()
             : QStringLiteral("YouTube Music");
    t.id = QStringLiteral("yt:") + videoId;
    t.path = videoId;
    t.source = QStringLiteral("youtube");
    t.quality = QCoreApplication::translate("YouTubeService", "YouTube Music · stream");
    t.cover = bigThumbnail(dig(r, "thumbnail.musicThumbnailRenderer.thumbnail.thumbnails").toArray());

    const QString key = TrackModel::albumKey(t);
    const auto [a, b] = Scanner::fallbackPalette(key);
    t.colorA = a;
    t.colorB = b;
    t.seed = Scanner::stableSeed(key);
    return t;
}

QString clientVersion()
{
    return QStringLiteral("1.") + QDate::currentDate().addDays(-1).toString(QStringLiteral("yyyyMMdd")) + QStringLiteral(".01.00");
}

} // namespace

YtMusicApi::Client YtMusicApi::webClient()
{
    return { QStringLiteral("WEB_REMIX"), clientVersion(), "67", kUserAgent };
}

YtMusicApi::Client YtMusicApi::androidClient()
{
    const QString version = QStringLiteral("7.21.50");
    return { QStringLiteral("ANDROID_MUSIC"), version, "21",
             "com.google.android.apps.youtube.music/" + version.toLatin1() + " (Linux; U; Android 14) gzip" };
}

YtMusicApi::YtMusicApi(QObject *parent)
    : QObject(parent)
    , m_web(webClient())
{
}

void YtMusicApi::call(const QString &endpoint, QJsonObject body, JsonCallback callback, const QUrlQuery &query)
{
    callAs(m_web, endpoint, std::move(body), std::move(callback), query);
}

void YtMusicApi::callAs(const Client &client, const QString &endpoint, QJsonObject body, JsonCallback callback,
                        const QUrlQuery &query)
{
    QUrl url(kOrigin + QStringLiteral("/youtubei/v1/") + endpoint);
    QUrlQuery q = query;
    q.addQueryItem(QStringLiteral("prettyPrint"), QStringLiteral("false"));
    url.setQuery(q);

    body.insert("context", QJsonObject {
        { "client", QJsonObject {
            { "clientName", client.name }, { "clientVersion", client.version },
            { "hl", hl() }, { "gl", gl() } } },
        { "user", QJsonObject {} },
    });

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::UserAgentHeader, client.userAgent);
    request.setRawHeader("Origin", kOrigin.toLatin1());
    request.setRawHeader("X-Origin", kOrigin.toLatin1());
    request.setRawHeader("Referer", (kOrigin + '/').toLatin1());
    request.setRawHeader("Accept-Language", (hl() + QStringLiteral(";q=0.9")).toLatin1());
    request.setRawHeader("X-Youtube-Client-Name", client.id);
    request.setRawHeader("X-Youtube-Client-Version", client.version.toLatin1());
    if (m_cookies.isLoggedIn()) {
        request.setRawHeader("Cookie", m_cookies.headerFor(url));
        request.setRawHeader("Authorization", m_cookies.authorization(kOrigin));
        request.setRawHeader("X-Goog-AuthUser", "0");
    }

    QNetworkReply *reply = m_net.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [reply, callback] {
        reply->deleteLater();
        const QByteArray data = reply->readAll();
        const QJsonObject json = QJsonDocument::fromJson(data).object();
        if (reply->error() != QNetworkReply::NoError) {
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            QString message = dig(json, "error.message").toString();
            if (message.isEmpty())
                message = reply->errorString();
            callback(json, status == 401 || status == 403
                               ? tr("Your YouTube session has expired — sign in again")
                               : message);
            return;
        }
        callback(json, QString());
    });
}

QList<Track> YtMusicApi::parseTracks(const QJsonValue &root)
{
    QList<QJsonObject> items;
    collect(root, QStringLiteral("musicResponsiveListItemRenderer"), items);
    QList<Track> out;
    QSet<QString> seen;
    for (const QJsonObject &item : std::as_const(items)) {
        const auto t = parseItem(item);
        if (t && !seen.contains(t->id)) {
            seen.insert(t->id);
            out << *t;
        }
    }
    return out;
}

QVariantList YtMusicApi::parsePlaylists(const QJsonValue &root)
{
    QList<QJsonObject> items;
    collect(root, QStringLiteral("musicTwoRowItemRenderer"), items);
    QVariantList out;
    static const QRegularExpression number(QStringLiteral("(\\d[\\d\\s ]*)"));
    for (const QJsonObject &r : std::as_const(items)) {
        const QString browseId = dig(r, "navigationEndpoint.browseEndpoint.browseId").toString();
        if (!browseId.startsWith(QLatin1String("VL")) || browseId == QLatin1String("VLLM") || browseId == QLatin1String("VLSE"))
            continue;
        const QString subtitle = runsText(r.value("subtitle"));
        int count = -1;
        auto it = number.globalMatch(subtitle);
        while (it.hasNext())
            count = it.next().captured(1).remove(' ').remove(QChar(0xa0)).toInt();
        out << QVariantMap {
            { "id", browseId.mid(2) },
            { "title", runsText(r.value("title")) },
            { "subtitle", subtitle },
            { "count", count },
            { "cover", bigThumbnail(dig(r, "thumbnailRenderer.musicThumbnailRenderer.thumbnail.thumbnails").toArray()) },
        };
    }
    return out;
}

void YtMusicApi::browseTracks(const QString &browseId, TracksCallback callback, int maxPages)
{
    browsePage(browseId, QString(), false, {}, maxPages, std::move(callback));
}

void YtMusicApi::browsePage(const QString &browseId, const QString &continuation, bool commandStyle,
                            QList<Track> collected, int pagesLeft, TracksCallback callback)
{
    QJsonObject body;
    QUrlQuery query;
    if (continuation.isEmpty()) {
        body.insert("browseId", browseId);
    } else if (commandStyle) {
        body.insert("continuation", continuation);
    } else {
        query.addQueryItem("ctoken", continuation);
        query.addQueryItem("continuation", continuation);
        query.addQueryItem("type", "next");
    }

    call(QStringLiteral("browse"), body, [=, this](const QJsonObject &json, const QString &error) mutable {
        if (!error.isEmpty()) {
            callback(collected, error);
            return;
        }
        QSet<QString> seen;
        for (const Track &t : std::as_const(collected))
            seen.insert(t.id);
        for (const Track &t : parseTracks(json))
            if (!seen.contains(t.id))
                collected << t;

        // Next page: the new style (continuationCommand) or the old one (nextContinuationData).
        QList<QJsonObject> commands;
        collect(json, QStringLiteral("continuationCommand"), commands);
        QList<QJsonObject> legacy;
        collect(json, QStringLiteral("nextContinuationData"), legacy);
        QString next;
        bool nextCommand = false;
        if (!commands.isEmpty()) {
            next = commands.first().value("token").toString();
            nextCommand = true;
        } else if (!legacy.isEmpty()) {
            next = legacy.first().value("continuation").toString();
        }

        if (next.isEmpty() || pagesLeft <= 1)
            callback(collected, QString());
        else
            browsePage(browseId, next, nextCommand, collected, pagesLeft - 1, callback);
    }, query);
}

void YtMusicApi::fetchAccount(std::function<void(const Account &, const QString &)> callback)
{
    call(QStringLiteral("account/account_menu"), {}, [callback](const QJsonObject &json, const QString &error) {
        Account account;
        QList<QJsonObject> headers;
        collect(json, QStringLiteral("activeAccountHeaderRenderer"), headers);
        if (!headers.isEmpty()) {
            const QJsonObject h = headers.first();
            account.name = runsText(h.value("accountName"));
            account.handle = runsText(h.value("channelHandle"));
            account.photo = bigThumbnail(dig(h, "accountPhoto.thumbnails").toArray());
        }
        callback(account, error.isEmpty() && account.name.isEmpty() ? tr("No account data") : error);
    });
}

void YtMusicApi::fetchLibraryPlaylists(std::function<void(const QVariantList &, const QString &)> callback)
{
    call(QStringLiteral("browse"), { { "browseId", "FEmusic_liked_playlists" } },
         [callback](const QJsonObject &json, const QString &error) {
             callback(error.isEmpty() ? parsePlaylists(json) : QVariantList(), error);
         });
}

void YtMusicApi::search(const QString &query, TracksCallback callback, const QString &filter)
{
    QJsonObject body { { "query", query } };
    // The search filter chips ("Songs", "Videos") of music.youtube.com.
    if (filter == QLatin1String("songs"))
        body.insert("params", QStringLiteral("EgWKAQIIAWoMEA4QChADEAQQCRAF"));
    else if (filter == QLatin1String("videos"))
        body.insert("params", QStringLiteral("EgWKAQIQAWoMEA4QChADEAQQCRAF"));
    call(QStringLiteral("search"), body, [callback](const QJsonObject &json, const QString &error) {
        callback(error.isEmpty() ? parseTracks(json) : QList<Track>(), error);
    });
}

// ---- artists --------------------------------------------------------------------

namespace {

QString lastThumbnail(const QJsonValue &thumbnails)
{
    const QJsonArray arr = thumbnails.toArray();
    return arr.isEmpty() ? QString() : arr.last().toObject().value("url").toString();
}

QString pageTypeOf(const QJsonObject &browseEndpoint)
{
    return dig(browseEndpoint, "browseEndpointContextSupportedConfigs.browseEndpointContextMusicConfig.pageType").toString();
}

// One card of a carousel (album, single, playlist, related artist); empty for videos and other items.
QVariantMap parseCard(const QJsonObject &r)
{
    QJsonObject browse = dig(r, "navigationEndpoint.browseEndpoint").toObject();
    if (browse.isEmpty())
        browse = dig(r, "title.runs.0.navigationEndpoint.browseEndpoint").toObject();
    const QString browseId = browse.value("browseId").toString();
    const QString type = pageTypeOf(browse);
    QString kind;
    if (type == QLatin1String("MUSIC_PAGE_TYPE_ALBUM"))
        kind = QStringLiteral("album");
    else if (type == QLatin1String("MUSIC_PAGE_TYPE_PLAYLIST"))
        kind = QStringLiteral("playlist");
    else if (type == QLatin1String("MUSIC_PAGE_TYPE_ARTIST") || type == QLatin1String("MUSIC_PAGE_TYPE_USER_CHANNEL"))
        kind = QStringLiteral("artist");
    if (kind.isEmpty() || browseId.isEmpty())
        return {};
    // Albums carry their track list as a playlist ("OLAK5uy_…") on the play button; playlists are "VL<id>".
    QString playlistId = dig(r, "thumbnailOverlay.musicItemThumbnailOverlayRenderer.content.musicPlayButtonRenderer"
                                ".playNavigationEndpoint.watchPlaylistEndpoint.playlistId").toString();
    if (playlistId.isEmpty() && kind == QLatin1String("playlist") && browseId.startsWith(QLatin1String("VL")))
        playlistId = browseId.mid(2);
    return {
        { "kind", kind }, { "browseId", browseId }, { "playlistId", playlistId },
        { "title", runsText(r.value("title")) }, { "subtitle", runsText(r.value("subtitle")) },
        { "cover", lastThumbnail(dig(r, "thumbnailRenderer.musicThumbnailRenderer.thumbnail.thumbnails")) },
    };
}

} // namespace

YtMusicApi::Artist YtMusicApi::parseArtist(const QJsonObject &root)
{
    Artist a;
    QJsonObject header = dig(root, "header.musicImmersiveHeaderRenderer").toObject();
    if (header.isEmpty())
        header = dig(root, "header.musicVisualHeaderRenderer").toObject();
    a.name = runsText(header.value("title"));
    a.description = runsText(header.value("description")).trimmed();
    a.subscribers = runsText(dig(header, "subscriptionButton.subscribeButtonRenderer.longSubscriberCountText"));
    if (a.subscribers.isEmpty())
        a.subscribers = runsText(dig(header, "subscriptionButton.subscribeButtonRenderer.subscriberCountText"));
    a.photo = lastThumbnail(dig(header, "thumbnail.musicThumbnailRenderer.thumbnail.thumbnails"));
    if (a.photo.isEmpty())
        a.photo = lastThumbnail(dig(header, "foregroundThumbnail.musicThumbnailRenderer.thumbnail.thumbnails"));

    const QJsonArray sections = dig(root, "contents.singleColumnBrowseResultsRenderer.tabs.0.tabRenderer.content"
                                          ".sectionListRenderer.contents").toArray();
    for (const QJsonValue &sv : sections) {
        const QJsonObject s = sv.toObject();
        if (s.contains("musicShelfRenderer")) {
            const QJsonObject shelf = s.value("musicShelfRenderer").toObject();
            if (a.songs.isEmpty())
                a.songs = parseTracks(shelf);
            const QString all = dig(shelf, "bottomEndpoint.browseEndpoint.browseId").toString();
            if (a.songsPlaylistId.isEmpty() && all.startsWith(QLatin1String("VL")))
                a.songsPlaylistId = all.mid(2);
        } else if (s.contains("musicCarouselShelfRenderer")) {
            const QJsonObject shelf = s.value("musicCarouselShelfRenderer").toObject();
            QVariantList items;
            QString kind;
            for (const QJsonValue &iv : shelf.value("contents").toArray()) {
                const QVariantMap card = parseCard(iv.toObject().value("musicTwoRowItemRenderer").toObject());
                if (card.isEmpty())
                    continue;
                items << card;
                const QString k = card.value("kind").toString();
                if (kind.isEmpty())
                    kind = k == QLatin1String("album") ? QStringLiteral("albums")
                         : k == QLatin1String("artist") ? QStringLiteral("artists") : QStringLiteral("playlists");
            }
            if (items.isEmpty())
                continue;   // videos, live performances…
            a.sections << QVariantMap {
                { "title", runsText(dig(shelf, "header.musicCarouselShelfBasicHeaderRenderer.title")) },
                { "kind", kind }, { "items", items } };
        }
    }
    return a;
}

void YtMusicApi::fetchArtist(const QString &browseId, std::function<void(const Artist &, const QString &)> callback)
{
    call(QStringLiteral("browse"), { { "browseId", browseId } },
         [browseId, callback](const QJsonObject &json, const QString &error) {
        if (!error.isEmpty()) {
            callback({}, error);
            return;
        }
        Artist a = parseArtist(json);
        a.browseId = browseId;
        callback(a, a.name.isEmpty() ? tr("No artist data") : QString());
    });
}

void YtMusicApi::findArtistId(const QString &name, std::function<void(const QString &, const QString &)> callback)
{
    call(QStringLiteral("search"), { { "query", name } }, [name, callback](const QJsonObject &json, const QString &error) {
        if (!error.isEmpty()) {
            callback({}, error);
            return;
        }
        // Every run that links to an artist page — the first one named exactly like the artist wins.
        std::function<QString(const QJsonValue &)> walk = [&](const QJsonValue &v) -> QString {
            if (v.isObject()) {
                const QJsonObject o = v.toObject();
                const QJsonObject browse = dig(o, "navigationEndpoint.browseEndpoint").toObject();
                const QString type = pageTypeOf(browse);
                if ((type == QLatin1String("MUSIC_PAGE_TYPE_ARTIST") || type == QLatin1String("MUSIC_PAGE_TYPE_USER_CHANNEL"))
                    && o.value("text").toString().compare(name, Qt::CaseInsensitive) == 0)
                    return browse.value("browseId").toString();
                for (auto it = o.begin(); it != o.end(); ++it)
                    if (const QString id = walk(it.value()); !id.isEmpty())
                        return id;
            } else if (v.isArray()) {
                for (const QJsonValue &item : v.toArray())
                    if (const QString id = walk(item); !id.isEmpty())
                        return id;
            }
            return {};
        };
        callback(walk(json), QString());
    });
}

void YtMusicApi::setLiked(const QString &videoId, bool liked)
{
    if (!authenticated())
        return;
    call(liked ? QStringLiteral("like/like") : QStringLiteral("like/removelike"),
         { { "target", QJsonObject { { "videoId", videoId } } } },
         [](const QJsonObject &, const QString &) {});
}

// ---- lyrics -------------------------------------------------------------------

Lyrics YtMusicApi::parseTimedLyrics(const QJsonValue &root)
{
    // Mobile client: ...timedLyricsModel.lyricsData.{timedLyricsData[], sourceMessage}
    QList<QJsonObject> models;
    collect(root, QStringLiteral("timedLyricsModel"), models);
    Lyrics out;
    if (models.isEmpty())
        return out;
    const QJsonObject data = models.first().value("lyricsData").toObject();
    for (const QJsonValue &v : data.value("timedLyricsData").toArray()) {
        const QJsonObject o = v.toObject();
        const QJsonObject cue = o.value("cueRange").toObject();
        const qint64 start = cue.value("startTimeMilliseconds").toVariant().toLongLong();
        const qint64 end = cue.value("endTimeMilliseconds").toVariant().toLongLong();
        const QString text = o.value("lyricLine").toString().trimmed();
        // "♪" lines mark an instrumental break.
        if (text.isEmpty() || text == QStringLiteral("\u266A")) {
            if (!out.lines.isEmpty() && !out.lines.last().text.isEmpty())
                out.lines << LyricLine {};
            continue;
        }
        out.lines << LyricLine { start, end > start ? end : -1, text };
    }
    while (!out.lines.isEmpty() && out.lines.last().text.isEmpty())
        out.lines.removeLast();
    if (out.isEmpty())
        return {};
    out.synced = true;
    out.source = data.value("sourceMessage").toString();
    out.origin = QStringLiteral("youtube");
    out.fillEnds();
    return out;
}

Lyrics YtMusicApi::parsePlainLyrics(const QJsonValue &root)
{
    QList<QJsonObject> shelves;
    collect(root, QStringLiteral("musicDescriptionShelfRenderer"), shelves);
    if (shelves.isEmpty())
        return {};
    Lyrics out = Lyrics::fromPlainText(runsText(shelves.first().value("description")));
    if (out.isEmpty())
        return {};
    out.source = runsText(shelves.first().value("footer")).trimmed();
    out.origin = QStringLiteral("youtube");
    return out;
}

void YtMusicApi::fetchLyrics(const QString &videoId, LyricsCallback callback)
{
    // 1) "next" returns the player tabs; the "Lyrics" tab points to a browseId starting with "MPLY".
    const QJsonObject body { { "videoId", videoId }, { "isAudioOnly", true } };
    call(QStringLiteral("next"), body, [this, callback](const QJsonObject &json, const QString &error) {
        if (!error.isEmpty()) {
            callback({}, error);
            return;
        }
        QList<QJsonObject> endpoints;
        collect(json, QStringLiteral("browseEndpoint"), endpoints);
        QString browseId;
        for (const QJsonObject &e : std::as_const(endpoints)) {
            const QString id = e.value("browseId").toString();
            if (id.startsWith(QLatin1String("MPLY"))) {
                browseId = id;
                break;
            }
        }
        if (browseId.isEmpty()) {
            callback({}, QString());
            return;
        }

        // 2) The timed version (like in the phone app), 3) plain text as a fallback.
        const QJsonObject browse { { "browseId", browseId } };
        callAs(androidClient(), QStringLiteral("browse"), browse,
               [this, browse, callback](const QJsonObject &timedJson, const QString &) {
            const Lyrics timed = parseTimedLyrics(timedJson);
            if (!timed.isEmpty()) {
                callback(timed, QString());
                return;
            }
            call(QStringLiteral("browse"), browse, [callback](const QJsonObject &plainJson, const QString &plainError) {
                callback(plainError.isEmpty() ? parsePlainLyrics(plainJson) : Lyrics(), plainError);
            });
        });
    });
}

// ---- matching local files -----------------------------------------------------

namespace {

// "Title (feat. X) - Remastered 2011" -> "title"
QString coreTitle(const QString &title)
{
    static const QRegularExpression extras(
        QStringLiteral("\\s*(\\(|\\[|\\s-\\s).*(feat|ft\\.|remaster|live|version|wersja|edit|mix|mono|stereo|deluxe).*$"),
        QRegularExpression::CaseInsensitiveOption);
    // Music videos: "Title (Official Video)", "Title [Lyrics]", "Title | 4K"
    static const QRegularExpression videoTags(
        QStringLiteral("\\s*(\\(|\\[|\\|)[^)\\]|]*(official|video|audio|lyric|visuali[sz]er|teledysk|\\bhd\\b|\\b4k\\b|\\bmv\\b)[^)\\]]*(\\)|\\]|$)"),
        QRegularExpression::CaseInsensitiveOption);
    // "Title feat. X" without brackets (video uploads)
    static const QRegularExpression bareFeat(QStringLiteral("\\s+(feat\\.?|ft\\.)\\s.*$"), QRegularExpression::CaseInsensitiveOption);
    QString t = title;
    t.remove(videoTags);
    t.remove(extras);
    t.remove(bareFeat);
    return LyricsAligner::normalize(t);
}

// "Artist - Title (Official Video)" (video uploads) -> "Title (Official Video)"
QString withoutArtistPrefix(const QString &title, const QString &artist)
{
    static const QRegularExpression dash(QStringLiteral("\\s[-–—]\\s"));
    const auto m = dash.match(title);
    if (!m.hasMatch() || artist.isEmpty())
        return title;
    const QString left = LyricsAligner::normalize(title.left(m.capturedStart()));
    if (left.isEmpty() || !(left.contains(artist) || artist.contains(left)))
        return title;
    return title.mid(m.capturedEnd());
}

// The kind of version a title names ("live", "remix", "cover", "spedup"…). Words that are rare in
// real titles count anywhere; the ambiguous ones ("Live Forever", "Cover Me") only in the extras —
// brackets or after " - ".
QSet<QString> variantTags(const QString &title)
{
    static const QRegularExpression anywhere(
        QStringLiteral("\\b(sped[\\s-]*up|speed[\\s-]*up|slowed|reverb|nightcore|karaoke|8d(?:\\s*audio)?|bass[\\s-]*boosted|daycore)\\b"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression inExtras(
        QStringLiteral("\\b(cover|live|remix|acoustic|instrumental|unplugged|mashup|tribute|demo|piano version|orchestral)\\b"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression extrasStart(QStringLiteral("\\(|\\[|\\s[-–—|]\\s"));

    QSet<QString> tags;
    const auto add = [&tags](const QRegularExpression &re, const QString &text) {
        auto it = re.globalMatch(text);
        while (it.hasNext()) {
            QString tag = LyricsAligner::normalize(it.next().captured(1));
            if (tag == QLatin1String("speedup"))
                tag = QStringLiteral("spedup");
            else if (tag.startsWith(QLatin1String("8d")))
                tag = QStringLiteral("8d");
            tags.insert(tag);
        }
    };
    add(anywhere, title);
    if (const int at = int(title.indexOf(extrasStart)); at >= 0)
        add(inExtras, title.mid(at));
    return tags;
}

} // namespace

double YtMusicApi::matchScore(const Track &local, const Track &result)
{
    const QString artist = LyricsAligner::normalize(local.artist);
    const QString resultTitle = withoutArtistPrefix(
        result.title, LyricsAligner::normalize(local.artist.section(QStringLiteral(", "), 0, 0)));
    const double ts = LyricsAligner::similarity(coreTitle(local.title), coreTitle(resultTitle));
    if (ts < 0.6)
        return -1;
    const QString ra = LyricsAligner::normalize(result.artist);
    const double as = artist.isEmpty() ? 0.5
                    : ra.isEmpty() ? 0.0
                    : (ra.contains(artist) || artist.contains(ra)) ? 1.0
                    : LyricsAligner::similarity(artist, ra);
    double score = ts * 2 + as;
    if (as < 0.5)
        score -= 0.5;   // someone else's recording of the same title (a cover)
    if (local.duration > 0 && result.duration > 0) {
        const int diff = std::abs(local.duration - result.duration);
        if (diff > 12)
            return -1;   // a different version (e.g. live)
        score += 1.0 - diff / 12.0;
    }

    // A cover / live / remix / sped up version when the original was asked for (or the other way round).
    const QSet<QString> wanted = variantTags(local.title);
    const QSet<QString> found = variantTags(resultTitle) + variantTags(result.artist);
    if (!(found - wanted).isEmpty())
        score -= 1.5;
    if (!(wanted - found).isEmpty())
        score -= 0.75;
    return score;
}

int YtMusicApi::bestMatch(const Track &local, const QList<Track> &results)
{
    int best = -1;
    double bestScore = 0;
    for (int i = 0; i < results.size(); ++i) {
        const double score = matchScore(local, results.at(i));
        if (score > bestScore) {
            bestScore = score;
            best = i;
        }
    }
    return bestScore >= kMatchThreshold ? best : -1;
}
