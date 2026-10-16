#include "YtCookies.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <dpapi.h>
#endif

namespace {

QByteArray protect(const QByteArray &plain)
{
#ifdef Q_OS_WIN
    DATA_BLOB in { DWORD(plain.size()), reinterpret_cast<BYTE *>(const_cast<char *>(plain.constData())) };
    DATA_BLOB out {};
    if (!CryptProtectData(&in, L"Nokturn", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
        return {};
    QByteArray result(reinterpret_cast<const char *>(out.pbData), int(out.cbData));
    LocalFree(out.pbData);
    return result;
#else
    return plain;
#endif
}

QByteArray unprotect(const QByteArray &data)
{
#ifdef Q_OS_WIN
    DATA_BLOB in { DWORD(data.size()), reinterpret_cast<BYTE *>(const_cast<char *>(data.constData())) };
    DATA_BLOB out {};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
        return {};
    QByteArray result(reinterpret_cast<const char *>(out.pbData), int(out.cbData));
    LocalFree(out.pbData);
    return result;
#else
    return data;
#endif
}

bool domainMatches(const QString &cookieDomain, const QString &host)
{
    if (cookieDomain.startsWith('.'))
        return host.endsWith(cookieDomain) || host == cookieDomain.mid(1);
    return host == cookieDomain;
}

} // namespace

QString YtCookies::value(const QString &name) const
{
    for (const YtCookie &c : cookies)
        if (c.name == name && c.domain.endsWith(QLatin1String("youtube.com")))
            return c.value;
    return {};
}

bool YtCookies::isLoggedIn() const
{
    const bool hasSapisid = !value("SAPISID").isEmpty() || !value("__Secure-3PAPISID").isEmpty();
    return hasSapisid && !value("LOGIN_INFO").isEmpty();
}

QByteArray YtCookies::headerFor(const QUrl &url) const
{
    const QString host = url.host();
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    QStringList parts;
    for (const YtCookie &c : cookies) {
        if (c.expires > 0 && c.expires < now)
            continue;
        if (domainMatches(c.domain, host))
            parts << c.name + '=' + c.value;
    }
    return parts.join(QStringLiteral("; ")).toUtf8();
}

QByteArray YtCookies::authorization(const QString &origin) const
{
    QString sapisid = value("SAPISID");
    if (sapisid.isEmpty())
        sapisid = value("__Secure-3PAPISID");
    if (sapisid.isEmpty())
        return {};
    const QString ts = QString::number(QDateTime::currentSecsSinceEpoch());
    const QByteArray hash = QCryptographicHash::hash((ts + ' ' + sapisid + ' ' + origin).toUtf8(),
                                                     QCryptographicHash::Sha1).toHex();
    return "SAPISIDHASH " + ts.toLatin1() + '_' + hash;
}

QString YtCookies::toNetscape() const
{
    QString out = QStringLiteral("# Netscape HTTP Cookie File\n");
    for (const YtCookie &c : cookies) {
        out += QStringLiteral("%1\t%2\t%3\t%4\t%5\t%6\t%7\n")
                   .arg(c.domain,
                        c.domain.startsWith('.') ? QStringLiteral("TRUE") : QStringLiteral("FALSE"),
                        c.path,
                        c.secure ? QStringLiteral("TRUE") : QStringLiteral("FALSE"),
                        QString::number(std::max<qint64>(0, c.expires)),
                        c.name, c.value);
    }
    return out;
}

YtCookies YtCookies::fromCdp(const QJsonArray &cdpCookies)
{
    YtCookies result;
    for (const QJsonValue &v : cdpCookies) {
        const QJsonObject o = v.toObject();
        YtCookie c;
        c.domain = o.value("domain").toString();
        if (!c.domain.endsWith(QLatin1String("youtube.com")))
            continue;   // potrzebujemy tylko ciasteczek YouTube
        c.name = o.value("name").toString();
        c.value = o.value("value").toString();
        c.path = o.value("path").toString(QStringLiteral("/"));
        const double expires = o.value("expires").toDouble(-1);
        c.expires = expires > 0 ? qint64(expires) : 0;
        c.secure = o.value("secure").toBool();
        c.httpOnly = o.value("httpOnly").toBool();
        result.cookies << c;
    }
    return result;
}

YtCookies YtCookies::fromHeader(const QString &header)
{
    YtCookies result;
    QString text = header.trimmed();
    if (text.startsWith(QLatin1String("cookie:"), Qt::CaseInsensitive))
        text = text.mid(7).trimmed();
    for (const QString &part : text.split(';', Qt::SkipEmptyParts)) {
        const int eq = part.indexOf('=');
        if (eq <= 0)
            continue;
        YtCookie c;
        c.name = part.left(eq).trimmed();
        c.value = part.mid(eq + 1).trimmed();
        c.domain = QStringLiteral(".youtube.com");
        c.secure = true;
        result.cookies << c;
    }
    return result;
}

bool YtCookies::save(const QString &file) const
{
    QJsonArray arr;
    for (const YtCookie &c : cookies) {
        arr.append(QJsonObject {
            { "name", c.name }, { "value", c.value }, { "domain", c.domain }, { "path", c.path },
            { "expires", double(c.expires) }, { "secure", c.secure }, { "httpOnly", c.httpOnly } });
    }
    const QByteArray sealed = protect(QJsonDocument(arr).toJson(QJsonDocument::Compact));
    if (sealed.isEmpty())
        return false;
    QSaveFile out(file);
    if (!out.open(QIODevice::WriteOnly))
        return false;
    out.write(sealed);
    return out.commit();
}

YtCookies YtCookies::load(const QString &file)
{
    YtCookies result;
    QFile in(file);
    if (!in.open(QIODevice::ReadOnly))
        return result;
    const QJsonArray arr = QJsonDocument::fromJson(unprotect(in.readAll())).array();
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        YtCookie c;
        c.name = o.value("name").toString();
        c.value = o.value("value").toString();
        c.domain = o.value("domain").toString();
        c.path = o.value("path").toString(QStringLiteral("/"));
        c.expires = qint64(o.value("expires").toDouble());
        c.secure = o.value("secure").toBool();
        c.httpOnly = o.value("httpOnly").toBool();
        result.cookies << c;
    }
    return result;
}
