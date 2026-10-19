#include "TrackModel.h"

#include <QRegularExpression>

#include <algorithm>

TrackModel::TrackModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int TrackModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_tracks.size());
}

QVariant TrackModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || !isValidRow(index.row()))
        return {};

    const Track &t = m_tracks.at(index.row());
    switch (role) {
    case IdRole:           return t.id;
    case PathRole:         return t.path;
    case TitleRole:        return t.title;
    case ArtistRole:       return t.artist;
    case AlbumArtistRole:  return t.albumArtist.isEmpty() ? t.artist : t.albumArtist;
    case AlbumRole:        return t.album;
    case AlbumKeyRole:     return albumKey(t);
    case GenreRole:        return t.genre;
    case YearRole:         return t.year;
    case TrackNoRole:      return t.trackNo;
    case DurationRole:     return t.duration;
    case DurationTextRole:
        // Tracks from YouTube without a given length only learn it once the stream has loaded.
        return t.duration <= 0 && t.source != QLatin1String("radio") ? QString() : formatDuration(t.duration);
    case SourceRole:       return t.source;
    case QualityRole:      return t.quality;
    case CoverRole:        return t.cover;
    case ColorARole:       return t.colorA;
    case ColorBRole:       return t.colorB;
    case SeedRole:         return t.seed;
    case LikedRole:        return t.liked;
    case DemoRole:         return t.demo;
    case RowRole:          return index.row();
    case ArtistIdRole:     return t.artistId;
    default:               return {};
    }
}

QHash<int, QByteArray> TrackModel::roleNames() const
{
    return {
        { IdRole, "id" },
        { PathRole, "path" },
        { TitleRole, "title" },
        { ArtistRole, "artist" },
        { ArtistIdRole, "artistId" },
        { AlbumArtistRole, "albumArtist" },
        { AlbumRole, "album" },
        { AlbumKeyRole, "albumKey" },
        { GenreRole, "genre" },
        { YearRole, "year" },
        { TrackNoRole, "trackNo" },
        { DurationRole, "duration" },
        { DurationTextRole, "durationText" },
        { SourceRole, "source" },
        { QualityRole, "quality" },
        { CoverRole, "cover" },
        { ColorARole, "colorA" },
        { ColorBRole, "colorB" },
        { SeedRole, "seed" },
        { LikedRole, "liked" },
        { DemoRole, "demo" },
        { RowRole, "row" },
    };
}

void TrackModel::setTracks(QList<Track> tracks)
{
    beginResetModel();
    m_tracks = std::move(tracks);
    m_rowById.clear();
    m_rowById.reserve(m_tracks.size());
    for (int i = 0; i < m_tracks.size(); ++i)
        m_rowById.insert(m_tracks.at(i).id, i);
    endResetModel();
    emit countChanged();
    ++m_revision;
    emit revisionChanged();
}

void TrackModel::appendTracks(const QList<Track> &tracks)
{
    // Rows are only added at the end: existing row numbers stay valid, so the revision (which makes
    // every list re-read its rows) doesn't change.
    QList<Track> fresh;
    bool updated = false;
    for (const Track &t : tracks) {
        if (const auto it = m_rowById.constFind(t.id); it != m_rowById.cend()) {
            // Already here (e.g. from the cache) — newer details from the source replace stale ones.
            Track &old = m_tracks[*it];
            if (old.title != t.title || old.artist != t.artist || old.artistId != t.artistId || old.album != t.album
                || (old.cover != t.cover && !t.cover.isEmpty()) || (old.duration != t.duration && t.duration > 0)) {
                old.title = t.title;
                old.artist = t.artist;
                old.artistId = t.artistId;
                old.album = t.album;
                if (!t.cover.isEmpty())
                    old.cover = t.cover;
                if (t.duration > 0)
                    old.duration = t.duration;
                emit dataChanged(index(*it), index(*it));
                updated = true;
            }
            continue;
        }
        if (std::none_of(fresh.cbegin(), fresh.cend(), [&](const Track &f) { return f.id == t.id; }))
            fresh << t;
    }
    if (updated) {
        ++m_revision;   // rows read their data through the revision
        emit revisionChanged();
    }
    if (fresh.isEmpty())
        return;
    const int first = int(m_tracks.size());
    beginInsertRows({}, first, first + int(fresh.size()) - 1);
    for (const Track &t : std::as_const(fresh)) {
        m_rowById.insert(t.id, int(m_tracks.size()));
        m_tracks << t;
    }
    endInsertRows();
    emit countChanged();
}

QVariantMap TrackModel::get(int row) const
{
    QVariantMap map;
    if (!isValidRow(row))
        return map;

    const QModelIndex idx = index(row);
    const auto names = roleNames();
    for (auto it = names.cbegin(); it != names.cend(); ++it)
        map.insert(QString::fromUtf8(it.value()), data(idx, it.key()));
    return map;
}

void TrackModel::toggleLiked(int row)
{
    if (!isValidRow(row))
        return;

    m_tracks[row].liked = !m_tracks[row].liked;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, { LikedRole });
    ++m_revision;
    emit revisionChanged();
    emit likedChanged(row, m_tracks[row].liked);
}

void TrackModel::setColors(int row, const QColor &a, const QColor &b)
{
    if (!isValidRow(row))
        return;
    m_tracks[row].colorA = a;
    m_tracks[row].colorB = b;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, { ColorARole, ColorBRole });
}

QString TrackModel::formatDuration(int seconds)
{
    if (seconds <= 0)
        return tr("LIVE");
    if (seconds >= 3600)
        return QStringLiteral("%1:%2:%3").arg(seconds / 3600)
            .arg((seconds / 60) % 60, 2, 10, QLatin1Char('0'))
            .arg(seconds % 60, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

QString TrackModel::albumKey(const Track &t)
{
    const QString artist = t.albumArtist.isEmpty() ? t.artist : t.albumArtist;
    return artist.toCaseFolded() + QChar(0x1f) + t.album.toCaseFolded();
}

QString TrackModel::primaryArtist(const QString &artist)
{
    static const QRegularExpression split(QStringLiteral("\\s*(?:,|;|\\s(?:feat\\.?|ft\\.?|featuring)\\s)\\s*"),
                                          QRegularExpression::CaseInsensitiveOption);
    const QString first = artist.split(split).value(0).trimmed();
    return first.isEmpty() ? artist.trimmed() : first;
}
