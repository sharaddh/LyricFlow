#include "OverlayWindow.h"
#include "SettingsDialog.h"
#include "LyricsSearchDialog.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QCursor>
#include <QDir>
#include <QFont>
#include <QIcon>
#include <QGuiApplication>
#include <QLinearGradient>
#include <QMenu>
#include <QPainter>
#include <QPaintEvent>
#include <QPixmap>
#include <QStringList>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QScreen>
#include <QSettings>
#include <QShortcut>
#include <QAction>
#include <QKeyEvent>
#include <QTimer>
#include <QMessageBox>
#include <QRegularExpression>
#include <QDateTime>
#include <QtMath>
#include <QTextDocument>
#include <QResizeEvent>
#include <QSystemTrayIcon>
#include <QPropertyAnimation>
#include <QVariantAnimation>
#include <QEasingCurve>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

QIcon makeAppIcon()
{
    return QIcon(QStringLiteral(":/app.ico"));
}

#ifdef Q_OS_WIN

// Keep the overlay out of the taskbar and Alt+Tab list (it lives in the system
// tray instead) by turning it into a tool window.
void hideFromTaskbar(HWND hwnd)
{
    if (!hwnd) return;
    LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    ex |= WS_EX_TOOLWINDOW;
    ex &= ~static_cast<LONG_PTR>(WS_EX_APPWINDOW);
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, ex);
}

#endif

bool windowsRunEntryPresent()
{
#ifdef Q_OS_WIN
    QSettings run(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                  QSettings::NativeFormat);
    return run.contains(QStringLiteral("Capson")) || run.contains(QStringLiteral("LyricFlow"));
#else
    return false;
#endif
}

} // namespace

OverlayWindow::OverlayWindow(QWidget *parent) : QWidget(parent)
{
    setWindowTitle(QStringLiteral("Capson"));
    setObjectName(QStringLiteral("overlayRoot"));
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    // Translucency must be requested BEFORE the native window is created;
    // otherwise the window is created opaque and the painted card never shows.
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground, true);
#ifdef Q_OS_WIN
    hideFromTaskbar(reinterpret_cast<HWND>(winId()));
#endif
    setMinimumSize(260, 90);
    resize(660, 170);
    setFocusPolicy(Qt::StrongFocus);
    setWindowIcon(makeAppIcon());

    // The lyrics are laid out and painted through this document. It is measured
    // (for the window height) and painted (in paintEvent) from the same source,
    // so wrapping and the reserved height always agree.
    textDoc = new QTextDocument(this);
    textDoc->setDocumentMargin(0);
    textDoc->setTextWidth(qMax(160, width() - 2 * padX));

    syncTimer = new QTimer(this);
    syncTimer->setInterval(80);
    connect(syncTimer, &QTimer::timeout, this, &OverlayWindow::updateSyncedLine);

    fadeAnimation = new QPropertyAnimation(this, "textOpacity", this);
    fadeAnimation->setDuration(180);
    fadeAnimation->setEasingCurve(QEasingCurve::OutCubic);

    scaleAnimation = new QVariantAnimation(this);
    scaleAnimation->setDuration(180);
    scaleAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(scaleAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        setCaptionScale(value.toReal());
    });

    // Real playback position / play-pause state from the OS media session so the
    // lyrics stay in sync when the user pauses, seeks or jumps in the track.
    mediaSession = new MediaSession(this);
    connect(mediaSession, &MediaSession::stateChanged, this, &OverlayWindow::onMediaState);
    connect(mediaSession, &MediaSession::availabilityChanged, this, [this](bool available) {
        if (!available) mediaSessionActive = false;
    });

    // Automatic song detection.
    mediaDetector = new MediaDetector(this);
    autoClient = new LyricsClient(this);
    connect(mediaDetector, &MediaDetector::nowPlayingChanged, this, &OverlayWindow::onNowPlaying);
    connect(autoClient, &LyricsClient::queryFinished, this, &OverlayWindow::handleAutoResults);
    connect(autoClient, &LyricsClient::queryFailed, this, [this](quint64 token, const QString &message) {
        if (token != autoRequestId) return;
        currentLyrics = message;
        updateText();
    });

    tipTimer = new QTimer(this);
    tipTimer->setSingleShot(true);
    connect(tipTimer, &QTimer::timeout, this, [this]() { showingTip = false; updateText(); });

    loadSettings();
    updateText();
    applyStyle();

    // Only fall back to the default top-centre spot on the very first run;
    // otherwise reopen exactly where the user left the overlay.
    if (!geometryRestored) {
        if (auto *screen = QApplication::primaryScreen()) {
            const QRect area = screen->availableGeometry();
            move(area.x() + (area.width() - width()) / 2, area.y() + 35);
        }
    }

    setupTray();
    applyWindowsStartup(startWithWindows);
    if (showTips) showStartupTip();

    auto *settingsShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+,")), this);
    connect(settingsShortcut, &QShortcut::activated, this, &OverlayWindow::openSettings);
    auto *styleShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+S")), this);
    connect(styleShortcut, &QShortcut::activated, this, &OverlayWindow::showStyleMenu);
    auto *hideShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+H")), this);
    connect(hideShortcut, &QShortcut::activated, this, [this]() { setVisible(!isVisible()); });
    auto *lyricsShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+L")), this);
    connect(lyricsShortcut, &QShortcut::activated, this, &OverlayWindow::findLyrics);

    // Quick font sizing.
    for (const QString &key : {QStringLiteral("Ctrl+="), QStringLiteral("Ctrl++"),
                               QStringLiteral("Ctrl+Shift+=")}) {
        auto *bigger = new QShortcut(QKeySequence(key), this);
        connect(bigger, &QShortcut::activated, this, [this]() { changeFontSize(2); });
    }
    auto *smaller = new QShortcut(QKeySequence(QStringLiteral("Ctrl+-")), this);
    connect(smaller, &QShortcut::activated, this, [this]() { changeFontSize(-2); });
    auto *resetSize = new QShortcut(QKeySequence(QStringLiteral("Ctrl+0")), this);
    connect(resetSize, &QShortcut::activated, this, [this]() {
        fontSize = 25;
        applyStyle();
        updateText();
        saveSettings();
    });

    mediaDetector->start(qBound(250, pollIntervalMs, 5000));
    mediaSession->start(150);

    // Make sure the last position is remembered even when quitting from the tray.
    connect(qApp, &QApplication::aboutToQuit, this, &OverlayWindow::saveSettings);
}

void OverlayWindow::updateText()
{
    lastRenderedHtml.clear();

    if (showingTip) {
        const int tipSize = qMax(12, fontSize * 55 / 100);
        setDocumentHtml(
            QStringLiteral("<div style='text-align:center; color:%1; font-size:%2px; opacity:0.9;'>"
                           "Right-click or double-click the overlay for options.<br>"
                           "Drag to move it · scroll to resize the text.</div>")
                .arg(resolvedTextColor)
                .arg(tipSize));
        return;
    }

    // Nothing is playing: keep the overlay completely blank.
    if (!songActive) {
        textDoc->setHtml(QString());
        renderedHtml.clear();
        lastRenderedHtml.clear();
        fitToContent();
        return;
    }

    if (!syncedLines.isEmpty() && syncTimer->isActive()) {
        renderSynced();
        return;
    }

    QString text;
    if (showSongInfo) {
        const int infoSize = qMax(11, fontSize * 45 / 100);
        text = QStringLiteral("<div style='text-align:center; color:%1;'>"
                              "<div style='font-size:%2px; letter-spacing:1px; opacity:0.85;'>%3 — %4</div>"
                              "<div style='height:8px'></div>"
                              "<div style='line-height:150%'>%5</div></div>")
                   .arg(resolvedTextColor)
                   .arg(infoSize)
                   .arg(currentSong.toHtmlEscaped(), currentArtist.toHtmlEscaped(),
                        currentLyrics.toHtmlEscaped());
    } else {
        text = QStringLiteral("<div style='text-align:center; color:%1; line-height:150%'>%2</div>")
                   .arg(resolvedTextColor, currentLyrics.toHtmlEscaped());
    }
    setDocumentHtml(text);
}

void OverlayWindow::applyStyle()
{
    const StyleSkin skin =
        SettingsDialog::styleSkin(currentStyle, opacityPercent);

    // Custom colour overrides only the text; the accent is the skin's highlight
    // colour, so the current word keeps the look of the chosen skin.
    const QString color = useCustomColor ? textColor.name() : skin.textColor;
    resolvedTextColor = color;
    resolvedAccentColor = useCustomColor ? color : skin.accentColor;
    padX = skin.padX;
    padY = skin.padY;

    skinFill = skin.fill;
    skinBorderColor = skin.borderColor;
    skinBorderWidth = skin.borderWidth;
    skinRadius = skin.radius;
    drawsCard = skin.drawsCard;

    applyTextFont();
    update();
    fitToContent();
}

// Paints the rounded, translucent card behind the lyrics. This is done by hand
// because a style-sheet background is not rendered on a per-pixel-alpha window.
void OverlayWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (drawsCard) {
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        if (skinBorderWidth > 0 && skinBorderColor.alpha() > 0)
            painter.setPen(QPen(skinBorderColor, skinBorderWidth));
        else
            painter.setPen(Qt::NoPen);

        if (skinFill.style() == Qt::LinearGradientPattern && skinFill.gradient()) {
            const auto *src = static_cast<const QLinearGradient *>(skinFill.gradient());
            QLinearGradient g(rect().topLeft(), rect().bottomRight());
            for (const auto &stop : src->stops())
                g.setColorAt(stop.first, stop.second);
            painter.setBrush(g);
        } else {
            painter.setBrush(skinFill);
        }

        painter.drawRoundedRect(r, skinRadius, skinRadius);
    }

    // Draw the lyrics. The document already wrapped itself to exactly this width,
    // so its height matches the height fitToContent() reserved for it.
    if (textDoc && !textDoc->isEmpty()) {
        const int contentWidth = qMax(160, width() - 2 * padX);
        textDoc->setTextWidth(contentWidth);
        const qreal docH = textDoc->size().height();
        const qreal areaTop = padY;
        const qreal areaH = qMax(qreal(0), height() - 2.0 * padY);
        const qreal y = areaTop + qMax(qreal(0), (areaH - docH) / 2.0);

        painter.save();
        painter.setOpacity(m_textOpacity);
        painter.translate(padX, y);
        textDoc->drawContents(&painter, QRectF(0, 0, contentWidth, docH));
        painter.restore();
    }
}

int OverlayWindow::effectiveFontSize() const
{
    return qMax(6, qRound(fontSize * m_captionScale));
}

void OverlayWindow::applyTextFont()
{
    QFont f(fontFamily, effectiveFontSize());
    f.setBold(fontBold);
    // Fall back to fonts that actually carry Indic (Devanagari) and symbol
    // glyphs so Hindi lyrics and their vowel marks render instead of vanishing.
    f.setFamilies({fontFamily, QStringLiteral("Nirmala UI"), QStringLiteral("Segoe UI"),
                   QStringLiteral("Segoe UI Symbol"), QStringLiteral("Segoe UI Emoji")});
    textDoc->setDefaultFont(f);
    if (!syncedLines.isEmpty() && syncTimer->isActive())
        renderSynced();
}

void OverlayWindow::setTextOpacity(qreal opacity)
{
    m_textOpacity = opacity;
    update();
}

void OverlayWindow::setCaptionScale(qreal scale)
{
    if (qFuzzyCompare(m_captionScale, scale)) return;
    m_captionScale = scale;
    applyTextFont();
}

void OverlayWindow::animateCaption()
{
    if (animationType == QLatin1String("None") || !fadeAnimation) {
        setTextOpacity(1.0);
        return;
    }

    fadeAnimation->stop();
    setTextOpacity(0.2);
    fadeAnimation->setStartValue(0.2);
    fadeAnimation->setEndValue(1.0);
    fadeAnimation->start();

    if (animationType == QLatin1String("Pop") && scaleAnimation) {
        scaleAnimation->stop();
        scaleAnimation->setStartValue(0.9);
        scaleAnimation->setEndValue(1.0);
        scaleAnimation->start();
    }
}

void OverlayWindow::changeFontSize(int delta)
{
    fontSize = qBound(10, fontSize + delta, 120);
    applyStyle();
    updateText();
    saveSettings();
}

LyricStyleOptions OverlayWindow::currentOptions() const
{
    LyricStyleOptions options;
    options.style = currentStyle;
    options.font = fontFamily;
    options.fontSize = fontSize;
    options.opacity = opacityPercent;
    options.bold = fontBold;
    options.customColor = useCustomColor;
    options.textColor = textColor;
    options.showSongInfo = showSongInfo;
    options.autoDetect = autoDetect;
    options.pollIntervalMs = pollIntervalMs;
    options.wordByWord = wordByWord;
    options.upcomingWords = upcomingWords;
    options.captionStyle = captionStyle;
    options.emphasizeWord = emphasizeWord;
    options.animation = animationType;
    options.lineSpacing = lineSpacing;
    options.showTips = showTips;
    options.startWithWindows = startWithWindows;
    return options;
}

void OverlayWindow::applyOptions(const LyricStyleOptions &options)
{
    currentStyle = options.style;
    fontFamily = options.font;
    fontSize = options.fontSize;
    opacityPercent = options.opacity;
    fontBold = options.bold;
    useCustomColor = options.customColor;
    textColor = options.textColor;
    showSongInfo = options.showSongInfo;
    autoDetect = options.autoDetect;
    pollIntervalMs = options.pollIntervalMs;
    wordByWord = options.wordByWord;
    upcomingWords = options.upcomingWords;
    emphasizeWord = options.emphasizeWord;
    captionStyle = options.captionStyle;
    animationType = options.animation;
    lineSpacing = options.lineSpacing;
    showTips = options.showTips;
    startWithWindows = options.startWithWindows;

    applyWindowsStartup(startWithWindows);
    mediaDetector->setInterval(pollIntervalMs);
    applyStyle();
    updateText();
    saveSettings();
}

void OverlayWindow::setStyle(const QString &style)
{
    currentStyle = style;
    applyStyle();
    saveSettings();
}

void OverlayWindow::showStyleMenu()
{
    QMenu menu(this);
    for (const QString &style : SettingsDialog::availableStyles()) {
        QAction *action = menu.addAction(style);
        action->setCheckable(true);
        action->setChecked(style == currentStyle);
        connect(action, &QAction::triggered, this, [this, style]() { setStyle(style); });
    }
    menu.addSeparator();
    menu.addAction(QStringLiteral("Find lyrics…  (Ctrl+Shift+L)"), this, &OverlayWindow::findLyrics);
    menu.addAction(QStringLiteral("Customize…  (Ctrl+,)"), this, &OverlayWindow::openSettings);
    menu.addAction(QStringLiteral("Quit Capson"), qApp, &QApplication::quit);
    menu.exec(QCursor::pos());
}

void OverlayWindow::openSettings()
{
    SettingsDialog dialog(currentOptions(), this);
    if (dialog.exec() == QDialog::Accepted)
        applyOptions(dialog.options());
}

void OverlayWindow::findLyrics()
{
    LyricsSearchDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Find lyrics"));
    if (dialog.exec() == QDialog::Accepted && dialog.hasSelection()) {
        lastAutoQuery.clear();
        loadLyrics(dialog.selectedResult());
    }
}

void OverlayWindow::loadLyrics(const LyricsResult &result)
{
    songActive = true;
    if (showingTip) {
        showingTip = false;
        if (tipTimer) tipTimer->stop();
    }
    currentSong = result.trackName.isEmpty() ? QStringLiteral("Unknown song") : result.trackName;
    currentArtist = result.artistName.isEmpty() ? QStringLiteral("Unknown artist") : result.artistName;
    currentTrackId = makeTrackId(currentSong, currentArtist);
    plainLyrics = result.plainLyrics;
    syncedLines.clear();
    parseSyncedLyrics(result.syncedLyrics);

    if (!syncedLines.isEmpty()) {
        elapsedOffset = QDateTime::currentMSecsSinceEpoch();
        haveSyncBase = true;
        syncBaseMs = 0;
        syncBaseStampMs = QDateTime::currentMSecsSinceEpoch();
        positionTrustworthy = false;
        hasLastReport = false;
        lastLineIndex = -10;
        lastWordIndex = -10;
        m_captionScale = 1.0;
        setTextOpacity(1.0);
        if (!syncTimer->isActive()) syncTimer->start();
        currentLyrics = syncedLines.isEmpty() ? QString() : syncedLines.first().text;
    } else {
        syncTimer->stop();
        currentLyrics = plainLyrics.isEmpty()
            ? QStringLiteral("No lyrics text is available for this result.")
            : plainLyrics;
    }
    updateText();
    fitToContent();
    updateTrayNowPlaying();
    saveSettings();
}

// Grows the window height so wrapped lyrics and the song info always fit,
// keeping the user's chosen width. Without this the last line could be cut off.
void OverlayWindow::fitToContent()
{
    // Wrap the document at exactly the width it will be painted at (window minus
    // padding) so the measured height and the painted height can never disagree.
    const int contentWidth = qMax(160, width() - 2 * padX);
    const int docHeight = measureContentHeight(contentWidth);
    const int target = qMax(minimumHeight(), docHeight + 2 * padY + 6);
    if (target != height())
        resize(width(), target);
    update();
}

int OverlayWindow::measureContentHeight(int width) const
{
    if (!textDoc)
        return 0;
    textDoc->setTextWidth(qMax(160, width));
    return qCeil(textDoc->size().height());
}

// Sets the rich text shown by the overlay and re-fits the window height to it.
void OverlayWindow::setDocumentHtml(const QString &html)
{
    renderedHtml = html;
    lastRenderedHtml = html;
    if (textDoc) {
        textDoc->setHtml(html);
        textDoc->setTextWidth(qMax(160, width() - 2 * padX));
    }
    fitToContent();
}

void OverlayWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (textDoc) {
        textDoc->setTextWidth(qMax(160, width() - 2 * padX));
        update();
    }
}

void OverlayWindow::showStartupTip()
{
    showingTip = true;
    updateText();
    if (tipTimer) tipTimer->start(9000);
}

void OverlayWindow::applyWindowsStartup(bool enabled)
{
#ifdef Q_OS_WIN
    QSettings run(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                  QSettings::NativeFormat);
    if (enabled) {
        const QString path = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        run.setValue(QStringLiteral("Capson"), QStringLiteral("\"%1\"").arg(path));
        run.remove(QStringLiteral("LyricFlow")); // clean up legacy entry
    } else {
        run.remove(QStringLiteral("Capson"));
    }
#else
    Q_UNUSED(enabled);
#endif
}

void OverlayWindow::updateSyncedLine()
{
    if (syncedLines.isEmpty()) return;
    renderSynced();
}

// Parses standard ("[mm:ss.xx]line") and enhanced ("[mm:ss.xx]<mm:ss.xx>word")
// LRC. When only line timestamps exist the words of each line are distributed
// across the line's duration proportional to their length so we can highlight
// them word by word.
void OverlayWindow::parseSyncedLyrics(const QString &synced)
{
    static const QRegularExpression lineRegex(
        QStringLiteral(R"(^\s*\[(\d{2}):(\d{2})(?:[.:](\d{1,3}))?\]\s*(.*)$)"));
    static const QRegularExpression wordTag(
        QStringLiteral(R"(<(\d{1,2}):(\d{2})(?:[.:](\d{1,3}))?>)"));

    const auto fractionToMs = [](QString fraction) -> qint64 {
        if (fraction.isEmpty()) return 0;
        if (fraction.size() == 1) fraction += QStringLiteral("00");
        else if (fraction.size() == 2) fraction += QStringLiteral("0");
        else fraction = fraction.left(3);
        return fraction.toLongLong();
    };

    struct RawLine { qint64 ms; QString content; };
    QVector<RawLine> raw;

    const QStringList lines = synced.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        auto match = lineRegex.match(line);
        if (!match.hasMatch()) continue;
        const qint64 ms = (match.captured(1).toLongLong() * 60 + match.captured(2).toLongLong()) * 1000
            + fractionToMs(match.captured(3));
        raw.append({ms, match.captured(4).trimmed()});
    }

    for (int i = 0; i < raw.size(); ++i) {
        const RawLine &entry = raw.at(i);
        LyricLine out;
        out.milliseconds = entry.ms;
        out.endMilliseconds = (i + 1 < raw.size()) ? raw.at(i + 1).ms : entry.ms + 4000;

        // Exact word timing via enhanced-LRC tags when the provider supplies them.
        QVector<LyricWord> words;
        auto it = wordTag.globalMatch(entry.content);
        QVector<QRegularExpressionMatch> tags;
        while (it.hasNext()) tags.append(it.next());
        if (!tags.isEmpty()) {
            for (int t = 0; t < tags.size(); ++t) {
                const auto &tag = tags.at(t);
                const qint64 ms = (tag.captured(1).toLongLong() * 60 + tag.captured(2).toLongLong()) * 1000
                    + fractionToMs(tag.captured(3));
                const int start = tag.capturedEnd();
                const int end = (t + 1 < tags.size()) ? tags.at(t + 1).capturedStart()
                                                      : entry.content.size();
                const QString text = entry.content.mid(start, end - start).trimmed();
                if (!text.isEmpty()) words.append({ms, text});
            }
        }

        if (words.isEmpty()) {
            const QStringList parts = entry.content.split(QRegularExpression(QStringLiteral(R"(\s+)")),
                                                          Qt::SkipEmptyParts);
            if (!parts.isEmpty()) {
                QVector<int> weights;
                int total = 0;
                for (const QString &part : parts) {
                    const int weight = qMax(1, part.size());
                    weights.append(weight);
                    total += weight;
                }
                qint64 span = out.endMilliseconds - out.milliseconds;
                if (span <= 0) span = static_cast<qint64>(total) * 300;
                qint64 acc = 0;
                for (int p = 0; p < parts.size(); ++p) {
                    const qint64 when = out.milliseconds + span * acc / total;
                    words.append({when, parts.at(p)});
                    acc += weights.at(p);
                }
            }
        }

        for (const LyricWord &word : words) {
            if (!out.text.isEmpty()) out.text += QLatin1Char(' ');
            out.text += word.text;
        }
        if (out.text.isEmpty()) out.text = entry.content;
        out.words = words;
        syncedLines.append(out);
    }
}

qint64 OverlayWindow::currentPlaybackMs() const
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (mediaSessionActive && haveSyncBase && positionTrustworthy)
        return syncBaseMs + (mediaPlaying ? now - syncBaseStampMs : 0);
    return now - elapsedOffset;
}

void OverlayWindow::onMediaState(const MediaState &state)
{
    if (!state.valid || state.title.isEmpty()) {
        mediaSessionActive = false;
        sessionTitle.clear();
        sessionArtist.clear();
        sessionSourceApp.clear();
        sessionPlaying = false;
        return;
    }

    // Remember the OS-reported song so we can fall back to it when window-title
    // detection cannot see the playing app (e.g. a background browser tab).
    sessionTitle = state.title;
    sessionArtist = state.artist;
    sessionSourceApp = state.sourceApp;
    sessionPlaying = state.isPlaying();

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (state.positionMs > 0 || state.durationMs > 0)
        positionTrustworthy = true;

    if (!mediaSessionActive || !haveSyncBase) {
        syncBaseMs = state.positionMs;
        syncBaseStampMs = now;
        haveSyncBase = true;
    } else if (!hasLastReport || state.positionMs != lastReportedMs || qAbs(state.positionMs - lastReportedMs) > 1000) {
        // A fresh report: trust it. Big jumps are seeks; small ones are the
        // normal tick. Either way we re-anchor so the overlay tracks exactly.
        syncBaseMs = state.positionMs;
        syncBaseStampMs = now;
    }
    // else: the player kept reporting the same position - keep our own clock running.

    hasLastReport = true;
    lastReportedMs = state.positionMs;
    mediaPlaying = state.isPlaying();
    mediaSessionActive = true;

    if (!syncedLines.isEmpty()) {
        renderSynced();
    }
}

void OverlayWindow::renderSynced()
{
    if (syncedLines.isEmpty()) return;

    const qint64 pos = currentPlaybackMs();
    int lineIndex = 0;
    if (syncedLines.isEmpty()) return;
    for (int i = 0; i < syncedLines.size(); ++i) {
        if (syncedLines.at(i).milliseconds <= pos) lineIndex = i;
        else break;
    }
    if (lineIndex >= syncedLines.size()) lineIndex = syncedLines.size() - 1;
    const LyricLine &line = syncedLines.at(lineIndex);

    int wordIndex = -1;
    for (int w = 0; w < line.words.size(); ++w) {
        if (line.words.at(w).milliseconds <= pos) wordIndex = w;
        else break;
    }

    QColor base(resolvedTextColor);
    if (!base.isValid()) base = QColor(Qt::white);
    base.setAlpha(255);
    QColor accent(resolvedAccentColor);
    if (!accent.isValid()) accent = base;
    accent.setAlpha(255);
    const auto css = [](const QColor &c, int alpha) {
        return QStringLiteral("rgba(%1,%2,%3,%4)")
            .arg(c.red()).arg(c.green()).arg(c.blue())
            .arg(QString::number(alpha / 255.0, 'f', 3));
    };
    const QString curCss = accent.name();
    const QString doneCss = css(base, 150);
    const QString upcomingCss = css(base, 80);
    const QString pillCss = css(accent, 80);

    const int gap = qMax(0, effectiveFontSize() * lineSpacing / 100);

    const bool hideUpcoming = (upcomingWords == QLatin1String("Hidden"));
    const bool nextOnly = (upcomingWords == QLatin1String("Next word"));

    const int fs = effectiveFontSize();
    const bool classic = captionStyle.isEmpty() || captionStyle == QLatin1String("Classic");
    const bool story = captionStyle == QLatin1String("Instagram Story");
    const bool bigSmall = captionStyle == QLatin1String("Big & Small");
    const bool spotlight = captionStyle == QLatin1String("Spotlight");
    const bool karaoke = captionStyle == QLatin1String("Karaoke Glow");
    const QString weight = fontBold ? QStringLiteral("bold") : QStringLiteral("normal");

    // Base style for the lyric block. Presets that mix font sizes reserve the
    // tallest possible line via line-height so the card never jumps word to word.
    QString blockStyle =
        QStringLiteral("text-align:center; color:%1; line-height:150%").arg(resolvedTextColor);

    QString lineHtml;
    if (spotlight) {
        // Only the word being sung, big and centered, changing quickly.
        int w = wordIndex;
        if (w < 0 && !line.words.isEmpty()) w = 0;
        const QString word = (!line.words.isEmpty() && w >= 0 && w < line.words.size())
                                 ? line.words.at(w).text
                                 : line.text;
        blockStyle = QStringLiteral("text-align:center; color:%1; line-height:150%; font-size:%2px")
                         .arg(resolvedTextColor)
                         .arg(qRound(fs * 1.9));
        if (!word.isEmpty())
            lineHtml = QStringLiteral("<span style=\"color:%1; font-weight:%2;\">%3</span>")
                           .arg(curCss, weight, word.toHtmlEscaped());
    } else if (!classic && !line.words.isEmpty()) {
        // Instagram Story / Big & Small / Karaoke Glow: the whole line stays laid
        // out (unseen words are transparent) so wrapping and height stay stable.
        QStringList pieces;
        for (int w = 0; w < line.words.size(); ++w) {
            const bool sung = (w <= wordIndex);
            const bool current = (w == wordIndex);
            QString text = line.words.at(w).text.toHtmlEscaped();
            if (story) text = text.toUpper();

            const bool reveal = story ? sung : true;   // Story reveals as it is sung.
            if (!reveal) {
                pieces << QStringLiteral("<span style=\"color:transparent;\">%1</span>").arg(text);
                continue;
            }

            QString style;
            if (story) {
                style = current
                            ? QStringLiteral("color:%1; background-color:%2; font-weight:bold;")
                                  .arg(curCss, pillCss)
                            : QStringLiteral("color:%1; font-weight:bold;").arg(css(base, 150));
            } else if (bigSmall) {
                style = current
                            ? QStringLiteral("color:%1; font-weight:%2;").arg(curCss, weight)
                            : QStringLiteral("color:%1; font-size:%2px;")
                                  .arg(sung ? doneCss : upcomingCss)
                                  .arg(qRound(fs * 0.7));
            } else { // Karaoke Glow
                style = current
                            ? QStringLiteral("color:%1; font-size:%2px; font-weight:%3;")
                                  .arg(curCss).arg(qRound(fs * 1.15)).arg(weight)
                            : QStringLiteral("color:%1;").arg(sung ? doneCss : upcomingCss);
            }
            pieces << QStringLiteral("<span style=\"%1\">%2</span>").arg(style, text);
        }
        lineHtml = pieces.join(QLatin1Char(' '));

        if (bigSmall)
            blockStyle = QStringLiteral("text-align:center; color:%1; line-height:150%; font-size:%2px")
                             .arg(resolvedTextColor)
                             .arg(qRound(fs * 1.7));
        else if (karaoke)
            blockStyle = QStringLiteral("text-align:center; color:%1; line-height:175%")
                             .arg(resolvedTextColor);
        else // Instagram Story
            blockStyle = QStringLiteral("text-align:center; color:%1; line-height:165%")
                             .arg(resolvedTextColor);
    } else if (wordByWord && !line.words.isEmpty()) {
        QStringList pieces;
        for (int w = 0; w < line.words.size(); ++w) {
            const bool sung = (w <= wordIndex);
            const bool reveal = sung || (!hideUpcoming && (!nextOnly || w == wordIndex + 1));
            const QString text = line.words.at(w).text.toHtmlEscaped();
            if (!reveal) {
                // Not shown yet, but keep its space (transparent) so the line
                // measures and wraps exactly like the full line. Otherwise the
                // wrapped height grows as words appear and the bottom of the
                // overlay clips the text.
                pieces << QStringLiteral("<span style=\"color:transparent;\">%1</span>").arg(text);
                continue;
            }
            if (emphasizeWord && w == wordIndex) {
                pieces << QStringLiteral("<span style=\"color:%1; background-color:%2;\">"
                                         "\u2009%3\u2009</span>")
                              .arg(curCss, pillCss, text);
            } else if (w == wordIndex) {
                pieces << QStringLiteral("<span style=\"color:%1;\">%2</span>")
                              .arg(curCss, text);
            } else {
                pieces << QStringLiteral("<span style=\"color:%1;\">%2</span>")
                              .arg(sung ? doneCss : upcomingCss, text);
            }
        }
        lineHtml = pieces.join(QLatin1Char(' '));
    } else if (!line.text.isEmpty()) {
        lineHtml = QStringLiteral("<span style=\"color:%1;\">%2</span>")
                       .arg(curCss, line.text.toHtmlEscaped());
    }

    // The generous line-height reserves room for tall Indic vowel marks (matras)
    // so the first line is not clipped at the top of the overlay.
    QString html;
    if (showSongInfo) {
        const int infoSize = qMax(11, fs * 45 / 100);
        html = QStringLiteral("<div style='text-align:center; color:%1; font-size:%2px; "
                              "letter-spacing:1px; opacity:0.85;'>"
                              "%3 — %4</div><div style='height:%5px'></div>"
                              "<div style='%6'>%7</div>")
                   .arg(resolvedTextColor)
                   .arg(infoSize)
                   .arg(currentSong.toHtmlEscaped(), currentArtist.toHtmlEscaped())
                   .arg(gap)
                   .arg(blockStyle, lineHtml);
    } else {
        html = QStringLiteral("<div style='%1'>%2</div>").arg(blockStyle, lineHtml);
    }

    currentLyrics = line.text;
    const bool lineChanged = (lineIndex != lastLineIndex);
    const bool wordChanged = (wordIndex != lastWordIndex);
    lastLineIndex = lineIndex;
    lastWordIndex = wordIndex;
    if (html != lastRenderedHtml) {
        setDocumentHtml(html);
    }
    if (spotlight ? wordChanged : lineChanged) {
        animateCaption();
    }
}

LyricsResult OverlayWindow::pickBest(const QList<LyricsResult> &results, const NowPlaying &np) const
{
    LyricsResult best;
    int bestScore = -1;
    for (const LyricsResult &result : results) {
        int score = 0;
        if (!result.syncedLyrics.isEmpty()) score += 2;
        if (!np.artist.isEmpty()) {
            if (result.artistName.contains(np.artist, Qt::CaseInsensitive)) score += 4;
            else if (np.artist.contains(result.artistName, Qt::CaseInsensitive)) score += 3;
        }
        if (!np.title.isEmpty() && result.trackName.contains(np.title, Qt::CaseInsensitive)) score += 4;
        if (score > bestScore) {
            bestScore = score;
            best = result;
        }
    }
    return best;
}

void OverlayWindow::handleAutoResults(quint64 token, const QList<LyricsResult> &results)
{
    if (token != autoRequestId) return; // stale response
    if (results.isEmpty()) {
        currentLyrics = QStringLiteral("No lyrics found for \"%1\".").arg(currentSong);
        syncedLines.clear();
        syncTimer->stop();
        updateText();
        return;
    }
    loadLyrics(pickBest(results, mediaDetector->current()));
}

void OverlayWindow::startSong(const QString &title, const QString &artist,
                              const QString &sourceApp, const QString &query)
{
    if (showingTip) {
        showingTip = false;
        if (tipTimer) tipTimer->stop();
    }
        const QString tid = makeTrackId(title, artist);
        if (songActive && tid == currentTrackId && !syncedLines.isEmpty()) {
            currentLyrics = QStringLiteral("Searching lyrics for \"%1\"...").arg(title);
            updateText();
            const quint64 token = ++autoRequestId;
            autoClient->searchAuto(title, artist, query, token);
            return;
        }

    songActive = true;
    currentTrackId = tid;
    lastAutoQuery = query;
    currentSong = title;
    currentArtist = artist.isEmpty() ? sourceApp : artist;
    syncedLines.clear();
    syncTimer->stop();
    plainLyrics.clear();
            haveSyncBase = false;
            positionTrustworthy = false;
            hasLastReport = false;
            lastLineIndex = -1;
            lastWordIndex = -1;
            m_captionScale = 1.0;
            setTextOpacity(1.0);
            currentTrackId.clear();
            syncedLines.clear();
            syncTimer->stop();
            updateText();
    updateTrayNowPlaying();

    const quint64 token = ++autoRequestId;
    autoClient->searchAuto(title, artist, query, token);
}

void OverlayWindow::onNowPlaying(const NowPlaying &np)
{
    if (!autoDetect) {
        updateTrayNowPlaying();
        return;
    }

    // Prefer MediaSession when playing as it's more reliable and stable
    if (sessionPlaying && !sessionTitle.isEmpty()) {
        const QString query = sessionArtist.isEmpty()
            ? sessionTitle
            : sessionArtist + QStringLiteral(" ") + sessionTitle;
        const QString tid = makeTrackId(sessionTitle, sessionArtist);
        if (tid != currentTrackId || !songActive) {
            startSong(sessionTitle, sessionArtist, sessionSourceApp, query);
            return;
        }
        updateTrayNowPlaying();
        return;
    }

    // Window-title detection
    if (np.valid) {
        const QString tid = makeTrackId(np.title, np.artist);
        if (tid == currentTrackId && songActive) {
            updateTrayNowPlaying();
            return;
        }
        startSong(np.title, np.artist, np.sourceApp, np.query);
        return;
    }

    // Playback stopped / nothing detected: wipe the overlay clean.
    if (songActive || !lastAutoQuery.isEmpty()) {
        songActive = false;
        lastAutoQuery.clear();
        syncedLines.clear();
        syncTimer->stop();
        plainLyrics.clear();
        currentSong.clear();
        currentArtist.clear();
        currentLyrics.clear();
        haveSyncBase = false;
        positionTrustworthy = false;
        hasLastReport = false;
        lastLineIndex = -1;
        lastWordIndex = -1;
        m_captionScale = 1.0;
        setTextOpacity(1.0);
        currentTrackId.clear();
        syncedLines.clear();
        syncTimer->stop();
        updateText();
    }
    updateTrayNowPlaying();
}

void OverlayWindow::setupTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) return;

    trayIcon = new QSystemTrayIcon(makeAppIcon(), this);
    trayIcon->setToolTip(QStringLiteral("Capson"));

    auto *menu = new QMenu(this);
    trayNowPlayingAction = menu->addAction(QStringLiteral("Nothing playing"));
    trayNowPlayingAction->setEnabled(false);
    menu->addSeparator();
    menu->addAction(QStringLiteral("Find lyrics…"), this, &OverlayWindow::findLyrics);
    menu->addAction(QStringLiteral("Customize…"), this, &OverlayWindow::openSettings);

    auto *stylesMenu = menu->addMenu(QStringLiteral("Style"));
    for (const QString &style : SettingsDialog::availableStyles()) {
        QAction *action = stylesMenu->addAction(style);
        action->setCheckable(true);
        action->setChecked(style == currentStyle);
        connect(action, &QAction::triggered, this, [this, style]() { setStyle(style); });
    }

    menu->addSeparator();
    menu->addAction(QStringLiteral("Play / Pause"), this, [this]() {
        mediaDetector->triggerMediaKey(MediaDetector::MediaKey::PlayPause);
    });
    menu->addAction(QStringLiteral("Next track"), this, [this]() {
        mediaDetector->triggerMediaKey(MediaDetector::MediaKey::Next);
    });
    menu->addAction(QStringLiteral("Previous track"), this, [this]() {
        mediaDetector->triggerMediaKey(MediaDetector::MediaKey::Previous);
    });
    menu->addSeparator();
    menu->addAction(QStringLiteral("Show / Hide overlay"), this, [this]() { setVisible(!isVisible()); });
    menu->addAction(QStringLiteral("Quit Capson"), qApp, &QApplication::quit);

    trayIcon->setContextMenu(menu);
    connect(trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
            setVisible(!isVisible());
    });
    trayIcon->show();
    updateTrayNowPlaying();
}

void OverlayWindow::updateTrayNowPlaying()
{
    const QString nowPlaying = (currentSong.isEmpty() || currentSong == QLatin1String("Capson"))
        ? QStringLiteral("Nothing playing")
        : (currentArtist.isEmpty() ? currentSong
                                   : currentSong + QStringLiteral(" — ") + currentArtist);
    if (trayNowPlayingAction)
        trayNowPlayingAction->setText(nowPlaying);
    if (trayIcon)
        trayIcon->setToolTip(QStringLiteral("Capson — ") + nowPlaying);
}

void OverlayWindow::contextMenuEvent(QContextMenuEvent *event)
{
    showStyleMenu();
    event->accept();
}

void OverlayWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
#ifdef Q_OS_WIN
    // Re-apply in case Qt recreated the native window; keeps it out of the
    // taskbar and Alt+Tab on every show.
    hideFromTaskbar(reinterpret_cast<HWND>(winId()));
#endif
}

void OverlayWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        dragging = true;
        dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void OverlayWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - dragOffset);
        event->accept();
    }
}

void OverlayWindow::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    dragging = false;
    saveSettings();
}

void OverlayWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        openSettings();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void OverlayWindow::wheelEvent(QWheelEvent *event)
{
    const int steps = event->angleDelta().y() / 120;
    if (steps != 0) {
        changeFontSize(steps * 2);
        event->accept();
        return;
    }
    QWidget::wheelEvent(event);
}

void OverlayWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) { showStyleMenu(); event->accept(); return; }
    QWidget::keyPressEvent(event);
}

void OverlayWindow::saveSettings()
{
    QSettings settings;
    settings.setValue(QStringLiteral("overlay/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("style"), currentStyle);
    settings.setValue(QStringLiteral("fontFamily"), fontFamily);
    settings.setValue(QStringLiteral("fontSize"), fontSize);
    settings.setValue(QStringLiteral("opacity"), opacityPercent);
    settings.setValue(QStringLiteral("bold"), fontBold);
    settings.setValue(QStringLiteral("customColor"), useCustomColor);
    settings.setValue(QStringLiteral("textColor"), textColor.name());
    settings.setValue(QStringLiteral("showSongInfo"), showSongInfo);
    settings.setValue(QStringLiteral("autoDetect"), autoDetect);
    settings.setValue(QStringLiteral("pollInterval"), pollIntervalMs);
    settings.setValue(QStringLiteral("wordByWord"), wordByWord);
    settings.setValue(QStringLiteral("upcomingWords"), upcomingWords);
    settings.setValue(QStringLiteral("captionStyle"), captionStyle);
    settings.setValue(QStringLiteral("emphasizeWord"), emphasizeWord);
    settings.setValue(QStringLiteral("captionAnimation"), animationType);
    settings.setValue(QStringLiteral("lineSpacing"), lineSpacing);
    settings.setValue(QStringLiteral("showTips"), showTips);
    settings.setValue(QStringLiteral("startWithWindows"), startWithWindows);
}

void OverlayWindow::loadSettings()
{
    QSettings settings;
    currentStyle = settings.value(QStringLiteral("style"), currentStyle).toString();
    fontFamily = settings.value(QStringLiteral("fontFamily"), fontFamily).toString();
    fontSize = settings.value(QStringLiteral("fontSize"), fontSize).toInt();
    opacityPercent = settings.value(QStringLiteral("opacity"), opacityPercent).toInt();
    fontBold = settings.value(QStringLiteral("bold"), fontBold).toBool();
    useCustomColor = settings.value(QStringLiteral("customColor"), useCustomColor).toBool();
    textColor = QColor(settings.value(QStringLiteral("textColor"), textColor.name()).toString());
    showSongInfo = settings.value(QStringLiteral("showSongInfo"), showSongInfo).toBool();
    autoDetect = settings.value(QStringLiteral("autoDetect"), autoDetect).toBool();
    pollIntervalMs = settings.value(QStringLiteral("pollInterval"), pollIntervalMs).toInt();
    wordByWord = settings.value(QStringLiteral("wordByWord"), wordByWord).toBool();
    upcomingWords = settings.value(QStringLiteral("upcomingWords"), upcomingWords).toString();
    captionStyle = settings.value(QStringLiteral("captionStyle"), captionStyle).toString();
    emphasizeWord = settings.value(QStringLiteral("emphasizeWord"), emphasizeWord).toBool();
    animationType = settings.value(QStringLiteral("captionAnimation"), animationType).toString();
    lineSpacing = settings.value(QStringLiteral("lineSpacing"), lineSpacing).toInt();
    showTips = settings.value(QStringLiteral("showTips"), showTips).toBool();
    startWithWindows = settings.value(QStringLiteral("startWithWindows"),
                                      startWithWindows).toBool();
    startWithWindows = windowsRunEntryPresent();

    pollIntervalMs = qBound(250, pollIntervalMs, 5000);

    const QByteArray geometry = settings.value(QStringLiteral("overlay/geometry")).toByteArray();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
        geometryRestored = true;
    }
}
