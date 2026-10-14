#pragma once

#include <QObject>
#include <QString>

struct NowPlaying {
    QString title;
    QString artist;
    QString album;
    QString sourceApp;
    QString query;
    bool valid = false;

    bool operator==(const NowPlaying &other) const
    {
        return valid == other.valid && title == other.title && artist == other.artist
            && sourceApp == other.sourceApp;
    }
    bool operator!=(const NowPlaying &other) const { return !(*this == other); }
};

class QTimer;

class MediaDetector final : public QObject
{
    Q_OBJECT
public:
    enum class MediaKey { PlayPause, Next, Previous, Stop };

    explicit MediaDetector(QObject *parent = nullptr);

    void start(int intervalMs = 800);
    void stop();
    bool isRunning() const;
    void setInterval(int intervalMs);
    void pollNow();
    NowPlaying current() const { return m_current; }

    void triggerMediaKey(MediaKey key);

signals:
    void nowPlayingChanged(const NowPlaying &nowPlaying);
    void playbackActiveChanged(bool active);

private slots:
    void poll();

private:
    QTimer *m_timer = nullptr;
    NowPlaying m_current;
    bool m_playbackActive = false;
};
