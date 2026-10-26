#include "Updater.h"

#include "AppPaths.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QProcess>
#include <QStandardPaths>
#include <QVersionNumber>

#include <cstdio>
#include <memory>

namespace {

const char *kReleaseApi = "https://api.github.com/repos/zr3mb/apkadomuzy/releases/latest";
constexpr int kCheckInterval = 6 * 60 * 60 * 1000;   // check again every 6 hours

QNetworkRequest githubRequest(const QUrl &url)
{
    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "Nokturn");
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    return req;
}

// The release asset this platform installs from.
#ifdef Q_OS_WIN
const char *kAssetSuffix = "-setup.exe";
#elif defined(Q_OS_ANDROID)
const char *kAssetSuffix = "-arm64.apk";
#else
const char *kAssetSuffix = "-x86_64.AppImage";
#endif

// Path of the running AppImage (set by the AppImage runtime); empty outside an AppImage.
QString appImagePath()
{
#ifdef Q_OS_WIN
    return {};
#else
    return qEnvironmentVariable("APPIMAGE");
#endif
}

QVersionNumber parseVersion(QString tag)
{
    if (tag.startsWith(QLatin1Char('v')) || tag.startsWith(QLatin1Char('V')))
        tag.remove(0, 1);
    return QVersionNumber::fromString(tag);
}

} // namespace

Updater *Updater::instance()
{
    static Updater *inst = new Updater;
    return inst;
}

Updater *Updater::create(QQmlEngine *, QJSEngine *)
{
    Updater *u = instance();
    QJSEngine::setObjectOwnership(u, QJSEngine::CppOwnership);
    return u;
}

Updater::Updater(QObject *parent)
    : QObject(parent)
{
    // First check shortly after startup — it doesn't compete with loading the library.
    QTimer::singleShot(5000, this, [this] { check(false); });
    m_timer.setInterval(kCheckInterval);
    connect(&m_timer, &QTimer::timeout, this, [this] { check(false); });
    m_timer.start();
}

QString Updater::currentVersion() const
{
    return QStringLiteral(NOKTURN_VERSION);
}

bool Updater::canInstall() const
{
#ifdef Q_OS_WIN
    return true;
#else
    // Linux: only the AppImage can replace itself (and only where we may write next to it);
    // a build installed from source gets the release page instead.
    const QString appImage = appImagePath();
    return !appImage.isEmpty() && QFileInfo(QFileInfo(appImage).absolutePath()).isWritable();
#endif
}

void Updater::check(bool manual)
{
    if (m_state != QLatin1String("idle"))
        return;
    setState(QStringLiteral("checking"));
    // NOKTURN_UPDATE_API overrides the API URL (testing with a local, fake release).
    QString api = qEnvironmentVariable("NOKTURN_UPDATE_API");
    if (api.isEmpty())
        api = QString::fromLatin1(kReleaseApi);
    QNetworkReply *reply = m_net.get(githubRequest(QUrl(api)));
    connect(reply, &QNetworkReply::finished, this, [this, reply, manual] { onRelease(reply, manual); });
}

void Updater::onRelease(QNetworkReply *reply, bool manual)
{
    reply->deleteLater();
    setState(QStringLiteral("idle"));
    if (reply->error() != QNetworkReply::NoError) {
        // No releases (404) or no network — silent during automatic checks.
        if (manual)
            emit notice(tr("Couldn't check for updates"), QStringLiteral("close"));
        return;
    }
    const QJsonObject release = QJsonDocument::fromJson(reply->readAll()).object();
    const QString tag = release.value(QStringLiteral("tag_name")).toString();
    const QVersionNumber latest = parseVersion(tag);
    if (latest.isNull() || latest <= QVersionNumber::fromString(currentVersion())) {
        if (manual)
            emit notice(tr("You have the latest version"), QStringLiteral("check"));
        return;
    }

    m_latest = latest.toString();
    m_notes = release.value(QStringLiteral("body")).toString().trimmed();
    m_page = QUrl(release.value(QStringLiteral("html_url")).toString());
    m_installerUrl.clear();
    m_shaUrl.clear();
    m_digest.clear();
    const QJsonArray assets = release.value(QStringLiteral("assets")).toArray();
    for (const QJsonValue &v : assets) {
        const QJsonObject a = v.toObject();
        const QString name = a.value(QStringLiteral("name")).toString();
        const QUrl url(a.value(QStringLiteral("browser_download_url")).toString());
        if (name.endsWith(QLatin1String(kAssetSuffix), Qt::CaseInsensitive)) {
            m_installerUrl = url;
            const QString digest = a.value(QStringLiteral("digest")).toString();
            if (digest.startsWith(QLatin1String("sha256:")))
                m_digest = digest.mid(7).toLatin1().toLower();
        } else if (name.endsWith(QLatin1String(kAssetSuffix) + QStringLiteral(".sha256"), Qt::CaseInsensitive)) {
            m_shaUrl = url;
        }
    }

    // "Skip this version" silences the automatic dialog — a manual check always shows it.
    const QString skipped = AppPaths::settings()->value("update/skipped").toString();
    m_prompt = manual || skipped != m_latest;
    emit changed();
}

void Updater::later()
{
    m_prompt = false;
    emit changed();
}

void Updater::skip()
{
    AppPaths::settings()->setValue("update/skipped", m_latest);
    AppPaths::settings()->sync();
    later();
}

void Updater::install()
{
    if (!available() || m_state != QLatin1String("idle"))
        return;
    if (!canInstall() || !m_installerUrl.isValid()) {
        // Android: the browser downloads the APK and the system installer takes it from there.
#ifdef Q_OS_ANDROID
        QDesktopServices::openUrl(m_installerUrl.isValid() ? m_installerUrl : m_page);
#else
        QDesktopServices::openUrl(m_page);
#endif
        later();
        return;
    }
    startDownload();
}

void Updater::startDownload()
{
#ifdef Q_OS_WIN
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/Nokturn-update");
    QDir().mkpath(dir);
    m_installerPath = dir + QChar('/') + m_installerUrl.fileName();
#else
    // Next to the running AppImage (same file system), so the final swap is an atomic rename.
    m_installerPath = appImagePath() + QStringLiteral(".new");
#endif
    m_file.setFileName(m_installerPath);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        fail(tr("Couldn't save the update"));
        return;
    }
    m_progress = 0;
    setState(QStringLiteral("downloading"));

    // The installer (~90 MB) goes straight to disk, and the hash is computed on the fly.
    auto hash = std::make_shared<QCryptographicHash>(QCryptographicHash::Sha256);
    QNetworkReply *reply = m_net.get(githubRequest(m_installerUrl));
    connect(reply, &QNetworkReply::readyRead, this, [this, reply, hash] {
        const QByteArray chunk = reply->readAll();
        hash->addData(chunk);
        m_file.write(chunk);
    });
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        m_progress = total > 0 ? qreal(received) / total : 0;
        emit changed();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, hash] {
        reply->deleteLater();
        const QByteArray rest = reply->readAll();
        hash->addData(rest);
        m_file.write(rest);
        m_file.close();
        if (reply->error() != QNetworkReply::NoError) {
            fail(tr("Couldn't download the update: %1").arg(reply->errorString()));
            return;
        }
        const QByteArray actual = hash->result().toHex();
        if (!m_digest.isEmpty()) {
            verifyAndRun(m_digest == actual ? actual : QByteArray());
            return;
        }
        if (!m_shaUrl.isValid()) {
            fail(tr("The release has no checksum — aborted"));
            return;
        }
        QNetworkReply *sums = m_net.get(githubRequest(m_shaUrl));
        connect(sums, &QNetworkReply::finished, this, [this, sums, actual] {
            sums->deleteLater();
            const QByteArray expected = sums->readAll().trimmed().split(' ').value(0).toLower();
            verifyAndRun(sums->error() == QNetworkReply::NoError && expected == actual ? actual : QByteArray());
        });
    });
}

void Updater::verifyAndRun(const QByteArray &expectedSha)
{
    if (expectedSha.isEmpty()) {
        QFile::remove(m_installerPath);
        fail(tr("The downloaded update doesn't match the checksum — aborted"));
        return;
    }
    setState(QStringLiteral("installing"));
#ifdef Q_OS_WIN
    // Silent mode: a progress window without questions. The setup downloads the release, closes Nokturn,
    // replaces the files in the same folder and starts the new version.
    const QStringList args = { QStringLiteral("/SILENT"), QStringLiteral("/SUPPRESSMSGBOXES"),
                               QStringLiteral("/NORESTART"), QStringLiteral("/NOCANCEL") };
    if (!QProcess::startDetached(m_installerPath, args)) {
        fail(tr("Couldn't start the installer"));
        return;
    }
#else
    // AppImage: make the new file executable and rename it over the running one — Linux keeps the old
    // file alive for the running process, so this is safe. Then start the new version.
    const QString target = appImagePath();
    QFile::setPermissions(m_installerPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                                               | QFileDevice::ReadGroup | QFileDevice::ExeGroup
                                               | QFileDevice::ReadOther | QFileDevice::ExeOther);
    if (std::rename(QFile::encodeName(m_installerPath).constData(), QFile::encodeName(target).constData()) != 0) {
        QFile::remove(m_installerPath);
        fail(tr("Couldn't replace the AppImage"));
        return;
    }
    if (!QProcess::startDetached(target, {})) {
        fail(tr("Couldn't start the new version — start Nokturn again"));
        return;
    }
#endif
    AppPaths::settings()->sync();
    QCoreApplication::quit();
}

void Updater::fail(const QString &text)
{
    if (m_file.isOpen())
        m_file.close();
    setState(QStringLiteral("idle"));
    emit notice(text, QStringLiteral("close"));
}

void Updater::setState(const QString &state)
{
    m_state = state;
    emit changed();
}
