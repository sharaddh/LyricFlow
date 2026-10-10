#pragma once
#include <QWidget>
#include <QPoint>
#include <QList>
#include "LyricsClient.h"

class QLabel;
class QMouseEvent;
class QContextMenuEvent;
class QTimer;

class OverlayWindow final : public QWidget
{
    Q_OBJECT
public:
    explicit OverlayWindow(QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QLabel *lyricsLabel = nullptr;
    QPoint dragOffset;
    bool dragging = false;
    QString currentStyle = "Glass Card";
    QString currentLyrics = "Right-click to find a song";
    QString currentSong = "LyricFlow";
    QString currentArtist = "C++ desktop lyrics";
    QString fontFamily = "Segoe UI";
    int fontSize = 25;
    int opacityPercent = 88;
    bool showSongInfo = true;
    QList<LyricLine> syncedLines;
    QString plainLyrics;
    qint64 elapsedOffset = 0;
    QTimer *syncTimer = nullptr;

    void applyStyle();
    void updateText();
    void openSettings();
    void showStyleMenu();
    void saveSettings();
    void loadSettings();
    void setStyle(const QString &style);
    void findLyrics();
    void loadLyrics(const LyricsResult &result);
    void updateSyncedLine();
};
