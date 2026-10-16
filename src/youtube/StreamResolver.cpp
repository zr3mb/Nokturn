#include "StreamResolver.h"

#include "AppPaths.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QNetworkReply>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTimer>
#include <QUrlQuery>
#include <QUuid>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

namespace {

const char *kReleaseBase = "https://github.com/yt-dlp/yt-dlp/releases/latest/download/";

// Nazwa pliku w katalogu narzędzi i nazwa samodzielnej wersji w wydaniu na GitHubie
// (Linux: "yt-dlp_linux" nie wymaga Pythona, w przeciwieństwie do "yt-dlp").
QString engineFileName()
{
#ifdef Q_OS_WIN
    return QStringLiteral("yt-dlp.exe");
#else
    return QStringLiteral("yt-dlp");
#endif
}

QString releaseAsset()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("yt-dlp.exe");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("yt-dlp_macos");
#else
    const QString arch = QSysInfo::currentCpuArchitecture();
    if (arch == QLatin1String("arm64"))
        return QStringLiteral("yt-dlp_linux_aarch64");
    return QStringLiteral("yt-dlp_linux");
#endif
}

void hideConsole(QProcess *process)
{
#ifdef Q_OS_WIN
    // Nokturn jest aplikacją okienkową — bez tego każde wywołanie yt-dlp mignęłoby konsolą.
    process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
        args->flags |= CREATE_NO_WINDOW;
    });
#else
    Q_UNUSED(process);
#endif
}

} // namespace

StreamResolver::StreamResolver(QObject *parent)
    : QObject(parent)
{
}

QString StreamResolver::toolsDir() const
{
    const QString dir = AppPaths::dataDir() + QStringLiteral("/tools");
    QDir().mkpath(dir);
    return dir;
}

void StreamResolver::detect()
{
    const QStringList candidates = {
        toolsDir() + QChar('/') + engineFileName(),
        QCoreApplication::applicationDirPath() + QChar('/') + engineFileName(),
        QStandardPaths::findExecutable(QStringLiteral("yt-dlp")),
    };
    m_enginePath.clear();
    for (const QString &c : candidates) {
        if (!c.isEmpty() && QFile::exists(c)) {
            m_enginePath = c;
            break;
        }
    }
    m_version.clear();
    emit engineChanged();
    if (ready())
        readVersion();
}

void StreamResolver::readVersion()
{
    auto *process = new QProcess(this);
    hideConsole(process);
    connect(process, &QProcess::finished, this, [this, process] {
        m_version = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
        process->deleteLater();
        emit engineChanged();
    });
    process->start(m_enginePath, { QStringLiteral("--version") });
}

QStringList StreamResolver::baseArguments() const
{
    QStringList args = {
        QStringLiteral("--no-warnings"), QStringLiteral("--no-playlist"), QStringLiteral("--no-progress"),
        QStringLiteral("-f"), QStringLiteral("bestaudio[ext=m4a]/bestaudio/best"),
        QStringLiteral("--print"), QStringLiteral("urls"),
    };
    // yt-dlp potrzebuje środowiska JavaScript do pełnej obsługi YouTube (domyślnie szuka Deno).
    const QString deno = QStandardPaths::findExecutable(QStringLiteral("deno"));
    const QString node = QStandardPaths::findExecutable(QStringLiteral("node"));
    if (deno.isEmpty() && !node.isEmpty())
        args << QStringLiteral("--js-runtimes") << QStringLiteral("node:") + node;
    return args;
}

QUrl StreamResolver::cachedUrl(const QString &videoId) const
{
    const auto it = m_cache.constFind(videoId);
    if (it == m_cache.cend() || it->expires < QDateTime::currentDateTimeUtc().addSecs(120))
        return {};
    return it->url;
}

void StreamResolver::resolve(const QString &videoId)
{
    if (const QUrl cached = cachedUrl(videoId); cached.isValid()) {
        QTimer::singleShot(0, this, [this, videoId, cached] { emit resolved(videoId, cached); });
        return;
    }
    if (m_running.contains(videoId))
        return;   // już trwa — wynik przyjdzie sygnałem
    if (!ready()) {
        emit failed(videoId, QStringLiteral("Brak silnika yt-dlp"));
        return;
    }
    run(videoId, m_cookies.isLoggedIn());
}

void StreamResolver::run(const QString &videoId, bool withCookies)
{
    QStringList args = baseArguments();

    // Ciasteczka konta (lepsza jakość dla Premium, utwory z ograniczeniem wieku).
    // Plik istnieje tylko na czas działania procesu.
    QString cookieFile;
    if (withCookies) {
        cookieFile = toolsDir() + QStringLiteral("/cookies-%1.txt").arg(QUuid::createUuid().toString(QUuid::Id128));
        QFile f(cookieFile);
        if (f.open(QIODevice::WriteOnly)) {
            f.write(m_cookies.toNetscape().toUtf8());
            f.close();
            args << QStringLiteral("--cookies") << cookieFile;
        }
    }
    args << QStringLiteral("https://music.youtube.com/watch?v=") + videoId;

    auto *process = new QProcess(this);
    hideConsole(process);
    m_running.insert(videoId, process);
    connect(process, &QProcess::finished, this, [this, process, videoId, cookieFile](int code) {
        m_running.remove(videoId);
        process->deleteLater();
        if (!cookieFile.isEmpty())
            QFile::remove(cookieFile);

        const QStringList out = QString::fromUtf8(process->readAllStandardOutput()).split('\n', Qt::SkipEmptyParts);
        const QString url = out.isEmpty() ? QString() : out.first().trimmed();
        if (code == 0 && url.startsWith(QLatin1String("http"))) {
            const QUrl u(url);
            const qint64 expire = QUrlQuery(u).queryItemValue(QStringLiteral("expire")).toLongLong();
            const QDateTime expires = expire > 0 ? QDateTime::fromSecsSinceEpoch(expire, QTimeZone::UTC)
                                                 : QDateTime::currentDateTimeUtc().addSecs(3600);
            m_cache.insert(videoId, { u, expires });
            emit resolved(videoId, u);
            return;
        }
        QStringList err = QString::fromUtf8(process->readAllStandardError()).split('\n', Qt::SkipEmptyParts);
        QString message = err.isEmpty() ? QStringLiteral("yt-dlp zakończył się błędem") : err.last().trimmed();
        message.remove(QStringLiteral("ERROR: "));
        // Z ciasteczkami yt-dlp używa klientów webowych, które bez Deno/Node dostają
        // tylko miniatury („Requested format is not available”). Bez konta działa klient
        // niewymagający JavaScriptu, więc próbujemy jeszcze raz anonimowo.
        if (!cookieFile.isEmpty() && message.contains(QLatin1String("Requested format is not available"))) {
            run(videoId, false);
            return;
        }
        emit failed(videoId, message);
    });
    process->start(m_enginePath, args);
}

void StreamResolver::download()
{
    if (m_downloading)
        return;
    m_downloading = true;
    m_progress = 0;
    emit engineChanged();

    // Najpierw lista sum kontrolnych, potem sam program — porównujemy SHA-256 przed zapisaniem.
    QNetworkReply *sumsReply = m_net.get(QNetworkRequest(QUrl(QString::fromLatin1(kReleaseBase) + "SHA2-256SUMS")));
    connect(sumsReply, &QNetworkReply::finished, this, [this, sumsReply] {
        sumsReply->deleteLater();
        if (sumsReply->error() != QNetworkReply::NoError) {
            m_downloading = false;
            emit engineChanged();
            emit notice(QStringLiteral("Nie udało się pobrać yt-dlp: ") + sumsReply->errorString(), QStringLiteral("close"));
            return;
        }
        const QByteArray sums = sumsReply->readAll();
        QNetworkReply *binReply = m_net.get(QNetworkRequest(QUrl(QString::fromLatin1(kReleaseBase) + releaseAsset())));
        connect(binReply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
            m_progress = total > 0 ? qreal(received) / total : 0;
            emit engineChanged();
        });
        connect(binReply, &QNetworkReply::finished, this, [this, binReply, sums] {
            binReply->deleteLater();
            if (binReply->error() != QNetworkReply::NoError) {
                m_downloading = false;
                emit engineChanged();
                emit notice(QStringLiteral("Nie udało się pobrać yt-dlp: ") + binReply->errorString(), QStringLiteral("close"));
                return;
            }
            finishDownload(binReply->readAll(), sums);
        });
    });
}

void StreamResolver::finishDownload(const QByteArray &binary, const QByteArray &sums)
{
    m_downloading = false;
    const QByteArray hash = QCryptographicHash::hash(binary, QCryptographicHash::Sha256).toHex();
    bool verified = false;
    for (const QByteArray &line : sums.split('\n')) {
        if (line.trimmed().endsWith(" " + releaseAsset().toLatin1()) && line.startsWith(hash)) {
            verified = true;
            break;
        }
    }
    if (!verified) {
        emit engineChanged();
        emit notice(QStringLiteral("Pobrany yt-dlp nie zgadza się z sumą kontrolną — przerwano"), QStringLiteral("close"));
        return;
    }
    const QString target = toolsDir() + QChar('/') + engineFileName();
    QSaveFile out(target);
    if (out.open(QIODevice::WriteOnly) && out.write(binary) == binary.size() && out.commit()) {
        QFile::setPermissions(target, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                                          | QFileDevice::ReadGroup | QFileDevice::ExeGroup
                                          | QFileDevice::ReadOther | QFileDevice::ExeOther);
        m_cache.clear();
        detect();
        emit notice(QStringLiteral("Silnik odtwarzania YouTube jest gotowy"), QStringLiteral("check"));
    } else {
        emit engineChanged();
        emit notice(QStringLiteral("Nie udało się zapisać yt-dlp"), QStringLiteral("close"));
    }
}
