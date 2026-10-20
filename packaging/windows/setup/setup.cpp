// Nokturn setup for Windows: a small installer in the app's own look. It downloads the release from
// GitHub (a .tar.zst checked against the SHA-256 built into this file), installs per user without
// admin rights and doubles as the uninstaller.
//
//   Nokturn-<version>-setup.exe              the window: Install / Update
//   ... /SILENT                               in-app update: a progress window, then Nokturn starts
//   ... /VERYSILENT                           no window at all
//   ... /DIR="C:\path"                        install there
//   uninstall.exe --uninstall [/SILENT]       (copied into the install folder)
// For testing: --payload <file.tar.zst> (no download), --url <url>, --test (separate registry key and
// shortcut names, data is never removed), --no-launch, --preview <screen> <out.png> [scale].
// Built by build-installer.ps1, which writes setup_config.h.

#ifndef UNICODE
#define UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <bcrypt.h>
#include <d2d1.h>
#include <dwmapi.h>
#include <dwrite.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <tlhelp32.h>
#include <wincodec.h>
#include <winhttp.h>

#include <zstd.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "setup_config.h"

#ifndef WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY
#define WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY 4
#endif

#define WIDE_(x) L##x
#define WIDE(x) WIDE_(x)

namespace {

const wchar_t *const kVersion = WIDE(SETUP_VERSION);
const char *const kPayloadSha256 = SETUP_PAYLOAD_SHA256;
constexpr uint64_t kPayloadSize = SETUP_PAYLOAD_SIZE;
constexpr uint64_t kInstalledSize = SETUP_INSTALLED_SIZE;
const wchar_t *const kUninstallRoot = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\";
// The Inno Setup installer used up to 0.11: its entry and uninstaller go when we take over its folder.
const wchar_t *const kInnoId = L"{6B0E2F4A-3C1D-4E8B-9A57-0D2C8F1B7E3A}_is1";
const wchar_t *const kManifest = L"install.manifest";   // the installed files, one per line
const wchar_t *const kUninstaller = L"uninstall.exe";

bool g_polish = false;
#define TR(en, pl) (g_polish ? pl : en)

struct Options {
    bool silent = false, verySilent = false, uninstall = false, test = false, noLaunch = false;
    std::wstring dir, payload, url, previewScreen, previewFile;
    float previewScale = 1;
    DWORD waitPid = 0;
} g_opt;

// ---- strings and paths ------------------------------------------------------------

std::wstring format(const wchar_t *fmt, ...)
{
    wchar_t buf[2048];
    va_list args;
    va_start(args, fmt);
    vswprintf(buf, 2048, fmt, args);
    va_end(args);
    return buf;
}

std::wstring widen(const std::string &s)
{
    if (s.empty())
        return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), w.data(), n);
    return w;
}

std::string narrow(const std::wstring &w)
{
    if (w.empty())
        return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}

std::wstring lower(std::wstring s)
{
    if (!s.empty())
        CharLowerBuffW(s.data(), DWORD(s.size()));
    return s;
}

std::wstring trimSlash(std::wstring s)
{
    while (s.size() > 3 && (s.back() == L'\\' || s.back() == L'/'))
        s.pop_back();
    return s;
}

std::wstring join(const std::wstring &a, const std::wstring &b)
{
    if (a.empty())
        return b;
    return a.back() == L'\\' ? a + b : a + L'\\' + b;
}

std::wstring parentOf(const std::wstring &path)
{
    const size_t i = path.find_last_of(L"\\/");
    return i == std::wstring::npos ? std::wstring() : path.substr(0, i);
}

std::wstring fileName(const std::wstring &path)
{
    const size_t i = path.find_last_of(L"\\/");
    return i == std::wstring::npos ? path : path.substr(i + 1);
}

bool fileExists(const std::wstring &path)
{
    const DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

bool dirExists(const std::wstring &path)
{
    const DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

uint64_t fileSize(const std::wstring &path)
{
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data))
        return 0;
    return (uint64_t(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
}

bool makeDirs(const std::wstring &path)
{
    if (path.empty() || dirExists(path))
        return true;
    const std::wstring parent = parentOf(path);
    if (!parent.empty() && parent != path && !makeDirs(parent))
        return false;
    return CreateDirectoryW(path.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
}

void removeTree(const std::wstring &dir)
{
    WIN32_FIND_DATAW fd;
    const HANDLE find = FindFirstFileW(join(dir, L"*").c_str(), &fd);
    if (find != INVALID_HANDLE_VALUE) {
        do {
            const std::wstring name = fd.cFileName;
            if (name == L"." || name == L"..")
                continue;
            const std::wstring full = join(dir, name);
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && !(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
                removeTree(full);
            } else if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                RemoveDirectoryW(full.c_str());
            } else {
                SetFileAttributesW(full.c_str(), FILE_ATTRIBUTE_NORMAL);
                DeleteFileW(full.c_str());
            }
        } while (FindNextFileW(find, &fd));
        FindClose(find);
    }
    RemoveDirectoryW(dir.c_str());
}

// Removes the folders a deleted file leaves empty, up to (not including) root.
void removeEmptyParents(const std::wstring &root, const std::wstring &relative)
{
    std::wstring dir = parentOf(join(root, relative));
    while (dir.size() > root.size() && RemoveDirectoryW(dir.c_str()))
        dir = parentOf(dir);
}

std::string readAll(const std::wstring &path)
{
    const HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE)
        return {};
    std::string data(size_t(fileSize(path)), '\0');
    DWORD read = 0;
    ReadFile(f, data.data(), DWORD(data.size()), &read, nullptr);
    CloseHandle(f);
    data.resize(read);
    return data;
}

bool writeAll(const std::wstring &path, const std::string &data)
{
    const HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE)
        return false;
    DWORD written = 0;
    const bool ok = WriteFile(f, data.data(), DWORD(data.size()), &written, nullptr) && written == data.size();
    CloseHandle(f);
    return ok;
}

std::wstring knownFolder(REFKNOWNFOLDERID id)
{
    PWSTR path = nullptr;
    std::wstring out;
    if (SUCCEEDED(SHGetKnownFolderPath(id, KF_FLAG_CREATE, nullptr, &path)))
        out = path;
    CoTaskMemFree(path);
    return out;
}

std::wstring tempDir()
{
    wchar_t buf[MAX_PATH + 1];
    const DWORD n = GetTempPathW(MAX_PATH + 1, buf);
    return trimSlash(std::wstring(buf, n));
}

std::wstring selfPath()
{
    std::wstring buf(32768, L'\0');
    buf.resize(GetModuleFileNameW(nullptr, buf.data(), DWORD(buf.size())));
    return buf;
}

bool inside(const std::wstring &path, const std::wstring &dir)
{
    const std::wstring p = lower(path), d = lower(trimSlash(dir)) + L'\\';
    return p.compare(0, d.size(), d) == 0;
}

int compareVersions(const std::wstring &a, const std::wstring &b)
{
    int x[3] = {}, y[3] = {};
    swscanf(a.c_str(), L"%d.%d.%d", &x[0], &x[1], &x[2]);
    swscanf(b.c_str(), L"%d.%d.%d", &y[0], &y[1], &y[2]);
    for (int i = 0; i < 3; ++i)
        if (x[i] != y[i])
            return x[i] < y[i] ? -1 : 1;
    return 0;
}

// ---- registry, shortcuts, processes -------------------------------------------------

std::wstring ourKey() { return kUninstallRoot + std::wstring(g_opt.test ? L"Nokturn-test" : L"Nokturn"); }
std::wstring innoKey() { return kUninstallRoot + std::wstring(g_opt.test ? L"Nokturn-inno-test" : kInnoId); }
std::wstring shortcutName() { return g_opt.test ? L"Nokturn (test).lnk" : L"Nokturn.lnk"; }
std::wstring startMenuShortcut() { return join(knownFolder(FOLDERID_Programs), shortcutName()); }
std::wstring desktopShortcut() { return join(knownFolder(FOLDERID_Desktop), shortcutName()); }
std::wstring defaultDir() { return join(knownFolder(FOLDERID_UserProgramFiles), L"Nokturn"); }

std::wstring regString(HKEY root, const std::wstring &key, const wchar_t *name)
{
    DWORD size = 0;
    if (RegGetValueW(root, key.c_str(), name, RRF_RT_REG_SZ, nullptr, nullptr, &size) != ERROR_SUCCESS)
        return {};
    std::wstring s(size / sizeof(wchar_t) + 1, L'\0');
    size = DWORD(s.size() * sizeof(wchar_t));
    if (RegGetValueW(root, key.c_str(), name, RRF_RT_REG_SZ, nullptr, s.data(), &size) != ERROR_SUCCESS)
        return {};
    s.resize(wcslen(s.c_str()));
    return s;
}

void regSet(HKEY key, const wchar_t *name, const std::wstring &value)
{
    RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE *>(value.c_str()),
                   DWORD((value.size() + 1) * sizeof(wchar_t)));
}

void regSet(HKEY key, const wchar_t *name, DWORD value)
{
    RegSetValueExW(key, name, 0, REG_DWORD, reinterpret_cast<const BYTE *>(&value), sizeof value);
}

// Where Nokturn is installed already: by this setup, or by the Inno Setup one before it.
struct Existing {
    std::wstring dir, version;
};

Existing findExisting()
{
    for (const std::wstring &key : { ourKey(), innoKey() }) {
        const std::wstring dir = trimSlash(regString(HKEY_CURRENT_USER, key, L"InstallLocation"));
        if (!dir.empty() && fileExists(join(dir, L"Nokturn.exe")))
            return { dir, regString(HKEY_CURRENT_USER, key, L"DisplayVersion") };
    }
    return {};
}

bool makeShortcut(const std::wstring &lnk, const std::wstring &target, const std::wstring &workDir, const std::wstring &icon)
{
    IShellLinkW *link = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link))))
        return false;
    link->SetPath(target.c_str());
    link->SetWorkingDirectory(workDir.c_str());
    link->SetIconLocation(icon.c_str(), 0);
    link->SetDescription(L"Nokturn");
    IPersistFile *file = nullptr;
    const bool ok = SUCCEEDED(link->QueryInterface(IID_PPV_ARGS(&file))) && SUCCEEDED(file->Save(lnk.c_str(), TRUE));
    if (file)
        file->Release();
    link->Release();
    return ok;
}

std::vector<DWORD> processesIn(const std::wstring &dir)
{
    std::vector<DWORD> out;
    const HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return out;
    PROCESSENTRY32W entry { sizeof entry };
    std::wstring image(32768, L'\0');
    if (Process32FirstW(snap, &entry)) {
        do {
            if (entry.th32ProcessID == GetCurrentProcessId())
                continue;
            const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
            if (!process)
                continue;
            DWORD size = DWORD(image.size());
            if (QueryFullProcessImageNameW(process, 0, image.data(), &size) && inside(image.substr(0, size), dir))
                out.push_back(entry.th32ProcessID);
            CloseHandle(process);
        } while (Process32NextW(snap, &entry));
    }
    CloseHandle(snap);
    return out;
}

// Nokturn (and the tools it started from its folder) has to let go of the files: asked to close
// first, ended after a few seconds.
void closeAppsIn(const std::wstring &dir)
{
    std::vector<DWORD> pids = processesIn(dir);
    if (pids.empty())
        return;
    EnumWindows([](HWND window, LPARAM param) -> BOOL {
        const auto &pids = *reinterpret_cast<const std::vector<DWORD> *>(param);
        DWORD pid = 0;
        GetWindowThreadProcessId(window, &pid);
        if (std::find(pids.begin(), pids.end(), pid) != pids.end())
            PostMessageW(window, WM_CLOSE, 0, 0);
        return TRUE;
    }, LPARAM(&pids));
    for (int i = 0; i < 50 && !processesIn(dir).empty(); ++i)
        Sleep(100);
    for (const DWORD pid : processesIn(dir)) {
        if (const HANDLE process = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid)) {
            TerminateProcess(process, 0);
            WaitForSingleObject(process, 3000);
            CloseHandle(process);
        }
    }
}

void launchApp(const std::wstring &dir)
{
    if (g_opt.noLaunch)
        return;
    const std::wstring exe = join(dir, L"Nokturn.exe");
    std::wstring command = L"\"" + exe + L"\"";
    STARTUPINFOW si { sizeof si };
    PROCESS_INFORMATION pi {};
    if (CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr, dir.c_str(), &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}

// ---- downloading and unpacking --------------------------------------------------------

enum Phase { Idle, Connecting, Downloading, Verifying, Closing, Installing, Removing, Finishing, Finished };

struct Progress {
    std::atomic<int> phase { Idle };
    std::atomic<uint64_t> done { 0 }, total { 0 }, started { 0 };
    std::atomic<bool> cancel { false };

    void reset()
    {
        phase = Idle;
        done = total = started = 0;
        cancel = false;
    }
};

class Sha256
{
public:
    Sha256()
    {
        BCryptOpenAlgorithmProvider(&m_alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
        BCryptCreateHash(m_alg, &m_hash, nullptr, 0, nullptr, 0, 0);
    }
    ~Sha256()
    {
        if (m_hash)
            BCryptDestroyHash(m_hash);
        if (m_alg)
            BCryptCloseAlgorithmProvider(m_alg, 0);
    }
    void add(const void *data, size_t size) { BCryptHashData(m_hash, PUCHAR(data), ULONG(size), 0); }
    std::string hex()
    {
        UCHAR digest[32] = {};
        BCryptFinishHash(m_hash, digest, sizeof digest, 0);
        char out[65] = {};
        for (int i = 0; i < 32; ++i)
            snprintf(out + 2 * i, 3, "%02x", digest[i]);
        return out;
    }

private:
    BCRYPT_ALG_HANDLE m_alg = nullptr;
    BCRYPT_HASH_HANDLE m_hash = nullptr;
};

std::string sha256File(const std::wstring &path)
{
    const HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (f == INVALID_HANDLE_VALUE)
        return {};
    Sha256 hash;
    std::vector<char> buf(1 << 20);
    DWORD read = 0;
    while (ReadFile(f, buf.data(), DWORD(buf.size()), &read, nullptr) && read > 0)
        hash.add(buf.data(), read);
    CloseHandle(f);
    return hash.hex();
}

std::wstring netError(DWORD code)
{
    switch (code) {
    case ERROR_WINHTTP_CANNOT_CONNECT:
    case ERROR_WINHTTP_NAME_NOT_RESOLVED:
    case ERROR_WINHTTP_TIMEOUT:
    case ERROR_WINHTTP_CONNECTION_ERROR:
        return TR(L"Couldn't connect to GitHub. Check your internet connection and try again.",
                  L"Nie udało się połączyć z GitHubem. Sprawdź połączenie z internetem i spróbuj ponownie.");
    case ERROR_WINHTTP_SECURE_FAILURE:
        return TR(L"Couldn't open a secure connection to GitHub.", L"Nie udało się nawiązać bezpiecznego połączenia z GitHubem.");
    default:
        return format(TR(L"The download failed (error %lu).", L"Pobieranie nie powiodło się (błąd %lu)."), code);
    }
}

struct NetHandle {
    HINTERNET h = nullptr;
    explicit NetHandle(HINTERNET handle = nullptr) : h(handle) {}
    ~NetHandle() { reset(nullptr); }
    NetHandle(const NetHandle &) = delete;
    NetHandle &operator=(const NetHandle &) = delete;
    void reset(HINTERNET handle)
    {
        if (h)
            WinHttpCloseHandle(h);
        h = handle;
    }
    operator HINTERNET() const { return h; }
};

std::wstring payloadUrl()
{
    if (!g_opt.url.empty())
        return g_opt.url;
    return format(L"https://github.com/zr3mb/Nokturn/releases/download/v%ls/Nokturn-%ls-win64.tar.zst", kVersion, kVersion);
}

// Saves url to target, hashing on the way.
bool download(const std::wstring &url, const std::wstring &target, Progress &p, std::string &sha, std::wstring &error)
{
    URL_COMPONENTSW parts {};
    parts.dwStructSize = sizeof parts;
    std::wstring host(256, L'\0'), path(4096, L'\0'), extra(4096, L'\0');
    parts.lpszHostName = host.data();
    parts.dwHostNameLength = DWORD(host.size());
    parts.lpszUrlPath = path.data();
    parts.dwUrlPathLength = DWORD(path.size());
    parts.lpszExtraInfo = extra.data();
    parts.dwExtraInfoLength = DWORD(extra.size());
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts)) {
        error = netError(GetLastError());
        return false;
    }
    const std::wstring agent = std::wstring(L"Nokturn-Setup/") + kVersion;
    NetHandle session(WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session)   // before Windows 8.1
        session.reset(WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session) {
        error = netError(GetLastError());
        return false;
    }
    WinHttpSetTimeouts(session, 0, 20000, 20000, 30000);
    NetHandle connection(WinHttpConnect(session, host.c_str(), parts.nPort, 0));
    const std::wstring object = path.c_str() + std::wstring(extra.c_str());
    NetHandle request(connection ? WinHttpOpenRequest(connection, L"GET", object.c_str(), nullptr, WINHTTP_NO_REFERER,
                                                      WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                      parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0)
                                 : nullptr);
    if (!request || !WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
        || !WinHttpReceiveResponse(request, nullptr)) {
        error = netError(GetLastError());
        return false;
    }
    DWORD status = 0, size = sizeof status;
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                        &status, &size, WINHTTP_NO_HEADER_INDEX);
    if (status != 200) {
        error = format(TR(L"GitHub answered with error %lu. Try again in a moment.", L"GitHub odpowiedział błędem %lu. Spróbuj za chwilę."), status);
        return false;
    }
    uint64_t total = kPayloadSize;
    wchar_t length[32] = {};
    size = sizeof length;
    if (WinHttpQueryHeaders(request, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, length, &size, WINHTTP_NO_HEADER_INDEX))
        total = _wcstoui64(length, nullptr, 10);

    const HANDLE file = CreateFileW(target.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        error = TR(L"Couldn't save the download in the temporary folder.", L"Nie udało się zapisać pobieranego pliku w folderze tymczasowym.");
        return false;
    }
    p.total = total;
    p.done = 0;
    p.started = GetTickCount64();
    p.phase = Downloading;
    Sha256 hash;
    std::vector<char> buf(256 * 1024);
    bool ok = true;
    for (;;) {
        if (p.cancel) {
            ok = false;
            break;
        }
        DWORD read = 0;
        if (!WinHttpReadData(request, buf.data(), DWORD(buf.size()), &read)) {
            ok = false;
            error = netError(GetLastError());
            break;
        }
        if (read == 0)
            break;
        DWORD written = 0;
        if (!WriteFile(file, buf.data(), read, &written, nullptr) || written != read) {
            ok = false;
            error = TR(L"Couldn't save the download. Is the disk full?", L"Nie udało się zapisać pobieranego pliku. Czy dysk jest pełny?");
            break;
        }
        hash.add(buf.data(), read);
        p.done += read;
    }
    CloseHandle(file);
    if (ok && p.done != total) {
        ok = false;
        error = TR(L"The download was interrupted. Try again.", L"Pobieranie zostało przerwane. Spróbuj ponownie.");
    }
    if (ok)
        sha = hash.hex();
    return ok;
}

// A streaming reader for the (ustar) tar inside the package: files are written as they arrive.
class Untar
{
public:
    explicit Untar(std::wstring root) : m_root(std::move(root)) {}
    ~Untar() { closeFile(); }

    bool feed(const char *p, size_t n)
    {
        while (n > 0 && !m_end) {
            if (m_data > 0) {
                const size_t take = size_t(std::min<uint64_t>(m_data, n));
                if (m_out != INVALID_HANDLE_VALUE) {
                    DWORD written = 0;
                    if (!WriteFile(m_out, p, DWORD(take), &written, nullptr) || written != take) {
                        m_error = format(TR(L"Couldn't write %ls. Is the disk full?", L"Nie udało się zapisać %ls. Czy dysk jest pełny?"),
                                         m_current.c_str());
                        return false;
                    }
                }
                m_data -= take;
                p += take;
                n -= take;
                if (m_data == 0)
                    closeFile();
            } else if (m_pad > 0) {
                const size_t take = size_t(std::min<uint64_t>(m_pad, n));
                m_pad -= take;
                p += take;
                n -= take;
            } else {
                const size_t take = std::min(sizeof m_header - m_fill, n);
                memcpy(m_header + m_fill, p, take);
                m_fill += take;
                p += take;
                n -= take;
                if (m_fill == sizeof m_header) {
                    m_fill = 0;
                    if (!beginEntry())
                        return false;
                }
            }
        }
        return true;
    }

    bool finished() const { return m_end; }
    const std::vector<std::wstring> &files() const { return m_files; }
    const std::wstring &error() const { return m_error; }

private:
    bool beginEntry()
    {
        if (std::all_of(m_header, m_header + sizeof m_header, [](char c) { return c == 0; })) {
            m_end = true;
            return true;
        }
        auto field = [this](int offset, int length) { return std::string(m_header + offset, strnlen(m_header + offset, length)); };
        std::string name = field(0, 100);
        if (memcmp(m_header + 257, "ustar", 5) == 0) {
            const std::string prefix = field(345, 155);
            if (!prefix.empty())
                name = prefix + "/" + name;
        }
        uint64_t size = 0;
        for (int i = 124; i < 136 && m_header[i]; ++i)
            if (m_header[i] >= '0' && m_header[i] <= '7')
                size = size * 8 + uint64_t(m_header[i] - '0');
        const char type = m_header[156];
        m_data = size;
        m_pad = (512 - size % 512) % 512;

        // Only plain relative paths: nothing may land outside the install folder.
        std::wstring relative;
        size_t start = 0;
        while (start <= name.size()) {
            size_t end = name.find('/', start);
            if (end == std::string::npos)
                end = name.size();
            const std::string part = name.substr(start, end - start);
            start = end + 1;
            if (part.empty() || part == ".")
                continue;
            if (part == ".." || part.find_first_of(":\\") != std::string::npos) {
                m_error = TR(L"The package is damaged. Try again.", L"Paczka jest uszkodzona. Spróbuj ponownie.");
                return false;
            }
            relative = relative.empty() ? widen(part) : relative + L'\\' + widen(part);
        }
        if (relative.empty())
            return true;
        const std::wstring full = join(m_root, relative);
        if (type == '5')
            return makeDirs(full) || fail(full);
        if (type != '0' && type != '\0')
            return true;   // skipped: its data goes nowhere
        if (!makeDirs(parentOf(full)))
            return fail(full);
        for (int attempt = 0;; ++attempt) {
            SetFileAttributesW(full.c_str(), FILE_ATTRIBUTE_NORMAL);   // a read-only leftover
            m_out = CreateFileW(full.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (m_out != INVALID_HANDLE_VALUE)
                break;
            const DWORD e = GetLastError();
            if ((e == ERROR_SHARING_VIOLATION || e == ERROR_LOCK_VIOLATION || e == ERROR_ACCESS_DENIED) && attempt < 20) {
                Sleep(250);
                continue;
            }
            if (e == ERROR_SHARING_VIOLATION || e == ERROR_LOCK_VIOLATION) {
                m_error = format(TR(L"%ls is in use. Close Nokturn and try again.", L"Plik %ls jest używany. Zamknij Nokturn i spróbuj ponownie."),
                                 relative.c_str());
                return false;
            }
            return fail(full);
        }
        m_current = relative;
        m_files.push_back(relative);
        if (size == 0)
            closeFile();
        return true;
    }

    bool fail(const std::wstring &path)
    {
        m_error = format(TR(L"Couldn't write %ls.", L"Nie udało się zapisać %ls."), path.c_str());
        return false;
    }

    void closeFile()
    {
        if (m_out != INVALID_HANDLE_VALUE) {
            CloseHandle(m_out);
            m_out = INVALID_HANDLE_VALUE;
        }
    }

    std::wstring m_root, m_current, m_error;
    std::vector<std::wstring> m_files;
    char m_header[512] = {};
    size_t m_fill = 0;
    uint64_t m_data = 0, m_pad = 0;
    HANDLE m_out = INVALID_HANDLE_VALUE;
    bool m_end = false;
};

bool extract(const std::wstring &archive, const std::wstring &root, Progress &p, std::vector<std::wstring> &files, std::wstring &error)
{
    const std::wstring damaged = TR(L"The package is damaged. Try again.", L"Paczka jest uszkodzona. Spróbuj ponownie.");
    const HANDLE in = CreateFileW(archive.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (in == INVALID_HANDLE_VALUE) {
        error = damaged;
        return false;
    }
    ZSTD_DCtx *z = ZSTD_createDCtx();
    ZSTD_DCtx_setParameter(z, ZSTD_d_windowLogMax, 31);
    std::vector<char> inBuf(ZSTD_DStreamInSize()), outBuf(ZSTD_DStreamOutSize());
    Untar tar(root);
    bool ok = true;
    size_t last = 1;
    for (;;) {
        DWORD read = 0;
        if (!ReadFile(in, inBuf.data(), DWORD(inBuf.size()), &read, nullptr)) {
            ok = false;
            error = damaged;
            break;
        }
        if (read == 0)
            break;
        // zstd keeps the frame's last byte until everything before it is out, so this drains it all.
        ZSTD_inBuffer zin { inBuf.data(), read, 0 };
        while (ok && zin.pos < zin.size) {
            ZSTD_outBuffer zout { outBuf.data(), outBuf.size(), 0 };
            last = ZSTD_decompressStream(z, &zout, &zin);
            if (ZSTD_isError(last)) {
                ok = false;
                error = damaged;
            } else if (!tar.feed(outBuf.data(), zout.pos)) {
                ok = false;
                error = tar.error();
            }
            p.done += zout.pos;
        }
        if (!ok)
            break;
    }
    ZSTD_freeDCtx(z);
    CloseHandle(in);
    if (ok && (last != 0 || !tar.finished())) {
        ok = false;
        error = damaged;
    }
    files = tar.files();
    return ok;
}

std::vector<std::wstring> readManifest(const std::wstring &dir)
{
    std::vector<std::wstring> files;
    const std::wstring text = widen(readAll(join(dir, kManifest)));
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find(L'\n', start);
        if (end == std::wstring::npos)
            end = text.size();
        std::wstring line = text.substr(start, end - start);
        if (!line.empty() && line.back() == L'\r')
            line.pop_back();
        if (!line.empty() && line.find(L"..") == std::wstring::npos && line.find(L':') == std::wstring::npos)
            files.push_back(line);
        start = end + 1;
    }
    return files;
}

// ---- installing and removing ------------------------------------------------------------

void registerApp(const std::wstring &dir)
{
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, ourKey().c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        return;
    const std::wstring uninstall = L"\"" + join(dir, kUninstaller) + L"\" --uninstall" + (g_opt.test ? L" --test" : L"");
    SYSTEMTIME now;
    GetLocalTime(&now);
    regSet(key, L"DisplayName", g_opt.test ? L"Nokturn (test)" : L"Nokturn");
    regSet(key, L"DisplayVersion", kVersion);
    regSet(key, L"DisplayIcon", join(dir, L"nokturn.ico"));
    regSet(key, L"Publisher", L"zr3mb");
    regSet(key, L"URLInfoAbout", L"https://github.com/zr3mb/Nokturn");
    regSet(key, L"HelpLink", L"https://github.com/zr3mb/Nokturn/issues");
    regSet(key, L"InstallLocation", dir);
    regSet(key, L"InstallDate", format(L"%04u%02u%02u", now.wYear, now.wMonth, now.wDay));
    regSet(key, L"UninstallString", uninstall);
    regSet(key, L"QuietUninstallString", uninstall + L" /SILENT");
    regSet(key, L"NoModify", DWORD(1));
    regSet(key, L"NoRepair", DWORD(1));
    regSet(key, L"EstimatedSize", DWORD(kInstalledSize / 1024));
    RegCloseKey(key);
}

// The Inno Setup install this one replaces: its uninstaller and entry go.
void retireInno(const std::wstring &dir)
{
    const std::wstring key = innoKey();
    const std::wstring location = trimSlash(regString(HKEY_CURRENT_USER, key, L"InstallLocation"));
    if (location.empty() || lower(location) != lower(dir))
        return;
    for (const wchar_t *name : { L"unins000.exe", L"unins000.dat", L"unins000.msg" }) {
        const std::wstring path = join(dir, name);
        SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL);
        DeleteFileW(path.c_str());
    }
    RegDeleteTreeW(HKEY_CURRENT_USER, key.c_str());
}

struct Plan {
    std::wstring dir;
    bool desktop = true;
};

bool runInstall(const Plan &plan, Progress &p, std::wstring &error)
{
    const std::wstring dir = trimSlash(plan.dir);
    const std::wstring noAccess = format(TR(L"Can't write to %ls. Choose another folder.", L"Nie można zapisywać w %ls. Wybierz inny folder."),
                                         dir.c_str());
    if (!makeDirs(dir)) {
        error = noAccess;
        return false;
    }
    const HANDLE probe = CreateFileW(join(dir, L".nokturn-setup").c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                     FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (probe == INVALID_HANDLE_VALUE) {
        error = noAccess;
        return false;
    }
    CloseHandle(probe);

    // The package: downloaded to the temporary folder (a complete earlier download is reused).
    std::wstring archive = g_opt.payload;
    if (archive.empty()) {
        archive = join(tempDir(), format(L"Nokturn-%ls-win64.tar.zst", kVersion));
        p.phase = Connecting;
        if (!(fileSize(archive) == kPayloadSize && sha256File(archive) == kPayloadSha256)) {
            const std::wstring part = archive + L".part";
            std::string sha;
            if (!download(payloadUrl(), part, p, sha, error)) {
                DeleteFileW(part.c_str());
                return false;
            }
            p.phase = Verifying;
            if (sha != kPayloadSha256) {
                DeleteFileW(part.c_str());
                error = TR(L"The download is damaged. Try again.", L"Pobrany plik jest uszkodzony. Spróbuj ponownie.");
                return false;
            }
            MoveFileExW(part.c_str(), archive.c_str(), MOVEFILE_REPLACE_EXISTING);
        }
    } else {
        p.phase = Verifying;
        if (sha256File(archive) != kPayloadSha256) {
            error = TR(L"The package doesn't match this setup.", L"Paczka nie pasuje do tego instalatora.");
            return false;
        }
    }
    if (p.cancel)
        return false;

    p.phase = Closing;
    closeAppsIn(dir);

    const std::vector<std::wstring> previous = readManifest(dir);
    p.done = 0;
    p.total = kInstalledSize;
    p.phase = Installing;
    std::vector<std::wstring> files;
    if (!extract(archive, dir, p, files, error))
        return false;

    p.phase = Finishing;
    // What the previous version had and this one doesn't.
    std::set<std::wstring> now;
    for (const std::wstring &f : files)
        now.insert(lower(f));
    for (const std::wstring &f : previous) {
        if (now.count(lower(f)) || lower(f) == lower(kUninstaller))
            continue;
        const std::wstring path = join(dir, f);
        SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL);
        DeleteFileW(path.c_str());
        removeEmptyParents(dir, f);
    }
    std::string manifest;
    for (const std::wstring &f : files)
        manifest += narrow(f) + "\r\n";
    writeAll(join(dir, kManifest), manifest);
    const std::wstring uninstaller = join(dir, kUninstaller);
    if (lower(selfPath()) != lower(uninstaller))
        CopyFileW(selfPath().c_str(), uninstaller.c_str(), FALSE);

    retireInno(dir);
    registerApp(dir);
    const std::wstring exe = join(dir, L"Nokturn.exe"), icon = join(dir, L"nokturn.ico");
    makeShortcut(startMenuShortcut(), exe, dir, icon);
    if (plan.desktop)
        makeShortcut(desktopShortcut(), exe, dir, icon);
    else
        DeleteFileW(desktopShortcut().c_str());
    if (g_opt.payload.empty())
        DeleteFileW(archive.c_str());
    p.phase = Finished;
    return true;
}

bool runUninstall(const std::wstring &dir, bool removeData, Progress &p, std::wstring &error)
{
    p.phase = Closing;
    closeAppsIn(dir);
    p.phase = Removing;
    const std::vector<std::wstring> files = readManifest(dir);
    p.total = files.size() + 1;
    for (const std::wstring &f : files) {
        const std::wstring path = join(dir, f);
        SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL);
        DeleteFileW(path.c_str());
        removeEmptyParents(dir, f);
        ++p.done;
    }
    for (const wchar_t *name : { kUninstaller, kManifest })
        DeleteFileW(join(dir, name).c_str());
    RemoveDirectoryW(dir.c_str());   // only if nothing else is left in it
    DeleteFileW(startMenuShortcut().c_str());
    DeleteFileW(desktopShortcut().c_str());
    RegDeleteTreeW(HKEY_CURRENT_USER, ourKey().c_str());
    if (removeData && !g_opt.test) {
        for (REFKNOWNFOLDERID base : { FOLDERID_RoamingAppData, FOLDERID_LocalAppData }) {
            const std::wstring org = join(knownFolder(base), L"Nokturn");
            removeTree(join(org, L"Nokturn"));
            RemoveDirectoryW(org.c_str());
        }
    }
    if (fileExists(join(dir, L"Nokturn.exe"))) {
        error = TR(L"Some files couldn't be removed. Close Nokturn and try again.",
                   L"Nie udało się usunąć części plików. Zamknij Nokturn i spróbuj ponownie.");
        return false;
    }
    p.done = p.total.load();
    p.phase = Finished;
    return true;
}

// ---- the window ---------------------------------------------------------------------------

namespace color {
constexpr uint32_t bg = 0x101114, surface = 0x1a1c20, text = 0xe8e4dd, textDim = 0xa19e98,
                   textFaint = 0x6e6d6a, accent = 0x9fb5a8, onAccent = 0x15161a, success = 0x9cc0a0, danger = 0xc98585;
}

D2D1_COLOR_F rgb(uint32_t c, float alpha = 1) { return D2D1::ColorF(c, alpha); }

D2D1_COLOR_F mix(D2D1_COLOR_F a, D2D1_COLOR_F b, float t)
{
    return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t };
}

constexpr float kWidth = 480;
enum Screen { Welcome, Working, Done, Failed, Confirm, Removed };
enum Control { None = -1, Minimize, Close, Primary, Secondary, Change, Toggle, ControlCount };

template<typename T> void release(T *&p)
{
    if (p) {
        p->Release();
        p = nullptr;
    }
}

class Ui
{
public:
    Screen screen = Welcome;
    bool uninstalling = false;
    Existing existing;
    std::wstring dir;
    bool desktop = true, removeData = false;
    Progress progress;
    std::wstring error;
    float height = 440;
    float shownProgress = 0;

    bool initGraphics();
    void paint(ID2D1RenderTarget *rt);
    int run(HINSTANCE instance);
    bool renderPng(const std::wstring &file, float scale);

private:
    static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT handle(UINT msg, WPARAM wp, LPARAM lp);
    std::vector<std::pair<Control, D2D1_RECT_F>> controls() const;
    Control hit(float x, float y) const;
    bool enabled(Control c) const;
    void activate(Control c);
    void start();
    void finish(bool ok);
    void chooseFolder();
    bool tick();
    double targetProgress() const;
    bool updating() const { return !existing.dir.empty() && lower(existing.dir) == lower(dir); }
    std::wstring subtitle() const;
    std::wstring primaryLabel() const;
    std::wstring secondaryLabel() const;
    std::wstring phaseLabel() const;
    void drawLogo(ID2D1RenderTarget *rt, float x, float y, float size);
    void resize();

    HWND m_hwnd = nullptr;
    float m_scale = 1;
    ID2D1Factory *m_d2d = nullptr;
    IDWriteFactory *m_dw = nullptr;
    ID2D1HwndRenderTarget *m_rt = nullptr;
    IDWriteTextFormat *m_title = nullptr, *m_sub = nullptr, *m_body = nullptr, *m_label = nullptr, *m_path = nullptr,
                      *m_button = nullptr, *m_buttonSmall = nullptr, *m_status = nullptr, *m_small = nullptr, *m_message = nullptr;
    float m_anim[ControlCount] = {};
    float m_toggle = 1;
    Control m_hot = None, m_pressed = None;
    bool m_tracking = false;
    std::thread m_worker;
    std::mutex m_mutex;
    std::wstring m_workerError;
};

Ui g_ui;

bool Ui::initGraphics()
{
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_d2d))
        || FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown **>(&m_dw))))
        return false;
    // Segoe UI Variable like the app (Windows 11), plain Segoe UI before it.
    auto family = [this](const wchar_t *preferred) {
        IDWriteFontCollection *fonts = nullptr;
        UINT32 index = 0;
        BOOL found = FALSE;
        if (SUCCEEDED(m_dw->GetSystemFontCollection(&fonts))) {
            fonts->FindFamilyName(preferred, &index, &found);
            fonts->Release();
        }
        return std::wstring(found ? preferred : L"Segoe UI");
    };
    const std::wstring display = family(L"Segoe UI Variable Display"), text = family(L"Segoe UI Variable Text");
    auto make = [this](const std::wstring &font, float size, DWRITE_FONT_WEIGHT weight, bool wrap = false) {
        IDWriteTextFormat *f = nullptr;
        m_dw->CreateTextFormat(font.c_str(), nullptr, weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"", &f);
        f->SetParagraphAlignment(wrap ? DWRITE_PARAGRAPH_ALIGNMENT_NEAR : DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        f->SetWordWrapping(wrap ? DWRITE_WORD_WRAPPING_WRAP : DWRITE_WORD_WRAPPING_NO_WRAP);
        if (!wrap) {
            IDWriteInlineObject *ellipsis = nullptr;
            m_dw->CreateEllipsisTrimmingSign(f, &ellipsis);
            const DWRITE_TRIMMING trimming { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
            f->SetTrimming(&trimming, ellipsis);
            if (ellipsis)
                ellipsis->Release();
        }
        return f;
    };
    m_title = make(display, 30, DWRITE_FONT_WEIGHT_BOLD);
    m_sub = make(text, 14, DWRITE_FONT_WEIGHT_NORMAL);
    m_body = make(text, 14, DWRITE_FONT_WEIGHT_NORMAL);
    m_label = make(text, 12, DWRITE_FONT_WEIGHT_NORMAL);
    m_path = make(text, 13.5f, DWRITE_FONT_WEIGHT_NORMAL);
    {
        // Long paths lose their middle, so the "\Nokturn" at the end stays visible.
        IDWriteInlineObject *ellipsis = nullptr;
        m_dw->CreateEllipsisTrimmingSign(m_path, &ellipsis);
        const DWRITE_TRIMMING trimming { DWRITE_TRIMMING_GRANULARITY_CHARACTER, L'\\', 1 };
        m_path->SetTrimming(&trimming, ellipsis);
        if (ellipsis)
            ellipsis->Release();
    }
    m_button = make(text, 14, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    m_buttonSmall = make(text, 13, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    m_status = make(text, 14.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    m_small = make(text, 12.5f, DWRITE_FONT_WEIGHT_NORMAL);
    m_message = make(text, 14, DWRITE_FONT_WEIGHT_NORMAL, true);
    return true;
}

std::wstring Ui::subtitle() const
{
    const double mb = double(kPayloadSize) / (1024 * 1024);
    switch (screen) {
    case Welcome:
        if (!updating())
            return format(TR(L"Version %ls · %.0f MB to download", L"Wersja %ls · do pobrania %.0f MB"), kVersion, mb);
        if (compareVersions(existing.version, kVersion) < 0)
            return format(TR(L"Update %ls → %ls · %.0f MB to download", L"Aktualizacja %ls → %ls · do pobrania %.0f MB"),
                          existing.version.c_str(), kVersion, mb);
        return format(TR(L"Version %ls is installed · this setup has %ls", L"Zainstalowana wersja %ls · ten instalator ma %ls"),
                      existing.version.c_str(), kVersion);
    case Working:
        if (uninstalling)
            return TR(L"Removing Nokturn", L"Usuwanie Nokturna");
        return format(updating() ? TR(L"Updating to version %ls", L"Aktualizacja do wersji %ls")
                                 : TR(L"Installing version %ls", L"Instalowanie wersji %ls"), kVersion);
    case Done:
        return updating() ? format(TR(L"Updated to version %ls", L"Zaktualizowano do wersji %ls"), kVersion)
                          : std::wstring(TR(L"Installed. Have fun listening!", L"Zainstalowano. Miłego słuchania!"));
    case Failed:
        return TR(L"Something went wrong", L"Coś poszło nie tak");
    case Confirm:
        return format(TR(L"Remove Nokturn %ls from this computer?", L"Usunąć Nokturn %ls z tego komputera?"), existing.version.c_str());
    case Removed:
        return TR(L"Nokturn has been removed", L"Nokturn został usunięty");
    }
    return {};
}

std::wstring Ui::primaryLabel() const
{
    switch (screen) {
    case Welcome:
        if (!updating())
            return TR(L"Install", L"Zainstaluj");
        return compareVersions(existing.version, kVersion) < 0 ? TR(L"Update", L"Zaktualizuj") : TR(L"Reinstall", L"Zainstaluj ponownie");
    case Done: return TR(L"Open Nokturn", L"Otwórz Nokturn");
    case Failed: return TR(L"Try again", L"Spróbuj ponownie");
    case Confirm: return TR(L"Uninstall", L"Odinstaluj");
    default: return TR(L"Close", L"Zamknij");
    }
}

std::wstring Ui::secondaryLabel() const
{
    return screen == Working || screen == Confirm ? TR(L"Cancel", L"Anuluj") : TR(L"Close", L"Zamknij");
}

std::wstring Ui::phaseLabel() const
{
    switch (progress.phase.load()) {
    case Connecting: return TR(L"Connecting to GitHub…", L"Łączenie z GitHubem…");
    case Downloading: return TR(L"Downloading", L"Pobieranie");
    case Verifying: return TR(L"Checking the download…", L"Sprawdzanie pobranego pliku…");
    case Closing: return TR(L"Closing Nokturn…", L"Zamykanie Nokturna…");
    case Installing: return TR(L"Installing", L"Instalowanie");
    case Removing: return TR(L"Removing files", L"Usuwanie plików");
    case Finishing:
    case Finished: return TR(L"Finishing…", L"Kończenie…");
    default: return TR(L"Getting ready…", L"Przygotowanie…");
    }
}

double Ui::targetProgress() const
{
    const double done = double(progress.done), total = std::max(1.0, double(progress.total));
    const double part = std::min(1.0, done / total);
    switch (progress.phase.load()) {
    case Downloading: return 0.85 * part;
    case Verifying:
    case Closing: return uninstalling ? 0.05 : 0.85;
    case Installing: return 0.85 + 0.13 * part;
    case Removing: return 0.05 + 0.9 * part;
    case Finishing: return 0.98;
    case Finished: return 1;
    default: return 0;
    }
}

std::vector<std::pair<Control, D2D1_RECT_F>> Ui::controls() const
{
    std::vector<std::pair<Control, D2D1_RECT_F>> c;
    c.push_back({ Minimize, { kWidth - 84, 8, kWidth - 48, 40 } });
    c.push_back({ Close, { kWidth - 44, 8, kWidth - 8, 40 } });
    const float top = height - 68, bottom = top + 44;
    auto one = [&](Control id, float w) { c.push_back({ id, { (kWidth - w) / 2, top, (kWidth + w) / 2, bottom } }); };
    auto two = [&](float w1, float w2) {
        const float x = (kWidth - w1 - 12 - w2) / 2;
        c.push_back({ Primary, { x, top, x + w1, bottom } });
        c.push_back({ Secondary, { x + w1 + 12, top, x + w1 + 12 + w2, bottom } });
    };
    switch (screen) {
    case Welcome:
        c.push_back({ Change, { kWidth - 44 - 84, 249, kWidth - 44, 279 } });
        c.push_back({ Toggle, { 28, 292, kWidth - 28, 348 } });
        one(Primary, 200);
        break;
    case Working:
        if (!g_opt.silent)
            one(Secondary, 140);
        break;
    case Done:
    case Failed:
        two(screen == Done ? 170 : 180, 120);
        break;
    case Confirm:
        c.push_back({ Toggle, { 28, 236, kWidth - 28, 296 } });
        two(160, 120);
        break;
    case Removed:
        one(Primary, 160);
        break;
    }
    return c;
}

bool Ui::enabled(Control c) const
{
    const bool cancellable = progress.phase <= Downloading && !uninstalling;
    if (screen == Working && (c == Close || c == Secondary))
        return cancellable;
    return true;
}

Control Ui::hit(float x, float y) const
{
    for (const auto &[id, r] : controls())
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom)
            return id;
    return None;
}

void Ui::drawLogo(ID2D1RenderTarget *rt, float x, float y, float size)
{
    // The app icon (packaging/linux/nokturn.svg): a soft gradient tile with four bars.
    const float k = size / 128;
    const D2D1_GRADIENT_STOP stops[2] = { { 0, rgb(0xb3c6bb) }, { 1, rgb(0x3f5061) } };
    ID2D1GradientStopCollection *collection = nullptr;
    ID2D1LinearGradientBrush *gradient = nullptr;
    ID2D1SolidColorBrush *white = nullptr;
    rt->CreateGradientStopCollection(stops, 2, &collection);
    rt->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(D2D1::Point2F(x, y + 4 * k), D2D1::Point2F(x, y + 124 * k)),
                                  collection, &gradient);
    rt->CreateSolidColorBrush(rgb(0xf4f0ea), &white);
    rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x + 4 * k, y + 4 * k, x + 124 * k, y + 124 * k), 38 * k, 38 * k), gradient);
    const float bars[4][4] = { { 35, 52, 10, 24 }, { 52, 36, 10, 56 }, { 69, 44, 10, 40 }, { 86, 56, 10, 16 } };
    for (const auto &b : bars)
        rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x + b[0] * k, y + b[1] * k, x + (b[0] + b[2]) * k, y + (b[1] + b[3]) * k),
                                                   5 * k, 5 * k), white);
    release(white);
    release(gradient);
    release(collection);
}

void Ui::paint(ID2D1RenderTarget *rt)
{
    ID2D1SolidColorBrush *brush = nullptr;
    rt->CreateSolidColorBrush(rgb(color::text), &brush);
    rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    auto fill = [&](const D2D1_RECT_F &r, float radius, D2D1_COLOR_F c) {
        brush->SetColor(c);
        rt->FillRoundedRectangle(D2D1::RoundedRect(r, radius, radius), brush);
    };
    auto text = [&](IDWriteTextFormat *f, const std::wstring &s, const D2D1_RECT_F &r, D2D1_COLOR_F c,
                     DWRITE_TEXT_ALIGNMENT align = DWRITE_TEXT_ALIGNMENT_LEADING) {
        f->SetTextAlignment(align);
        brush->SetColor(c);
        rt->DrawText(s.c_str(), UINT32(s.size()), f, r, brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
    };
    auto rectOf = [&](Control id) {
        for (const auto &[c, r] : controls())
            if (c == id)
                return r;
        return D2D1_RECT_F {};
    };
    auto button = [&](Control id, const std::wstring &label, bool primary) {
        const D2D1_RECT_F r = rectOf(id);
        const float h = m_anim[id];
        const float alpha = enabled(id) ? 1.f : 0.45f;
        if (primary)
            fill(r, (r.bottom - r.top) / 2, mix(rgb(color::accent, alpha), rgb(0xb2c4b9, alpha), h));
        else
            fill(r, (r.bottom - r.top) / 2, rgb(color::text, (0.08f + 0.05f * h) * alpha));
        text(r.bottom - r.top < 40 ? m_buttonSmall : m_button, label, r,
             primary ? rgb(color::onAccent, alpha) : rgb(color::text, alpha), DWRITE_TEXT_ALIGNMENT_CENTER);
    };
    auto toggle = [&](float right, float centerY, bool on) {
        const D2D1_RECT_F r { right - 46, centerY - 13, right, centerY + 13 };
        fill(r, 13, mix(rgb(color::text, 0.11f + 0.05f * m_anim[Toggle]), rgb(color::accent), m_toggle));
        const float x = r.left + 3 + 10 + m_toggle * 20;
        brush->SetColor(mix(rgb(color::text), rgb(color::onAccent), m_toggle));
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, centerY), 10, 10), brush);
        (void)on;
    };

    rt->Clear(rgb(color::bg));

    // Window buttons
    for (const Control id : { Minimize, Close }) {
        const D2D1_RECT_F r = rectOf(id);
        if (m_anim[id] > 0)
            fill(r, 8, rgb(color::text, 0.08f * m_anim[id]));
        brush->SetColor(rgb(enabled(id) ? color::textDim : color::textFaint, enabled(id) ? 1.f : 0.5f));
        const float cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2;
        if (id == Minimize) {
            rt->DrawLine(D2D1::Point2F(cx - 5, cy), D2D1::Point2F(cx + 5, cy), brush, 1.2f);
        } else {
            rt->DrawLine(D2D1::Point2F(cx - 5, cy - 5), D2D1::Point2F(cx + 5, cy + 5), brush, 1.2f);
            rt->DrawLine(D2D1::Point2F(cx + 5, cy - 5), D2D1::Point2F(cx - 5, cy + 5), brush, 1.2f);
        }
    }

    drawLogo(rt, (kWidth - 76) / 2, 54, 76);
    text(m_title, L"Nokturn", { 0, 140, kWidth, 184 }, rgb(color::text), DWRITE_TEXT_ALIGNMENT_CENTER);
    const D2D1_COLOR_F subColor = screen == Done || screen == Removed ? rgb(color::success)
                                  : screen == Failed                  ? rgb(color::danger)
                                                                      : rgb(color::textDim);
    text(m_sub, subtitle(), { 28, 186, kWidth - 28, 210 }, subColor, DWRITE_TEXT_ALIGNMENT_CENTER);

    const D2D1_COLOR_F border = rgb(0xffffff, 0.05f);
    auto card = [&](const D2D1_RECT_F &r) {
        fill(r, 16, rgb(color::surface));
        brush->SetColor(border);
        rt->DrawRoundedRectangle(D2D1::RoundedRect({ r.left + 0.5f, r.top + 0.5f, r.right - 0.5f, r.bottom - 0.5f }, 16, 16), brush, 1);
    };

    switch (screen) {
    case Welcome: {
        card({ 28, 236, kWidth - 28, 348 });
        text(m_label, TR(L"Install to", L"Folder instalacji"), { 46, 248, kWidth - 140, 264 }, rgb(color::textFaint));
        text(m_path, dir, { 46, 265, kWidth - 140, 284 }, rgb(color::text));
        button(Change, TR(L"Change", L"Zmień"), false);
        fill({ 46, 292, kWidth - 46, 293 }, 0, rgb(color::text, 0.06f));
        text(m_body, TR(L"Desktop shortcut", L"Skrót na pulpicie"), { 46, 292, kWidth - 120, 348 }, rgb(color::text));
        toggle(kWidth - 46, 320, desktop);
        button(Primary, primaryLabel(), true);
        break;
    }
    case Working: {
        const int phase = progress.phase;
        text(m_status, phaseLabel(), { 28, 240, kWidth - 170, 266 }, rgb(color::text));
        std::wstring numbers, detail;
        const double mb = 1024.0 * 1024.0;
        if (phase == Downloading) {
            numbers = format(L"%.0f / %.0f MB", double(progress.done) / mb, double(progress.total) / mb);
            const double seconds = (GetTickCount64() - progress.started) / 1000.0;
            if (seconds > 1 && progress.done > 0) {
                const double rate = double(progress.done) / seconds;
                const double left = (double(progress.total) - double(progress.done)) / rate;
                detail = left >= 90 ? format(TR(L"%.1f MB/s · about %.0f min left", L"%.1f MB/s · zostało ok. %.0f min"), rate / mb, std::ceil(left / 60))
                                    : format(TR(L"%.1f MB/s · about %.0f s left", L"%.1f MB/s · zostało ok. %.0f s"), rate / mb, std::ceil(left));
            }
        } else if (phase >= Installing) {
            numbers = format(L"%.0f%%", shownProgress * 100);
        }
        text(m_small, numbers, { kWidth - 220, 240, kWidth - 28, 266 }, rgb(color::textDim), DWRITE_TEXT_ALIGNMENT_TRAILING);
        fill({ 28, 278, kWidth - 28, 284 }, 3, rgb(color::text, 0.12f));
        if (shownProgress > 0.002f)
            fill({ 28, 278, 28 + std::max(6.f, (kWidth - 56) * shownProgress), 284 }, 3, rgb(color::accent));
        text(m_small, detail, { 28, 294, kWidth - 28, 316 }, rgb(color::textFaint));
        if (!g_opt.silent)
            button(Secondary, secondaryLabel(), false);
        break;
    }
    case Done:
        text(m_body, TR(L"You'll find Nokturn in the Start menu.", L"Nokturn znajdziesz w menu Start."), { 28, 240, kWidth - 28, 270 },
             rgb(color::textDim), DWRITE_TEXT_ALIGNMENT_CENTER);
        button(Primary, primaryLabel(), true);
        button(Secondary, secondaryLabel(), false);
        break;
    case Failed:
        text(m_message, error, { 48, 236, kWidth - 48, height - 84 }, rgb(color::textDim), DWRITE_TEXT_ALIGNMENT_CENTER);
        button(Primary, primaryLabel(), true);
        button(Secondary, secondaryLabel(), false);
        break;
    case Confirm:
        card({ 28, 236, kWidth - 28, 296 });
        text(m_body, TR(L"Also remove my library and settings", L"Usuń też bibliotekę i ustawienia"), { 46, 236, kWidth - 120, 296 },
             rgb(color::text));
        toggle(kWidth - 46, 266, removeData);
        text(m_small, TR(L"Otherwise they stay, in case you come back.", L"Inaczej zostaną na wypadek powrotu."),
             { 28, 304, kWidth - 28, 326 }, rgb(color::textFaint), DWRITE_TEXT_ALIGNMENT_CENTER);
        button(Primary, primaryLabel(), true);
        button(Secondary, secondaryLabel(), false);
        break;
    case Removed:
        text(m_body, removeData ? TR(L"Your library and settings are gone too.", L"Biblioteka i ustawienia też zostały usunięte.")
                                : TR(L"Your library and settings are still there.", L"Biblioteka i ustawienia zostały na dysku."),
             { 28, 240, kWidth - 28, 270 }, rgb(color::textDim), DWRITE_TEXT_ALIGNMENT_CENTER);
        button(Primary, primaryLabel(), true);
        break;
    }
    release(brush);
}

bool Ui::tick()
{
    bool changed = false;
    auto approach = [&](float &v, float target, float speed) {
        float next = v + (target - v) * speed;
        if (std::fabs(target - next) < 0.003f)
            next = target;
        if (next != v) {
            v = next;
            changed = true;
        }
    };
    for (int i = 0; i < ControlCount; ++i)
        approach(m_anim[i], m_hot == i && enabled(Control(i)) ? 1.f : 0.f, 0.25f);
    approach(m_toggle, (screen == Confirm ? removeData : desktop) ? 1.f : 0.f, 0.3f);
    if (screen == Working) {
        approach(shownProgress, float(targetProgress()), 0.12f);
        changed = true;   // the numbers move too
    }
    return changed;
}

void Ui::start()
{
    if (m_worker.joinable())
        m_worker.join();
    progress.reset();
    shownProgress = 0;
    screen = Working;
    const Plan plan { dir, desktop };
    const bool uninstall = uninstalling, wipe = removeData;
    m_worker = std::thread([this, plan, uninstall, wipe] {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        std::wstring err;
        const bool ok = uninstall ? runUninstall(plan.dir, wipe, progress, err) : runInstall(plan, progress, err);
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_workerError = err;
        }
        CoUninitialize();
        PostMessageW(m_hwnd, WM_APP, ok, 0);
    });
}

void Ui::finish(bool ok)
{
    if (m_worker.joinable())
        m_worker.join();
    if (ok) {
        if (g_opt.silent) {
            if (!uninstalling)
                launchApp(dir);
            DestroyWindow(m_hwnd);
            return;
        }
        screen = uninstalling ? Removed : Done;
    } else if (progress.cancel) {
        screen = Welcome;
    } else {
        std::lock_guard<std::mutex> lock(m_mutex);
        error = m_workerError;
        screen = Failed;
    }
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void Ui::chooseFolder()
{
    IFileOpenDialog *dialog = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog))))
        return;
    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    dialog->SetTitle(TR(L"Where should Nokturn be installed?", L"Gdzie zainstalować Nokturn?"));
    IShellItem *folder = nullptr;
    const std::wstring startAt = dirExists(dir) ? dir : parentOf(dir);
    if (SUCCEEDED(SHCreateItemFromParsingName(startAt.c_str(), nullptr, IID_PPV_ARGS(&folder)))) {
        dialog->SetFolder(folder);
        folder->Release();
    }
    IShellItem *result = nullptr;
    if (SUCCEEDED(dialog->Show(m_hwnd)) && SUCCEEDED(dialog->GetResult(&result))) {
        PWSTR path = nullptr;
        if (SUCCEEDED(result->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
            // A folder of its own, so uninstalling never touches anything else.
            std::wstring chosen = trimSlash(path);
            if (lower(fileName(chosen)) != L"nokturn")
                chosen = join(chosen, L"Nokturn");
            dir = chosen;
        }
        CoTaskMemFree(path);
        result->Release();
    }
    dialog->Release();
}

void Ui::activate(Control c)
{
    if (c == None || !enabled(c))
        return;
    switch (c) {
    case Minimize:
        ShowWindow(m_hwnd, SW_MINIMIZE);
        break;
    case Close:
        if (screen == Working)
            progress.cancel = true;
        DestroyWindow(m_hwnd);
        return;
    case Change:
        chooseFolder();
        break;
    case Toggle:
        (screen == Confirm ? removeData : desktop) = !(screen == Confirm ? removeData : desktop);
        break;
    case Primary:
        if (screen == Done) {
            launchApp(dir);
            DestroyWindow(m_hwnd);
            return;
        }
        if (screen == Removed) {
            DestroyWindow(m_hwnd);
            return;
        }
        start();
        break;
    case Secondary:
        if (screen == Working) {
            progress.cancel = true;
            break;
        }
        DestroyWindow(m_hwnd);
        return;
    default:
        break;
    }
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void Ui::resize()
{
    m_scale = GetDpiForWindow(m_hwnd) / 96.f;
    HMONITOR monitor = MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info { sizeof info };
    GetMonitorInfoW(monitor, &info);
    const int w = int(std::lround(kWidth * m_scale)), h = int(std::lround(height * m_scale));
    const RECT &area = info.rcWork;
    SetWindowPos(m_hwnd, nullptr, area.left + (area.right - area.left - w) / 2, area.top + (area.bottom - area.top - h) / 2, w, h,
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

LRESULT CALLBACK Ui::proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_NCCREATE)
        g_ui.m_hwnd = hwnd;
    return g_ui.handle(msg, wp, lp);
}

LRESULT Ui::handle(UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_NCCALCSIZE:
        if (wp)
            return 0;   // the whole window is ours to draw; DWM still adds the shadow and rounded corners
        break;
    case WM_NCHITTEST: {
        POINT pt { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        ScreenToClient(m_hwnd, &pt);
        return hit(pt.x / m_scale, pt.y / m_scale) != None ? HTCLIENT : HTCAPTION;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(m_hwnd, &ps);
        if (!m_rt) {
            RECT rc;
            GetClientRect(m_hwnd, &rc);
            const float dpi = 96 * m_scale;
            m_d2d->CreateHwndRenderTarget(D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(), dpi, dpi),
                                          D2D1::HwndRenderTargetProperties(m_hwnd, D2D1::SizeU(rc.right, rc.bottom)), &m_rt);
        }
        if (m_rt) {
            m_rt->BeginDraw();
            paint(m_rt);
            if (m_rt->EndDraw() == HRESULT(D2DERR_RECREATE_TARGET))
                release(m_rt);
        }
        EndPaint(m_hwnd, &ps);
        return 0;
    }
    case WM_SIZE:
        if (m_rt)
            m_rt->Resize(D2D1::SizeU(LOWORD(lp), HIWORD(lp)));
        return 0;
    case WM_DPICHANGED: {
        m_scale = HIWORD(wp) / 96.f;
        const RECT *r = reinterpret_cast<const RECT *>(lp);
        SetWindowPos(m_hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
        if (m_rt)
            m_rt->SetDpi(96 * m_scale, 96 * m_scale);
        return 0;
    }
    case WM_TIMER:
        if (tick())
            InvalidateRect(m_hwnd, nullptr, FALSE);
        return 0;
    case WM_MOUSEMOVE: {
        const Control c = hit(GET_X_LPARAM(lp) / m_scale, GET_Y_LPARAM(lp) / m_scale);
        m_hot = c;
        if (!m_tracking) {
            TRACKMOUSEEVENT t { sizeof t, TME_LEAVE, m_hwnd, 0 };
            m_tracking = TrackMouseEvent(&t);
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        m_tracking = false;
        m_hot = None;
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT && m_hot >= Primary && enabled(m_hot)) {
            SetCursor(LoadCursor(nullptr, IDC_HAND));
            return TRUE;
        }
        break;
    case WM_LBUTTONDOWN:
        m_pressed = hit(GET_X_LPARAM(lp) / m_scale, GET_Y_LPARAM(lp) / m_scale);
        SetCapture(m_hwnd);
        return 0;
    case WM_LBUTTONUP: {
        ReleaseCapture();
        const Control c = hit(GET_X_LPARAM(lp) / m_scale, GET_Y_LPARAM(lp) / m_scale);
        const Control pressed = m_pressed;
        m_pressed = None;
        if (c == pressed)
            activate(c);
        return 0;
    }
    case WM_KEYDOWN:
        if (wp == VK_RETURN) {
            for (const auto &[id, r] : controls())
                if (id == Primary)
                    activate(Primary);
        } else if (wp == VK_ESCAPE) {
            activate(Close);
        }
        return 0;
    case WM_CLOSE:
        activate(Close);
        return 0;
    case WM_APP:
        finish(wp != 0);
        return 0;
    case WM_DESTROY:
        KillTimer(m_hwnd, 1);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(m_hwnd, msg, wp, lp);
}

int Ui::run(HINSTANCE instance)
{
    if (!initGraphics())
        return 1;
    m_toggle = (screen == Confirm ? removeData : desktop) ? 1.f : 0.f;
    WNDCLASSEXW wc { sizeof wc };
    wc.lpfnWndProc = proc;
    wc.hInstance = instance;
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    wc.hIconSm = HICON(LoadImageW(instance, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"NokturnSetup";
    RegisterClassExW(&wc);
    const wchar_t *title = uninstalling ? TR(L"Uninstall Nokturn", L"Odinstaluj Nokturn") : TR(L"Nokturn Setup", L"Instalator Nokturna");
    CreateWindowExW(WS_EX_APPWINDOW, wc.lpszClassName, title, WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_THICKFRAME,
                    CW_USEDEFAULT, CW_USEDEFAULT, 480, 440, nullptr, nullptr, instance, nullptr);
    if (!m_hwnd)
        return 1;
    const BOOL dark = TRUE;
    DwmSetWindowAttribute(m_hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof dark);
    const int round = 2;   // DWMWCP_ROUND (Windows 11)
    DwmSetWindowAttribute(m_hwnd, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &round, sizeof round);
    const COLORREF edge = RGB(0x2b, 0x2d, 0x32);
    DwmSetWindowAttribute(m_hwnd, 34 /* DWMWA_BORDER_COLOR */, &edge, sizeof edge);
    const MARGINS margins { 1, 1, 1, 1 };
    DwmExtendFrameIntoClientArea(m_hwnd, &margins);
    resize();
    SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    ShowWindow(m_hwnd, SW_SHOW);
    SetForegroundWindow(m_hwnd);
    SetTimer(m_hwnd, 1, 15, nullptr);
    if (screen == Working)
        start();

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (m_worker.joinable()) {
        // Closed during the download: nothing has been touched yet, so there's nothing to wait for.
        if (progress.cancel)
            ExitProcess(0);
        m_worker.join();
    }
    return screen == Failed ? 1 : 0;
}

bool Ui::renderPng(const std::wstring &file, float scale)
{
    if (!initGraphics())
        return false;
    IWICImagingFactory *wic = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic))))
        return false;
    const UINT w = UINT(kWidth * scale), h = UINT(height * scale);
    IWICBitmap *bitmap = nullptr;
    ID2D1RenderTarget *rt = nullptr;
    wic->CreateBitmap(w, h, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, &bitmap);
    m_d2d->CreateWicBitmapRenderTarget(bitmap,
                                       D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
                                                                    D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
                                                                    96 * scale, 96 * scale),
                                       &rt);
    if (!rt)
        return false;
    m_toggle = (screen == Confirm ? removeData : desktop) ? 1.f : 0.f;
    rt->BeginDraw();
    paint(rt);
    rt->EndDraw();
    IWICStream *stream = nullptr;
    IWICBitmapEncoder *encoder = nullptr;
    IWICBitmapFrameEncode *frame = nullptr;
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppPBGRA;
    const bool ok = SUCCEEDED(wic->CreateStream(&stream)) && SUCCEEDED(stream->InitializeFromFilename(file.c_str(), GENERIC_WRITE))
                    && SUCCEEDED(wic->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder))
                    && SUCCEEDED(encoder->Initialize(stream, WICBitmapEncoderNoCache)) && SUCCEEDED(encoder->CreateNewFrame(&frame, nullptr))
                    && SUCCEEDED(frame->Initialize(nullptr)) && SUCCEEDED(frame->SetSize(w, h)) && SUCCEEDED(frame->SetPixelFormat(&format))
                    && SUCCEEDED(frame->WriteSource(bitmap, nullptr)) && SUCCEEDED(frame->Commit()) && SUCCEEDED(encoder->Commit());
    release(frame);
    release(encoder);
    release(stream);
    release(rt);
    release(bitmap);
    release(wic);
    return ok;
}

void parseArgs()
{
    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; i < argc; ++i) {
        const std::wstring arg = argv[i], up = lower(arg);
        auto next = [&] { return i + 1 < argc ? std::wstring(argv[++i]) : std::wstring(); };
        if (up == L"/silent")
            g_opt.silent = true;
        else if (up == L"/verysilent")
            g_opt.silent = g_opt.verySilent = true;
        else if (up.rfind(L"/dir=", 0) == 0)
            g_opt.dir = arg.substr(5);
        else if (up == L"--uninstall")
            g_opt.uninstall = true;
        else if (up == L"--dir")
            g_opt.dir = next();
        else if (up == L"--wait")
            g_opt.waitPid = DWORD(_wtoi(next().c_str()));
        else if (up == L"--payload")
            g_opt.payload = next();
        else if (up == L"--url")
            g_opt.url = next();
        else if (up == L"--test")
            g_opt.test = true;
        else if (up == L"--no-launch")
            g_opt.noLaunch = true;
        else if (up == L"--preview") {
            g_opt.previewScreen = next();
            g_opt.previewFile = next();
            if (i + 1 < argc && argv[i + 1][0] != L'-' && argv[i + 1][0] != L'/')
                g_opt.previewScale = float(_wtof(next().c_str()));
        }
        // Inno Setup's other switches (/SUPPRESSMSGBOXES, /NORESTART, /NOCANCEL…) need nothing here.
    }
    LocalFree(argv);
}

int preview()
{
    const std::wstring &s = g_opt.previewScreen;
    g_ui.dir = defaultDir();
    if (s == L"update" || s == L"done" || s == L"uninstall" || s == L"removed")
        g_ui.existing = { g_ui.dir, L"0.11.0" };
    if (s == L"working" || s == L"silent") {
        g_ui.screen = Working;
        g_ui.progress.phase = Downloading;
        g_ui.progress.total = kPayloadSize;
        g_ui.progress.done = kPayloadSize * 2 / 5;
        g_ui.progress.started = GetTickCount64() - 9000;
        g_ui.shownProgress = 0.34f;
        if (s == L"silent") {
            g_opt.silent = true;
            g_ui.height = 340;
        }
    } else if (s == L"done") {
        g_ui.screen = Done;
    } else if (s == L"error") {
        g_ui.screen = Failed;
        g_ui.error = netError(ERROR_WINHTTP_CANNOT_CONNECT);
    } else if (s == L"uninstall") {
        g_ui.screen = Confirm;
        g_ui.uninstalling = true;
        g_ui.removeData = false;
    } else if (s == L"removed") {
        g_ui.screen = Removed;
        g_ui.uninstalling = true;
    }
    if (g_ui.screen == Confirm)
        g_ui.removeData = false;
    return g_ui.renderPng(g_opt.previewFile, g_opt.previewScale) ? 0 : 1;
}

// The uninstaller can't delete itself: a copy in the temporary folder does the work.
bool relaunchFromTemp(const std::wstring &dir)
{
    const std::wstring copy = join(tempDir(), format(L"Nokturn-uninstall-%lu.exe", GetCurrentProcessId()));
    if (!CopyFileW(selfPath().c_str(), copy.c_str(), FALSE))
        return false;
    std::wstring command = format(L"\"%ls\" --uninstall --dir \"%ls\" --wait %lu", copy.c_str(), dir.c_str(), GetCurrentProcessId());
    if (g_opt.verySilent)
        command += L" /VERYSILENT";
    else if (g_opt.silent)
        command += L" /SILENT";
    if (g_opt.test)
        command += L" --test";
    STARTUPINFOW si { sizeof si };
    PROCESS_INFORMATION pi {};
    if (!CreateProcessW(copy.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr, tempDir().c_str(), &si, &pi))
        return false;
    AllowSetForegroundWindow(pi.dwProcessId);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

void deleteSelfLater()
{
    wchar_t shell[MAX_PATH] = L"cmd.exe";
    GetEnvironmentVariableW(L"ComSpec", shell, MAX_PATH);
    std::wstring command = format(L"\"%ls\" /c ping -n 3 127.0.0.1 >nul & del /f /q \"%ls\"", shell, selfPath().c_str());
    STARTUPINFOW si { sizeof si };
    PROCESS_INFORMATION pi {};
    if (CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, tempDir().c_str(), &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    g_polish = PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_POLISH;
    parseArgs();
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (!g_opt.previewScreen.empty())
        return preview();

    if (g_opt.uninstall) {
        std::wstring dir = trimSlash(g_opt.dir);
        if (dir.empty())
            dir = trimSlash(regString(HKEY_CURRENT_USER, ourKey(), L"InstallLocation"));
        if (dir.empty())
            dir = parentOf(selfPath());
        if (!g_opt.waitPid && inside(selfPath(), dir))
            return relaunchFromTemp(dir) ? 0 : 1;
        if (g_opt.waitPid) {
            if (const HANDLE parent = OpenProcess(SYNCHRONIZE, FALSE, g_opt.waitPid)) {
                WaitForSingleObject(parent, 10000);
                CloseHandle(parent);
            }
        }
        int code = 0;
        if (g_opt.verySilent) {
            Progress p;
            std::wstring error;
            code = runUninstall(dir, false, p, error) ? 0 : 1;
        } else {
            g_ui.uninstalling = true;
            g_ui.dir = dir;
            g_ui.existing = { dir, regString(HKEY_CURRENT_USER, ourKey(), L"DisplayVersion") };
            g_ui.screen = g_opt.silent ? Working : Confirm;
            if (g_opt.silent)
                g_ui.height = 340;
            code = g_ui.run(instance);
        }
        if (g_opt.waitPid)
            deleteSelfLater();
        return code;
    }

    const Existing existing = findExisting();
    std::wstring dir = trimSlash(g_opt.dir);
    if (dir.empty())
        dir = existing.dir.empty() ? defaultDir() : existing.dir;
    const bool desktop = existing.dir.empty() || lower(existing.dir) != lower(dir) || fileExists(desktopShortcut());
    if (g_opt.verySilent) {
        Progress p;
        std::wstring error;
        if (!runInstall({ dir, desktop }, p, error))
            return 1;
        launchApp(dir);
        return 0;
    }
    g_ui.existing = existing;
    g_ui.dir = dir;
    g_ui.desktop = desktop;
    if (g_opt.silent) {
        g_ui.screen = Working;
        g_ui.height = 340;
    }
    return g_ui.run(instance);
}
