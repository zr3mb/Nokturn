#include "AudioCache.h"

#include "AppPaths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QThreadPool>

#include <algorithm>

namespace {

constexpr qint64 kDefaultLimit = qint64(1024) * 1024 * 1024;
constexpr qint64 kPlayWorth = 2 * 24 * 3600;   // each play keeps a track as if it was used two days later
constexpr int kMaxCountedPlays = 30;

} // namespace

AudioCache::AudioCache(QObject *parent)
    : QObject(parent)
{
    // A test profile keeps its own cache; otherwise the system's cache location (not the roaming profile).
    const QString profile = qEnvironmentVariable("NOKTURN_PROFILE");
    m_dir = (profile.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                               : AppPaths::dataDir() + QStringLiteral("/cache"))
            + QStringLiteral("/audio");
    QDir().mkpath(m_dir);
    m_limit = AppPaths::settings()->value(QStringLiteral("cache/audioLimit"), kDefaultLimit).toLongLong();
    load();
    trim();
}

bool AudioCache::validId(const QString &id)
{
    static const QRegularExpression re(QStringLiteral("^[A-Za-z0-9_-]{6,64}$"));
    return re.match(id).hasMatch();
}

QString AudioCache::pathOf(const QString &id) const
{
    return m_dir + QChar('/') + id + QStringLiteral(".m4a");
}

void AudioCache::load()
{
    QHash<QString, QJsonObject> index;
    QFile file(m_dir + QStringLiteral("/index.json"));
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
        for (auto it = root.begin(); it != root.end(); ++it)
            index.insert(it.key(), it.value().toObject());
    }
    const QFileInfoList files = QDir(m_dir).entryInfoList({ QStringLiteral("*.m4a") }, QDir::Files);
    for (const QFileInfo &info : files) {
        const QString id = info.completeBaseName();
        if (!validId(id) || info.size() <= 0) {
            QFile::remove(info.absoluteFilePath());
            continue;
        }
        const QJsonObject meta = index.value(id);
        Item item;
        item.size = info.size();
        item.plays = std::max(1, meta.value(QStringLiteral("plays")).toInt());
        const qint64 used = meta.value(QStringLiteral("used")).toInteger();
        item.used = used > 0 ? QDateTime::fromMSecsSinceEpoch(used, QTimeZone::UTC) : info.lastModified().toUTC();
        m_items.insert(id, item);
        m_used += item.size;
    }
    // Half-written files from an interrupted save
    for (const QFileInfo &info : QDir(m_dir).entryInfoList(QDir::Files))
        if (info.suffix() != QLatin1String("m4a") && info.fileName() != QLatin1String("index.json"))
            QFile::remove(info.absoluteFilePath());
}

void AudioCache::save()
{
    QJsonObject root;
    for (auto it = m_items.cbegin(); it != m_items.cend(); ++it)
        root.insert(it.key(), QJsonObject{ { "plays", it->plays }, { "used", it->used.toMSecsSinceEpoch() } });
    QSaveFile file(m_dir + QStringLiteral("/index.json"));
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        file.commit();
    }
}

void AudioCache::played(const QString &id)
{
    if (!validId(id))
        return;
    const auto it = m_items.find(id);
    if (it == m_items.end()) {
        ++m_pendingPlays[id];
        return;
    }
    it->plays = std::min(it->plays + 1, kMaxCountedPlays);
    it->used = QDateTime::currentDateTimeUtc();
    save();
}

void AudioCache::store(const QString &id, const QByteArray &data)
{
    if (!validId(id) || m_limit <= 0 || data.isEmpty() || data.size() > m_limit / 4
        || m_items.contains(id) || m_writing.contains(id))
        return;
    m_writing.insert(id);
    const QString path = pathOf(id);
    QPointer<AudioCache> self(this);
    QThreadPool::globalInstance()->start([self, id, data, path] {
        QSaveFile file(path);
        const bool ok = file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit();
        QMetaObject::invokeMethod(self, [self, id, ok, size = data.size()] {
            if (!self)
                return;
            self->m_writing.remove(id);
            if (!ok)
                return;
            Item item;
            item.size = size;
            item.used = QDateTime::currentDateTimeUtc();
            item.plays = std::clamp(self->m_pendingPlays.take(id), 1, kMaxCountedPlays);
            self->m_items.insert(id, item);
            self->m_used += size;
            self->trim();
            self->save();
            emit self->changed();
        });
    });
}

void AudioCache::remove(const QString &id)
{
    const auto it = m_items.find(id);
    if (it == m_items.end())
        return;
    if (!QFile::remove(pathOf(id)) && QFile::exists(pathOf(id)))
        return;   // still open by a player: it stays for now
    m_used -= it->size;
    m_items.erase(it);
    save();
    emit changed();
}

void AudioCache::clear()
{
    const QStringList ids = m_items.keys();
    for (const QString &id : ids) {
        if (QFile::remove(pathOf(id)) || !QFile::exists(pathOf(id))) {
            m_used -= m_items.value(id).size;
            m_items.remove(id);
        }
    }
    m_pendingPlays.clear();
    save();
    emit changed();
}

void AudioCache::setLimitBytes(qint64 bytes)
{
    bytes = std::max<qint64>(0, bytes);
    if (bytes == m_limit)
        return;
    m_limit = bytes;
    AppPaths::settings()->setValue(QStringLiteral("cache/audioLimit"), bytes);
    if (trim())
        save();
    emit changed();
}

bool AudioCache::trim()
{
    if (m_used <= m_limit)
        return false;
    // Least worth keeping first: played rarely and long ago.
    QList<QString> order = m_items.keys();
    const auto worth = [this](const QString &id) {
        const Item &item = m_items[id];
        return item.used.toSecsSinceEpoch() + item.plays * kPlayWorth;
    };
    std::sort(order.begin(), order.end(), [&](const QString &a, const QString &b) { return worth(a) < worth(b); });
    bool removed = false;
    for (const QString &id : order) {
        if (m_used <= m_limit)
            break;
        if (m_inUse.contains(id) || !QFile::remove(pathOf(id)))
            continue;
        m_used -= m_items.value(id).size;
        m_items.remove(id);
        removed = true;
    }
    if (removed)
        emit changed();
    return removed;
}
