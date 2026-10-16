#pragma once

#include <QDir>
#include <QFile>
#include <QString>

#include <string>

// Ścieżka w formacie, jakiego oczekuje TagLib::FileName: wchar_t* na Windows
// (pełny Unicode), bajty w kodowaniu systemu plików na Linuksie i macOS.
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
