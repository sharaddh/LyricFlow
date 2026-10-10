#include "MediaSession.h"

#include <QMetaObject>

#include <atomic>
#include <chrono>
#include <thread>
#include <cstddef>

#ifdef Q_OS_WIN
#include <windows.h>
#include <hstring.h>
#include <inspectable.h>
#endif

namespace {

#ifdef Q_OS_WIN

// ---------------------------------------------------------------------------
// Minimal raw-ABI declarations of the Windows.Media.Control interfaces.
// Layouts match the Windows Runtime ABI exactly (IInspectable + methods in
// declaration order) so vtable calls are valid.
// ---------------------------------------------------------------------------

struct IGlobalSystemMediaTransportControlsSessionManager;
struct IGlobalSystemMediaTransportControlsSession;
struct IGlobalSystemMediaTransportControlsSessionMediaProperties;
struct IGlobalSystemMediaTransportControlsSessionPlaybackInfo;

struct WrtTimeSpan { INT64 Duration; };

template <class T>
struct IWrtAsyncOperation : public IInspectable {
    virtual HRESULT STDMETHODCALLTYPE put_Completed(void *handler) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Completed(void **handler) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetResults(T *result) = 0;
};

struct IWrtAsyncInfo : public IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_Id(UINT32 *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Status(int *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_ErrorCode(HRESULT *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE Cancel() = 0;
    virtual HRESULT STDMETHODCALLTYPE Close() = 0;
};

struct IGlobalSystemMediaTransportControlsSessionManagerStatics : public IInspectable {
    virtual HRESULT STDMETHODCALLTYPE RequestAsync(
        IWrtAsyncOperation<IGlobalSystemMediaTransportControlsSessionManager *> **operation) = 0;
};

struct IGlobalSystemMediaTransportControlsSessionManager : public IInspectable {
    virtual HRESULT STDMETHODCALLTYPE GetCurrentSession(
        IGlobalSystemMediaTransportControlsSession **result) = 0;
};

struct IGlobalSystemMediaTransportControlsSessionTimelineProperties;
struct IGlobalSystemMediaTransportControlsSession : public IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_SourceAppUserModelId(HSTRING *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE TryGetMediaPropertiesAsync(
        IWrtAsyncOperation<IGlobalSystemMediaTransportControlsSessionMediaProperties *> **operation) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetTimelineProperties(
        IGlobalSystemMediaTransportControlsSessionTimelineProperties **result) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPlaybackInfo(
        IGlobalSystemMediaTransportControlsSessionPlaybackInfo **result) = 0;
};

struct IGlobalSystemMediaTransportControlsSessionMediaProperties : public IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_Title(HSTRING *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Subtitle(HSTRING *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_AlbumArtist(HSTRING *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Artist(HSTRING *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_AlbumTitle(HSTRING *value) = 0;
};

struct IGlobalSystemMediaTransportControlsSessionPlaybackInfo : public IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_Controls(void **value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_PlaybackStatus(int *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_PlaybackType(void **value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_AutoRepeatMode(void **value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_PlaybackRate(void **value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsShuffleActive(void **value) = 0;
};

struct IGlobalSystemMediaTransportControlsSessionTimelineProperties : public IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_StartTime(WrtTimeSpan *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_EndTime(WrtTimeSpan *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_MinSeekTime(WrtTimeSpan *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_MaxSeekTime(WrtTimeSpan *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Position(WrtTimeSpan *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_LastUpdatedTime(void *value) = 0;
};

// Well-known IIDs.
const GUID kIIDAsyncInfo = {0x00000036, 0x0000, 0x0000,
                            {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
const GUID kIIDManagerStatics = {0x2050c4ee, 0x11a0, 0x57de,
                                 {0xae, 0xd7, 0xc9, 0x7c, 0x70, 0x33, 0x82, 0x45}};

// Dynamic entry points (resolved from combase.dll) so we never fail to launch.
using PFN_RoInitialize = HRESULT(WINAPI *)(int);
using PFN_RoGetActivationFactory = HRESULT(WINAPI *)(HSTRING, REFIID, void **);
using PFN_WindowsCreateString = HRESULT(WINAPI *)(LPCWSTR, UINT32, HSTRING *);
using PFN_WindowsDeleteString = HRESULT(WINAPI *)(HSTRING);
using PFN_WindowsGetStringRawBuffer = PCWSTR(WINAPI *)(HSTRING, UINT32 *);
using PFN_RoUninitialize = void(WINAPI *)();

struct WinRtApi {
    HMODULE module = nullptr;
    PFN_RoInitialize roInitialize = nullptr;
    PFN_RoUninitialize roUninitialize = nullptr;
    PFN_RoGetActivationFactory roGetActivationFactory = nullptr;
    PFN_WindowsCreateString windowsCreateString = nullptr;
    PFN_WindowsDeleteString windowsDeleteString = nullptr;
    PFN_WindowsGetStringRawBuffer windowsGetStringRawBuffer = nullptr;

    bool load()
    {
        module = LoadLibraryW(L"combase.dll");
        if (!module) return false;
        roInitialize = reinterpret_cast<PFN_RoInitialize>(GetProcAddress(module, "RoInitialize"));
        roUninitialize = reinterpret_cast<PFN_RoUninitialize>(GetProcAddress(module, "RoUninitialize"));
        roGetActivationFactory = reinterpret_cast<PFN_RoGetActivationFactory>(
            GetProcAddress(module, "RoGetActivationFactory"));
        windowsCreateString = reinterpret_cast<PFN_WindowsCreateString>(
            GetProcAddress(module, "WindowsCreateString"));
        windowsDeleteString = reinterpret_cast<PFN_WindowsDeleteString>(
            GetProcAddress(module, "WindowsDeleteString"));
        windowsGetStringRawBuffer = reinterpret_cast<PFN_WindowsGetStringRawBuffer>(
            GetProcAddress(module, "WindowsGetStringRawBuffer"));
        return roInitialize && roUninitialize && roGetActivationFactory && windowsCreateString
            && windowsDeleteString && windowsGetStringRawBuffer;
    }
};

#endif // Q_OS_WIN

} // namespace

// ---------------------------------------------------------------------------
// MediaSession implementation
// ---------------------------------------------------------------------------

struct MediaSession::Impl {
#ifdef Q_OS_WIN
    WinRtApi api;
#endif
    std::atomic<bool> stop{false};
    std::thread worker;
    int intervalMs = 250;
};

MediaSession::MediaSession(QObject *parent) : QObject(parent)
{
    m_impl = new Impl;
}

MediaSession::~MediaSession()
{
    stop();
    delete m_impl;
    m_impl = nullptr;
}

void MediaSession::start(int intervalMs)
{
    stop();
    m_impl->intervalMs = intervalMs;
    m_impl->stop = false;
    m_impl->worker = std::thread([this]() {
#ifdef Q_OS_WIN
        WinRtApi &api = m_impl->api;
        if (!api.load() || FAILED(api.roInitialize(1 /*MTA*/))) {
            QMetaObject::invokeMethod(this, [this]() { reportUnavailable(); }, Qt::QueuedConnection);
            return;
        }

        const auto toQString = [&api](HSTRING value) -> QString {
            if (!value) return {};
            UINT32 length = 0;
            PCWSTR buffer = api.windowsGetStringRawBuffer(value, &length);
            if (!buffer || length == 0) return {};
            return QString::fromWCharArray(buffer, static_cast<int>(length));
        };

        const auto waitAsync = [](auto *operation, auto **result) -> HRESULT {
            if (!operation) return E_POINTER;
            IWrtAsyncInfo *info = nullptr;
            HRESULT hr = operation->QueryInterface(kIIDAsyncInfo, reinterpret_cast<void **>(&info));
            if (FAILED(hr) || !info) {
                hr = operation->GetResults(result);
                operation->Release();
                return hr;
            }
            for (int i = 0; i < 1500; ++i) {
                int status = 0;
                if (FAILED(info->get_Status(&status))) break;
                if (status == 1) break;             // Completed
                if (status == 2 || status == 3) {   // Canceled / Error
                    info->Release();
                    operation->Release();
                    return E_FAIL;
                }
                Sleep(2);
            }
            hr = operation->GetResults(result);
            info->Release();
            operation->Release();
            return hr;
        };

        const wchar_t *className =
            L"Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager";
        HSTRING classId = nullptr;
        if (FAILED(api.windowsCreateString(className, static_cast<UINT32>(wcslen(className)), &classId))) {
            QMetaObject::invokeMethod(this, &MediaSession::reportUnavailable, Qt::QueuedConnection);
            return;
        }

        IGlobalSystemMediaTransportControlsSessionManagerStatics *statics = nullptr;
        HRESULT hr = api.roGetActivationFactory(classId, kIIDManagerStatics,
                                                reinterpret_cast<void **>(&statics));
        api.windowsDeleteString(classId);
        if (FAILED(hr) || !statics) {
            QMetaObject::invokeMethod(this, [this]() { reportUnavailable(); }, Qt::QueuedConnection);
            return;
        }

        IGlobalSystemMediaTransportControlsSessionManager *manager = nullptr;
        IWrtAsyncOperation<IGlobalSystemMediaTransportControlsSessionManager *> *op = nullptr;
        if (SUCCEEDED(statics->RequestAsync(&op)))
            waitAsync(op, &manager);
        statics->Release();
        if (!manager) {
            QMetaObject::invokeMethod(this, [this]() { reportUnavailable(); }, Qt::QueuedConnection);
            return;
        }

        while (!m_impl->stop.load()) {
            MediaState state;
            IGlobalSystemMediaTransportControlsSession *session = nullptr;
            if (SUCCEEDED(manager->GetCurrentSession(&session)) && session) {
                HSTRING appId = nullptr;
                if (SUCCEEDED(session->get_SourceAppUserModelId(&appId))) {
                    state.sourceApp = toQString(appId);
                    api.windowsDeleteString(appId);
                }

                IWrtAsyncOperation<IGlobalSystemMediaTransportControlsSessionMediaProperties *> *propOp = nullptr;
                if (SUCCEEDED(session->TryGetMediaPropertiesAsync(&propOp)) && propOp) {
                    IGlobalSystemMediaTransportControlsSessionMediaProperties *props = nullptr;
                    if (SUCCEEDED(waitAsync(propOp, &props)) && props) {
                        HSTRING title = nullptr, artist = nullptr, album = nullptr;
                        if (SUCCEEDED(props->get_Title(&title))) {
                            state.title = toQString(title);
                            api.windowsDeleteString(title);
                        }
                        if (SUCCEEDED(props->get_Artist(&artist))) {
                            state.artist = toQString(artist);
                            api.windowsDeleteString(artist);
                        }
                        if (SUCCEEDED(props->get_AlbumTitle(&album))) {
                            state.album = toQString(album);
                            api.windowsDeleteString(album);
                        }
                        props->Release();
                    }
                }

                IGlobalSystemMediaTransportControlsSessionPlaybackInfo *playback = nullptr;
                if (SUCCEEDED(session->GetPlaybackInfo(&playback)) && playback) {
                    int status = 0;
                    if (SUCCEEDED(playback->get_PlaybackStatus(&status)))
                        state.status = status;
                    playback->Release();
                }

                IGlobalSystemMediaTransportControlsSessionTimelineProperties *timeline = nullptr;
                if (SUCCEEDED(session->GetTimelineProperties(&timeline)) && timeline) {
                    WrtTimeSpan position{}, start{}, end{};
                    if (SUCCEEDED(timeline->get_Position(&position))
                        && SUCCEEDED(timeline->get_StartTime(&start))
                        && SUCCEEDED(timeline->get_EndTime(&end))) {
                        state.positionMs = (position.Duration - start.Duration) / 10000;
                        state.durationMs = (end.Duration - start.Duration) / 10000;
                    }
                    timeline->Release();
                }

                session->Release();
            }

            state.valid = !state.title.isEmpty();
            if (state.positionMs < 0) state.positionMs = 0;
            if (state.durationMs < 0) state.durationMs = 0;

            QMetaObject::invokeMethod(this, [this, state]() { deliver(state); },
                                      Qt::QueuedConnection);

            for (int waited = 0; waited < m_impl->intervalMs && !m_impl->stop.load(); waited += 25)
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }

        manager->Release();
        api.roUninitialize();
#else
        QMetaObject::invokeMethod(this, [this]() { reportUnavailable(); }, Qt::QueuedConnection);
#endif
    });
}

void MediaSession::stop()
{
    if (!m_impl || !m_impl->worker.joinable()) return;
    m_impl->stop = true;
    m_impl->worker.join();
}

void MediaSession::deliver(const MediaState &state)
{
    const bool nowAvailable = state.valid;
    if (nowAvailable != m_available) {
        m_available = nowAvailable;
        emit availabilityChanged(m_available);
    }
    m_current = state;
    emit stateChanged(state);
}

void MediaSession::reportUnavailable()
{
    if (m_available) {
        m_available = false;
        emit availabilityChanged(false);
    }
}
