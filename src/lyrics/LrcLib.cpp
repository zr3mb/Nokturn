#include "LrcLib.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QUrlQuery>

#include <cmath>

namespace {

const char *kApi = "https://lrclib.net/api";

} // namespace

LrcLib::LrcLib(QObject *parent)
    : QObject(parent)
{
}

QString LrcLib::cleanTitle(const QString &title)
{
    // Brackets with video / version noise, then "feat." tails and " - Remastered 2011" style suffixes.
    static const QRegularExpression noise(
        QStringLiteral("\\s*[\\(\\[][^\\)\\]]*(official|video|audio|lyric|visuali[sz]er|remaster|feat\\.?|ft\\.|with |prod\\.|explicit|clean|hd|4k|mv)[^\\)\\]]*[\\)\\]]"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression feat(QStringLiteral("\\s+(feat\\.?|ft\\.|featuring)\\s.*$"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression suffix(QStringLiteral("\\s+-\\s+.*(remaster|version|edit|mix|live|mono|stereo).*$"),
                                           QRegularExpression::CaseInsensitiveOption);
    QString t = title;
    t.remove(noise);
    t.remove(feat);
    t.remove(suffix);
    return t.trimmed().isEmpty() ? title.trimmed() : t.trimmed();
}

QString LrcLib::firstArtist(const QString &artist)
{
    static const QRegularExpression separators(QStringLiteral(",\\s+| & | x | feat\\.? | ft\\. "), QRegularExpression::CaseInsensitiveOption);
    return artist.split(separators).value(0).trimmed();
}

void LrcLib::get(const QUrl &url, std::function<void(const QByteArray &, int)> done)
{
    QNetworkRequest request(url);
    // LRCLIB asks clients to identify themselves.
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Nokturn/%1 (https://github.com/zr3mb/Nokturn)").arg(QCoreApplication::applicationVersion()));
    request.setTransferTimeout(12000);
    QNetworkReply *reply = m_net.get(request);
    connect(reply, &QNetworkReply::finished, this, [reply, done] {
        reply->deleteLater();
        done(reply->readAll(), reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
    });
}

Lyrics LrcLib::toLyrics(const QJsonObject &record)
{
    Lyrics l;
    const QString synced = record.value("syncedLyrics").toString();
    if (!synced.trimmed().isEmpty()) {
        l = Lyrics::fromLrc(synced);
    } else {
        const QString plain = record.value("plainLyrics").toString();
        if (!plain.trimmed().isEmpty())
            l = Lyrics::fromPlainText(plain);
    }
    if (!l.isEmpty()) {
        l.source = QCoreApplication::translate("LyricsService", "Source: %1").arg(QStringLiteral("LRCLIB"));
        l.origin = QStringLiteral("lrclib");
    }
    return l;
}

void LrcLib::fetch(const QString &artist, const QString &title, const QString &album, int durationSec,
                   std::function<void(const Lyrics &)> done)
{
    const QString name = cleanTitle(title);
    const QString by = firstArtist(artist);
    if (name.isEmpty() || by.isEmpty()) {
        done({});
        return;
    }

    // Search: the result closest in duration (a live or extended version has other timing).
    auto search = [this, name, by, durationSec, done](const Lyrics &fallback) {
        QUrl url(QString::fromLatin1(kApi) + QStringLiteral("/search"));
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("track_name"), name);
        q.addQueryItem(QStringLiteral("artist_name"), by);
        url.setQuery(q);
        get(url, [durationSec, done, fallback](const QByteArray &body, int status) {
            const QJsonArray results = status == 200 ? QJsonDocument::fromJson(body).array() : QJsonArray();
            QJsonObject best;
            double bestDiff = 1e9;
            bool bestSynced = false;
            for (const QJsonValue &v : results) {
                const QJsonObject r = v.toObject();
                if (r.value("instrumental").toBool())
                    continue;
                const double diff = durationSec > 0 ? std::abs(r.value("duration").toDouble() - durationSec) : 0.0;
                if (diff > 12)
                    continue;
                const bool synced = !r.value("syncedLyrics").toString().trimmed().isEmpty();
                // Timed lyrics first, then the closest duration.
                if (best.isEmpty() || (synced && !bestSynced) || (synced == bestSynced && diff < bestDiff)) {
                    best = r;
                    bestDiff = diff;
                    bestSynced = synced;
                }
            }
            const Lyrics found = best.isEmpty() ? Lyrics() : toLyrics(best);
            done(found.synced || fallback.isEmpty() ? found : fallback);
        });
    };

    if (durationSec <= 0) {
        search({});
        return;
    }
    // Exact lookup first (artist + title + duration ±2 s).
    QUrl url(QString::fromLatin1(kApi) + QStringLiteral("/get"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("track_name"), name);
    q.addQueryItem(QStringLiteral("artist_name"), by);
    if (!album.trimmed().isEmpty())
        q.addQueryItem(QStringLiteral("album_name"), album.trimmed());
    q.addQueryItem(QStringLiteral("duration"), QString::number(durationSec));
    url.setQuery(q);
    get(url, [search, done](const QByteArray &body, int status) {
        const Lyrics l = status == 200 ? toLyrics(QJsonDocument::fromJson(body).object()) : Lyrics();
        if (l.synced)
            done(l);
        else
            search(l);   // not found, or only plain text — a search may find a timed version
    });
}
