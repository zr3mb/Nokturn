#include "GameOverlay.h"

#include "AppPaths.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QCursor>
#include <QDir>
#include <QGuiApplication>
#include <QJSEngine>
#include <QKeySequence>
#include <QLocalServer>
#include <QLocalSocket>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QScreen>
#include <QWindow>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <xinput.h>
#endif

#ifdef Q_OS_LINUX
#  include "ShortcutPortal.h"
#  include <cerrno>
#  include <fcntl.h>
#  include <linux/input.h>
#  include <sys/ioctl.h>
#  include <unistd.h>
#  ifdef NOKTURN_LAYERSHELL
#    include <LayerShellQt/window.h>
#  endif
#endif

#include <algorithm>
#include <atomic>
#include <cstdlib>

namespace {

constexpr int kDefaultKey = Qt::Key_M;
constexpr int kDefaultModifiers = int(Qt::ControlModifier | Qt::ShiftModifier);

// The pad's buttons, whatever the platform reports them as
enum PadButton : unsigned {
    PadA = 1u << 0, PadB = 1u << 1, PadX = 1u << 2, PadY = 1u << 3,
    PadLB = 1u << 4, PadRB = 1u << 5, PadView = 1u << 6, PadMenu = 1u << 7,
    PadUp = 1u << 8, PadDown = 1u << 9, PadLeft = 1u << 10, PadRight = 1u << 11,
};

QString serverName()
{
    // Per user: another account's Nokturn must not get the requests.
    QString user = qEnvironmentVariable("USER", qEnvironmentVariable("USERNAME", QStringLiteral("user")));
    user.remove(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]")));
    return QStringLiteral("nokturn-overlay-") + user;
}

#ifdef Q_OS_WIN
// The hotkey comes from a low-level keyboard hook, not RegisterHotKey: a game that reads the keyboard
// through Raw Input with RIDEV_NOHOTKEYS (Euro Truck Simulator 2 while driving) turns registered
// hotkeys off, a hook still sees the keys. It runs on a thread of its own, so Windows doesn't drop it
// when the GUI thread is busy for a moment, and swallows the key so the game doesn't get it too.
std::atomic<UINT> g_hookVk { 0 };      // 0 = off
std::atomic<UINT> g_hookMods { 0 };    // MOD_* bits
std::atomic<bool> g_hookDown { false };
DWORD g_hookThread = 0;

LRESULT CALLBACK keyboardHook(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION) {
        const auto *key = reinterpret_cast<const KBDLLHOOKSTRUCT *>(lParam);
        const UINT vk = g_hookVk.load();
        if (vk && key->vkCode == vk) {
            const bool down = wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;
            if (!down) {
                if (g_hookDown.exchange(false))
                    return 1;   // the release of a press we took
            } else {
                auto held = [](int k) { return (GetAsyncKeyState(k) & 0x8000) != 0; };
                UINT mods = 0;
                if (held(VK_CONTROL)) mods |= MOD_CONTROL;
                if (held(VK_SHIFT)) mods |= MOD_SHIFT;
                if (held(VK_MENU)) mods |= MOD_ALT;
                if (held(VK_LWIN) || held(VK_RWIN)) mods |= MOD_WIN;
                if (mods == g_hookMods.load()) {
                    if (!g_hookDown.exchange(true))   // not the auto-repeat
                        QMetaObject::invokeMethod(GameOverlay::instance(), &GameOverlay::toggle, Qt::QueuedConnection);
                    return 1;
                }
            }
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

DWORD WINAPI hookThread(LPVOID)
{
    HHOOK hook = SetWindowsHookExW(WH_KEYBOARD_LL, keyboardHook, GetModuleHandleW(nullptr), 0);
    if (!hook) {
        qWarning("GameOverlay: no keyboard hook (%lu)", GetLastError());
        return 1;
    }
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    }
    UnhookWindowsHookEx(hook);
    return 0;
}

// A Qt key as a Windows virtual key; 0 when it can't be a hotkey.
UINT virtualKey(int key)
{
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        return 'A' + UINT(key - Qt::Key_A);
    if (key >= Qt::Key_0 && key <= Qt::Key_9)
        return '0' + UINT(key - Qt::Key_0);
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24)
        return VK_F1 + UINT(key - Qt::Key_F1);
    switch (key) {
    case Qt::Key_Space: return VK_SPACE;
    case Qt::Key_QuoteLeft: return VK_OEM_3;
    case Qt::Key_Insert: return VK_INSERT;
    case Qt::Key_Delete: return VK_DELETE;
    case Qt::Key_Home: return VK_HOME;
    case Qt::Key_End: return VK_END;
    case Qt::Key_PageUp: return VK_PRIOR;
    case Qt::Key_PageDown: return VK_NEXT;
    case Qt::Key_Pause: return VK_PAUSE;
    case Qt::Key_ScrollLock: return VK_SCROLL;
    case Qt::Key_Minus: return VK_OEM_MINUS;
    case Qt::Key_Equal: return VK_OEM_PLUS;
    case Qt::Key_BracketLeft: return VK_OEM_4;
    case Qt::Key_BracketRight: return VK_OEM_6;
    case Qt::Key_Backslash: return VK_OEM_5;
    case Qt::Key_Semicolon: return VK_OEM_1;
    case Qt::Key_Apostrophe: return VK_OEM_7;
    case Qt::Key_Comma: return VK_OEM_COMMA;
    case Qt::Key_Period: return VK_OEM_PERIOD;
    case Qt::Key_Slash: return VK_OEM_2;
    default: return 0;
    }
}

using XInputGetStateFn = DWORD(WINAPI *)(DWORD, XINPUT_STATE *);

XInputGetStateFn xinputGetState()
{
    // From System32 only; xinput1_4 is in Windows 8+, 9_1_0 is the older fallback.
    static const XInputGetStateFn fn = [] {
        for (const wchar_t *dll : { L"xinput1_4.dll", L"xinput9_1_0.dll" })
            if (HMODULE module = LoadLibraryExW(dll, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32))
                if (auto f = reinterpret_cast<XInputGetStateFn>(reinterpret_cast<void *>(GetProcAddress(module, "XInputGetState"))))
                    return f;
        return XInputGetStateFn(nullptr);
    }();
    return fn;
}
#endif

#ifdef Q_OS_LINUX
// The XDG shortcut syntax ("CTRL+SHIFT+m") for the trigger the portal is asked for.
QString portalTrigger(int key, int modifiers)
{
    QStringList parts;
    if (modifiers & Qt::ControlModifier) parts << QStringLiteral("CTRL");
    if (modifiers & Qt::AltModifier) parts << QStringLiteral("ALT");
    if (modifiers & Qt::ShiftModifier) parts << QStringLiteral("SHIFT");
    if (modifiers & Qt::MetaModifier) parts << QStringLiteral("LOGO");
    parts << QKeySequence(key).toString(QKeySequence::PortableText).toLower();
    return parts.join(QLatin1Char('+'));
}

bool testBit(const unsigned long *bits, int bit)
{
    constexpr int perLong = sizeof(unsigned long) * 8;
    return (bits[bit / perLong] >> (bit % perLong)) & 1;
}
#endif

// Keys that work alone; others need Ctrl or Alt, so they don't fire while typing.
bool standalone(int key)
{
    return (key >= Qt::Key_F1 && key <= Qt::Key_F24) || key == Qt::Key_Pause || key == Qt::Key_ScrollLock
        || key == Qt::Key_Insert;
}

} // namespace

GameOverlay *GameOverlay::instance()
{
    static GameOverlay *inst = new GameOverlay;
    return inst;
}

GameOverlay *GameOverlay::create(QQmlEngine *, QJSEngine *)
{
    GameOverlay *o = instance();
    QJSEngine::setObjectOwnership(o, QJSEngine::CppOwnership);
    return o;
}

GameOverlay::GameOverlay(QObject *parent)
    : QObject(parent)
{
    auto settings = AppPaths::settings();
    m_enabled = settings->value("overlay/enabled", true).toBool();
    m_padEnabled = settings->value("overlay/pad", true).toBool();
    const QKeySequence seq = QKeySequence::fromString(settings->value("overlay/hotkey").toString(), QKeySequence::PortableText);
    if (!seq.isEmpty()) {
        m_key = seq[0].key();
        m_modifiers = int(seq[0].keyboardModifiers());
    } else {
        m_key = kDefaultKey;
        m_modifiers = kDefaultModifiers;
    }
    m_clock.start();
    m_padTimer.setInterval(60);
    connect(&m_padTimer, &QTimer::timeout, this, &GameOverlay::pollPad);
    listen();
    if (supported()) {
        QTimer::singleShot(0, this, [this] {
            registerHotkey();
            if (m_enabled && m_padEnabled)
                m_padTimer.start();
        });
    }
}

GameOverlay::~GameOverlay()
{
    unregisterHotkey();
#ifdef Q_OS_WIN
    if (g_hookThread)
        PostThreadMessageW(g_hookThread, WM_QUIT, 0, 0);
#endif
    closePad();
}

bool GameOverlay::supported() const
{
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    return true;
#else
    return false;
#endif
}

// ---- "nokturn --overlay" ----------------------------------------------------------

bool GameOverlay::sendToRunning()
{
    QLocalSocket socket;
    socket.connectToServer(serverName());
    if (!socket.waitForConnected(500))
        return false;
    socket.write("overlay\n");
    socket.waitForBytesWritten(500);
    socket.disconnectFromServer();
    return true;
}

void GameOverlay::listen()
{
    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server->listen(serverName())) {
        // A socket left behind by a crash — unless another Nokturn is really there.
        QLocalSocket probe;
        probe.connectToServer(serverName());
        if (probe.waitForConnected(200))
            return;
        QLocalServer::removeServer(serverName());
        if (!m_server->listen(serverName()))
            return;
    }
    connect(m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *socket = m_server->nextPendingConnection()) {
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
                while (socket->canReadLine())
                    if (socket->readLine().trimmed() == "overlay")
                        toggle();
            });
        }
    });
}

QString GameOverlay::toggleCommand() const
{
    const QString appImage = qEnvironmentVariable("APPIMAGE");
    if (!appImage.isEmpty())
        return QStringLiteral("\"%1\" --overlay").arg(appImage);
    const QString exe = QCoreApplication::applicationFilePath();
    if (exe.startsWith(QLatin1String("/usr/")) || exe.startsWith(QDir::homePath() + QLatin1String("/.local/bin/")))
        return QStringLiteral("nokturn --overlay");
    return QStringLiteral("\"%1\" --overlay").arg(exe);
}

// ---- settings and the hotkey ------------------------------------------------------

QString GameOverlay::hotkeyMode() const
{
#ifdef Q_OS_WIN
    return QStringLiteral("native");
#else
    return m_portal && m_portal->available() ? QStringLiteral("portal") : QStringLiteral("command");
#endif
}

QString GameOverlay::hotkey() const
{
#ifdef Q_OS_LINUX
    return m_portal ? m_portal->trigger() : QString();
#else
    return QKeySequence(QKeyCombination(Qt::KeyboardModifiers(m_modifiers), Qt::Key(m_key))).toString(QKeySequence::NativeText);
#endif
}

bool GameOverlay::canConfigureHotkey() const
{
#ifdef Q_OS_LINUX
    return m_portal && m_portal->canConfigure();
#else
    return false;
#endif
}

void GameOverlay::configureHotkey()
{
#ifdef Q_OS_LINUX
    if (m_portal)
        m_portal->configure();
#endif
}

void GameOverlay::copyToClipboard(const QString &text)
{
    QGuiApplication::clipboard()->setText(text);
}

void GameOverlay::setEnabled(bool on)
{
    if (on == m_enabled)
        return;
    m_enabled = on;
    if (!on)
        hide();
    unregisterHotkey();
    registerHotkey();
    if (m_enabled && m_padEnabled && supported())
        m_padTimer.start();
    else
        m_padTimer.stop();
    saveSettings();
    emit settingsChanged();
}

void GameOverlay::setPadEnabled(bool on)
{
    if (on == m_padEnabled)
        return;
    m_padEnabled = on;
    if (m_enabled && m_padEnabled && supported())
        m_padTimer.start();
    else
        m_padTimer.stop();
    saveSettings();
    emit settingsChanged();
}

bool GameOverlay::setHotkey(int key, int modifiers)
{
    modifiers &= int(Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier);
    if (!standalone(key) && !(modifiers & (Qt::ControlModifier | Qt::AltModifier)))
        return false;
#ifdef Q_OS_WIN
    if (!virtualKey(key))
        return false;
#endif
    unregisterHotkey();
    m_key = key;
    m_modifiers = modifiers;
    registerHotkey();
    saveSettings();
    emit settingsChanged();
    return true;
}

void GameOverlay::resetHotkey()
{
    setHotkey(kDefaultKey, kDefaultModifiers);
}

void GameOverlay::saveSettings()
{
    auto settings = AppPaths::settings();
    settings->setValue("overlay/enabled", m_enabled);
    settings->setValue("overlay/pad", m_padEnabled);
    settings->setValue("overlay/hotkey",
        QKeySequence(QKeyCombination(Qt::KeyboardModifiers(m_modifiers), Qt::Key(m_key))).toString(QKeySequence::PortableText));
}

void GameOverlay::registerHotkey()
{
#ifdef Q_OS_WIN
    m_hotkeyTaken = false;
    if (!m_enabled)
        return;
    UINT mods = 0;
    if (m_modifiers & Qt::ControlModifier) mods |= MOD_CONTROL;
    if (m_modifiers & Qt::ShiftModifier) mods |= MOD_SHIFT;
    if (m_modifiers & Qt::AltModifier) mods |= MOD_ALT;
    if (m_modifiers & Qt::MetaModifier) mods |= MOD_WIN;
    g_hookMods = mods;
    g_hookVk = virtualKey(m_key);
    if (!g_hookThread)
        CloseHandle(CreateThread(nullptr, 0, hookThread, nullptr, 0, &g_hookThread));
    m_hotkeyRegistered = true;
#elif defined(Q_OS_LINUX)
    // Once per run: the desktop keeps the binding (and may ask the user about it the first time).
    if (!m_enabled || m_portal || QGuiApplication::platformName() == QLatin1String("offscreen"))
        return;
    m_portal = new ShortcutPortal(this);
    connect(m_portal, &ShortcutPortal::changed, this, &GameOverlay::settingsChanged);
    connect(m_portal, &ShortcutPortal::activated, this, [this](const QString &token) {
        m_activationToken = token;
        toggle();
    });
    m_portal->start(QStringLiteral("overlay"), tr("Open the Nokturn menu over a game"), portalTrigger(m_key, m_modifiers));
#endif
}

void GameOverlay::unregisterHotkey()
{
#ifdef Q_OS_WIN
    g_hookVk = 0;   // the hook stays, ignoring every key
    m_hotkeyRegistered = false;
#endif
}

// ---- open / close -----------------------------------------------------------------

void GameOverlay::show()
{
    if (m_open || !m_enabled)
        return;
    QScreen *screen = QGuiApplication::primaryScreen();
#ifdef Q_OS_WIN
    // The game is the window in front (not one of ours); the menu covers its monitor.
    HWND front = GetForegroundWindow();
    DWORD pid = 0;
    GetWindowThreadProcessId(front, &pid);
    m_previous = front && pid != GetCurrentProcessId() ? quintptr(front) : 0;
    POINT center {};
    RECT rect {};
    if (front && GetWindowRect(front, &rect)) {
        center = { (rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2 };
    } else {
        GetCursorPos(&center);
    }
    // Qt keeps a screen's top-left in native pixels and scales its size.
    for (QScreen *s : QGuiApplication::screens()) {
        const QRect g = s->geometry();
        const QRect native(g.topLeft(), g.size() * s->devicePixelRatio());
        if (native.contains(QPoint(center.x, center.y))) {
            screen = s;
            break;
        }
    }
#else
    // X11 knows the pointer; on Wayland a layer surface goes where the compositor puts it anyway.
    if (QScreen *s = QGuiApplication::screenAt(QCursor::pos()))
        screen = s;
#endif
    m_area = screen ? screen->geometry() : QRect(0, 0, 1280, 720);
    m_open = true;
    m_held.clear();
    m_padTimer.setInterval(16);
    emit openChanged();
}

void GameOverlay::hide()
{
    if (!m_open)
        return;
    m_open = false;
    m_padTimer.setInterval(60);
    emit openChanged();
#ifdef Q_OS_WIN
    // Back to the game (we're in front, so we may hand the focus over).
    const HWND game = reinterpret_cast<HWND>(m_previous);
    if (game && IsWindow(game)) {
        if (IsIconic(game))
            ShowWindow(game, SW_RESTORE);
        SetForegroundWindow(game);
    }
#endif
    // Wayland compositors give the focus back to the game themselves.
    m_previous = 0;
}

void GameOverlay::toggle()
{
    m_open ? hide() : show();
}

void GameOverlay::prepare(QWindow *window)
{
    if (!window)
        return;
    // Hidden between uses, it keeps its GPU context and scene: the covers it cached belong to them.
    if (auto *quick = qobject_cast<QQuickWindow *>(window)) {
        quick->setPersistentGraphics(true);
        quick->setPersistentSceneGraph(true);
    }
#if defined(Q_OS_LINUX) && defined(NOKTURN_LAYERSHELL)
    // A layer surface above everything, full games included, with the keyboard while it's shown.
    // The compositor puts it on the output it chooses (the focused one).
    if (QGuiApplication::platformName().startsWith(QLatin1String("wayland")) && !window->handle()) {
        if (auto *layer = LayerShellQt::Window::get(window)) {
            layer->setLayer(LayerShellQt::Window::LayerOverlay);
            layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityExclusive);
            layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop | LayerShellQt::Window::AnchorBottom
                                                            | LayerShellQt::Window::AnchorLeft | LayerShellQt::Window::AnchorRight));
            layer->setExclusiveZone(-1);
            layer->setScope(QStringLiteral("nokturn-overlay"));
            m_layerShell = true;
        }
    }
#endif
}

void GameOverlay::activate(QWindow *window)
{
    if (!window)
        return;
#ifdef Q_OS_WIN
    // Windows lets only the app in front take the focus. The hotkey counts as our input, the pad
    // doesn't — joining the front window's input queue for a moment makes the switch allowed.
    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    const DWORD front = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
    const DWORD self = GetCurrentThreadId();
    const bool attached = front && front != self && AttachThreadInput(self, front, TRUE);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(hwnd);
    BringWindowToTop(hwnd);
    SetFocus(hwnd);
    if (attached)
        AttachThreadInput(self, front, FALSE);
#else
    // Without layer-shell (GNOME) the menu is a full-screen window the compositor stacks over the game.
    if (!m_layerShell)
        window->showFullScreen();
    // Wayland: the portal's activation token lets the window take the focus (Qt reads it from here).
    if (!m_activationToken.isEmpty()) {
        qputenv("XDG_ACTIVATION_TOKEN", m_activationToken.toUtf8());
        m_activationToken.clear();
    }
#endif
    window->requestActivate();
}

void GameOverlay::setView(QQuickItem *view)
{
    if (m_view == view)
        return;
    m_view = view;
    emit viewChanged();
}

// ---- the pad ----------------------------------------------------------------------

void GameOverlay::closePad()
{
#ifdef Q_OS_LINUX
    if (m_padFd >= 0)
        ::close(m_padFd);
    m_padFd = -1;
    m_evButtons = m_evHat = 0;
    m_evX = m_evY = 0;
#endif
    m_padSlot = -1;
}

bool GameOverlay::readPad(unsigned &buttons, int &x, int &y)
{
    const qint64 now = m_clock.elapsed();
#ifdef Q_OS_WIN
    const XInputGetStateFn getState = xinputGetState();
    if (!getState)
        return false;
    XINPUT_STATE state {};
    if (m_padSlot < 0) {
        if (now < m_nextPadScan)
            return false;
        m_nextPadScan = now + 3000;
        for (DWORD i = 0; i < XUSER_MAX_COUNT && m_padSlot < 0; ++i)
            if (getState(i, &state) == ERROR_SUCCESS)
                m_padSlot = int(i);
        if (m_padSlot < 0)
            return false;
    } else if (getState(DWORD(m_padSlot), &state) != ERROR_SUCCESS) {
        m_padSlot = -1;
        m_nextPadScan = now + 3000;
        return false;
    }
    const WORD w = state.Gamepad.wButtons;
    buttons = 0;
    if (w & XINPUT_GAMEPAD_A) buttons |= PadA;
    if (w & XINPUT_GAMEPAD_B) buttons |= PadB;
    if (w & XINPUT_GAMEPAD_X) buttons |= PadX;
    if (w & XINPUT_GAMEPAD_Y) buttons |= PadY;
    if (w & XINPUT_GAMEPAD_LEFT_SHOULDER) buttons |= PadLB;
    if (w & XINPUT_GAMEPAD_RIGHT_SHOULDER) buttons |= PadRB;
    if (w & XINPUT_GAMEPAD_BACK) buttons |= PadView;
    if (w & XINPUT_GAMEPAD_START) buttons |= PadMenu;
    if (w & XINPUT_GAMEPAD_DPAD_UP) buttons |= PadUp;
    if (w & XINPUT_GAMEPAD_DPAD_DOWN) buttons |= PadDown;
    if (w & XINPUT_GAMEPAD_DPAD_LEFT) buttons |= PadLeft;
    if (w & XINPUT_GAMEPAD_DPAD_RIGHT) buttons |= PadRight;
    x = state.Gamepad.sThumbLX;
    y = state.Gamepad.sThumbLY;
    return true;
#elif defined(Q_OS_LINUX)
    if (m_padFd < 0) {
        if (now < m_nextPadScan)
            return false;
        m_nextPadScan = now + 3000;
        // The first event device that looks like a pad: gamepad buttons and a stick, no letter keys —
        // virtual keyboards (ydotool, remote desktops) claim every button there is.
        // logind gives the seat's user access to pads.
        const QStringList devices = QDir(QStringLiteral("/dev/input")).entryList({ QStringLiteral("event*") }, QDir::System);
        for (const QString &name : devices) {
            const QByteArray path = QByteArrayLiteral("/dev/input/") + name.toLatin1();
            const int fd = ::open(path.constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
            if (fd < 0)
                continue;
            unsigned long keys[(KEY_MAX + 1) / (8 * sizeof(unsigned long)) + 1] = {};
            unsigned long axes[(ABS_MAX + 1) / (8 * sizeof(unsigned long)) + 1] = {};
            const bool pad = ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys) >= 0
                          && ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(axes)), axes) >= 0
                          && testBit(keys, BTN_SOUTH) && testBit(axes, ABS_X) && !testBit(keys, KEY_Q);
            if (pad) {
                for (int axis : { ABS_X, ABS_Y }) {
                    input_absinfo info {};
                    if (ioctl(fd, EVIOCGABS(axis), &info) >= 0 && info.maximum > info.minimum) {
                        m_absMin[axis] = info.minimum;
                        m_absMax[axis] = info.maximum;
                    }
                }
                char label[128] = {};
                ioctl(fd, EVIOCGNAME(sizeof(label) - 1), label);
                qInfo("GameOverlay: pad %s (%s)", label, path.constData());
                m_padFd = fd;
                break;
            }
            ::close(fd);
        }
        if (m_padFd < 0)
            return false;
    }
    // Everything that happened since the last tick
    auto scale = [this](int axis, int value) {
        const double mid = (m_absMin[axis] + m_absMax[axis]) / 2.0, half = (m_absMax[axis] - m_absMin[axis]) / 2.0;
        return int(std::clamp((value - mid) / half, -1.0, 1.0) * 32767);
    };
    input_event events[64];
    ssize_t n;
    while ((n = ::read(m_padFd, events, sizeof(events))) > 0) {
        for (ssize_t i = 0; i < n / ssize_t(sizeof(input_event)); ++i) {
            const input_event &e = events[i];
            if (e.type == EV_KEY) {
                unsigned bit = 0;
                switch (e.code) {
                case BTN_SOUTH: bit = PadA; break;
                case BTN_EAST: bit = PadB; break;
                case BTN_NORTH: bit = PadX; break;    // BTN_X — Xbox pads (xpad) report X and Y by their names
                case BTN_WEST: bit = PadY; break;     // BTN_Y
                case BTN_TL: bit = PadLB; break;
                case BTN_TR: bit = PadRB; break;
                case BTN_SELECT: bit = PadView; break;
                case BTN_START: bit = PadMenu; break;
                case BTN_DPAD_UP: bit = PadUp; break;
                case BTN_DPAD_DOWN: bit = PadDown; break;
                case BTN_DPAD_LEFT: bit = PadLeft; break;
                case BTN_DPAD_RIGHT: bit = PadRight; break;
                default: break;
                }
                if (e.value)
                    m_evButtons |= bit;
                else
                    m_evButtons &= ~bit;
            } else if (e.type == EV_ABS) {
                switch (e.code) {
                case ABS_X: m_evX = scale(ABS_X, e.value); break;
                case ABS_Y: m_evY = -scale(ABS_Y, e.value); break;   // evdev: down is positive
                case ABS_HAT0X:
                    m_evHat = (m_evHat & ~(PadLeft | PadRight)) | (e.value < 0 ? PadLeft : e.value > 0 ? PadRight : 0);
                    break;
                case ABS_HAT0Y:
                    m_evHat = (m_evHat & ~(PadUp | PadDown)) | (e.value < 0 ? PadUp : e.value > 0 ? PadDown : 0);
                    break;
                default: break;
                }
            }
        }
    }
    if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        closePad();   // unplugged
        m_nextPadScan = now + 1000;
        return false;
    }
    buttons = m_evButtons | m_evHat;
    x = m_evX;
    y = m_evY;
    return true;
#else
    Q_UNUSED(now)
    Q_UNUSED(buttons)
    Q_UNUSED(x)
    Q_UNUSED(y)
    return false;
#endif
}

void GameOverlay::pollPad()
{
    unsigned buttons = 0;
    int x = 0, y = 0;
    const bool connected = readPad(buttons, x, y);
    if (connected != m_padConnected) {
        m_padConnected = connected;
        emit padConnectedChanged();
    }
    if (!connected)
        return;
    const qint64 now = m_clock.elapsed();
    const unsigned pressed = buttons & ~m_lastButtons;
    m_lastButtons = buttons;

    // View + Menu held for a moment opens / closes the menu.
    const unsigned combo = PadView | PadMenu;
    if ((buttons & combo) == combo) {
        if (m_comboSince < 0)
            m_comboSince = now;
        if (!m_comboFired && now - m_comboSince >= 400) {
            m_comboFired = true;
            toggle();
        }
        return;
    }
    m_comboSince = -1;
    m_comboFired = false;
    if (!m_open)
        return;

    // Directions: the d-pad or the left stick, repeating while held.
    QString direction;
    if (buttons & PadLeft) direction = QStringLiteral("left");
    else if (buttons & PadRight) direction = QStringLiteral("right");
    else if (buttons & PadUp) direction = QStringLiteral("up");
    else if (buttons & PadDown) direction = QStringLiteral("down");
    else {
        constexpr int dead = 16000;
        if (std::abs(x) > std::abs(y) && std::abs(x) > dead)
            direction = x > 0 ? QStringLiteral("right") : QStringLiteral("left");
        else if (std::abs(y) > dead)
            direction = y > 0 ? QStringLiteral("up") : QStringLiteral("down");
    }
    if (direction.isEmpty()) {
        m_held.clear();
    } else if (direction != m_held) {
        m_held = direction;
        m_heldNext = now + 380;
        emit padInput(direction);
    } else if (now >= m_heldNext) {
        m_heldNext = now + 110;
        emit padInput(direction);
    }

    if (pressed & PadA) emit padInput(QStringLiteral("accept"));
    if (pressed & PadB) emit padInput(QStringLiteral("back"));
    if (pressed & PadY) emit padInput(QStringLiteral("playPause"));
    if (pressed & PadLB) emit padInput(QStringLiteral("previousTrack"));
    if (pressed & PadRB) emit padInput(QStringLiteral("nextTrack"));
}
