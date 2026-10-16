#pragma once

#include <QFile>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

class QNetworkReply;
class QQmlEngine;
class QJSEngine;

// Updates from GitHub Releases: checks the latest release at startup and every few hours,
// and when it's newer — shows a dialog. On Windows it downloads the installer
// (SHA-256 verified), runs it in silent mode and closes the app; the installer
// overwrites the previous version and starts the new one. On other systems it opens the release page.
class Updater : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY changed)
    Q_PROPERTY(QString notes READ notes NOTIFY changed)
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool prompt READ prompt NOTIFY changed)
    Q_PROPERTY(QString state READ state NOTIFY changed)       // idle | checking | downloading | installing
    Q_PROPERTY(qreal progress READ progress NOTIFY changed)
    Q_PROPERTY(bool canInstall READ canInstall CONSTANT)

public:
    static Updater *instance();
    static Updater *create(QQmlEngine *, QJSEngine *);

    QString currentVersion() const;
    QString latestVersion() const { return m_latest; }
    QString notes() const { return m_notes; }
    bool available() const { return !m_latest.isEmpty(); }
    bool prompt() const { return m_prompt; }
    QString state() const { return m_state; }
    qreal progress() const { return m_progress; }
    bool canInstall() const;

    Q_INVOKABLE void check(bool manual = false);
    Q_INVOKABLE void install();
    Q_INVOKABLE void later();
    Q_INVOKABLE void skip();

signals:
    void changed();
    void notice(const QString &text, const QString &icon);

private:
    explicit Updater(QObject *parent = nullptr);

    void onRelease(QNetworkReply *reply, bool manual);
    void startDownload();
    void finishDownload(QNetworkReply *reply);
    void verifyAndRun(const QByteArray &expectedSha);
    void fail(const QString &text);
    void setState(const QString &state);

    QNetworkAccessManager m_net;
    QTimer m_timer;
    QString m_latest;
    QString m_notes;
    QUrl m_page;
    QUrl m_installerUrl;
    QUrl m_shaUrl;          // the .sha256 file next to the installer (when the API gives no hash)
    QByteArray m_digest;    // SHA-256 from the asset's "digest" field in the GitHub API
    QString m_installerPath;
    QFile m_file;
    bool m_prompt = false;
    QString m_state = QStringLiteral("idle");
    qreal m_progress = 0;
};
