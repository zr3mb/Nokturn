#pragma once

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

// Where the app keeps its data (settings, library cache, covers).
// The EVERPLAY_PROFILE variable points to a separate directory — handy for testing.
namespace AppPaths {

// The app was called Nokturn before: its folders were <base>/Nokturn/Nokturn. The first start under the
// new name moves them to <base>/Everplay/Everplay, so the library, sign-ins and caches stay. Data that
// can't be moved in one piece (another program holds a file) is copied; a cache that can't is left.
inline void adoptFormerName()
{
    if (qEnvironmentVariableIsSet("EVERPLAY_PROFILE"))
        return;
    const auto adopt = [](QStandardPaths::StandardLocation location, bool copyIfNeeded) {
        // The <organization>/<application> folder: the end of the path, or (the cache on Windows) its parent.
        QString now = QDir::cleanPath(QStandardPaths::writableLocation(location));
        const QString suffix = QStringLiteral("/Everplay/Everplay");
        const qsizetype at = now.lastIndexOf(suffix + QLatin1Char('/'), -1, Qt::CaseInsensitive);
        if (at >= 0)
            now.truncate(at + suffix.size());
        else if (!now.endsWith(suffix, Qt::CaseInsensitive))
            return;   // Android and other systems without the organization in the path
        const QString base = now.chopped(suffix.size());
        const QString former = base + QStringLiteral("/Nokturn/Nokturn");
        if (QFileInfo::exists(now) || !QFileInfo(former).isDir())
            return;
        QDir().mkpath(base + QStringLiteral("/Everplay"));
        if (QDir().rename(former, now)) {
            QDir().rmdir(base + QStringLiteral("/Nokturn"));   // only when nothing else is in it
            return;
        }
        if (!copyIfNeeded)
            return;
        QDir().mkpath(now);
        QDirIterator it(former, QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QFileInfo item(it.next());
            const QString target = now + item.filePath().mid(former.size());
            if (item.isDir())
                QDir().mkpath(target);
            else
                QFile::copy(item.filePath(), target);
        }
    };
    adopt(QStandardPaths::AppDataLocation, true);
    adopt(QStandardPaths::CacheLocation, false);
}

inline QString dataDir()
{
    QString dir = qEnvironmentVariable("EVERPLAY_PROFILE");
    if (dir.isEmpty())
        dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir;
}

inline QString coverDir()
{
    const QString dir = dataDir() + QStringLiteral("/covers");
    QDir().mkpath(dir);
    return dir;
}

// One shared instance — writing to disk happens in batches (sync), not on every change.
inline QSettings *settings()
{
    static QSettings *instance = new QSettings(dataDir() + QStringLiteral("/settings.ini"), QSettings::IniFormat);
    return instance;
}

} // namespace AppPaths
