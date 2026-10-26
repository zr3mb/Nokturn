#include "AndroidSupport.h"

#include <QCoreApplication>
#include <QFutureWatcher>
#include <QOperatingSystemVersion>
#include <QtCore/private/qandroidextras_p.h>

namespace AndroidSupport {

void requestAudioPermission(std::function<void(bool granted)> done)
{
    const bool tiramisu = QOperatingSystemVersion::current() >= QOperatingSystemVersion(QOperatingSystemVersion::Android, 13);
    const QString permission = tiramisu ? QStringLiteral("android.permission.READ_MEDIA_AUDIO")
                                        : QStringLiteral("android.permission.READ_EXTERNAL_STORAGE");
    if (QtAndroidPrivate::checkPermission(permission).result() == QtAndroidPrivate::Authorized) {
        done(true);
        return;
    }
    auto *watcher = new QFutureWatcher<QtAndroidPrivate::PermissionResult>(qApp);
    QObject::connect(watcher, &QFutureWatcher<QtAndroidPrivate::PermissionResult>::finished, qApp, [watcher, done] {
        watcher->deleteLater();
        done(watcher->result() == QtAndroidPrivate::Authorized);
    });
    watcher->setFuture(QtAndroidPrivate::requestPermission(permission));
}

QString folderPath(const QUrl &url)
{
    if (url.isLocalFile())
        return url.toLocalFile();
    // content://com.android.externalstorage.documents/tree/primary%3AMusic%2FAlbums
    const QString path = url.path(QUrl::FullyDecoded);
    const qsizetype tree = path.indexOf(QLatin1String("/tree/"));
    if (url.scheme() != QLatin1String("content") || tree < 0)
        return url.toString();
    QString document = path.mid(tree + 6);
    document = document.section(QLatin1String("/document/"), 0, 0);
    const QString volume = document.section(QLatin1Char(':'), 0, 0);
    const QString relative = document.section(QLatin1Char(':'), 1);
    const QString base = volume == QLatin1String("primary") ? QStringLiteral("/storage/emulated/0")
                                                            : QStringLiteral("/storage/") + volume;
    return relative.isEmpty() ? base : base + QLatin1Char('/') + relative;
}

} // namespace AndroidSupport
