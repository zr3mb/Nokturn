#pragma once

#include <QHash>
#include <QJsonValue>
#include <QObject>
#include <QPointer>
#include <QTranslator>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

// Translations kept as JSON (i18n/<lang>.json, embedded in resources): English source text
// (qsTr / tr) -> translated text. Plural forms are an array picked by the language's plural
// rules, so "%n tracks" can map to ["%n utwór", "%n utwory", "%n utworów"].   // i18n-ignore
// Compatible with the standard qsTr()/tr() calls, so moving to Qt Linguist later needs no code changes.
class JsonTranslator : public QTranslator
{
public:
    bool loadJson(const QString &path, const QString &language);
    bool isEmpty() const override { return m_map.isEmpty(); }
    QString translate(const char *context, const char *sourceText, const char *disambiguation,
                      int n) const override;

private:
    int pluralIndex(int n) const;

    QHash<QString, QJsonValue> m_map;
    QString m_language;
};

// Interface language: "system" (Polish when the system is Polish, otherwise English), "en" or "pl".
// Changing it swaps the translator and retranslates every qsTr() binding live.
class Language : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY changed)
    Q_PROPERTY(QString effective READ effective NOTIFY changed)

public:
    static Language *instance();
    static Language *create(QQmlEngine *engine, QJSEngine *);

    QString language() const { return m_language; }
    void setLanguage(const QString &language);
    QString effective() const { return m_effective; }

    // Installs the translator — called once in main() before QML loads.
    void apply();

signals:
    void changed();

private:
    explicit Language(QObject *parent = nullptr);

    QString m_language;
    QString m_effective = QStringLiteral("en");
    JsonTranslator m_translator;
    bool m_installed = false;
    QPointer<QQmlEngine> m_engine;
};
