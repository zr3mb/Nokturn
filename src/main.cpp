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

    // Styl "Basic" pozwala w pełni przestylizować kontrolki w QML.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QFont font(QStringLiteral("Segoe UI Variable Text"));
    font.setHintingPreference(QFont::PreferNoHinting);
    QGuiApplication::setFont(font);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    engine.loadFromModule("Nokturn", "Main");
    return app.exec();
}
