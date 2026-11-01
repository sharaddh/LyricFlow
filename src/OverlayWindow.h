#pragma once
#include <QWidget>
#include <QPoint>
#include <QList>
#include <QColor>
#include <QBrush>
#include <QFont>
#include "LyricsClient.h"
#include "MediaDetector.h"
#include "MediaSession.h"
#include "SettingsDialog.h"

class QTextDocument;
class QPainter;
class QMouseEvent;
class QWheelEvent;
class QContextMenuEvent;
class QShowEvent;
class QResizeEvent;
class QTimer;
class QSystemTrayIcon;
class QAction;
class QPropertyAnimation;
class QVariantAnimation;

class OverlayWindow final : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal captionScale READ captionScale WRITE setCaptionScale)
    Q_PROPERTY(qreal textOpacity READ textOpacity WRITE setTextOpacity)
    Q_PROPERTY(qreal wordProgress READ wordProgress WRITE setWordProgress)
public:
    explicit OverlayWindow(QWidget *parent = nullptr);

    qreal captionScale() const { return m_captionScale; }
    void setCaptionScale(qreal scale);

    qreal textOpacity() const { return m_textOpacity; }
    void setTextOpacity(qreal opacity);

    qreal wordProgress() const { return m_wordProgress; }
    void setWordProgress(qreal progress);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    // Lyrics are rendered with a QTextDocument we control directly: the very same
    // document is used to measure the wrapped height and to paint it, so the
    // window can never collapse to a single line and clip the text.
    QTextDocument *textDoc = nullptr;
    qreal m_textOpacity = 1.0;
    QPoint dragOffset;
    bool dragging = false;

    QString currentStyle = QStringLiteral("Glass Card");
    QString currentLyrics;
    QString currentSong;
    QString currentArtist;
    QString fontFamily = QStringLiteral("Segoe UI");
    int fontSize = 25;
    int opacityPercent = 88;
    bool fontBold = true;
    bool useCustomColor = false;
    QColor textColor = QColor(255, 255, 255);
    bool showSongInfo = true;
    bool autoDetect = true;
    int pollIntervalMs = 1500;
    bool showTips = true;
    bool startWithWindows = false;

    bool wordByWord = true;
    QString upcomingWords = QStringLiteral("All words");
    bool emphasizeWord = false;
    QString captionStyle = QStringLiteral("Single Word");
    QString animationType = QStringLiteral("Fade");
    int lineSpacing = 45;
    int lyricOffsetMs = 0;
    qreal m_captionScale = 1.0;
    int lastLineIndex = -1;
    int lastWordIndex = -1;
    QPropertyAnimation *fadeAnimation = nullptr;
    QVariantAnimation *scaleAnimation = nullptr;

    // Single-word captions are painted by hand (not through the document) so the
    // active word can slide, scale and cross-fade at 60 fps.
    bool m_wordModeActive = false;
    bool m_lyricsDirty = true;
    QString m_activeWord;
    QString m_previousWord;
    int m_wordLineIndex = -1;
    int m_wordIndex = -1;
    qreal m_wordProgress = 1.0;
    QPropertyAnimation *wordAnimation = nullptr;

    QList<LyricLine> syncedLines;
    QString plainLyrics;
    qint64 elapsedOffset = 0;
    QTimer *syncTimer = nullptr;
    QString resolvedTextColor = QStringLiteral("white");
    QString resolvedAccentColor = QStringLiteral("#bfe2ff");
    int padX = 30;
    int padY = 22;
    QString renderedHtml;
    QString lastRenderedHtml;

    // Card appearance painted in paintEvent. A style-sheet background does not
    // render on a per-pixel-alpha (translucent) top-level window, so the card is
    // drawn directly with QPainter instead.
    QBrush skinFill;
    QColor skinBorderColor = QColor(Qt::transparent);
    int skinBorderWidth = 0;
    int skinRadius = 22;
    bool drawsCard = true;

    MediaSession *mediaSession = nullptr;
    bool mediaSessionActive = false;
    bool mediaPlaying = false;
    bool haveSyncBase = false;
    bool positionTrustworthy = false;
    bool hasLastReport = false;
    qint64 lastReportedMs = 0;
    qint64 syncBaseMs = 0;
    qint64 syncBaseStampMs = 0;
    QString sessionTitle;
    QString sessionArtist;
    QString sessionSourceApp;
    bool sessionPlaying = false;
    QString lastSessionQuery;
    QString currentTrackId;

    MediaDetector *mediaDetector = nullptr;
    LyricsClient *autoClient = nullptr;
    QString lastAutoQuery;
    quint64 autoRequestId = 0;
    QSystemTrayIcon *trayIcon = nullptr;
    QAction *trayNowPlayingAction = nullptr;

    bool songActive = false;
    bool showingTip = false;
    bool geometryRestored = false;
    QTimer *tipTimer = nullptr;

    LyricStyleOptions currentOptions() const;
    void applyOptions(const LyricStyleOptions &options);
    void applyStyle();
    void updateText();
    void openSettings();
    void showStyleMenu();
    void saveSettings();
    void loadSettings();
    void setStyle(const QString &style);
    void findLyrics();
    void loadLyrics(const LyricsResult &result);
    void parseSyncedLyrics(const QString &synced);
    void updateSyncedLine();
    void renderSynced();
    bool usesSingleWordCaptions() const;
    int effectiveFontSize() const;
    int measureContentHeight(int width) const;
    int singleWordContentHeight() const;
    int cardInset() const { return drawsCard ? 8 : 0; }
    QFont captionFont(int pixelSize, bool bold) const;
    void paintSingleWord(QPainter &painter);
    void updateSingleWord(const LyricLine &line, int lineIndex, int wordIndex);
    void restartWordAnimation(int durationMs);
    void applyTextFont();
    void setDocumentHtml(const QString &html);
    void animateCaption();
    void fitToContent();
    void showStartupTip();
    void applyWindowsStartup(bool enabled);
    qint64 currentPlaybackMs() const;
    void onMediaState(const MediaState &state);
    void changeFontSize(int delta);
    void setupTray();
    void updateTrayNowPlaying();
    void handleAutoResults(quint64 token, const QList<LyricsResult> &results);
    LyricsResult pickBest(const QList<LyricsResult> &results, const NowPlaying &np) const;
    void startSong(const QString &title, const QString &artist, const QString &sourceApp, const QString &query);
    inline QString makeTrackId(const QString &title, const QString &artist) const { return (title + QLatin1Char('|') + artist).toLower().trimmed(); }

private slots:
    void onNowPlaying(const NowPlaying &nowPlaying);
};
