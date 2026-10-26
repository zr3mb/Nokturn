#include "Language.h"
#include "Library.h"
#include "online/OnlineService.h"
#include "platform/GameOverlay.h"

#include <QDesktopServices>
#include <QElapsedTimer>
#include <QFont>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTimer>

#ifdef Q_OS_ANDROID
#  include "platform/AndroidSupport.h"
#  include <QtWebView/QtWebView>
#endif

int main(int argc, char *argv[])
{
    // Images (covers) give up their memory copy once they're uploaded to the GPU, and skip the shared
    // texture atlas — covers are drawn from a mipmapped copy of their own, so the atlas only kept a
    // second one. Variables set by the user win.
    if (!qEnvironmentVariableIsSet("QSG_TRANSIENT_IMAGES"))
        qputenv("QSG_TRANSIENT_IMAGES", "1");
    if (!qEnvironmentVariableIsSet("QSG_ATLAS_SIZE_LIMIT"))
        qputenv("QSG_ATLAS_SIZE_LIMIT", "1");

#ifdef Q_OS_ANDROID
    QtWebView::initialize();   // the in-app sign-in page (before the application object)
#endif
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

    // "nokturn --overlay" / "--lyrics-overlay": the running Nokturn opens or closes its overlay;
    // with none running, this one starts and opens it.
    const bool overlayRequest = app.arguments().contains(QStringLiteral("--overlay"));
    const bool lyricsOverlayRequest = app.arguments().contains(QStringLiteral("--lyrics-overlay"));
    if (overlayRequest && GameOverlay::sendToRunning(QStringLiteral("overlay")))
        return 0;
    if (lyricsOverlayRequest && GameOverlay::sendToRunning(QStringLiteral("lyrics-overlay")))
        return 0;
    // "nokturn nokturn://r/<code>" (the system opens links this way): a running Nokturn opens it.
    QString link;
    for (const QString &arg : app.arguments().mid(1))
        if ((arg.startsWith(QLatin1String("nokturn:")) || arg.startsWith(QLatin1String("http"))) && OnlineService::isLink(arg))
            link = arg;
    if (!link.isEmpty() && GameOverlay::sendToRunning(QStringLiteral("open ") + link))
        return 0;

    Language::instance()->apply();

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    engine.loadFromModule("Nokturn", "Main");
    QObject::connect(GameOverlay::instance(), &GameOverlay::linkReceived, OnlineService::instance(), &OnlineService::openLink);
    if (!link.isEmpty())
        QTimer::singleShot(1200, OnlineService::instance(), [link] { OnlineService::instance()->openLink(link); });
#ifdef Q_OS_ANDROID
    // nokturn:// links from a browser or a chat app come as intents, through QDesktopServices.
    QDesktopServices::setUrlHandler(QStringLiteral("nokturn"), OnlineService::instance(), "openUrl");
    // The local library: the system Music folder once the storage permission is granted.
    AndroidSupport::requestAudioPermission([](bool granted) {
        if (!granted)
            return;
        Library *library = Library::instance();
        if (library->folders().isEmpty())
            library->addFolder(QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::MusicLocation)));
        else
            library->rescan(false);
    });
#endif
    if (overlayRequest)
        QTimer::singleShot(1500, GameOverlay::instance(), &GameOverlay::show);
    else if (lyricsOverlayRequest)
        QTimer::singleShot(1500, GameOverlay::instance(), &GameOverlay::showLyrics);

    // After a GPU reset the scene graph rebuilds its textures from the images' memory copies, which
    // QSG_TRANSIENT_IMAGES dropped — so the images are loaded again (Theme.imageEpoch changes their URLs).
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
    if (QObject *theme = engine.singletonInstance<QObject *>("Nokturn", "Theme"); window && theme) {
        QObject::connect(window, &QQuickWindow::sceneGraphInvalidated, theme, [theme] {
            theme->setProperty("imageEpoch", theme->property("imageEpoch").toInt() + 1);
        }, Qt::QueuedConnection);
    }

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
