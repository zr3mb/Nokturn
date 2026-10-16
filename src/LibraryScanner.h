#pragma once

#include "TrackModel.h"

#include <QHash>
#include <QImage>
#include <QJsonObject>
#include <QPromise>
#include <QStringList>

// Skanowanie folderów z muzyką. Działa w wątku roboczym (QtConcurrent),
// więc nie dotyka żadnych QObjectów — tylko czyta pliki i zwraca listę utworów.
namespace Scanner {

struct Input
{
    QStringList folders;
    QHash<QString, Track> cache;   // poprzedni wynik, klucz = ścieżka pliku
    QString coverDir;
};

struct Result
{
    QList<Track> tracks;
};

void scan(QPromise<Result> &promise, const Input &input);

QJsonObject toJson(const Track &t);
Track fromJson(const QJsonObject &o);

// Stonowana para kolorów dla albumu bez okładki (stała dla danego klucza).
QPair<QColor, QColor> fallbackPalette(const QString &key);
// Przygaszona para kolorów (jasny akcent + ciemne tło) wyliczona z okładki.
QPair<QColor, QColor> paletteFromImage(const QImage &image);
int stableSeed(const QString &key);

bool isUnder(const QString &path, const QString &folder);

} // namespace Scanner
