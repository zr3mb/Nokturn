#include <QFont>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("Nokturn"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Nokturn"));
    QGuiApplication::setOrganizationName(QStringLiteral("Nokturn"));
    QGuiApplication::setApplicationVersion(QStringLiteral(NOKTURN_VERSION));
    // Wayland/X11: powiązanie okna z plikiem nokturn.desktop (ikona, nazwa w panelu, MPRIS).
    QGuiApplication::setDesktopFileName(QStringLiteral("nokturn"));

    // Styl "Basic" pozwala w pełni przestylizować kontrolki w QML.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

#ifdef Q_OS_WIN
    QFont font(QStringLiteral("Segoe UI Variable Text"));
    font.setHintingPreference(QFont::PreferNoHinting);
    QGuiApplication::setFont(font);
#endif

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    engine.loadFromModule("Nokturn", "Main");
    return app.exec();
}
