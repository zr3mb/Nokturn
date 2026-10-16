#pragma once

#include "TrackModel.h"

#include <QHash>
#include <QImage>
#include <QJsonObject>
#include <QPromise>
#include <QStringList>

// Scanning music folders. Runs on a worker thread (QtConcurrent),
// so it doesn't touch any QObjects — it only reads files and returns a list of tracks.
namespace Scanner {

struct Input
{
    QStringList folders;
    QHash<QString, Track> cache;   // previous result, key = file path
    QString coverDir;
};

struct Result
{
    QList<Track> tracks;
};

void scan(QPromise<Result> &promise, const Input &input);

QJsonObject toJson(const Track &t);
Track fromJson(const QJsonObject &o);

// A muted color pair for an album without a cover (stable for a given key).
QPair<QColor, QColor> fallbackPalette(const QString &key);
// A muted color pair (light accent + dark background) computed from the cover.
QPair<QColor, QColor> paletteFromImage(const QImage &image);
int stableSeed(const QString &key);

bool isUnder(const QString &path, const QString &folder);

// Label for tracks without an artist tag — stored in the cache, relabeled when the language changes.
QString unknownArtist();
bool isUnknownArtist(const QString &artist);

} // namespace Scanner
