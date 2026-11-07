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

// Lyrics timed by forced alignment: `lineWords` holds, for every line of `lyrics`, the words the aligner
// found for it (empty for breaks and lines it didn't place). Those words are the line's own text, so they
// only need mapping onto it — nothing has to be matched.
Result fromLineWords(const Lyrics &lyrics, const QList<QList<Word>> &lineWords, qint64 durationMs = -1);

// The language of the lyrics as whisper's code ("pl", "en"…), from its letters and most common words;
// empty when the text doesn't tell clearly.
QString guessLanguage(const Lyrics &lyrics);

// Lyrics made from the transcription alone (a track nobody has lyrics for): timed lines, origin "stt-text".
Lyrics fromTranscription(const QList<Word> &words, qint64 durationMs = -1);

} // namespace LyricsAligner
