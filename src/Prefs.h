#pragma once

#include <QObject>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

// Proste ustawienia dla QML (wygląd, stan paneli, kreator pierwszego uruchomienia) —
// zapisywane w tym samym settings.ini co reszta aplikacji (AppPaths::settings()).
// Typowane odczyty, bo QSettings w formacie INI zwraca wartości logiczne jako tekst.
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
