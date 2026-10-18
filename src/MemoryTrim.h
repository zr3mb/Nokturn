#pragma once

#include <QObject>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

// Hands memory back to the system once the interface has let go of a lot of it (a page or the full-screen
// view closed, the library was rebuilt, the window was minimized): the JavaScript garbage collector runs
// and the C heap returns its free pages. A minimized window also drops cached graphics resources and, on
// Windows, the pages it isn't using — they come back as soon as they're needed.
class MemoryTrim : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Memory)
    QML_SINGLETON

public:
    static MemoryTrim *create(QQmlEngine *engine, QJSEngine *);

    // A burst of calls trims once, when things have settled.
    Q_INVOKABLE void trimSoon(int delayMs = 1500);

private:
    explicit MemoryTrim(QQmlEngine *engine);
    void trim();
    void releaseHeap();

    QQmlEngine *m_engine;
    QTimer m_timer;
};
