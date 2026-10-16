#include "Language.h"

#include "AppPaths.h"

#include <QCoreApplication>
#include <QFile>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QQmlEngine>

bool JsonTranslator::loadJson(const QString &path, const QString &language)
{
    m_map.clear();
    m_language = language;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    for (auto it = root.begin(); it != root.end(); ++it) {
        if (!it.key().startsWith(QLatin1Char('_')))   // "_comment" etc.
            m_map.insert(it.key(), it.value());
    }
    return true;
}

int JsonTranslator::pluralIndex(int n) const
{
    if (m_language == QLatin1String("pl")) {
        if (n == 1)
            return 0;
        const int d = n % 10, dd = n % 100;
        return (d >= 2 && d <= 4 && (dd < 12 || dd > 14)) ? 1 : 2;
    }
    return n == 1 ? 0 : 1;
}

QString JsonTranslator::translate(const char *, const char *sourceText, const char *disambiguation, int n) const
{
    QString key = QString::fromUtf8(sourceText);
    if (disambiguation && *disambiguation)
        key += QLatin1Char('|') + QString::fromUtf8(disambiguation);
    const auto it = m_map.constFind(key);
    if (it == m_map.cend())
        return {};
    if (it->isArray()) {
        const QJsonArray forms = it->toArray();
        if (forms.isEmpty())
            return {};
        const int i = n < 0 ? 0 : std::min<int>(pluralIndex(n), int(forms.size()) - 1);
        return forms.at(i).toString();   // %n is substituted by QCoreApplication::translate
    }
    return it->toString();
}

Language *Language::instance()
{
    static Language *inst = new Language;
    return inst;
}

Language *Language::create(QQmlEngine *engine, QJSEngine *)
{
    Language *l = instance();
    l->m_engine = engine;
    QJSEngine::setObjectOwnership(l, QJSEngine::CppOwnership);
    return l;
}

Language::Language(QObject *parent)
    : QObject(parent)
{
    m_language = AppPaths::settings()->value("ui/language", QStringLiteral("system")).toString();
}

void Language::setLanguage(const QString &language)
{
    if (language == m_language)
        return;
    m_language = language;
    AppPaths::settings()->setValue("ui/language", language);
    apply();
    if (m_engine)
        m_engine->retranslate();
}

void Language::apply()
{
    QString lang = m_language;
    if (lang != QLatin1String("en") && lang != QLatin1String("pl"))
        lang = QLocale::system().language() == QLocale::Polish ? QStringLiteral("pl") : QStringLiteral("en");

    if (m_installed) {
        QCoreApplication::removeTranslator(&m_translator);
        m_installed = false;
    }
    // English is the source language — en.json only carries plural forms ("%n tracks").
    if (m_translator.loadJson(QStringLiteral(":/i18n/%1.json").arg(lang), lang))
        m_installed = QCoreApplication::installTranslator(&m_translator);

    QLocale::setDefault(QLocale(lang == QLatin1String("pl") ? QLocale::Polish : QLocale::English));
    m_effective = lang;
    emit changed();
}
