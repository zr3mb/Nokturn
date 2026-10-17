#include "Language.h"

#include <QElapsedTimer>
#include <QFont>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QTimer>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("Nokturn"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Nokturn"));
    QGuiApplication::setOrganizationName(QStringLiteral("Nokturn"));
    QGuiApplication::setApplicationVersion(QStringLiteral(NOKTURN_VERSION));
    // Wayland/X11: ties the window to the nokturn.desktop file (icon, name in the panel, MPRIS).
    QGuiApplication::setDesktopFileName(QStringLiteral("nokturn"));

    // The "Basic" style allows fully restyling the controls in QML.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

#ifdef Q_OS_WIN
    QFont font(QStringLiteral("Segoe UI Variable Text"));
    font.setHintingPreference(QFont::PreferNoHinting);
    QGuiApplication::setFont(font);
#endif

    Language::instance()->apply();

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    engine.loadFromModule("Nokturn", "Main");

    // Diagnostics (NOKTURN_PERF_DEBUG=1): reports when the GUI thread was blocked — audio samples go
    // through it, so a stall over ~200 ms is heard as a gap.
    QTimer stallProbe;
    QElapsedTimer stallClock;
    if (qEnvironmentVariableIsSet("NOKTURN_PERF_DEBUG")) {
        stallProbe.setInterval(10);
        stallProbe.setTimerType(Qt::PreciseTimer);
        QObject::connect(&stallProbe, &QTimer::timeout, [&stallClock] {
            const qint64 gap = stallClock.restart();
            if (gap > 60)
                qInfo("perf: GUI thread blocked for %lld ms", gap);
        });
        stallClock.start();
        stallProbe.start();
    }
    return app.exec();
}
