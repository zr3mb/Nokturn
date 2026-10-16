#pragma once

#include "Lyrics.h"

#include <QList>
#include <QString>

// Aligning lyrics (without timestamps) to the transcription from speech recognition (words with timestamps).
// Global sequence alignment (Needleman–Wunsch with free end gaps):
// lyric and transcription words are compared "fuzzily" (Levenshtein distance),
// because recognizing singing over music often confuses single letters.
// Lines without matched words get a time interpolated from their neighbors.
namespace LyricsAligner {

struct Word
{
    QString text;
    qint64 start = 0;   // ms
    qint64 end = 0;
};

struct Result
{
    Lyrics lyrics;          // a copy of the input with the timestamps filled in (synced = true)
    double coverage = 0;    // share of lyric words that were matched (0..1)
    int anchoredLines = 0;  // lines with at least one matched word
};

QString normalize(const QString &word);
double similarity(const QString &a, const QString &b);

Result align(const Lyrics &lyrics, const QList<Word> &words, qint64 durationMs = -1);

} // namespace LyricsAligner
