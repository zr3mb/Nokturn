#pragma once

#include <QObject>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

// Simple settings for QML (appearance, panel state, first-run wizard) —
// stored in the same settings.ini as the rest of the app (AppPaths::settings()).
// Typed reads, because QSettings in INI format returns booleans as text.
class Prefs : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    static Prefs *instance();
    static Prefs *create(QQmlEngine *, QJSEngine *);

    Q_INVOKABLE bool boolValue(const QString &key, bool fallback) const;
    Q_INVOKABLE double realValue(const QString &key, double fallback) const;
    Q_INVOKABLE QString stringValue(const QString &key, const QString &fallback) const;
    Q_INVOKABLE bool contains(const QString &key) const;
    Q_INVOKABLE void setValue(const QString &key, const QVariant &value);

private:
    explicit Prefs(QObject *parent = nullptr) : QObject(parent) {}
};
