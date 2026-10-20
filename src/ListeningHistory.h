#pragma once

#include "TrackModel.h"

#include <QHash>
#include <QObject>
#include <QTimer>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

// What was listened to and when (history.json): one entry per play that counted (30 s, or half of a
// shorter track — PlayerController), with the seconds actually heard. Feeds "Recently played" and the
// stats page; the tracks are kept with it, so YouTube Music ones can be played again after a restart.
class ListeningHistory : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(int revision READ revision NOTIFY changed)

public:
    static ListeningHistory *instance();
    static ListeningHistory *create(QQmlEngine *, QJSEngine *);

    int revision() const { return m_revision; }

    void notePlay(const Track &t, double secondsSoFar);
    void addListening(const QString &id, double seconds);   // while the counted play goes on
    QStringList recentIds(int limit) const;                 // newest first, each track once

    // "month", "year" or "all": totals, top artists / tracks / genres and listening by hour.
    Q_INVOKABLE QVariantMap stats(const QString &period) const;

signals:
    void changed();

private:
    explicit ListeningHistory(QObject *parent = nullptr);

    struct Event { QString id; qint64 at = 0; double seconds = 0; };

    void load();
    void importPlays();       // the first run: the plays the recommendations had counted
    void save() const;
    void keepStreamingTracks();

    QList<Event> m_events;            // oldest first
    QHash<QString, Track> m_tracks;   // the tracks the events refer to
    QTimer m_saveTimer;
    int m_revision = 0;
};
