#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

// Lyrics of a track: lines with (optional) timestamps in milliseconds.
// An empty line (text == "") is a break between verses — it has no time of its own.
struct LyricLine
{
    qint64 start = -1;   // ms; -1 = no timestamp
    qint64 end = -1;
    QString text;
};

struct Lyrics
{
    QList<LyricLine> lines;
    bool synced = false;    // every non-empty line has a timestamp
    QString source;         // e.g. "Source: LyricFind" (footer from YouTube Music)
    QString origin;         // "youtube" | "local" | "stt" (timing aligned by speech recognition)

    bool isEmpty() const;
    int wordCount() const;
    QString plainText() const;

    QJsonObject toJson() const;
    static Lyrics fromJson(const QJsonObject &o);

    // Plain text -> lines without timestamps (keeps empty lines as breaks, trims the excess).
    static Lyrics fromPlainText(const QString &text);
    // LRC format: "[mm:ss.xx]text", several tags per line, [offset:±ms]. No tags -> plain text.
    static Lyrics fromLrc(const QString &text);
    // Fills in "end" from the start of the next line.
    void fillEnds(qint64 durationMs = -1);
};
