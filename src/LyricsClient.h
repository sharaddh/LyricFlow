#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QVector>
#include <QString>

struct LyricLine {
    qint64 milliseconds = 0;
    QString text;
};

struct LyricsResult {
    QString trackName;
    QString artistName;
    QString albumName;
    QString plainLyrics;
    QString syncedLyrics;
    QString sourceUrl;
};

class LyricsClient final : public QObject
{
    Q_OBJECT
public:
    explicit LyricsClient(QObject *parent = nullptr);
    void search(const QString &track, const QString &artist);

signals:
    void searchStarted();
    void searchFinished(const QList<LyricsResult> &results);
    void searchFailed(const QString &message);

private:
    QNetworkAccessManager manager;
};
