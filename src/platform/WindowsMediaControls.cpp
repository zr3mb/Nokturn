#include "WindowsMediaControls.h"

#include "PlayerController.h"
#include "TrackModel.h"
#include "WinRtMedia.h"

#include <QCoreApplication>
#include <QFile>
#include <QGuiApplication>
#include <QUrl>
#include <QWindow>

#include <roapi.h>
#include <shcore.h>
#include <shlwapi.h>
#include <winstring.h>

#include <algorithm>
#include <atomic>

using namespace winrt_abi;

namespace {

constexpr qint64 kTicksPerMs = 10000;   // TimeSpan in WinRT counts in 100 ns units

// Global hotkey ids (fallback mode without SMTC).
enum HotkeyId : int { HotkeyPlayPause = 0x4E01, HotkeyNext, HotkeyPrevious, HotkeyStop };

class HString
{
public:
    explicit HString(const QString &s)
    {
        WindowsCreateString(reinterpret_cast<const wchar_t *>(s.utf16()), UINT32(s.size()), &m_h);
    }
    ~HString()
    {
        if (m_h)
            WindowsDeleteString(m_h);
    }
    HString(const HString &) = delete;
    HString &operator=(const HString &) = delete;
    HSTRING get() const { return m_h; }

private:
    HSTRING m_h = nullptr;
};

// A minimal COM smart pointer.
template <typename T>
class Com
{
public:
    Com() = default;
    ~Com() { reset(); }
    Com(const Com &) = delete;
    Com &operator=(const Com &) = delete;
    T *get() const { return m_p; }
    T *operator->() const { return m_p; }
    explicit operator bool() const { return m_p != nullptr; }
    T **put()
    {
        reset();
        return &m_p;
    }
    void **putVoid() { return reinterpret_cast<void **>(put()); }
    void reset()
    {
        if (m_p) {
            m_p->Release();
            m_p = nullptr;
        }
    }

private:
    T *m_p = nullptr;
};

template <typename T>
HRESULT activationFactory(const char *className, const IID &iid, Com<T> &out)
{
    const HString name(QString::fromLatin1(className));
    return RoGetActivationFactory(name.get(), iid, out.putVoid());
}

// SMTC event delegates. The system calls them on its own thread — we forward the event to the GUI thread.
template <typename Base, typename Args>
class Delegate : public Base
{
public:
    Delegate(WindowsMediaControls *target, const IID &iid) : m_target(target), m_iid(iid) {}
    virtual ~Delegate() = default;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **out) override
    {
        if (!out)
            return E_POINTER;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, m_iid) || IsEqualIID(riid, IID_AgileObject)) {
            *out = static_cast<Base *>(this);
            this->AddRef();
            return S_OK;
        }
        *out = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++m_refs; }
    ULONG STDMETHODCALLTYPE Release() override
    {
        const ULONG refs = --m_refs;
        if (refs == 0)
            delete this;
        return refs;
    }

protected:
    template <typename F>
    void post(F &&fn)
    {
        QPointer<WindowsMediaControls> target = m_target;
        QMetaObject::invokeMethod(QCoreApplication::instance(), [target, fn] {
            if (target)
                fn(target.data());
        }, Qt::QueuedConnection);
    }

private:
    std::atomic<ULONG> m_refs { 1 };
    QPointer<WindowsMediaControls> m_target;
    IID m_iid;
};

class ButtonHandler final : public Delegate<IButtonPressedHandler, IButtonPressedEventArgs>
{
public:
    explicit ButtonHandler(WindowsMediaControls *target) : Delegate(target, IID_ButtonPressedHandler) {}

    HRESULT STDMETHODCALLTYPE Invoke(IInspectable *, IButtonPressedEventArgs *args) override
    {
        SmtcButton button;
        if (args && SUCCEEDED(args->get_Button(&button)))
            post([button](WindowsMediaControls *c) { c->handleButton(button); });
        return S_OK;
    }
};

class SeekHandler final : public Delegate<IPositionRequestedHandler, IPositionRequestedEventArgs>
{
public:
    explicit SeekHandler(WindowsMediaControls *target) : Delegate(target, IID_PositionRequestedHandler) {}

    HRESULT STDMETHODCALLTYPE Invoke(IInspectable *, IPositionRequestedEventArgs *args) override
    {
        TimeSpan position;
        if (args && SUCCEEDED(args->get_RequestedPlaybackPosition(&position))) {
            const qint64 ms = position.duration / kTicksPerMs;
            post([ms](WindowsMediaControls *c) { c->handleSeek(ms); });
        }
        return S_OK;
    }
};

} // namespace

struct WindowsMediaControls::Impl
{
    Com<ISmtc> smtc;
    Com<ISmtc2> smtc2;
    Com<IDisplayUpdater> updater;
    Com<ITimelineProperties> timeline;
    Com<IStreamReferenceStatics> streams;
    Com<IUriFactory> uris;
    EventToken buttonToken {};
    EventToken seekToken {};
    bool buttonsHooked = false;
    bool seekHooked = false;
};

WindowsMediaControls::WindowsMediaControls(PlayerController *player)
    : QObject(player)
    , m_player(player)
    , d(new Impl)
{
    // The window only exists after QML loads — we hook in once it's visible.
    m_attachTimer.setInterval(300);
    connect(&m_attachTimer, &QTimer::timeout, this, &WindowsMediaControls::tryAttach);
    m_attachTimer.start();

    // The timeline position in the system flyout isn't extrapolated — we refresh it every few seconds.
    m_timelineTimer.setInterval(5000);
    connect(&m_timelineTimer, &QTimer::timeout, this, &WindowsMediaControls::updateTimeline);
}

WindowsMediaControls::~WindowsMediaControls()
{
    if (d->smtc && d->buttonsHooked)
        d->smtc->remove_ButtonPressed(d->buttonToken);
    if (d->smtc2 && d->seekHooked)
        d->smtc2->remove_PlaybackPositionChangeRequested(d->seekToken);
    unregisterHotkeys();
    delete d;
}

void WindowsMediaControls::tryAttach()
{
    QWindow *window = nullptr;
    for (QWindow *w : QGuiApplication::topLevelWindows()) {
        if (w->isVisible() && w->type() == Qt::Window) {
            window = w;
            break;
        }
    }
    if (!window)
        return;
    m_attachTimer.stop();
    const HWND hwnd = reinterpret_cast<HWND>(window->winId());

    Com<ISmtcInterop> interop;
    HRESULT hr = activationFactory("Windows.Media.SystemMediaTransportControls", IID_SmtcInterop, interop);
    if (SUCCEEDED(hr))
        hr = interop->GetForWindow(hwnd, IID_Smtc, d->smtc.putVoid());
    if (FAILED(hr) || !d->smtc) {
        qWarning("SMTC unavailable (0x%08lx) — media keys as global hotkeys", static_cast<unsigned long>(hr));
        d->smtc.reset();
        registerHotkeys(quintptr(hwnd));
        return;
    }

    d->smtc->put_IsEnabled(true);
    d->smtc->put_IsPlayEnabled(true);
    d->smtc->put_IsPauseEnabled(true);
    d->smtc->put_IsStopEnabled(true);
    d->smtc->put_IsNextEnabled(true);
    d->smtc->put_IsPreviousEnabled(true);

    auto *buttons = new ButtonHandler(this);
    d->buttonsHooked = SUCCEEDED(d->smtc->add_ButtonPressed(buttons, &d->buttonToken));
    buttons->Release();
    if (!d->buttonsHooked)
        registerHotkeys(quintptr(hwnd));   // the flyout shows the track, and we handle the keys ourselves

    d->smtc->get_DisplayUpdater(d->updater.put());
    if (SUCCEEDED(d->smtc->QueryInterface(IID_Smtc2, d->smtc2.putVoid())) && d->smtc2) {
        auto *seek = new SeekHandler(this);
        d->seekHooked = SUCCEEDED(d->smtc2->add_PlaybackPositionChangeRequested(seek, &d->seekToken));
        seek->Release();

        Com<IInspectable> instance;
        const HString cls(QStringLiteral("Windows.Media.SystemMediaTransportControlsTimelineProperties"));
        if (SUCCEEDED(RoActivateInstance(cls.get(), instance.put())) && instance)
            instance->QueryInterface(IID_TimelineProperties, d->timeline.putVoid());
    }
    activationFactory("Windows.Storage.Streams.RandomAccessStreamReference", IID_StreamReferenceStatics, d->streams);
    activationFactory("Windows.Foundation.Uri", IID_UriFactory, d->uris);

    PlayerController *p = m_player;
    if (!p)
        return;
    connect(p, &PlayerController::currentChanged, this, [this] {
        updateMetadata();
        updateStatus();
        updateTimeline();
    });
    connect(p, &PlayerController::playingChanged, this, [this] {
        updateStatus();
        updateTimeline();
    });
    connect(p, &PlayerController::durationChanged, this, &WindowsMediaControls::updateTimeline);
    connect(p, &PlayerController::seeked, this, &WindowsMediaControls::updateTimeline);
    connect(p, &PlayerController::shuffleChanged, this, &WindowsMediaControls::updateStatus);
    connect(p, &PlayerController::repeatModeChanged, this, &WindowsMediaControls::updateStatus);

    updateMetadata();
    updateStatus();
    updateTimeline();
}

void WindowsMediaControls::updateMetadata()
{
    if (!d->updater)
        return;
    const Track *t = m_player ? m_player->currentTrack() : nullptr;
    if (!t) {
        d->updater->ClearAll();
        d->updater->Update();
        return;
    }

    d->updater->put_Type(Music);
    Com<IMusicDisplayProperties> music;
    if (SUCCEEDED(d->updater->get_MusicProperties(music.put())) && music) {
        music->put_Title(HString(t->title).get());
        music->put_Artist(HString(t->artist).get());
        music->put_AlbumArtist(HString(t->albumArtist.isEmpty() ? t->artist : t->albumArtist).get());
        Com<IMusicDisplayProperties2> music2;
        if (SUCCEEDED(music->QueryInterface(IID_MusicProperties2, music2.putVoid())) && music2) {
            music2->put_AlbumTitle(HString(t->album).get());
            music2->put_TrackNumber(UINT32(std::max(0, t->trackNo)));
        }
    }

    // Cover: an https URL (YouTube Music) or a file from the cover cache — through an in-memory stream,
    // because SMTC in a regular desktop app doesn't read file:// URLs.
    Com<IInspectable> thumbnail;
    if (d->streams) {
        if (t->cover.startsWith(QLatin1String("https://")) && d->uris) {
            Com<IInspectable> uri;
            if (SUCCEEDED(d->uris->CreateUri(HString(t->cover).get(), uri.put())) && uri)
                d->streams->CreateFromUri(uri.get(), thumbnail.put());
        } else if (t->cover.startsWith(QLatin1String("file:"))) {
            QFile file(QUrl(t->cover).toLocalFile());
            if (file.open(QIODevice::ReadOnly) && file.size() < 16 * 1024 * 1024) {
                const QByteArray bytes = file.readAll();
                if (IStream *memory = SHCreateMemStream(reinterpret_cast<const BYTE *>(bytes.constData()), UINT(bytes.size()))) {
                    Com<IInspectable> stream;
                    if (SUCCEEDED(CreateRandomAccessStreamOverStream(memory, BSOS_DEFAULT, IID_RandomAccessStream, stream.putVoid())) && stream)
                        d->streams->CreateFromStream(stream.get(), thumbnail.put());
                    memory->Release();
                }
            }
        }
    }
    d->updater->put_Thumbnail(thumbnail.get());
    d->updater->Update();
}

void WindowsMediaControls::updateStatus()
{
    if (!d->smtc || !m_player)
        return;
    const MediaPlaybackStatus status = !m_player->hasTrack() ? Closed : m_player->playing() ? Playing : Paused;
    d->smtc->put_PlaybackStatus(status);
    if (d->smtc2) {
        d->smtc2->put_ShuffleEnabled(m_player->shuffle());
        const int repeat = m_player->repeatMode();
        d->smtc2->put_AutoRepeatMode(repeat == 2 ? RepeatTrack : repeat == 1 ? RepeatList : RepeatNone);
    }
    if (status == Playing)
        m_timelineTimer.start();
    else
        m_timelineTimer.stop();
}

void WindowsMediaControls::updateTimeline()
{
    if (!d->smtc2 || !d->timeline || !m_player || !m_player->hasTrack())
        return;
    const qint64 durationMs = qint64(m_player->duration()) * 1000;
    if (durationMs <= 0)
        return;
    const qint64 positionMs = std::clamp<qint64>(qint64(m_player->position() * 1000), 0, durationMs);
    d->timeline->put_StartTime({ 0 });
    d->timeline->put_EndTime({ durationMs * kTicksPerMs });
    d->timeline->put_MinSeekTime({ 0 });
    d->timeline->put_MaxSeekTime({ durationMs * kTicksPerMs });
    d->timeline->put_Position({ positionMs * kTicksPerMs });
    d->smtc2->UpdateTimelineProperties(d->timeline.get());
}

void WindowsMediaControls::handleButton(int button)
{
    PlayerController *p = m_player;
    if (!p)
        return;
    switch (button) {
    case ButtonPlay: p->play(); break;
    case ButtonPause: p->pause(); break;
    case ButtonStop: p->pause(); p->seek(0); break;
    case ButtonNext: p->next(); break;
    case ButtonPrevious: p->previous(); break;
    default: break;
    }
}

void WindowsMediaControls::handleSeek(qint64 positionMs)
{
    if (m_player)
        m_player->seek(positionMs / 1000.0);
}

// ---- fallback mode: global hotkeys for the media keys ----------------------------

void WindowsMediaControls::registerHotkeys(quintptr hwnd)
{
    if (m_hotkeyWindow)
        return;
    const HWND window = reinterpret_cast<HWND>(hwnd);
    bool any = false;
    any |= RegisterHotKey(window, HotkeyPlayPause, MOD_NOREPEAT, VK_MEDIA_PLAY_PAUSE) != 0;
    any |= RegisterHotKey(window, HotkeyNext, MOD_NOREPEAT, VK_MEDIA_NEXT_TRACK) != 0;
    any |= RegisterHotKey(window, HotkeyPrevious, MOD_NOREPEAT, VK_MEDIA_PREV_TRACK) != 0;
    any |= RegisterHotKey(window, HotkeyStop, MOD_NOREPEAT, VK_MEDIA_STOP) != 0;
    if (!any)
        return;   // keys taken by another app
    m_hotkeyWindow = hwnd;
    QCoreApplication::instance()->installNativeEventFilter(this);
}

void WindowsMediaControls::unregisterHotkeys()
{
    if (!m_hotkeyWindow)
        return;
    const HWND window = reinterpret_cast<HWND>(m_hotkeyWindow);
    for (int id : { HotkeyPlayPause, HotkeyNext, HotkeyPrevious, HotkeyStop })
        UnregisterHotKey(window, id);
    if (QCoreApplication::instance())
        QCoreApplication::instance()->removeNativeEventFilter(this);
    m_hotkeyWindow = 0;
}

bool WindowsMediaControls::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *)
{
    if (eventType != "windows_generic_MSG" || !m_player)
        return false;
    const MSG *msg = static_cast<const MSG *>(message);
    if (msg->message != WM_HOTKEY || quintptr(msg->hwnd) != m_hotkeyWindow)
        return false;
    switch (int(msg->wParam)) {
    case HotkeyPlayPause: m_player->toggle(); return true;
    case HotkeyNext: m_player->next(); return true;
    case HotkeyPrevious: m_player->previous(); return true;
    case HotkeyStop: m_player->pause(); m_player->seek(0); return true;
    default: return false;
    }
}
