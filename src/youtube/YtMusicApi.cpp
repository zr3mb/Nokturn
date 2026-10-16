#include "YtMusicApi.h"

#include "LibraryScanner.h"

#include <QDate>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSet>

namespace {

const QString kOrigin = QStringLiteral("https://music.youtube.com");
const QByteArray kUserAgent =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/139.0.0.0 Safari/537.36";

// Bezpieczne zagłębianie się w JSON po ścieżce "a.b.0.c".
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

// Zbiera wszystkie obiekty o danym kluczu w dowolnym miejscu drzewa (kolejność dokumentu).
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
    // Miniatury z googleusercontent mają rozmiar w adresie — prosimy o większą.
    static const QRegularExpression size(QStringLiteral("=w\\d+-h\\d+"));
    url.replace(size, QStringLiteral("=w544-h544"));
    return url;
}

bool isNoiseRun(const QString &t)
{
    static const QRegularExpression punctuation(QStringLiteral("^[\\s•·,&|\\-–]*$"));
    if (punctuation.match(t).hasMatch() || t.trimmed() == QLatin1String("oraz") || t.trimmed() == QLatin1String("and"))
        return true;
    static const QSet<QString> words = {
        "Utwór", "Song", "Teledysk", "Video", "Film", "Odcinek", "Episode", "Singiel", "Single", "EP", "Album",
    };
    const QString s = t.trimmed();
    return s.isEmpty() || s == QLatin1String("•") || s == QLatin1String("&") || s == QLatin1String(",")
           || words.contains(s) || parseDuration(s) >= 0
           || s.contains(QLatin1String("wyświetl")) || s.contains(QLatin1String("views"))
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

    // Wyniki, które nie są utworami (wykonawca, album, playlista, profil, podcast).
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
            if (pageType == QLatin1String("MUSIC_PAGE_TYPE_ARTIST") || pageType == QLatin1String("MUSIC_PAGE_TYPE_USER_CHANNEL"))
                artists << text;
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
    t.quality = QStringLiteral("YouTube Music · strumień");
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
            { "hl", "pl" }, { "gl", "PL" } } },
        { "user", QJsonObject {} },
    });

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::UserAgentHeader, client.userAgent);
    request.setRawHeader("Origin", kOrigin.toLatin1());
    request.setRawHeader("X-Origin", kOrigin.toLatin1());
    request.setRawHeader("Referer", (kOrigin + '/').toLatin1());
    request.setRawHeader("Accept-Language", "pl-PL,pl;q=0.9");
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
                               ? QStringLiteral("Sesja YouTube wygasła — zaloguj się ponownie")
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

        // Kolejna strona: nowy styl (continuationCommand) albo stary (nextContinuationData).
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
        callback(account, error.isEmpty() && account.name.isEmpty() ? QStringLiteral("Brak danych konta") : error);
    });
}

void YtMusicApi::fetchLibraryPlaylists(std::function<void(const QVariantList &, const QString &)> callback)
{
    call(QStringLiteral("browse"), { { "browseId", "FEmusic_liked_playlists" } },
         [callback](const QJsonObject &json, const QString &error) {
             callback(error.isEmpty() ? parsePlaylists(json) : QVariantList(), error);
         });
}

void YtMusicApi::search(const QString &query, TracksCallback callback)
{
    call(QStringLiteral("search"), { { "query", query } }, [callback](const QJsonObject &json, const QString &error) {
        callback(error.isEmpty() ? parseTracks(json) : QList<Track>(), error);
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

// ---- teksty utworów -----------------------------------------------------------

Lyrics YtMusicApi::parseTimedLyrics(const QJsonValue &root)
{
    // Klient mobilny: ...timedLyricsModel.lyricsData.{timedLyricsData[], sourceMessage}
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
        // Linie "♪" oznaczają przerwę instrumentalną.
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
    // 1) "next" zwraca zakładki odtwarzacza; zakładka "Tekst" wskazuje browseId zaczynające się od "MPLY".
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

        // 2) Wersja z czasami (jak w aplikacji na telefon), 3) awaryjnie zwykły tekst.
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
