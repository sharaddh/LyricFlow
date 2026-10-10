#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QList>
#include <QVector>
#include <QString>
#include <functional>

struct LyricWord {
    qint64 milliseconds = 0;
    QString text;
};

struct LyricLine {
    qint64 milliseconds = 0;
    qint64 endMilliseconds = 0;
    QString text;
    QVector<LyricWord> words;
};

struct LyricsResult {
    QString trackName;
    QString artistName;
    QString albumName;
    QString plainLyrics;
    QString syncedLyrics;
    QString sourceUrl;
};

class QUrl;

class LyricsClient final : public QObject
{
    Q_OBJECT
public:
    explicit LyricsClient(QObject *parent = nullptr);

    void search(const QString &track, const QString &artist);
    void searchQuery(const QString &query, quint64 token = 0);
    // Runs a free-text search, then falls back to a track/artist query when the
    // first returns nothing. Transient server errors (429/5xx, timeouts) are
    // retried with a short backoff before giving up.
    void searchAuto(const QString &track, const QString &artist,
                    const QString &query, quint64 token = 0);

signals:
    void searchStarted();
    void searchFinished(const QList<LyricsResult> &results);
    void searchFailed(const QString &message);
    void queryFinished(quint64 token, const QList<LyricsResult> &results);
    void queryFailed(quint64 token, const QString &message);

private:
    using ResultsHandler = std::function<void(const QList<LyricsResult> &results,
                                              const QString &error)>;
    void fetch(const QUrl &url, int attempt, ResultsHandler handler);
    void fetchChain(const QList<QUrl> &urls, int index, quint64 token);

    QNetworkAccessManager manager;
};
