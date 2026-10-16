#pragma once

#include "Lyrics.h"

#include <QList>
#include <QString>

// Dopasowanie tekstu (bez czasów) do transkrypcji z rozpoznawania mowy (słowa z czasami).
// Globalne wyrównanie sekwencji (Needleman–Wunsch z darmowymi przerwami na końcach):
// słowa tekstu i transkrypcji są porównywane "rozmyto" (odległość Levenshteina),
// bo rozpoznawanie śpiewu nad muzyką często myli pojedyncze litery.
// Linie bez dopasowanych słów dostają czas interpolowany z sąsiadów.
namespace LyricsAligner {

struct Word
{
    QString text;
    qint64 start = 0;   // ms
    qint64 end = 0;
};

struct Result
{
    Lyrics lyrics;          // kopia wejścia z uzupełnionymi czasami (synced = true)
    double coverage = 0;    // udział słów tekstu, które udało się dopasować (0..1)
    int anchoredLines = 0;  // linie z co najmniej jednym dopasowanym słowem
};

QString normalize(const QString &word);
double similarity(const QString &a, const QString &b);

Result align(const Lyrics &lyrics, const QList<Word> &words, qint64 durationMs = -1);

} // namespace LyricsAligner
