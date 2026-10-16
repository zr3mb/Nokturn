#include "LibraryScanner.h"

#include "TagLibPath.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QImage>
#include <QUrl>

#include <taglib/audioproperties.h>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tpropertymap.h>
#include <taglib/tvariant.h>

#include <algorithm>

namespace Scanner {

namespace {

const QStringList kAudioSuffixes = {
    "mp3", "flac", "m4a", "aac", "ogg", "oga", "opus", "wav", "wma", "aif", "aiff", "ape", "wv", "mpc",
};

const QStringList kCoverNames = { "cover", "folder", "front", "album", "albumart", "albumartsmall" };

struct CoverInfo
{
    QString url;
    QColor colorA;
    QColor colorB;
};

QString qs(const TagLib::String &s)
{
    return QString::fromStdWString(s.toWString()).trimmed();
}

QByteArray md5(const QString &s)
{
    return QCryptographicHash::hash(s.toUtf8(), QCryptographicHash::Md5).toHex();
}

} // namespace

// Średni kolor okładki (z przewagą pikseli nasyconych), przygaszony do palety aplikacji.
QPair<QColor, QColor> paletteFromImage(const QImage &image)
{
    const QImage small = image.scaled(24, 24, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                             .convertToFormat(QImage::Format_RGB32);
    double r = 0, g = 0, b = 0, w = 0;
    for (int y = 0; y < small.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(small.constScanLine(y));
        for (int x = 0; x < small.width(); ++x) {
            const QColor c = QColor::fromRgb(line[x]);
            const double weight = 0.15 + c.hsvSaturationF() * c.valueF();
            r += c.redF() * weight;
            g += c.greenF() * weight;
            b += c.blueF() * weight;
            w += weight;
        }
    }
    const QColor avg = QColor::fromRgbF(float(r / w), float(g / w), float(b / w));
    float h = 0, s = 0, l = 0;
    avg.getHslF(&h, &s, &l);
    if (h < 0) {
        h = 0.08f;
        s = 0.04f;
    }
    s = std::clamp(s, 0.06f, 0.3f);
    return { QColor::fromHslF(h, s, 0.7f), QColor::fromHslF(h, std::min(s + 0.06f, 0.36f), 0.27f) };
}

namespace {

QImage squareCrop(const QImage &img, int size)
{
    const int side = std::min(img.width(), img.height());
    const QImage cropped = img.copy((img.width() - side) / 2, (img.height() - side) / 2, side, side);
    return side > size ? cropped.scaled(size, size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation) : cropped;
}

QImage folderImage(const QString &dirPath, QHash<QString, QImage> &byDir)
{
    auto it = byDir.constFind(dirPath);
    if (it != byDir.cend())
        return *it;

    QImage found;
    const QFileInfoList entries = QDir(dirPath).entryInfoList({ "*.jpg", "*.jpeg", "*.png" }, QDir::Files);
    for (const QFileInfo &fi : entries) {
        if (kCoverNames.contains(fi.completeBaseName().toLower())) {
            found.load(fi.absoluteFilePath());
            if (!found.isNull())
                break;
        }
    }
    byDir.insert(dirPath, found);
    return found;
}

QString formatQuality(const QString &suffix, int sampleRate, int bitrate)
{
    QStringList parts { suffix.toUpper() };
    if (sampleRate > 0)
        parts << QString::number(sampleRate / 1000.0, 'f', sampleRate % 1000 ? 1 : 0).replace('.', ',') + " kHz";
    if (bitrate > 0)
        parts << QString::number(bitrate) + " kbps";
    return parts.join(QStringLiteral(" · "));
}

Track readFile(const QString &path, const QFileInfo &fi, const QString &coverDir,
               QHash<QString, CoverInfo> &coverByAlbum, QHash<QString, QImage> &imageByDir)
{
    Track t;
    t.id = path;
    t.path = path;
    t.source = QStringLiteral("local");
    t.size = fi.size();
    t.mtime = fi.lastModified().toMSecsSinceEpoch();
    t.added = (fi.birthTime().isValid() ? fi.birthTime() : fi.lastModified()).toMSecsSinceEpoch();

    QByteArray picture;
    int sampleRate = 0, bitrate = 0;

    const TagLibPath native(path);
    TagLib::FileRef ref(native.get(), true, TagLib::AudioProperties::Average);
    if (!ref.isNull()) {
        if (const TagLib::Tag *tag = ref.tag()) {
            t.title = qs(tag->title());
            t.artist = qs(tag->artist());
            t.album = qs(tag->album());
            t.genre = qs(tag->genre());
            t.year = int(tag->year());
            t.trackNo = int(tag->track());
        }
        const TagLib::PropertyMap props = ref.properties();
        const auto aa = props.find("ALBUMARTIST");
        if (aa != props.end() && !aa->second.isEmpty())
            t.albumArtist = qs(aa->second.front());

        if (const TagLib::AudioProperties *ap = ref.audioProperties()) {
            t.duration = int((ap->lengthInMilliseconds() + 500) / 1000);
            sampleRate = ap->sampleRate();
            bitrate = ap->bitrate();
        }

        const auto pictures = ref.complexProperties("PICTURE");
        if (!pictures.isEmpty()) {
            const TagLib::ByteVector data = pictures.front().value("data").toByteVector();
            picture = QByteArray(data.data(), int(data.size()));
        }
    }

    if (t.title.isEmpty())
        t.title = fi.completeBaseName();
    if (t.artist.isEmpty())
        t.artist = QStringLiteral("Nieznany wykonawca");
    if (t.album.isEmpty())
        t.album = fi.dir().dirName();
    t.quality = formatQuality(fi.suffix(), sampleRate, bitrate);

    // Okładka: osadzona w pliku albo cover.jpg / folder.jpg w katalogu albumu.
    const QString key = TrackModel::albumKey(t);
    t.seed = stableSeed(key);
    CoverInfo cover = coverByAlbum.value(key);
    if (cover.url.isEmpty()) {
        QImage img;
        if (!picture.isEmpty())
            img.loadFromData(picture);
        if (img.isNull())
            img = folderImage(fi.absolutePath(), imageByDir);

        if (!img.isNull()) {
            const QString file = coverDir + '/' + QString::fromLatin1(md5(key)) + ".jpg";
            const QImage square = squareCrop(img, 512);
            square.save(file, "JPG", 88);
            const auto [a, b] = paletteFromImage(square);
            cover = { QUrl::fromLocalFile(file).toString(), a, b };
            coverByAlbum.insert(key, cover);
        } else if (!coverByAlbum.contains(key)) {
            const auto [a, b] = fallbackPalette(key);
            cover = { QString(), a, b };
            coverByAlbum.insert(key, cover);
        } else {
            cover = coverByAlbum.value(key);
        }
    }
    t.cover = cover.url;
    t.colorA = cover.colorA;
    t.colorB = cover.colorB;
    return t;
}

} // namespace

bool isUnder(const QString &path, const QString &folder)
{
    const QString f = QDir::cleanPath(folder);
    return path.startsWith(f + '/', Qt::CaseInsensitive) || path.compare(f, Qt::CaseInsensitive) == 0;
}

int stableSeed(const QString &key)
{
    return int(uchar(md5(key).at(0))) % 4;
}

QPair<QColor, QColor> fallbackPalette(const QString &key)
{
    static const QList<QPair<QColor, QColor>> palettes = {
        { QColor("#a3bcab"), QColor("#3e594d") }, { QColor("#93a8c3"), QColor("#36445c") },
        { QColor("#ccb893"), QColor("#5a4a34") }, { QColor("#ab9fc3"), QColor("#463d5c") },
        { QColor("#8fb7b3"), QColor("#2e5150") }, { QColor("#cda2a6"), QColor("#5c3a42") },
        { QColor("#c9a48c"), QColor("#573e33") }, { QColor("#c5bd95"), QColor("#545035") },
        { QColor("#a3b3c1"), QColor("#3b4a57") }, { QColor("#b7aac9"), QColor("#463c5c") },
    };
    const QByteArray h = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Md5);
    return palettes.at(int(uchar(h.at(1))) % palettes.size());
}

void scan(QPromise<Result> &promise, const Input &input)
{
    // 1. Zbierz pliki audio
    QStringList files;
    for (const QString &folder : input.folders) {
        QDirIterator it(folder, QDir::Files | QDir::Readable | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            if (promise.isCanceled())
                return;
            const QString path = QDir::cleanPath(it.next());
            if (kAudioSuffixes.contains(it.fileInfo().suffix().toLower()))
                files << path;
        }
    }
    files.removeDuplicates();
    promise.setProgressRange(0, int(files.size()));

    // 2. Odczytaj tagi (albo weź z cache, jeśli plik się nie zmienił)
    QDir().mkpath(input.coverDir);
    QHash<QString, CoverInfo> coverByAlbum;
    QHash<QString, QImage> imageByDir;
    Result result;
    result.tracks.reserve(files.size());

    int done = 0;
    for (const QString &path : files) {
        if (promise.isCanceled())
            return;
        const QFileInfo fi(path);
        const auto cached = input.cache.constFind(path);
        const bool fresh = cached != input.cache.cend()
                           && cached->size == fi.size()
                           && cached->mtime == fi.lastModified().toMSecsSinceEpoch()
                           && (cached->cover.isEmpty() || QFileInfo::exists(QUrl(cached->cover).toLocalFile()));
        if (fresh) {
            result.tracks << *cached;
            if (!cached->cover.isEmpty())
                coverByAlbum.insert(TrackModel::albumKey(*cached), { cached->cover, cached->colorA, cached->colorB });
        } else {
            Track t = readFile(path, fi, input.coverDir, coverByAlbum, imageByDir);
            if (cached != input.cache.cend())
                t.added = cached->added;   // zmieniony plik — zachowaj datę dodania
            result.tracks << t;
        }
        promise.setProgressValueAndText(++done, fi.fileName());
    }

    promise.addResult(result);
}

QJsonObject toJson(const Track &t)
{
    return {
        { "path", t.path }, { "title", t.title }, { "artist", t.artist }, { "albumArtist", t.albumArtist },
        { "album", t.album }, { "genre", t.genre }, { "year", t.year }, { "track", t.trackNo },
        { "duration", t.duration }, { "quality", t.quality }, { "cover", t.cover },
        { "colorA", t.colorA.name() }, { "colorB", t.colorB.name() }, { "seed", t.seed },
        { "size", double(t.size) }, { "mtime", double(t.mtime) }, { "added", double(t.added) },
    };
}

Track fromJson(const QJsonObject &o)
{
    Track t;
    t.path = o.value("path").toString();
    t.id = t.path;
    t.source = QStringLiteral("local");
    t.title = o.value("title").toString();
    t.artist = o.value("artist").toString();
    t.albumArtist = o.value("albumArtist").toString();
    t.album = o.value("album").toString();
    t.genre = o.value("genre").toString();
    t.year = o.value("year").toInt();
    t.trackNo = o.value("track").toInt();
    t.duration = o.value("duration").toInt();
    t.quality = o.value("quality").toString();
    t.cover = o.value("cover").toString();
    t.colorA = QColor(o.value("colorA").toString());
    t.colorB = QColor(o.value("colorB").toString());
    t.seed = o.value("seed").toInt();
    t.size = qint64(o.value("size").toDouble());
    t.mtime = qint64(o.value("mtime").toDouble());
    t.added = qint64(o.value("added").toDouble());
    return t;
}

} // namespace Scanner
