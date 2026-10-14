#pragma once

#include <QObject>
#include <QString>
#include <QtGlobal>
#include <cstdint>

// Snapshot of the current system media session.
struct MediaState {
    QString title;
    QString artist;
    QString album;
    QString sourceApp;
    qint64 positionMs = 0;   // playback position within the track
    qint64 durationMs = 0;   // track length (0 if unknown)
    double playbackRate = 1.0;
    int status = 0;          // 0=Closed 1=Opened 2=Changing 3=Stopped 4=Playing 5=Paused
    bool valid = false;

    bool isPlaying() const { return status == 4; }
    bool isPausedOrStopped() const { return status == 5 || status == 3 || status == 0; }

    bool sameTrackAs(const MediaState &other) const
    {
        return title.compare(other.title, Qt::CaseInsensitive) == 0
            && artist.compare(other.artist, Qt::CaseInsensitive) == 0;
    }
};

// Reads the Windows Global System Media Transport Controls session manager so we
// get the exact playback position, pause/play state and rich metadata for the
// song playing in ANY app (Spotify, browsers, local players).
//
// The WinRT API is used through its raw ABI (RoGetActivationFactory + vtable
// calls) so it works with a MinGW build. All blocking work happens on a worker
// thread; results are delivered to the GUI thread through a queued call.
class MediaSession final : public QObject
{
    Q_OBJECT
public:
    explicit MediaSession(QObject *parent = nullptr);
    ~MediaSession() override;

    void start(int intervalMs = 150);
    void stop();

    bool available() const { return m_available; }
    MediaState current() const { return m_current; }

signals:
    void stateChanged(const MediaState &state);
    void availabilityChanged(bool available);

private:
    void deliver(const MediaState &state);
    void reportUnavailable();

    struct Impl;
    Impl *m_impl = nullptr;
    bool m_available = false;
    MediaState m_current;
};
