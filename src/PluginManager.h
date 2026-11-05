#pragma once

#include <QHash>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QUrl>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

class QNetworkReply;
#include <QQuickItem>
class QQmlEngine;
class QJSEngine;

// What a plugin's QML gets as its `plugin` property: who it is, where its files are, and its own
// settings (kept in <data>/plugin-data/<id>.json, so they survive updates of the plugin).
class PluginContext : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QUrl dir READ dir CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)

public:
    PluginContext(const QString &id, const QString &path, const QString &version, QObject *parent);

    QString id() const { return m_id; }
    QUrl dir() const;
    QString version() const { return m_version; }

    // The plugin's own settings. A plugin binds to them with `plugin.setting("key", fallback)`
    // and changes them with `plugin.setSetting("key", value)`; bindings follow through `revision`.
    Q_PROPERTY(int revision READ revision NOTIFY settingChanged)
    int revision() const { return m_revision; }
    Q_INVOKABLE QVariant setting(const QString &key, const QVariant &fallback = {}) const;
    Q_INVOKABLE void setSetting(const QString &key, const QVariant &value);
    // A file of the plugin as a URL (images, shaders, other QML files).
    Q_INVOKABLE QUrl file(const QString &relative) const;

signals:
    void settingChanged(const QString &key);

private:
    QString dataFile() const;

    QString m_id;
    QString m_path;
    QString m_version;
    QJsonObject m_values;
    int m_revision = 0;
};

// User plugins: folders with a plugin.json and QML files, kept in <data>/plugins/<id>. A plugin can
// draw a wallpaper behind the interface, an overlay above it, run invisible logic (e.g. bind Theme
// properties) and bring its own colour themes. They are installed from a .zip or a folder, or from the
// catalog on the Everplay server (SHA-256 checked). Plugins run with the app's rights — the
// interface warns about that before an install. See docs/plugins.md.
class PluginManager : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Plugins)
    QML_SINGLETON

    Q_PROPERTY(QVariantList installed READ installed NOTIFY changed)
    Q_PROPERTY(QVariantMap themes READ themes NOTIFY changed)
    // Load errors of the plugins' QML by id (apart from `installed`, so a report doesn't rebuild them)
    Q_PROPERTY(QVariantMap errors READ errors NOTIFY errorsChanged)
    Q_PROPERTY(QVariantList catalog READ catalog NOTIFY catalogChanged)
    Q_PROPERTY(QString catalogState READ catalogState NOTIFY catalogChanged)   // idle | loading | ready | error
    Q_PROPERTY(QString busyId READ busyId NOTIFY busyChanged)                  // the plugin being installed
    Q_PROPERTY(int revision READ revision NOTIFY changed)                      // changes when files were replaced
    Q_PROPERTY(QUrl folder READ folder CONSTANT)
    Q_PROPERTY(QUrl docsUrl READ docsUrl NOTIFY changed)
    Q_PROPERTY(int apiVersion READ apiVersion CONSTANT)

public:
    static PluginManager *instance();
    static PluginManager *create(QQmlEngine *engine, QJSEngine *);

    static constexpr int kApiVersion = 1;

    QVariantList installed() const { return m_installed; }
    QVariantMap themes() const { return m_themes; }
    QVariantMap errors() const;
    QVariantList catalog() const { return m_catalog; }
    QString catalogState() const { return m_catalogState; }
    QString busyId() const { return m_busyId; }
    int revision() const { return m_revision; }
    QUrl folder() const;
    QUrl docsUrl() const;
    int apiVersion() const { return kApiVersion; }

    Q_INVOKABLE void setEnabled(const QString &id, bool enabled);
    Q_INVOKABLE void remove(const QString &id);
    // A .zip file, a plugin.json or the folder with it.
    Q_INVOKABLE void installFile(const QUrl &file);
    Q_INVOKABLE void installFromCatalog(const QString &id);
    Q_INVOKABLE void refreshCatalog();
    // Reads the plugins folder again (a plugin being written by hand) and reloads their QML.
    Q_INVOKABLE void reload();
    Q_INVOKABLE QObject *context(const QString &id);
    // Builds a plugin's component under `parent`, with `plugin` set before its bindings run (when
    // the root declares that property). Errors go to the plugin's card.
    Q_INVOKABLE QObject *instantiate(QObject *component, QQuickItem *parent, const QString &id);
    // A plugin's QML failed to load: shown on its card until the next reload.
    Q_INVOKABLE void reportError(const QString &id, const QString &error);

    static int compareVersions(const QString &a, const QString &b);

signals:
    // Right before the plugins are built again: what they changed in the interface is undone here
    // (Theme takes its own bindings back), then `changed` builds the ones still on.
    void aboutToChange();
    void changed();
    void catalogChanged();
    void errorsChanged();
    void busyChanged();
    void notice(const QString &text, const QString &icon);

private:
    explicit PluginManager(QObject *parent = nullptr);

    QString pluginsDir() const;
    void scan();
    bool installFolder(const QString &source, QString *error);
    bool installZip(const QString &zipPath, QString *error);
    void finishDownload(QNetworkReply *reply, const QVariantMap &entry);
    void rebuildCatalog();
    void setBusy(const QString &id);
    void filesReplaced();
    QString text(const QJsonValue &value) const;   // a string, or {"en": …, "pl": …}

    QPointer<QQmlEngine> m_engine;
    QNetworkAccessManager m_net;
    QVariantList m_installed;
    QVariantMap m_themes;
    QVariantList m_catalogRaw;
    QVariantList m_catalog;
    QUrl m_catalogUrl;
    QString m_catalogState = QStringLiteral("idle");
    QString m_busyId;
    QHash<QString, QString> m_errors;
    QHash<QString, QPointer<PluginContext>> m_contexts;
    int m_revision = 0;
};
