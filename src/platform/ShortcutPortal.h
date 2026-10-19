#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

class QDBusConnection;
class QDBusMessage;

// Linux (Wayland): a global shortcut through the XDG desktop portal (org.freedesktop.portal.GlobalShortcuts)
// — KDE Plasma, GNOME 48+, Hyprland. Apps can't grab keys on Wayland; the desktop asks the user once to
// accept (or assign) the shortcut, then reports when it's pressed. On Hyprland the key is bound in
// hyprland.conf ("global, nokturn:overlay").
class ShortcutPortal : public QObject
{
    Q_OBJECT

public:
    explicit ShortcutPortal(QObject *parent = nullptr);

    // Opens a session and binds the shortcut, asking for the given trigger ("CTRL+SHIFT+m").
    void start(const QString &id, const QString &description, const QString &preferredTrigger);
    bool available() const { return m_available; }       // the desktop offers the portal
    bool bound() const { return m_bound; }
    QString trigger() const { return m_trigger; }        // as the desktop describes it ("Ctrl+Shift+M"), may be empty
    bool canConfigure() const { return m_version >= 2; }
    void configure();                                    // the desktop's dialog for changing it (portal v2)

signals:
    void activated(const QString &activationToken);
    void changed();

private slots:
    void onResponse(uint response, const QVariantMap &results);
    void onActivated(const QDBusMessage &message);
    void onShortcutsChanged(const QDBusMessage &message);

private:
    QDBusConnection bus() const;
    void call(const QDBusMessage &message, const char *what);
    QString requestPath(const QString &token) const;
    void watchRequest(const QString &token, const char *step);
    void bind();
    void readShortcuts(const QVariant &value);

    bool m_available = false;
    bool m_bound = false;
    uint m_version = 0;
    QString m_id, m_description, m_preferred;
    QString m_session;
    QString m_trigger;
    QString m_step;          // the request being waited for: "session" or "bind"
    QString m_requestPath;
    int m_counter = 0;
};
