#include "PluginManager.h"

#include "AppPaths.h"
#include "Language.h"
#include "OnlineService.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QUuid>

#include <private/qzipreader_p.h>

namespace {

constexpr qint64 kMaxDownload = 30 * 1024 * 1024;     // a plugin archive from the catalog
constexpr qint64 kMaxUnpacked = 120 * 1024 * 1024;    // everything in an archive, unpacked
constexpr int kMaxFiles = 3000;

const QRegularExpression &idPattern()
{
    static const QRegularExpression re(QStringLiteral("^[a-z0-9][a-z0-9._-]{1,63}$"));
    return re;
}

QString settingKey(const QString &id)
{
    return QStringLiteral("plugins/enabled/") + id;
}

// A path inside the plugin's folder: relative, without "..", and pointing at an existing file.
QString pluginFile(const QString &dir, const QString &relative)
{
    if (relative.isEmpty())
        return {};
    const QString clean = QDir::cleanPath(relative);
    if (QDir::isAbsolutePath(clean) || clean.startsWith(QLatin1String("..")) || clean.contains(QLatin1Char(':')))
        return {};
    const QString path = dir + QLatin1Char('/') + clean;
    return QFileInfo(path).isFile() ? path : QString();
}

bool copyTree(const QString &from, const QString &to)
{
    if (!QDir().mkpath(to))
        return false;
    QDirIterator it(from, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QFileInfo info = it.fileInfo();
        if (info.isSymLink())
            continue;
        const QString target = to + QLatin1Char('/') + QDir(from).relativeFilePath(info.filePath());
        if (info.isDir()) {
            if (!QDir().mkpath(target))
                return false;
        } else if (!QFile::copy(info.filePath(), target)) {
            return false;
        }
    }
    return true;
}

// The folder holding plugin.json: the given one, or its only subfolder (an archive packed with the
// folder inside).
QString manifestRoot(const QString &dir)
{
    if (QFileInfo(dir + QStringLiteral("/plugin.json")).isFile())
        return dir;
    const QStringList subs = QDir(dir).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (subs.size() == 1 && QFileInfo(dir + QLatin1Char('/') + subs.first() + QStringLiteral("/plugin.json")).isFile())
        return dir + QLatin1Char('/') + subs.first();
    return {};
}

QJsonObject readManifest(const QString &dir, QString *error)
{
    QFile f(dir + QStringLiteral("/plugin.json"));
    if (!f.open(QIODevice::ReadOnly)) {
        *error = PluginManager::tr("The plugin has no plugin.json");
        return {};
    }
    QJsonParseError parse;
    const QJsonDocument doc = QJsonDocument::fromJson(f.read(1024 * 1024), &parse);
    if (!doc.isObject()) {
        *error = PluginManager::tr("plugin.json isn't valid JSON: %1").arg(parse.errorString());
        return {};
    }
    const QJsonObject m = doc.object();
    const QString id = m.value(QStringLiteral("id")).toString();
    if (!idPattern().match(id).hasMatch()) {
        *error = PluginManager::tr("plugin.json needs an \"id\" of lowercase letters, digits, dots and dashes");
        return {};
    }
    if (m.value(QStringLiteral("api")).toInt(1) > PluginManager::kApiVersion) {
        *error = PluginManager::tr("The plugin needs a newer version of Everplay");
        return {};
    }
    return m;
}

// Plugins written before the rename import the app's QML module by its former name ("import Everplay"),
// and the catalog keeps them that way for the older app versions: the import is changed on disk.
void adoptFormerImports(const QString &dir)
{
    static const QRegularExpression former(QStringLiteral("^(\\s*import\\s+)Everplay\\b"),
                                           QRegularExpression::MultilineOption);
    QDirIterator it(dir, { QStringLiteral("*.qml"), QStringLiteral("*.js"), QStringLiteral("*.mjs") }, QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QFile file(it.next());
        if (file.size() > 4 * 1024 * 1024 || !file.open(QIODevice::ReadOnly))
            continue;
        QString text = QString::fromUtf8(file.readAll());
        file.close();
        if (!former.match(text).hasMatch())
            continue;
        text.replace(former, QStringLiteral("\\1Everplay"));
        QSaveFile out(file.fileName());
        if (out.open(QIODevice::WriteOnly)) {
            out.write(text.toUtf8());
            out.commit();
        }
    }
}

} // namespace

// ---- PluginContext ---------------------------------------------------------------------------

PluginContext::PluginContext(const QString &id, const QString &path, const QString &version, QObject *parent)
    : QObject(parent), m_id(id), m_path(path), m_version(version)
{
    QFile f(dataFile());
    if (f.open(QIODevice::ReadOnly))
        m_values = QJsonDocument::fromJson(f.readAll()).object();
}

QUrl PluginContext::dir() const
{
    return QUrl::fromLocalFile(m_path + QLatin1Char('/'));
}

QString PluginContext::dataFile() const
{
    const QString dir = AppPaths::dataDir() + QStringLiteral("/plugin-data");
    QDir().mkpath(dir);
    return dir + QLatin1Char('/') + m_id + QStringLiteral(".json");
}

QVariant PluginContext::setting(const QString &key, const QVariant &fallback) const
{
    const auto it = m_values.constFind(key);
    return it == m_values.constEnd() ? fallback : it->toVariant();
}

void PluginContext::setSetting(const QString &key, const QVariant &value)
{
    const QJsonValue v = QJsonValue::fromVariant(value);
    if (m_values.value(key) == v)
        return;
    m_values.insert(key, v);
    QSaveFile f(dataFile());
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(m_values).toJson(QJsonDocument::Compact));
        f.commit();
    }
    ++m_revision;
    emit settingChanged(key);
}

QUrl PluginContext::file(const QString &relative) const
{
    const QString clean = QDir::cleanPath(relative);
    if (QDir::isAbsolutePath(clean) || clean.startsWith(QLatin1String("..")))
        return {};
    return QUrl::fromLocalFile(m_path + QLatin1Char('/') + clean);
}

// ---- PluginManager ---------------------------------------------------------------------------

PluginManager *PluginManager::instance()
{
    static PluginManager *inst = new PluginManager;
    return inst;
}

PluginManager *PluginManager::create(QQmlEngine *engine, QJSEngine *)
{
    PluginManager *p = instance();
    p->m_engine = engine;
    QJSEngine::setObjectOwnership(p, QJSEngine::CppOwnership);
    return p;
}

PluginManager::PluginManager(QObject *parent)
    : QObject(parent)
{
    // EVERPLAY_PLUGIN_CATALOG: another index.json (also a file:// URL), for testing a catalog.
    const QString custom = qEnvironmentVariable("EVERPLAY_PLUGIN_CATALOG");
    if (!custom.isEmpty()) {
        m_catalogUrl = QUrl::fromUserInput(custom);
    } else {
        QString server = OnlineService::instance()->serverUrl();
        if (server.isEmpty())
            server = QString::fromLatin1(OnlineService::kDefaultServer);
        while (server.endsWith(QLatin1Char('/')))
            server.chop(1);
        m_catalogUrl = QUrl(server + QStringLiteral("/plugins/index.json"));
    }
    connect(Language::instance(), &Language::changed, this, [this] { scan(); rebuildCatalog(); });
    scan();
}

QString PluginManager::pluginsDir() const
{
    const QString dir = AppPaths::dataDir() + QStringLiteral("/plugins");
    QDir().mkpath(dir);
    return dir;
}

QUrl PluginManager::folder() const
{
    return QUrl::fromLocalFile(pluginsDir());
}

QUrl PluginManager::docsUrl() const
{
    const bool polish = Language::instance()->effective() == QLatin1String("pl");
    return QUrl(QStringLiteral("https://github.com/zr3mb/Everplay/blob/main/docs/")
                + (polish ? QStringLiteral("plugins.pl.md") : QStringLiteral("plugins.md")));
}

QString PluginManager::text(const QJsonValue &value) const
{
    if (!value.isObject())
        return value.toString();
    const QJsonObject o = value.toObject();
    const QString lang = Language::instance()->effective();
    if (o.contains(lang))
        return o.value(lang).toString();
    if (o.contains(QStringLiteral("en")))
        return o.value(QStringLiteral("en")).toString();
    return o.isEmpty() ? QString() : o.begin()->toString();
}

int PluginManager::compareVersions(const QString &a, const QString &b)
{
    const QStringList pa = a.split(QLatin1Char('.'));
    const QStringList pb = b.split(QLatin1Char('.'));
    for (int i = 0; i < std::max(pa.size(), pb.size()); ++i) {
        const int x = pa.value(i).toInt();
        const int y = pb.value(i).toInt();
        if (x != y)
            return x < y ? -1 : 1;
    }
    return 0;
}

void PluginManager::scan()
{
    emit aboutToChange();
    QVariantList list;
    QVariantMap themes;
    const QString root = pluginsDir();
    const QStringList dirs = QDir(root).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &name : dirs) {
        if (name.startsWith(QLatin1Char('.')))
            continue;   // a half-finished install
        const QString dir = root + QLatin1Char('/') + name;
        adoptFormerImports(dir);
        QString error;
        const QJsonObject m = readManifest(dir, &error);
        const QString id = m.isEmpty() ? name : m.value(QStringLiteral("id")).toString();
        const bool enabled = AppPaths::settings()->value(settingKey(id), true).toBool();

        QVariantMap p {
            { "id", id },
            { "name", m.isEmpty() ? name : text(m.value(QStringLiteral("name"))) },
            { "version", m.value(QStringLiteral("version")).toString() },
            { "author", m.value(QStringLiteral("author")).toString() },
            { "description", text(m.value(QStringLiteral("description"))) },
            { "homepage", m.value(QStringLiteral("homepage")).toString() },
            { "enabled", enabled && error.isEmpty() },
            { "path", dir },
        };
        QStringList kinds;
        for (const char *slot : { "wallpaper", "overlay", "main", "settings" }) {
            const QString key = QString::fromLatin1(slot);
            const QString file = pluginFile(dir, m.value(key).toString());
            p.insert(key, file.isEmpty() ? QUrl() : QUrl::fromLocalFile(file));
            if (!file.isEmpty() && key != QLatin1String("settings"))
                kinds << key;
            else if (file.isEmpty() && m.contains(key) && error.isEmpty())
                error = tr("The file \"%1\" from plugin.json is missing").arg(m.value(key).toString());
        }
        const QString preview = pluginFile(dir, m.value(QStringLiteral("preview")).toString());
        p.insert(QStringLiteral("preview"), preview.isEmpty() ? QUrl() : QUrl::fromLocalFile(preview));

        // Colour themes: they join the built-in ones in Settings → Appearance as "<plugin>/<theme>".
        QVariantList themeNames;
        const QJsonArray t = m.value(QStringLiteral("themes")).toArray();
        for (const QJsonValue &v : t) {
            QJsonObject theme = v.toObject();
            const QString themeId = theme.value(QStringLiteral("id")).toString();
            if (!idPattern().match(themeId).hasMatch() || !theme.contains(QStringLiteral("bg")))
                continue;
            theme.insert(QStringLiteral("name"), text(theme.value(QStringLiteral("name"))));
            if (theme.value(QStringLiteral("name")).toString().isEmpty())
                theme.insert(QStringLiteral("name"), themeId);
            themeNames << theme.value(QStringLiteral("name")).toString();
            if (enabled && error.isEmpty())
                themes.insert(id + QLatin1Char('/') + themeId, theme.toVariantMap());
        }
        if (!themeNames.isEmpty())
            kinds << QStringLiteral("theme");
        p.insert(QStringLiteral("themes"), themeNames);
        p.insert(QStringLiteral("kinds"), kinds);
        p.insert(QStringLiteral("error"), error);
        list << p;
    }
    m_installed = list;
    m_themes = themes;
    emit changed();
}

void PluginManager::filesReplaced()
{
    // The new files must be compiled again: the engine would otherwise keep the old components.
    if (m_engine)
        m_engine->clearComponentCache();
    m_errors.clear();
    emit errorsChanged();
    ++m_revision;
    scan();
    rebuildCatalog();
}

void PluginManager::reload()
{
    // Contexts keep their settings in memory: read them again too.
    for (const auto &ctx : std::as_const(m_contexts))
        if (ctx)
            ctx->deleteLater();
    m_contexts.clear();
    filesReplaced();
}

void PluginManager::setEnabled(const QString &id, bool enabled)
{
    AppPaths::settings()->setValue(settingKey(id), enabled);
    if (m_errors.remove(id))
        emit errorsChanged();
    scan();
}

QVariantMap PluginManager::errors() const
{
    QVariantMap map;
    for (auto it = m_errors.cbegin(); it != m_errors.cend(); ++it)
        map.insert(it.key(), it.value());
    return map;
}

void PluginManager::reportError(const QString &id, const QString &error)
{
    // Paths inside the plugin are shown relative to its folder, without the reload marker.
    QString text = error;
    for (const QVariant &v : std::as_const(m_installed))
        if (v.toMap().value(QStringLiteral("id")).toString() == id)
            text.replace(QUrl::fromLocalFile(v.toMap().value(QStringLiteral("path")).toString()).toString() + QLatin1Char('/'), QString());
    static const QRegularExpression marker(QStringLiteral("\\?r=\\d+"));
    text.remove(marker);
    if (m_errors.value(id) == text)
        return;
    m_errors.insert(id, text);
    qWarning("plugin %s: %s", qPrintable(id), qPrintable(text));
    // Later: the report comes while the plugins are being built, from bindings that read the list.
    QMetaObject::invokeMethod(this, &PluginManager::errorsChanged, Qt::QueuedConnection);
}

QObject *PluginManager::context(const QString &id)
{
    for (const QVariant &v : std::as_const(m_installed)) {
        const QVariantMap p = v.toMap();
        if (p.value(QStringLiteral("id")).toString() != id)
            continue;
        const QString version = p.value(QStringLiteral("version")).toString();
        QPointer<PluginContext> &ctx = m_contexts[id];
        if (!ctx || ctx->version() != version)
            ctx = new PluginContext(id, p.value(QStringLiteral("path")).toString(), version, this);
        QJSEngine::setObjectOwnership(ctx, QJSEngine::CppOwnership);
        return ctx;
    }
    return nullptr;
}

QObject *PluginManager::instantiate(QObject *componentObject, QQuickItem *parent, const QString &id)
{
    auto *component = qobject_cast<QQmlComponent *>(componentObject);
    if (!component || !parent || !m_engine)
        return nullptr;
    if (!component->isReady()) {
        reportError(id, component->errorString().trimmed());
        return nullptr;
    }
    // The engine's root context: a plugin sees the app's singletons, not the ids around its host.
    QObject *object = component->beginCreate(m_engine->rootContext());
    if (!object) {
        reportError(id, component->errorString().trimmed());
        return nullptr;
    }
    if (object->metaObject()->indexOfProperty("plugin") >= 0)
        object->setProperty("plugin", QVariant::fromValue(context(id)));
    object->setParent(parent);
    if (auto *item = qobject_cast<QQuickItem *>(object))
        item->setParentItem(parent);
    component->completeCreate();
    QJSEngine::setObjectOwnership(object, QJSEngine::CppOwnership);   // the host deletes it
    return object;
}

void PluginManager::remove(const QString &id)
{
    if (!idPattern().match(id).hasMatch())
        return;
    const QString dir = pluginsDir() + QLatin1Char('/') + id;
    // First out of the interface, then off the disk.
    AppPaths::settings()->remove(settingKey(id));
    QVariantList rest;
    for (const QVariant &v : std::as_const(m_installed))
        if (v.toMap().value(QStringLiteral("id")).toString() != id)
            rest << v;
    m_installed = rest;
    m_themes.removeIf([&](const auto &it) { return it.key().startsWith(id + QLatin1Char('/')); });
    emit aboutToChange();
    emit changed();
    QDir(dir).removeRecursively();
    QFile::remove(AppPaths::dataDir() + QStringLiteral("/plugin-data/") + id + QStringLiteral(".json"));
    if (m_contexts.contains(id) && m_contexts.value(id))
        m_contexts.value(id)->deleteLater();
    m_contexts.remove(id);
    filesReplaced();
    emit notice(tr("Plugin removed"), QStringLiteral("check"));
}

bool PluginManager::installFolder(const QString &source, QString *error)
{
    const QString root = manifestRoot(source);
    if (root.isEmpty()) {
        *error = tr("No plugin.json found");
        return false;
    }
    const QJsonObject m = readManifest(root, error);
    if (m.isEmpty())
        return false;
    const QString id = m.value(QStringLiteral("id")).toString();
    const QString target = pluginsDir() + QLatin1Char('/') + id;
    if (QDir::cleanPath(root) == QDir::cleanPath(target))
        return true;   // already in place (a plugin written right in the plugins folder)

    // Copied next to the target first, then swapped in, so a failed copy leaves the old version.
    const QString staging = pluginsDir() + QStringLiteral("/.new-") + QUuid::createUuid().toString(QUuid::Id128);
    if (!copyTree(root, staging)) {
        QDir(staging).removeRecursively();
        *error = tr("Couldn't copy the plugin's files");
        return false;
    }
    QDir(target).removeRecursively();
    if (!QDir().rename(staging, target)) {
        QDir(staging).removeRecursively();
        *error = tr("Couldn't put the plugin in the plugins folder");
        return false;
    }
    AppPaths::settings()->setValue(settingKey(id), true);
    return true;
}

bool PluginManager::installZip(const QString &zipPath, QString *error)
{
    QZipReader zip(zipPath);
    if (!zip.isReadable() || zip.status() != QZipReader::NoError) {
        *error = tr("This isn't a readable .zip file");
        return false;
    }
    const auto entries = zip.fileInfoList();
    if (entries.size() > kMaxFiles) {
        *error = tr("The archive has too many files");
        return false;
    }
    qint64 total = 0;
    for (const auto &e : entries)
        total += e.size;
    if (total > kMaxUnpacked) {
        *error = tr("The archive is too large");
        return false;
    }
    const QString temp = pluginsDir() + QStringLiteral("/.unpack-") + QUuid::createUuid().toString(QUuid::Id128);
    const auto cleanup = qScopeGuard([&] { QDir(temp).removeRecursively(); });
    for (const auto &e : entries) {
        // No way out of the folder: absolute paths, drive letters and ".." are refused.
        const QString clean = QDir::cleanPath(e.filePath);
        if (clean.isEmpty() || QDir::isAbsolutePath(clean) || clean.startsWith(QLatin1String(".."))
            || clean.contains(QLatin1Char(':')) || e.isSymLink) {
            *error = tr("The archive has a file outside its folder");
            return false;
        }
        const QString path = temp + QLatin1Char('/') + clean;
        if (e.isDir) {
            QDir().mkpath(path);
            continue;
        }
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile out(path);
        if (!out.open(QIODevice::WriteOnly) || out.write(zip.fileData(e.filePath)) < 0) {
            *error = tr("Couldn't unpack the archive");
            return false;
        }
    }
    return installFolder(temp, error);
}

void PluginManager::installFile(const QUrl &file)
{
    const QString path = file.isLocalFile() ? file.toLocalFile() : file.toString();
    const QFileInfo info(path);
    QString error;
    bool ok = false;
    if (info.isDir())
        ok = installFolder(path, &error);
    else if (info.fileName() == QLatin1String("plugin.json"))
        ok = installFolder(info.absolutePath(), &error);
    else if (info.isFile())
        ok = installZip(path, &error);
    else
        error = tr("The file doesn't exist");
    if (!ok) {
        emit notice(tr("Couldn't install the plugin: %1").arg(error), QStringLiteral("close"));
        return;
    }
    filesReplaced();
    emit notice(tr("Plugin installed"), QStringLiteral("check"));
}

void PluginManager::setBusy(const QString &id)
{
    if (m_busyId == id)
        return;
    m_busyId = id;
    emit busyChanged();
}

// ---- the catalog -----------------------------------------------------------------------------

void PluginManager::refreshCatalog()
{
    if (m_catalogState == QLatin1String("loading"))
        return;
    m_catalogState = QStringLiteral("loading");
    emit catalogChanged();
    QNetworkRequest req(m_catalogUrl);
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    req.setTransferTimeout(15000);
    QNetworkReply *reply = m_net.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (reply->error() != QNetworkReply::NoError || !doc.isObject()) {
            m_catalogState = QStringLiteral("error");
            emit catalogChanged();
            return;
        }
        m_catalogRaw.clear();
        const QJsonArray list = doc.object().value(QStringLiteral("plugins")).toArray();
        for (const QJsonValue &v : list) {
            const QJsonObject o = v.toObject();
            const QString id = o.value(QStringLiteral("id")).toString();
            const QUrl file = m_catalogUrl.resolved(QUrl(o.value(QStringLiteral("file")).toString()));
            if (!idPattern().match(id).hasMatch() || !file.isValid()
                || o.value(QStringLiteral("sha256")).toString().size() != 64
                || o.value(QStringLiteral("api")).toInt(1) > kApiVersion)
                continue;
            QVariantMap e {
                { "id", id },
                { "name", text(o.value(QStringLiteral("name"))) },
                { "nameRaw", o.value(QStringLiteral("name")).toVariant() },
                { "descriptionRaw", o.value(QStringLiteral("description")).toVariant() },
                { "version", o.value(QStringLiteral("version")).toString() },
                { "author", o.value(QStringLiteral("author")).toString() },
                { "kinds", o.value(QStringLiteral("kinds")).toVariant() },
                { "size", o.value(QStringLiteral("size")).toDouble() },
                { "sha256", o.value(QStringLiteral("sha256")).toString().toLower() },
                { "file", file },
            };
            const QString preview = o.value(QStringLiteral("preview")).toString();
            e.insert(QStringLiteral("preview"), preview.isEmpty() ? QUrl() : m_catalogUrl.resolved(QUrl(preview)));
            m_catalogRaw << e;
        }
        m_catalogState = QStringLiteral("ready");
        rebuildCatalog();
    });
}

void PluginManager::rebuildCatalog()
{
    QHash<QString, QString> versions;
    for (const QVariant &v : std::as_const(m_installed))
        versions.insert(v.toMap().value(QStringLiteral("id")).toString(), v.toMap().value(QStringLiteral("version")).toString());
    m_catalog.clear();
    for (QVariant v : std::as_const(m_catalogRaw)) {
        QVariantMap e = v.toMap();
        const QString id = e.value(QStringLiteral("id")).toString();
        e.insert(QStringLiteral("name"), text(QJsonValue::fromVariant(e.value(QStringLiteral("nameRaw")))));
        e.insert(QStringLiteral("description"), text(QJsonValue::fromVariant(e.value(QStringLiteral("descriptionRaw")))));
        e.insert(QStringLiteral("installed"), versions.contains(id));
        e.insert(QStringLiteral("update"), versions.contains(id)
                 && compareVersions(versions.value(id), e.value(QStringLiteral("version")).toString()) < 0);
        m_catalog << e;
    }
    emit catalogChanged();
}

void PluginManager::installFromCatalog(const QString &id)
{
    if (!m_busyId.isEmpty())
        return;
    for (const QVariant &v : std::as_const(m_catalogRaw)) {
        const QVariantMap e = v.toMap();
        if (e.value(QStringLiteral("id")).toString() != id)
            continue;
        setBusy(id);
        QNetworkRequest req(e.value(QStringLiteral("file")).toUrl());
        req.setTransferTimeout(60000);
        QNetworkReply *reply = m_net.get(req);
        connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 got, qint64) {
            if (got > kMaxDownload)
                reply->abort();
        });
        connect(reply, &QNetworkReply::finished, this, [this, reply, e] { finishDownload(reply, e); });
        return;
    }
}

void PluginManager::finishDownload(QNetworkReply *reply, const QVariantMap &entry)
{
    reply->deleteLater();
    const auto done = qScopeGuard([this] { setBusy({}); });
    const QByteArray data = reply->readAll();
    if (reply->error() != QNetworkReply::NoError || data.isEmpty() || data.size() > kMaxDownload) {
        emit notice(tr("Couldn't download the plugin"), QStringLiteral("close"));
        return;
    }
    const QByteArray sha = QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex();
    if (sha != entry.value(QStringLiteral("sha256")).toString().toLatin1()) {
        emit notice(tr("The downloaded plugin doesn't match the catalog — not installed"), QStringLiteral("close"));
        return;
    }
    const QString zipPath = pluginsDir() + QStringLiteral("/.download-") + QUuid::createUuid().toString(QUuid::Id128) + QStringLiteral(".zip");
    const auto cleanup = qScopeGuard([&] { QFile::remove(zipPath); });
    QFile f(zipPath);
    if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size()) {
        emit notice(tr("Couldn't save the plugin"), QStringLiteral("close"));
        return;
    }
    f.close();
    QString error;
    if (!installZip(zipPath, &error)) {
        emit notice(tr("Couldn't install the plugin: %1").arg(error), QStringLiteral("close"));
        return;
    }
    filesReplaced();
    emit notice(tr("Installed %1").arg(entry.value(QStringLiteral("name")).toString()), QStringLiteral("check"));
}
