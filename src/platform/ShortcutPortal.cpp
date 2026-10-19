#include "ShortcutPortal.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QLoggingCategory>
#include <QStandardPaths>

namespace {

Q_LOGGING_CATEGORY(lcPortal, "nokturn.shortcuts")

const QString kService = QStringLiteral("org.freedesktop.portal.Desktop");
const QString kPath = QStringLiteral("/org/freedesktop/portal/desktop");
const QString kInterface = QStringLiteral("org.freedesktop.portal.GlobalShortcuts");
const QString kConnection = QStringLiteral("nokturn-shortcuts");

// One shortcut as the portal passes it: (s id, a{sv} options)
struct PortalShortcut
{
    QString id;
    QVariantMap options;
};
using PortalShortcuts = QList<PortalShortcut>;

QDBusArgument &operator<<(QDBusArgument &arg, const PortalShortcut &s)
{
    arg.beginStructure();
    arg << s.id << s.options;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, PortalShortcut &s)
{
    arg.beginStructure();
    arg >> s.id >> s.options;
    arg.endStructure();
    return arg;
}

// a{sv} inside a message comes as a QDBusArgument (or already as a map).
QVariantMap toMap(const QVariant &value)
{
    if (value.metaType() == QMetaType::fromType<QDBusArgument>()) {
        QVariantMap map;
        value.value<QDBusArgument>() >> map;
        return map;
    }
    return value.toMap();
}

// The portal knows an app by its .desktop file. An AppImage or a build from source has none installed —
// then a small one goes into ~/.local/share/applications (as AppImage integration tools do).
void ensureDesktopFile()
{
    if (!QStandardPaths::locate(QStandardPaths::ApplicationsLocation, QStringLiteral("nokturn.desktop")).isEmpty())
        return;
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    const QString appImage = qEnvironmentVariable("APPIMAGE");
    const QString exe = appImage.isEmpty() ? QCoreApplication::applicationFilePath() : appImage;
    QFile file(dir + QStringLiteral("/nokturn.desktop"));
    if (!QDir().mkpath(dir) || !file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;
    file.write(QStringLiteral("[Desktop Entry]\nType=Application\nName=Nokturn\nGenericName=Music Player\n"
                              "Exec=\"%1\" %U\nIcon=nokturn\nCategories=AudioVideo;Audio;Player;\nTerminal=false\n")
                   .arg(exe).toUtf8());
    qCInfo(lcPortal, "wrote %s", qPrintable(file.fileName()));
}

} // namespace

Q_DECLARE_METATYPE(PortalShortcut)
Q_DECLARE_METATYPE(PortalShortcuts)

ShortcutPortal::ShortcutPortal(QObject *parent)
    : QObject(parent)
{
    qDBusRegisterMetaType<PortalShortcut>();
    qDBusRegisterMetaType<PortalShortcuts>();
}

void ShortcutPortal::start(const QString &id, const QString &description, const QString &preferredTrigger)
{
    m_id = id;
    m_description = description;
    m_preferred = preferredTrigger;
    // A connection of its own: the portal takes the app's id only as the first call on a connection, and
    // Qt has already asked the portal about the theme on the shared one.
    QDBusConnection bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, kConnection);
    if (!bus.isConnected())
        return;
    ensureDesktopFile();

    // An app outside Flatpak / Snap tells the portal who it is (portal 1.19+); without an id the portal
    // refuses the session. Older portals guess it from the process.
    QDBusMessage reg = QDBusMessage::createMethodCall(kService, kPath, QStringLiteral("org.freedesktop.host.portal.Registry"),
                                                      QStringLiteral("Register"));
    reg << QStringLiteral("nokturn") << QVariantMap();
    auto *registered = new QDBusPendingCallWatcher(bus.asyncCall(reg), this);
    connect(registered, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        QDBusConnection bus = this->bus();
        w->deleteLater();
        if (w->isError())
            qCInfo(lcPortal, "register: %s", qPrintable(w->error().message()));
        QDBusMessage get = QDBusMessage::createMethodCall(kService, kPath, QStringLiteral("org.freedesktop.DBus.Properties"),
                                                          QStringLiteral("Get"));
        get << kInterface << QStringLiteral("version");
        auto *version = new QDBusPendingCallWatcher(bus.asyncCall(get), this);
        connect(version, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
            QDBusConnection bus = this->bus();
            w->deleteLater();
            const QDBusPendingReply<QDBusVariant> reply = *w;
            if (reply.isError()) {
                qCInfo(lcPortal, "no global shortcuts portal: %s", qPrintable(reply.error().message()));
                emit changed();
                return;
            }
            m_available = true;
            m_version = reply.value().variant().toUInt();
            bus.connect(kService, kPath, kInterface, QStringLiteral("Activated"), this, SLOT(onActivated(QDBusMessage)));
            bus.connect(kService, kPath, kInterface, QStringLiteral("ShortcutsChanged"), this, SLOT(onShortcutsChanged(QDBusMessage)));

            const QString token = QStringLiteral("nokturn%1").arg(++m_counter);
            watchRequest(token, "session");
            QDBusMessage create = QDBusMessage::createMethodCall(kService, kPath, kInterface, QStringLiteral("CreateSession"));
            create << QVariantMap { { "handle_token", token }, { "session_handle_token", QStringLiteral("nokturn_overlay") } };
            call(create, "CreateSession");
            emit changed();
        });
    });
}

QDBusConnection ShortcutPortal::bus() const
{
    return QDBusConnection(kConnection);
}

// An async call whose failure is only logged (the answer itself comes as the request's Response).
void ShortcutPortal::call(const QDBusMessage &message, const char *what)
{
    auto *watcher = new QDBusPendingCallWatcher(bus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [what](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        if (w->isError())
            qCWarning(lcPortal, "%s: %s", what, qPrintable(w->error().message()));
    });
}

QString ShortcutPortal::requestPath(const QString &token) const
{
    // /org/freedesktop/portal/desktop/request/SENDER/TOKEN — the sender's unique name without ":"
    // and with "." as "_". Known before the call, so the response can't be missed.
    QString sender = bus().baseService().mid(1);
    sender.replace(QLatin1Char('.'), QLatin1Char('_'));
    return QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token);
}

void ShortcutPortal::watchRequest(const QString &token, const char *step)
{
    QDBusConnection bus = this->bus();
    if (!m_requestPath.isEmpty())
        bus.disconnect(kService, m_requestPath, QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"),
                       this, SLOT(onResponse(uint,QVariantMap)));
    m_step = QString::fromLatin1(step);
    m_requestPath = requestPath(token);
    bus.connect(kService, m_requestPath, QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"),
                this, SLOT(onResponse(uint,QVariantMap)));
}

void ShortcutPortal::onResponse(uint response, const QVariantMap &results)
{
    if (response != 0) {
        // 1 = the user dismissed the dialog, 2 = something else ended it
        qCInfo(lcPortal, "%s: response %u", qPrintable(m_step), response);
        m_bound = false;
        emit changed();
        return;
    }
    if (m_step == QLatin1String("session")) {
        const QVariant handle = results.value(QStringLiteral("session_handle"));
        m_session = handle.canConvert<QDBusObjectPath>() ? handle.value<QDBusObjectPath>().path() : handle.toString();
        bind();
    } else if (m_step == QLatin1String("bind")) {
        readShortcuts(results.value(QStringLiteral("shortcuts")));
        m_bound = true;
        qCInfo(lcPortal, "bound, trigger \"%s\"", qPrintable(m_trigger));
        emit changed();
    }
}

void ShortcutPortal::bind()
{
    const QString token = QStringLiteral("nokturn%1").arg(++m_counter);
    watchRequest(token, "bind");
    const PortalShortcuts shortcuts { { m_id, { { "description", m_description }, { "preferred_trigger", m_preferred } } } };
    QDBusMessage call = QDBusMessage::createMethodCall(kService, kPath, kInterface, QStringLiteral("BindShortcuts"));
    call << QVariant::fromValue(QDBusObjectPath(m_session)) << QVariant::fromValue(shortcuts) << QString()
         << QVariantMap { { "handle_token", token } };
    this->call(call, "BindShortcuts");
}

void ShortcutPortal::readShortcuts(const QVariant &value)
{
    PortalShortcuts list;
    if (value.metaType() == QMetaType::fromType<QDBusArgument>())
        value.value<QDBusArgument>() >> list;
    else
        list = value.value<PortalShortcuts>();
    for (const PortalShortcut &s : std::as_const(list))
        if (s.id == m_id)
            m_trigger = s.options.value(QStringLiteral("trigger_description")).toString();
}

void ShortcutPortal::configure()
{
    if (m_session.isEmpty() || m_version < 2)
        return;
    QDBusMessage call = QDBusMessage::createMethodCall(kService, kPath, kInterface, QStringLiteral("ConfigureShortcuts"));
    call << QVariant::fromValue(QDBusObjectPath(m_session)) << QString() << QVariantMap();
    this->call(call, "ConfigureShortcuts");
}

void ShortcutPortal::onActivated(const QDBusMessage &message)
{
    // (o session_handle, s shortcut_id, t timestamp, a{sv} options)
    const QVariantList args = message.arguments();
    if (args.size() < 2 || args.at(1).toString() != m_id)
        return;
    const QVariantMap options = args.size() >= 4 ? toMap(args.at(3)) : QVariantMap();
    emit activated(options.value(QStringLiteral("activation_token")).toString());
}

void ShortcutPortal::onShortcutsChanged(const QDBusMessage &message)
{
    // (o session_handle, a(sa{sv}) shortcuts) — changed in the desktop's settings
    const QVariantList args = message.arguments();
    if (args.size() < 2)
        return;
    readShortcuts(args.at(1));
    emit changed();
}
