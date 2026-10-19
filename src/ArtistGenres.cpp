#include "ArtistGenres.h"

#include "AppPaths.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkReply>
#include <QSaveFile>
#include <QUrlQuery>

namespace {

QString stateFile() { return AppPaths::dataDir() + QStringLiteral("/artist-genres.json"); }

constexpr int kPaceMs = 3200;                                // ~19 requests a minute
constexpr int kBackoffMs = 60'000;                           // after "too many requests"
constexpr qint64 kKeepFoundMs = 180LL * 24 * 3600 * 1000;    // a genre is asked again after half a year
constexpr qint64 kKeepMissingMs = 30LL * 24 * 3600 * 1000;   // an unknown artist after a month

} // namespace

ArtistGenres *ArtistGenres::instance()
{
    static ArtistGenres *inst = new ArtistGenres;
    return inst;
}

ArtistGenres::ArtistGenres(QObject *parent)
    : QObject(parent)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &ArtistGenres::next);
    m_notify.setSingleShot(true);
    m_notify.setInterval(2500);
    connect(&m_notify, &QTimer::timeout, this, [this] {
        save();
        ++m_revision;
        emit changed();
    });
    load();
}

QString ArtistGenres::keyOf(const QString &artist)
{
    return artist.simplified().toLower();
}

QString ArtistGenres::genreOf(const QString &artist) const
{
    return m_known.value(keyOf(artist)).genre;
}

bool ArtistGenres::fresh(const QString &key) const
{
    const auto it = m_known.constFind(key);
    if (it == m_known.cend())
        return false;
    const qint64 age = QDateTime::currentMSecsSinceEpoch() - it->checked;
    return age < (it->genre.isEmpty() ? kKeepMissingMs : kKeepFoundMs);
}

void ArtistGenres::request(const QStringList &artists)
{
    for (const QString &name : artists) {
        const QString key = keyOf(name);
        if (key.isEmpty() || m_queued.contains(key) || fresh(key))
            continue;
        m_queued.insert(key);
        m_queue << name.simplified();
    }
    if (!m_busy && !m_timer.isActive() && !m_queue.isEmpty())
        m_timer.start(0);
}

void ArtistGenres::next()
{
    if (m_busy || m_queue.isEmpty())
        return;
    const QString name = m_queue.takeFirst();
    const QString key = keyOf(name);

    QUrl url(QStringLiteral("https://itunes.apple.com/search"));
    const QString country = QLocale::territoryToCode(QLocale::system().territory());
    url.setQuery(QUrlQuery {
        { QStringLiteral("term"), name },
        { QStringLiteral("entity"), QStringLiteral("musicArtist") },
        { QStringLiteral("limit"), QStringLiteral("5") },
        { QStringLiteral("country"), country.size() == 2 ? country : QStringLiteral("US") },
    });
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QByteArrayLiteral("Nokturn/" NOKTURN_VERSION));
    request.setTransferTimeout(15'000);
    m_busy = true;
    QNetworkReply *reply = m_net.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, name, key] {
        reply->deleteLater();
        m_busy = false;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 403 || status == 429) {
            m_queue.prepend(name);   // too many requests — wait, then go on
            m_timer.start(kBackoffMs);
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            // No network: stop here; the next look at the genres queues the rest again.
            m_queue.clear();
            m_queued.clear();
            return;
        }
        m_queued.remove(key);
        // The result named exactly like the artist (iTunes also returns similar names).
        QString genre;
        const QJsonArray results = QJsonDocument::fromJson(reply->readAll()).object().value("results").toArray();
        for (const QJsonValue &v : results) {
            const QJsonObject r = v.toObject();
            if (keyOf(r.value("artistName").toString()) == key) {
                genre = r.value("primaryGenreName").toString().trimmed();
                break;
            }
        }
        if (genre.compare(QLatin1String("undefined"), Qt::CaseInsensitive) == 0)
            genre.clear();
        m_known.insert(key, { genre, QDateTime::currentMSecsSinceEpoch() });
        m_notify.start();
        if (!m_queue.isEmpty())
            m_timer.start(kPaceMs);
    });
}

void ArtistGenres::load()
{
    QFile file(stateFile());
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    const QJsonObject artists = root.value("artists").toObject();
    for (auto it = artists.begin(); it != artists.end(); ++it) {
        const QJsonObject o = it.value().toObject();
        m_known.insert(it.key(), { o.value("genre").toString(), qint64(o.value("checked").toDouble()) });
    }
}

void ArtistGenres::save() const
{
    QJsonObject artists;
    for (auto it = m_known.cbegin(); it != m_known.cend(); ++it)
        artists.insert(it.key(), QJsonObject { { "genre", it->genre }, { "checked", double(it->checked) } });
    QSaveFile file(stateFile());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(QJsonObject { { "artists", artists } }).toJson(QJsonDocument::Compact));
        file.commit();
    }
}
