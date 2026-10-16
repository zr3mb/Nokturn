#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

// Tekst utworu: linie z (opcjonalnymi) znacznikami czasu w milisekundach.
// Pusta linia (text == "") to przerwa między zwrotkami — nie ma własnego czasu.
struct LyricLine
{
    qint64 start = -1;   // ms; -1 = brak znacznika
    qint64 end = -1;
    QString text;
};

struct Lyrics
{
    QList<LyricLine> lines;
    bool synced = false;    // każda niepusta linia ma znacznik czasu
    QString source;         // np. "Źródło: LyricFind" (stopka z YouTube Music)
    QString origin;         // "youtube" | "local" | "stt" (czasy dopasowane rozpoznawaniem mowy)

    bool isEmpty() const;
    int wordCount() const;
    QString plainText() const;

    QJsonObject toJson() const;
    static Lyrics fromJson(const QJsonObject &o);

    // Plain text -> linie bez czasów (zachowuje puste linie jako przerwy, przycina nadmiar).
    static Lyrics fromPlainText(const QString &text);
    // Format LRC: "[mm:ss.xx]tekst", wiele znaczników w linii, [offset:±ms]. Bez znaczników -> zwykły tekst.
    static Lyrics fromLrc(const QString &text);
    // Uzupełnia "end" na podstawie początku kolejnej linii.
    void fillEnds(qint64 durationMs = -1);
};
