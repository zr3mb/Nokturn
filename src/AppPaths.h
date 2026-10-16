#pragma once

#include <QDir>
#include <QSettings>
#include <QStandardPaths>

// Where the app keeps its data (settings, library cache, covers).
// The NOKTURN_PROFILE variable points to a separate directory — handy for testing.
namespace AppPaths {

inline QString dataDir()
{
    QString dir = qEnvironmentVariable("NOKTURN_PROFILE");
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
