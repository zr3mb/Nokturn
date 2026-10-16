#include "LyricsAligner.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace LyricsAligner {

namespace {

struct Token
{
    QString norm;
    int line = -1;        // LyricsAligner: indeks linii tekstu
    int indexInLine = 0;
    qint64 start = 0;     // transkrypcja: czasy słowa
    qint64 end = 0;
};

bool isNoise(const QString &raw)
{
    // Whisper oznacza fragmenty bez śpiewu np. "[Music]", "(muzyka)", "♪".
    return raw.contains(QChar('[')) || raw.contains(QChar(']')) || raw.contains(QChar('(')) || raw.contains(QChar(')'))
           || raw.contains(QChar(0x266A)) || raw.contains(QChar(0x266B)) || raw.contains(QChar('*'));
}

int levenshtein(const QString &a, const QString &b)
{
    const int n = int(a.size()), m = int(b.size());
    std::vector<int> prev(m + 1), cur(m + 1);
    for (int j = 0; j <= m; ++j)
        prev[j] = j;
    for (int i = 1; i <= n; ++i) {
        cur[0] = i;
        for (int j = 1; j <= m; ++j) {
            const int sub = prev[j - 1] + (a.at(i - 1) == b.at(j - 1) ? 0 : 1);
            cur[j] = std::min({ sub, prev[j] + 1, cur[j - 1] + 1 });
        }
        std::swap(prev, cur);
    }
    return prev[m];
}

// Wyniki w dziesiątych częściach punktu (liczby całkowite — szybciej i bez dryfu).
constexpr int kLyricGap = -6;       // słowo tekstu nierozpoznane
constexpr int kTranscriptGap = -4;  // słowo transkrypcji spoza tekstu (szum, chórki, przesłyszenia)
constexpr int kMismatch = -10;
constexpr double kThreshold = 0.72;

int pairScore(const QString &a, const QString &b)
{
    const double s = similarity(a, b);
    return s >= kThreshold ? int(10 + 10 * s) : kMismatch;
}

enum Step : std::uint8_t { None, Diag, Up, Left };

} // namespace

QString normalize(const QString &word)
{
    const QString decomposed = word.toLower().normalized(QString::NormalizationForm_KD);
    QString out;
    out.reserve(decomposed.size());
    for (const QChar c : decomposed) {
        if (c == QChar(0x0142))          // ł nie rozkłada się w NFKD
            out += QChar('l');
        else if (c.isLetterOrNumber())
            out += c;
        // znaki diakrytyczne (Mn), apostrofy i interpunkcja odpadają
    }
    return out;
}

double similarity(const QString &a, const QString &b)
{
    if (a == b)
        return 1.0;
    const int len = int(std::max(a.size(), b.size()));
    if (len == 0)
        return 0.0;
    // Krótkie słowa ("i", "a", "to") muszą się zgadzać dokładnie — inaczej wszystko pasuje do wszystkiego.
    if (std::min(a.size(), b.size()) <= 2)
        return 0.0;
    return 1.0 - double(levenshtein(a, b)) / len;
}

Result align(const Lyrics &lyrics, const QList<Word> &words, qint64 durationMs)
{
    Result result;
    result.lyrics = lyrics;

    std::vector<Token> lyr;
    std::vector<int> lineFirstToken(lyrics.lines.size(), -1);
    std::vector<int> lineTokenCount(lyrics.lines.size(), 0);
    for (int li = 0; li < lyrics.lines.size(); ++li) {
        int k = 0;
        for (const QString &w : lyrics.lines.at(li).text.split(QChar(' '), Qt::SkipEmptyParts)) {
            const QString n = normalize(w);
            if (n.isEmpty())
                continue;
            if (lineFirstToken[li] < 0)
                lineFirstToken[li] = int(lyr.size());
            lyr.push_back({ n, li, k++, 0, 0 });
        }
        lineTokenCount[li] = k;
    }

    std::vector<Token> tr;
    std::vector<qint64> durations;
    for (const Word &w : words) {
        if (isNoise(w.text))
            continue;
        // Jedno "słowo" z rozpoznawania może zawierać kilka (np. bez spacji przed interpunkcją).
        const QStringList parts = w.text.split(QChar(' '), Qt::SkipEmptyParts);
        const qint64 span = std::max<qint64>(1, w.end - w.start);
        for (int p = 0; p < parts.size(); ++p) {
            const QString n = normalize(parts.at(p));
            if (n.isEmpty())
                continue;
            const qint64 s = w.start + span * p / parts.size();
            const qint64 e = w.start + span * (p + 1) / parts.size();
            tr.push_back({ n, -1, 0, s, e });
            durations.push_back(e - s);
        }
    }

    const int n = int(lyr.size()), m = int(tr.size());
    if (n == 0 || m == 0 || qint64(n) * m > 12'000'000)
        return result;

    // Typowa długość słowa — do przesuwania początku linii, gdy pierwsze słowa nie zostały rozpoznane.
    std::nth_element(durations.begin(), durations.begin() + durations.size() / 2, durations.end());
    const qint64 wordMs = std::clamp<qint64>(durations[durations.size() / 2], 220, 650);

    // ---- wyrównanie ----
    std::vector<int> prev(m + 1, 0), cur(m + 1, 0);
    std::vector<std::uint8_t> steps(size_t(n + 1) * (m + 1), None);
    auto at = [m](int i, int j) { return size_t(i) * (m + 1) + j; };
    for (int i = 1; i <= n; ++i)
        steps[at(i, 0)] = Up;
    for (int j = 1; j <= m; ++j)
        steps[at(0, j)] = Left;
    // Wiersz/kolumna zerowa = 0: przerwy na początku są darmowe (intro, nierozpoznany początek).

    int best = 0, bestI = n, bestJ = m;
    for (int i = 1; i <= n; ++i) {
        cur[0] = 0;
        for (int j = 1; j <= m; ++j) {
            const int diag = prev[j - 1] + pairScore(lyr[i - 1].norm, tr[j - 1].norm);
            const int up = prev[j] + kLyricGap;
            const int left = cur[j - 1] + kTranscriptGap;
            if (diag >= up && diag >= left) {
                cur[j] = diag;
                steps[at(i, j)] = Diag;
            } else if (up >= left) {
                cur[j] = up;
                steps[at(i, j)] = Up;
            } else {
                cur[j] = left;
                steps[at(i, j)] = Left;
            }
        }
        // Koniec też jest darmowy: najlepszy wynik w ostatniej kolumnie albo w ostatnim wierszu.
        if (cur[m] > best) {
            best = cur[m];
            bestI = i;
            bestJ = m;
        }
        std::swap(prev, cur);
    }
    for (int j = 1; j <= m; ++j) {
        if (prev[j] > best) {
            best = prev[j];
            bestI = n;
            bestJ = j;
        }
    }

    std::vector<int> matchOf(n, -1);
    for (int i = bestI, j = bestJ; i > 0 && j > 0;) {
        switch (steps[at(i, j)]) {
        case Diag:
            if (similarity(lyr[i - 1].norm, tr[j - 1].norm) >= kThreshold)
                matchOf[i - 1] = j - 1;
            --i;
            --j;
            break;
        case Up:
            --i;
            break;
        default:
            --j;
            break;
        }
    }

    // ---- czasy linii ----
    struct Anchor { int line; int pos; qint64 start; qint64 lastEnd; };
    QList<Anchor> anchors;
    int matched = 0;
    for (int li = 0; li < lyrics.lines.size(); ++li) {
        const int first = lineFirstToken[li];
        if (first < 0)
            continue;
        int firstMatch = -1;
        qint64 lastEnd = -1;
        for (int t = first; t < first + lineTokenCount[li]; ++t) {
            if (matchOf[t] < 0)
                continue;
            ++matched;
            if (firstMatch < 0)
                firstMatch = t;
            lastEnd = tr[matchOf[t]].end;
        }
        if (firstMatch >= 0) {
            const qint64 start = tr[matchOf[firstMatch]].start - lyr[firstMatch].indexInLine * wordMs;
            anchors << Anchor { li, first, std::max<qint64>(0, start), lastEnd };
        }
    }
    result.coverage = double(matched) / n;
    result.anchoredLines = int(anchors.size());
    if (anchors.isEmpty())
        return result;

    QList<LyricLine> &lines = result.lyrics.lines;
    std::vector<qint64> lastWordEnd(lines.size(), -1);
    int ai = 0;
    for (int li = 0; li < lines.size(); ++li) {
        const int pos = lineFirstToken[li];
        if (pos < 0) {
            lines[li].start = lines[li].end = -1;
            continue;
        }
        while (ai < anchors.size() && anchors[ai].line < li)
            ++ai;
        if (ai < anchors.size() && anchors[ai].line == li) {
            lines[li].start = anchors[ai].start;
            lastWordEnd[li] = anchors[ai].lastEnd;
            continue;
        }
        const Anchor *before = ai > 0 ? &anchors[ai - 1] : nullptr;
        const Anchor *after = ai < anchors.size() ? &anchors[ai] : nullptr;
        qint64 t;
        if (before && after && after->pos > before->pos)
            t = before->start + (after->start - before->start) * (pos - before->pos) / (after->pos - before->pos);
        else if (after)
            t = after->start - qint64(after->pos - pos) * wordMs;
        else
            t = before->start + qint64(pos - before->pos) * wordMs;
        lines[li].start = std::max<qint64>(0, t);
    }

    // Kolejność musi być rosnąca (przesunięcie o nierozpoznane słowa mogło ją zaburzyć).
    qint64 last = -1;
    for (LyricLine &l : lines) {
        if (l.start < 0)
            continue;
        if (l.start <= last)
            l.start = last + 120;
        if (durationMs > 0)
            l.start = std::min(l.start, durationMs);
        last = l.start;
    }

    // Koniec linii: początek następnej, ale nie dłużej niż chwilę po ostatnim zaśpiewanym słowie
    // (w przerwie instrumentalnej nic nie powinno być podświetlone).
    qint64 next = durationMs;
    for (int li = int(lines.size()) - 1; li >= 0; --li) {
        LyricLine &l = lines[li];
        if (l.start < 0)
            continue;
        qint64 end = next >= 0 ? next : l.start + 6000;
        if (lastWordEnd[li] > l.start)
            end = std::min(end, lastWordEnd[li] + 1500);
        l.end = std::max(end, l.start + 300);
        next = l.start;
    }

    result.lyrics.synced = true;
    result.lyrics.origin = QStringLiteral("stt");
    return result;
}

} // namespace LyricsAligner
