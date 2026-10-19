#include "GameOverlay.h"

#include "AppPaths.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QJSEngine>
#include <QKeySequence>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>
#include <QWindow>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <xinput.h>
#endif

#include <cstdlib>

namespace {

constexpr int kHotkeyId = 0x4E4B;   // "NK" — the media keys (WindowsMediaControls) use 1–4
constexpr int kDefaultKey = Qt::Key_M;
constexpr int kDefaultModifiers = int(Qt::ControlModifier | Qt::ShiftModifier);

#ifdef Q_OS_WIN
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
    if (supported()) {
        QCoreApplication::instance()->installNativeEventFilter(this);
        // On the GUI thread's event loop: the hotkey belongs to the thread that registers it.
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
}

bool GameOverlay::supported() const
{
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

QString GameOverlay::hotkey() const
{
    return QKeySequence(QKeyCombination(Qt::KeyboardModifiers(m_modifiers), Qt::Key(m_key))).toString(QKeySequence::NativeText);
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
    if (!m_enabled || m_hotkeyRegistered)
        return;
    UINT mods = MOD_NOREPEAT;
    if (m_modifiers & Qt::ControlModifier) mods |= MOD_CONTROL;
    if (m_modifiers & Qt::ShiftModifier) mods |= MOD_SHIFT;
    if (m_modifiers & Qt::AltModifier) mods |= MOD_ALT;
    if (m_modifiers & Qt::MetaModifier) mods |= MOD_WIN;
    // No window: WM_HOTKEY is posted to this thread's queue (nativeEventFilter sees it).
    m_hotkeyRegistered = RegisterHotKey(nullptr, kHotkeyId, mods, virtualKey(m_key)) != 0;
    m_hotkeyTaken = !m_hotkeyRegistered;
    if (m_hotkeyTaken)
        qWarning("GameOverlay: %s is taken by another app", qPrintable(hotkey()));
#endif
}

void GameOverlay::unregisterHotkey()
{
#ifdef Q_OS_WIN
    if (m_hotkeyRegistered)
        UnregisterHotKey(nullptr, kHotkeyId);
    m_hotkeyRegistered = false;
#endif
}

bool GameOverlay::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *)
{
#ifdef Q_OS_WIN
    if (eventType != "windows_generic_MSG")
        return false;
    const MSG *msg = static_cast<const MSG *>(message);
    if (msg->message == WM_HOTKEY && msg->hwnd == nullptr && int(msg->wParam) == kHotkeyId) {
        toggle();
        return true;
    }
#else
    Q_UNUSED(eventType)
    Q_UNUSED(message)
#endif
    return false;
}

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
    m_previous = 0;
}

void GameOverlay::toggle()
{
    m_open ? hide() : show();
}

void GameOverlay::activate(QWindow *window)
{
    if (!window)
        return;
    // Hidden between uses, it keeps its GPU context and scene: the covers it cached belong to them.
    if (auto *quick = qobject_cast<QQuickWindow *>(window)) {
        quick->setPersistentGraphics(true);
        quick->setPersistentSceneGraph(true);
    }
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

void GameOverlay::pollPad()
{
#ifdef Q_OS_WIN
    const XInputGetStateFn getState = xinputGetState();
    if (!getState)
        return;
    const qint64 now = m_clock.elapsed();
    auto setConnected = [this](bool on) {
        if (on != m_padConnected) {
            m_padConnected = on;
            emit padConnectedChanged();
        }
    };
    XINPUT_STATE state {};
    if (m_padSlot < 0) {
        if (now < m_nextPadScan)
            return;
        m_nextPadScan = now + 3000;
        for (DWORD i = 0; i < XUSER_MAX_COUNT && m_padSlot < 0; ++i)
            if (getState(i, &state) == ERROR_SUCCESS)
                m_padSlot = int(i);
        if (m_padSlot < 0) {
            setConnected(false);
            return;
        }
    } else if (getState(DWORD(m_padSlot), &state) != ERROR_SUCCESS) {
        m_padSlot = -1;
        m_nextPadScan = now + 3000;
        setConnected(false);
        return;
    }
    setConnected(true);

    const unsigned buttons = state.Gamepad.wButtons;
    const unsigned pressed = buttons & ~m_lastButtons;
    m_lastButtons = buttons;

    // View + Menu held for a moment opens / closes the menu.
    const unsigned combo = XINPUT_GAMEPAD_BACK | XINPUT_GAMEPAD_START;
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
    if (buttons & XINPUT_GAMEPAD_DPAD_LEFT) direction = QStringLiteral("left");
    else if (buttons & XINPUT_GAMEPAD_DPAD_RIGHT) direction = QStringLiteral("right");
    else if (buttons & XINPUT_GAMEPAD_DPAD_UP) direction = QStringLiteral("up");
    else if (buttons & XINPUT_GAMEPAD_DPAD_DOWN) direction = QStringLiteral("down");
    else {
        const int x = state.Gamepad.sThumbLX, y = state.Gamepad.sThumbLY;
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

    if (pressed & XINPUT_GAMEPAD_A) emit padInput(QStringLiteral("accept"));
    if (pressed & XINPUT_GAMEPAD_B) emit padInput(QStringLiteral("back"));
    if (pressed & XINPUT_GAMEPAD_Y) emit padInput(QStringLiteral("playPause"));
    if (pressed & XINPUT_GAMEPAD_LEFT_SHOULDER) emit padInput(QStringLiteral("previousTrack"));
    if (pressed & XINPUT_GAMEPAD_RIGHT_SHOULDER) emit padInput(QStringLiteral("nextTrack"));
#endif
}
