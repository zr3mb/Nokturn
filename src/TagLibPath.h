#pragma once

#include <QDir>
#include <QFile>
#include <QString>

#include <string>

// A path in the format TagLib::FileName expects: wchar_t* on Windows
// (full Unicode), bytes in the file system encoding on Linux and macOS.
class TagLibPath
{
public:
#ifdef Q_OS_WIN
    explicit TagLibPath(const QString &path) : m_native(QDir::toNativeSeparators(path).toStdWString()) {}
    const wchar_t *get() const { return m_native.c_str(); }

private:
    std::wstring m_native;
#else
    explicit TagLibPath(const QString &path) : m_native(QFile::encodeName(path)) {}
    const char *get() const { return m_native.constData(); }

private:
    QByteArray m_native;
#endif
};
