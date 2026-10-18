#include "MemoryTrim.h"

#include <QGuiApplication>
#include <QQmlEngine>
#include <QQuickWindow>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <malloc.h>   // _heapmin
#elif defined(Q_OS_LINUX)
#  include <malloc.h>   // malloc_trim (glibc)
#endif

MemoryTrim *MemoryTrim::create(QQmlEngine *engine, QJSEngine *)
{
    return new MemoryTrim(engine);
}

MemoryTrim::MemoryTrim(QQmlEngine *engine)
    : m_engine(engine)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &MemoryTrim::trim);
}

void MemoryTrim::trimSoon(int delayMs)
{
    m_timer.start(delayMs);
}

void MemoryTrim::trim()
{
    m_engine->collectGarbage();
    // What the collector freed is deleted on the next pass of the event loop — the heap is trimmed after that.
    QTimer::singleShot(0, this, &MemoryTrim::releaseHeap);
}

void MemoryTrim::releaseHeap()
{
    [[maybe_unused]] bool minimized = false;
    for (QWindow *window : QGuiApplication::topLevelWindows()) {
        auto *quick = qobject_cast<QQuickWindow *>(window);
        if (quick && quick->visibility() == QWindow::Minimized) {
            quick->releaseResources();
            minimized = true;
        }
    }

#if defined(Q_OS_WIN)
    _heapmin();
    // HeapOptimizeResources (3, Windows 8.1+): decommits the free memory the heap keeps for reuse.
    struct { DWORD version = 1; DWORD flags = 0; } optimize;
    HeapSetInformation(GetProcessHeap(), HEAP_INFORMATION_CLASS(3), &optimize, sizeof(optimize));
    if (minimized)
        SetProcessWorkingSetSize(GetCurrentProcess(), SIZE_T(-1), SIZE_T(-1));
#elif defined(__GLIBC__)
    malloc_trim(0);
#endif
}
