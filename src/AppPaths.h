#pragma once

#include <QDir>
#include <QSettings>
#include <QStandardPaths>

// Miejsce na dane aplikacji (ustawienia, cache biblioteki, okładki).
// Zmienna NOKTURN_PROFILE pozwala wskazać osobny katalog — przydatne przy testach.
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

// Jedna współdzielona instancja — zapis na dysk odbywa się zbiorczo (sync), a nie przy każdej zmianie.
inline QSettings *settings()
{
    static QSettings *instance = new QSettings(dataDir() + QStringLiteral("/settings.ini"), QSettings::IniFormat);
    return instance;
}

} // namespace AppPaths
