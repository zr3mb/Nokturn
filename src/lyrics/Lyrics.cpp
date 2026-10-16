#include "Lyrics.h"

#include <QJsonArray>
#include <QRegularExpression>

#include <algorithm>

bool Lyrics::isEmpty() const
{
    return std::none_of(lines.cbegin(), lines.cend(), [](const LyricLine &l) { return !l.text.isEmpty(); });
}

int Lyrics::wordCount() const
{
    int n = 0;
    for (const LyricLine &l : lines)
        n += int(l.text.split(QChar(' '), Qt::SkipEmptyParts).size());
    return n;
}

QString Lyrics::plainText() const
{
    QStringList out;
    for (const LyricLine &l : lines)
        out << l.text;
    return out.join(QChar('\n'));
}

QJsonObject Lyrics::toJson() const
{
    QJsonArray arr;
    for (const LyricLine &l : lines)
        arr.append(QJsonArray { double(l.start), double(l.end), l.text });
    return { { "lines", arr }, { "synced", synced }, { "source", source }, { "origin", origin } };
}

Lyrics Lyrics::fromJson(const QJsonObject &o)
{
    Lyrics out;
    for (const QJsonValue &v : o.value("lines").toArray()) {
        const QJsonArray a = v.toArray();
        out.lines << LyricLine { qint64(a.at(0).toDouble(-1)), qint64(a.at(1).toDouble(-1)), a.at(2).toString() };
    }
    out.synced = o.value("synced").toBool();
    out.source = o.value("source").toString();
    out.origin = o.value("origin").toString();
    return out;
}

Lyrics Lyrics::fromPlainText(const QString &text)
{
    Lyrics out;
    QString normalized = text;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n")).replace(QChar('\r'), QChar('\n'));
    bool lastEmpty = true;   // bez pustych linii na początku i podwójnych przerw
    for (const QString &raw : normalized.split(QChar('\n'))) {
        const QString line = raw.trimmed();
        if (line.isEmpty() && lastEmpty)
            continue;
        out.lines << LyricLine { -1, -1, line };
        lastEmpty = line.isEmpty();
    }
    while (!out.lines.isEmpty() && out.lines.last().text.isEmpty())
        out.lines.removeLast();
    return out;
}

Lyrics Lyrics::fromLrc(const QString &text)
{
    static const QRegularExpression timeTag(QStringLiteral("\\[(\\d{1,3}):(\\d{1,2})(?:[.:](\\d{1,3}))?\\]"));
    static const QRegularExpression metaTag(QStringLiteral("^\\[([a-zA-Z]+):\\s*(.*)\\]$"));

    struct Stamped { qint64 at; QString text; };
    QList<Stamped> stamped;
    qint64 offset = 0;
    QString normalized = text;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));

    for (const QString &raw : normalized.split(QChar('\n'))) {
        const QString line = raw.trimmed();
        if (const auto meta = metaTag.match(line); meta.hasMatch() && !timeTag.match(line).hasMatch()) {
            if (meta.captured(1).compare(QLatin1String("offset"), Qt::CaseInsensitive) == 0)
                offset = meta.captured(2).toLongLong();
            continue;
        }
        QList<qint64> times;
        int pos = 0;
        auto m = timeTag.match(line, pos, QRegularExpression::NormalMatch, QRegularExpression::AnchorAtOffsetMatchOption);
        while (m.hasMatch()) {
            const QString frac = m.captured(3);
            qint64 ms = (m.captured(1).toLongLong() * 60 + m.captured(2).toLongLong()) * 1000;
            if (!frac.isEmpty())
                ms += frac.toLongLong() * (frac.size() == 1 ? 100 : frac.size() == 2 ? 10 : 1);
            times << ms;
            pos = m.capturedEnd();
            m = timeTag.match(line, pos, QRegularExpression::NormalMatch, QRegularExpression::AnchorAtOffsetMatchOption);
        }
        if (times.isEmpty())
            continue;
        // Znaczniki słów w wersji "enhanced LRC" (<mm:ss.xx>) nie są nam potrzebne.
        static const QRegularExpression wordTags(QStringLiteral("<\\d{1,3}:\\d{1,2}(?:[.:]\\d{1,3})?>"));
        const QString body = line.mid(pos).remove(wordTags).trimmed();
        for (qint64 t : std::as_const(times))
            stamped << Stamped { std::max<qint64>(0, t - offset), body };
    }

    if (stamped.isEmpty())
        return fromPlainText(text);

    std::stable_sort(stamped.begin(), stamped.end(), [](const Stamped &a, const Stamped &b) { return a.at < b.at; });
    Lyrics out;
    out.synced = true;
    for (const Stamped &s : std::as_const(stamped)) {
        // Pusta linia z czasem oznacza w LRC przerwę instrumentalną.
        if (s.text.isEmpty() && (out.lines.isEmpty() || out.lines.last().text.isEmpty()))
            continue;
        out.lines << LyricLine { s.text.isEmpty() ? -1 : s.at, -1, s.text };
    }
    out.fillEnds();
    return out;
}

void Lyrics::fillEnds(qint64 durationMs)
{
    qint64 next = durationMs;
    for (qsizetype i = lines.size() - 1; i >= 0; --i) {
        LyricLine &l = lines[i];
        if (l.text.isEmpty() || l.start < 0)
            continue;
        if (l.end < 0 || (next >= 0 && l.end > next))
            l.end = next >= 0 ? next : l.start + 5000;
        next = l.start;
    }
}
