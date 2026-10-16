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
#else
#  include <QMessageAuthenticationCode>
#  include <QProcess>
#  include <QRandomGenerator>
#endif

namespace {

#ifndef Q_OS_WIN
// Linux/macOS: a random key kept in the system password store (Secret Service:
// GNOME Keyring, KWallet) via secret-tool, and the file encrypted with a SHA-256 stream (counter
// mode) with HMAC-SHA256. Without a password store the file stays in plain text, but only with 0600 permissions.
const QByteArray kSealedMagic = "NKS1";
const QByteArray kPlainMagic = "NKP1";

QByteArray keyringKey(bool create)
{
    const QStringList attributes = { QStringLiteral("application"), QStringLiteral("nokturn"),
                                     QStringLiteral("type"), QStringLiteral("youtube-cookie-key") };
    QProcess lookup;
    lookup.start(QStringLiteral("secret-tool"), QStringList { QStringLiteral("lookup") } + attributes);
    if (!lookup.waitForFinished(8000))
        return {};
    QByteArray key = QByteArray::fromBase64(lookup.readAllStandardOutput().trimmed());
    if (key.size() == 32)
        return key;
    if (!create)
        return {};

    key.resize(32);
    QRandomGenerator::system()->generate(reinterpret_cast<quint32 *>(key.data()),
                                         reinterpret_cast<quint32 *>(key.data() + key.size()));
    QProcess store;
    store.start(QStringLiteral("secret-tool"),
                QStringList { QStringLiteral("store"), QStringLiteral("--label=Nokturn (YouTube Music session)") } + attributes);
    if (!store.waitForStarted(3000))
        return {};
    store.write(key.toBase64());
    store.closeWriteChannel();
    if (!store.waitForFinished(15000) || store.exitCode() != 0)
        return {};
    return key;
}

QByteArray keystream(const QByteArray &key, const QByteArray &nonce, qsizetype length)
{
    QByteArray out;
    out.reserve(length + 32);
    for (quint32 counter = 0; out.size() < length; ++counter) {
        QCryptographicHash h(QCryptographicHash::Sha256);
        h.addData(key);
        h.addData(nonce);
        h.addData(QByteArrayView(reinterpret_cast<const char *>(&counter), sizeof counter));
        out += h.result();
    }
    out.truncate(length);
    return out;
}

QByteArray xorWith(QByteArray data, const QByteArray &stream)
{
    for (qsizetype i = 0; i < data.size(); ++i)
        data[i] = char(data.at(i) ^ stream.at(i));
    return data;
}
#endif

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
    const QByteArray key = keyringKey(true);
    if (key.isEmpty())
        return kPlainMagic + plain;
    QByteArray nonce(16, Qt::Uninitialized);
    QRandomGenerator::system()->generate(reinterpret_cast<quint32 *>(nonce.data()),
                                         reinterpret_cast<quint32 *>(nonce.data() + nonce.size()));
    const QByteArray cipher = xorWith(plain, keystream(key, nonce, plain.size()));
    const QByteArray mac = QMessageAuthenticationCode::hash(nonce + cipher, key, QCryptographicHash::Sha256);
    return kSealedMagic + nonce + cipher + mac;
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
    if (data.startsWith(kPlainMagic))
        return data.mid(kPlainMagic.size());
    if (!data.startsWith(kSealedMagic))
        return data;   // older, plain-text format
    if (data.size() < kSealedMagic.size() + 16 + 32)
        return {};
    const QByteArray key = keyringKey(false);
    if (key.isEmpty())
        return {};
    const QByteArray body = data.mid(kSealedMagic.size());
    const QByteArray nonce = body.left(16);
    const QByteArray cipher = body.mid(16, body.size() - 16 - 32);
    const QByteArray mac = body.right(32);
    if (QMessageAuthenticationCode::hash(nonce + cipher, key, QCryptographicHash::Sha256) != mac)
        return {};
    return xorWith(cipher, keystream(key, nonce, cipher.size()));
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
            continue;   // we only need the YouTube cookies
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
    if (!out.commit())
        return false;
    // This is full access to the account — only the owner can read the file.
    QFile::setPermissions(file, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return true;
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
