#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>

// Network tracks kept on disk after they were played, so the next play starts at once and without
// the network. Up to a size limit (1 GB by default); past it the tracks played least, and longest
// ago, go first. Keyed by the stream id (a YouTube videoId).
class AudioCache : public QObject
{
    Q_OBJECT

public:
    explicit AudioCache(QObject *parent = nullptr);

    bool has(const QString &id) const { return m_items.contains(id); }
    QString pathOf(const QString &id) const;
    // A play started: counts towards keeping the track.
    void played(const QString &id);
    // A fully downloaded track; written to disk off the GUI thread.
    void store(const QString &id, const QByteArray &data);
    void remove(const QString &id);
    void clear();
    // Ids not to evict right now (the tracks on the decks).
    void setInUse(const QSet<QString> &ids) { m_inUse = ids; }

    qint64 limitBytes() const { return m_limit; }
    void setLimitBytes(qint64 bytes);
    qint64 usedBytes() const { return m_used; }

signals:
    void changed();

private:
    struct Item { qint64 size = 0; QDateTime used; int plays = 0; };

    static bool validId(const QString &id);
    void load();
    void save();
    bool trim();   // true when something was removed

    QString m_dir;
    QHash<QString, Item> m_items;
    QHash<QString, int> m_pendingPlays;   // plays counted before the file is written
    QSet<QString> m_writing;
    QSet<QString> m_inUse;
    qint64 m_limit = 0;
    qint64 m_used = 0;
};
