#include "Prefs.h"

#include "AppPaths.h"

#include <QJSEngine>

Prefs *Prefs::instance()
{
    static Prefs *inst = new Prefs;
    return inst;
}

Prefs *Prefs::create(QQmlEngine *, QJSEngine *)
{
    Prefs *p = instance();
    QJSEngine::setObjectOwnership(p, QJSEngine::CppOwnership);
    return p;
}

bool Prefs::boolValue(const QString &key, bool fallback) const
{
    return AppPaths::settings()->value(key, fallback).toBool();
}

double Prefs::realValue(const QString &key, double fallback) const
{
    bool ok = false;
    const double v = AppPaths::settings()->value(key, fallback).toDouble(&ok);
    return ok ? v : fallback;
}

QString Prefs::stringValue(const QString &key, const QString &fallback) const
{
    return AppPaths::settings()->value(key, fallback).toString();
}

bool Prefs::contains(const QString &key) const
{
    return AppPaths::settings()->contains(key);
}

void Prefs::setValue(const QString &key, const QVariant &value)
{
    AppPaths::settings()->setValue(key, value);
}
