#pragma once

// Minimalne deklaracje ABI (WinRT) dla System Media Transport Controls — tylko to, czego używa
// WindowsMediaControls. Własne deklaracje zamiast windows.media.h, bo nagłówki mingw-w64 nie
// zawsze zawierają SMTC. Kolejność metod i identyfikatory (IID) zgodne z Windows SDK
// (windows.media.idl, windows.storage.streams.idl, windows.foundation.idl); identyfikatory
// delegatów wyliczone algorytmem parametryzowanych interfejsów WinRT (UUID v5 z sygnatury).

#include <windows.h>
#include <inspectable.h>

namespace winrt_abi {

using boolean_t = unsigned char;
struct EventToken { INT64 value; };
struct TimeSpan { INT64 duration; };   // w jednostkach 100 ns

enum MediaPlaybackStatus : int { Closed = 0, Changing = 1, Stopped = 2, Playing = 3, Paused = 4 };
enum MediaPlaybackType : int { Unknown = 0, Music = 1, Video = 2, Image = 3 };
enum MediaPlaybackAutoRepeatMode : int { RepeatNone = 0, RepeatTrack = 1, RepeatList = 2 };
enum SmtcButton : int {
    ButtonPlay = 0, ButtonPause = 1, ButtonStop = 2, ButtonRecord = 3, ButtonFastForward = 4,
    ButtonRewind = 5, ButtonNext = 6, ButtonPrevious = 7, ButtonChannelUp = 8, ButtonChannelDown = 9,
};

// {ddb0472d-c911-4a1f-86d9-dc3d71a95f5a}
inline constexpr IID IID_SmtcInterop = { 0xddb0472d, 0xc911, 0x4a1f, { 0x86, 0xd9, 0xdc, 0x3d, 0x71, 0xa9, 0x5f, 0x5a } };
// {99fa3ff4-1742-42a6-902e-087d41f965ec}
inline constexpr IID IID_Smtc = { 0x99fa3ff4, 0x1742, 0x42a6, { 0x90, 0x2e, 0x08, 0x7d, 0x41, 0xf9, 0x65, 0xec } };
// {ea98d2f6-7f3c-4af2-a586-72889808efb1}
inline constexpr IID IID_Smtc2 = { 0xea98d2f6, 0x7f3c, 0x4af2, { 0xa5, 0x86, 0x72, 0x88, 0x98, 0x08, 0xef, 0xb1 } };
// {8abbc53e-fa55-4ecf-ad8e-c984e5dd1550}
inline constexpr IID IID_DisplayUpdater = { 0x8abbc53e, 0xfa55, 0x4ecf, { 0xad, 0x8e, 0xc9, 0x84, 0xe5, 0xdd, 0x15, 0x50 } };
// {6bbf0c59-d0a0-4d26-92a0-f978e1d18e7b}
inline constexpr IID IID_MusicProperties = { 0x6bbf0c59, 0xd0a0, 0x4d26, { 0x92, 0xa0, 0xf9, 0x78, 0xe1, 0xd1, 0x8e, 0x7b } };
// {00368462-97d3-44b9-b00f-008afcefaf18}
inline constexpr IID IID_MusicProperties2 = { 0x00368462, 0x97d3, 0x44b9, { 0xb0, 0x0f, 0x00, 0x8a, 0xfc, 0xef, 0xaf, 0x18 } };
// {b7f47116-a56f-4dc8-9e11-92031f4a87c2}
inline constexpr IID IID_ButtonPressedArgs = { 0xb7f47116, 0xa56f, 0x4dc8, { 0x9e, 0x11, 0x92, 0x03, 0x1f, 0x4a, 0x87, 0xc2 } };
// {b4493f88-eb28-4961-9c14-335e44f3e125}
inline constexpr IID IID_PositionRequestedArgs = { 0xb4493f88, 0xeb28, 0x4961, { 0x9c, 0x14, 0x33, 0x5e, 0x44, 0xf3, 0xe1, 0x25 } };
// {5125316a-c3a2-475b-8507-93534dc88f15}
inline constexpr IID IID_TimelineProperties = { 0x5125316a, 0xc3a2, 0x475b, { 0x85, 0x07, 0x93, 0x53, 0x4d, 0xc8, 0x8f, 0x15 } };
// TypedEventHandler<SystemMediaTransportControls*, SystemMediaTransportControlsButtonPressedEventArgs*>
// {0557e996-7b23-5bae-aa81-ea0d671143a4}
inline constexpr IID IID_ButtonPressedHandler = { 0x0557e996, 0x7b23, 0x5bae, { 0xaa, 0x81, 0xea, 0x0d, 0x67, 0x11, 0x43, 0xa4 } };
// TypedEventHandler<SystemMediaTransportControls*, PlaybackPositionChangeRequestedEventArgs*>
// {44e34f15-bdc0-50a7-ace4-39e91fb753f1}
inline constexpr IID IID_PositionRequestedHandler = { 0x44e34f15, 0xbdc0, 0x50a7, { 0xac, 0xe4, 0x39, 0xe9, 0x1f, 0xb7, 0x53, 0xf1 } };
// {857309dc-3fbf-4e7d-986f-ef3b1a07a964}
inline constexpr IID IID_StreamReferenceStatics = { 0x857309dc, 0x3fbf, 0x4e7d, { 0x98, 0x6f, 0xef, 0x3b, 0x1a, 0x07, 0xa9, 0x64 } };
// {905a0fe1-bc53-11df-8c49-001e4fc686da}
inline constexpr IID IID_RandomAccessStream = { 0x905a0fe1, 0xbc53, 0x11df, { 0x8c, 0x49, 0x00, 0x1e, 0x4f, 0xc6, 0x86, 0xda } };
// {44a9796f-723e-4fdf-a218-033e75b0c084}
inline constexpr IID IID_UriFactory = { 0x44a9796f, 0x723e, 0x4fdf, { 0xa2, 0x18, 0x03, 0x3e, 0x75, 0xb0, 0xc0, 0x84 } };
// {94ea2b94-e9cc-49e0-c0ff-ee64ca8f5b90}
inline constexpr IID IID_AgileObject = { 0x94ea2b94, 0xe9cc, 0x49e0, { 0xc0, 0xff, 0xee, 0x64, 0xca, 0x8f, 0x5b, 0x90 } };

struct IMusicDisplayProperties : IInspectable
{
    virtual HRESULT STDMETHODCALLTYPE get_Title(HSTRING *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Title(HSTRING value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_AlbumArtist(HSTRING *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_AlbumArtist(HSTRING value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Artist(HSTRING *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Artist(HSTRING value) = 0;
};

struct IMusicDisplayProperties2 : IInspectable
{
    virtual HRESULT STDMETHODCALLTYPE get_AlbumTitle(HSTRING *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_AlbumTitle(HSTRING value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_TrackNumber(UINT32 *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_TrackNumber(UINT32 value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Genres(IInspectable **value) = 0;
};

struct IDisplayUpdater : IInspectable
{
    virtual HRESULT STDMETHODCALLTYPE get_Type(MediaPlaybackType *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Type(MediaPlaybackType value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_AppMediaId(HSTRING *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_AppMediaId(HSTRING value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Thumbnail(IInspectable **value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Thumbnail(IInspectable *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_MusicProperties(IMusicDisplayProperties **value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_VideoProperties(IInspectable **value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_ImageProperties(IInspectable **value) = 0;
    virtual HRESULT STDMETHODCALLTYPE CopyFromFileAsync(MediaPlaybackType type, IInspectable *source, IInspectable **operation) = 0;
    virtual HRESULT STDMETHODCALLTYPE ClearAll() = 0;
    virtual HRESULT STDMETHODCALLTYPE Update() = 0;
};

struct IButtonPressedEventArgs : IInspectable
{
    virtual HRESULT STDMETHODCALLTYPE get_Button(SmtcButton *value) = 0;
};

struct IPositionRequestedEventArgs : IInspectable
{
    virtual HRESULT STDMETHODCALLTYPE get_RequestedPlaybackPosition(TimeSpan *value) = 0;
};

// Delegaty zdarzeń (IUnknown + Invoke).
struct IButtonPressedHandler : IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE Invoke(IInspectable *sender, IButtonPressedEventArgs *args) = 0;
};

struct IPositionRequestedHandler : IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE Invoke(IInspectable *sender, IPositionRequestedEventArgs *args) = 0;
};

struct ISmtc : IInspectable
{
    virtual HRESULT STDMETHODCALLTYPE get_PlaybackStatus(MediaPlaybackStatus *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_PlaybackStatus(MediaPlaybackStatus value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_DisplayUpdater(IDisplayUpdater **value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_SoundLevel(int *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsEnabled(boolean_t *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsEnabled(boolean_t value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsPlayEnabled(boolean_t *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsPlayEnabled(boolean_t value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsStopEnabled(boolean_t *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsStopEnabled(boolean_t value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsPauseEnabled(boolean_t *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsPauseEnabled(boolean_t value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsRecordEnabled(boolean_t *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsRecordEnabled(boolean_t value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsFastForwardEnabled(boolean_t *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsFastForwardEnabled(boolean_t value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsRewindEnabled(boolean_t *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsRewindEnabled(boolean_t value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsPreviousEnabled(boolean_t *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsPreviousEnabled(boolean_t value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsNextEnabled(boolean_t *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsNextEnabled(boolean_t value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsChannelUpEnabled(boolean_t *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsChannelUpEnabled(boolean_t value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsChannelDownEnabled(boolean_t *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsChannelDownEnabled(boolean_t value) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_ButtonPressed(IButtonPressedHandler *handler, EventToken *token) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_ButtonPressed(EventToken token) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_PropertyChanged(IUnknown *handler, EventToken *token) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_PropertyChanged(EventToken token) = 0;
};

struct ITimelineProperties : IInspectable
{
    virtual HRESULT STDMETHODCALLTYPE get_StartTime(TimeSpan *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_StartTime(TimeSpan value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_EndTime(TimeSpan *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_EndTime(TimeSpan value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_MinSeekTime(TimeSpan *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_MinSeekTime(TimeSpan value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_MaxSeekTime(TimeSpan *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_MaxSeekTime(TimeSpan value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Position(TimeSpan *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Position(TimeSpan value) = 0;
};

struct ISmtc2 : IInspectable
{
    virtual HRESULT STDMETHODCALLTYPE get_AutoRepeatMode(MediaPlaybackAutoRepeatMode *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_AutoRepeatMode(MediaPlaybackAutoRepeatMode value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_ShuffleEnabled(boolean_t *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_ShuffleEnabled(boolean_t value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_PlaybackRate(DOUBLE *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_PlaybackRate(DOUBLE value) = 0;
    virtual HRESULT STDMETHODCALLTYPE UpdateTimelineProperties(ITimelineProperties *properties) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_PlaybackPositionChangeRequested(IPositionRequestedHandler *handler, EventToken *token) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_PlaybackPositionChangeRequested(EventToken token) = 0;
    // dalsze zdarzenia (PlaybackRate/Shuffle/AutoRepeatChangeRequested) nie są używane
};

struct ISmtcInterop : IInspectable
{
    virtual HRESULT STDMETHODCALLTYPE GetForWindow(HWND window, REFIID riid, void **control) = 0;
};

struct IStreamReferenceStatics : IInspectable
{
    virtual HRESULT STDMETHODCALLTYPE CreateFromFile(IInspectable *file, IInspectable **reference) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateFromUri(IInspectable *uri, IInspectable **reference) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateFromStream(IInspectable *stream, IInspectable **reference) = 0;
};

struct IUriFactory : IInspectable
{
    virtual HRESULT STDMETHODCALLTYPE CreateUri(HSTRING uri, IInspectable **instance) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateWithRelativeUri(HSTRING base, HSTRING relative, IInspectable **instance) = 0;
};

} // namespace winrt_abi
