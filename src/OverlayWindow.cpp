#include "OverlayWindow.h"
#include "SettingsDialog.h"
#include "LyricsSearchDialog.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QCursor>
#include <QDir>
#include <QFont>
#include <QIcon>
#include <QLabel>
#include <QLinearGradient>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QStringList>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QScreen>
#include <QSettings>
#include <QShortcut>
#include <QVBoxLayout>
#include <QAction>
#include <QKeyEvent>
#include <QTimer>
#include <QMessageBox>
#include <QRegularExpression>
#include <QDateTime>
#include <QTextDocument>
#include <QSystemTrayIcon>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QVariantAnimation>
#include <QEasingCurve>

namespace {

QIcon makeAppIcon()
{
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QLinearGradient gradient(0, 0, 64, 64);
    gradient.setColorAt(0.0, QColor(124, 92, 255));
    gradient.setColorAt(1.0, QColor(77, 238, 234));
    painter.setBrush(gradient);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(pixmap.rect().adjusted(4, 4, -4, -4), 16, 16);

    painter.setPen(Qt::white);
    QFont font(QStringLiteral("Segoe UI"), 30, QFont::Bold);
    painter.setFont(font);
    painter.drawText(pixmap.rect(), Qt::AlignCenter, QStringLiteral("\u266A"));
    painter.end();
    return QIcon(pixmap);
}

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
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_StyledBackground, true);   // let the style sheet paint the card
    setMinimumSize(260, 90);
    resize(660, 170);
    setFocusPolicy(Qt::StrongFocus);
    setWindowIcon(makeAppIcon());

    lyricsLabel = new QLabel(this);
    lyricsLabel->setAlignment(Qt::AlignCenter);
    lyricsLabel->setWordWrap(true);
    lyricsLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    QSizePolicy labelPolicy = lyricsLabel->sizePolicy();
    labelPolicy.setHeightForWidth(true);
    lyricsLabel->setSizePolicy(labelPolicy);

    labelOpacity = new QGraphicsOpacityEffect(lyricsLabel);
    labelOpacity->setOpacity(1.0);
    lyricsLabel->setGraphicsEffect(labelOpacity);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->addWidget(lyricsLabel);

    syncTimer = new QTimer(this);
    syncTimer->setInterval(80);
    connect(syncTimer, &QTimer::timeout, this, &OverlayWindow::updateSyncedLine);

    fadeAnimation = new QPropertyAnimation(labelOpacity, "opacity", this);
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

    mediaDetector->start(pollIntervalMs);
    mediaSession->start(250);

    // Make sure the last position is remembered even when quitting from the tray.
    connect(qApp, &QApplication::aboutToQuit, this, &OverlayWindow::saveSettings);
}

void OverlayWindow::updateText()
{
    lastRenderedHtml.clear();

    if (showingTip) {
        const int tipSize = qMax(12, fontSize * 55 / 100);
        lyricsLabel->setText(
            QStringLiteral("<div style='font-size:%1px; opacity:0.9;'>"
                           "Right-click or double-click the overlay for options.<br>"
                           "Drag to move it · scroll to resize the text.</div>")
                .arg(tipSize));
        fitToContent();
        return;
    }

    // Nothing is playing: keep the overlay completely blank.
    if (!songActive) {
        lyricsLabel->clear();
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
        text = QStringLiteral("<div style='font-size:%1px; letter-spacing:1px; opacity:0.85;'>%2 — %3</div>"
                              "<div style='height:8px'></div>"
                              "<div style='line-height:150%'>%4</div>")
                   .arg(infoSize)
                   .arg(currentSong.toHtmlEscaped(), currentArtist.toHtmlEscaped(),
                        currentLyrics.toHtmlEscaped());
    } else {
        text = QStringLiteral("<div style='line-height:150%'>%1</div>")
                   .arg(currentLyrics.toHtmlEscaped());
    }
    lyricsLabel->setText(text);
    fitToContent();
}

void OverlayWindow::applyStyle()
{
    const QString customColor = textColor.name();
    const int alpha = qBound(0, opacityPercent, 100) * 255 / 100;

    QString windowCss;
    QString defaultColor = QStringLiteral("white");
    if (currentStyle == QLatin1String("Floating Text")) {
        windowCss = QStringLiteral("background: transparent; border: none;");
    } else if (currentStyle == QLatin1String("Instagram")) {
        windowCss = QStringLiteral("background-color: rgba(18,18,24,%1); border: 1px solid rgba(255,255,255,70); border-radius: 26px;").arg(alpha);
    } else if (currentStyle == QLatin1String("Karaoke")) {
        windowCss = QStringLiteral("background-color: rgba(12,12,18,%1); border: 2px solid #7c5cff; border-radius: 16px;").arg(alpha);
        defaultColor = QStringLiteral("#f8f6ff");
    } else if (currentStyle == QLatin1String("Neon")) {
        windowCss = QStringLiteral("background-color: rgba(8,10,22,%1); border: 1px solid #4deeea; border-radius: 18px;").arg(alpha);
        defaultColor = QStringLiteral("#8ffff9");
    } else if (currentStyle == QLatin1String("Minimal Bar")) {
        windowCss = QStringLiteral("background-color: rgba(20,20,20,%1); border-radius: 10px;").arg(alpha);
    } else if (currentStyle == QLatin1String("Album Card")) {
        windowCss = QStringLiteral("background-color: rgba(35,25,48,%1); border: 1px solid rgba(220,180,255,100); border-radius: 22px;").arg(alpha);
        defaultColor = QStringLiteral("#fff7ff");
    } else {
        windowCss = QStringLiteral("background-color: rgba(35,38,52,%1); border: 1px solid rgba(255,255,255,75); border-radius: 22px;").arg(alpha);
    }

    const QString color = useCustomColor ? customColor : defaultColor;
    resolvedTextColor = color;
    const QString labelCss = QStringLiteral("QLabel { color: %1; background: transparent; }").arg(color);
    setStyleSheet(QStringLiteral("OverlayWindow {") + windowCss + QStringLiteral("}"));
    lyricsLabel->setStyleSheet(labelCss);
    applyLabelFont();
    fitToContent();
}

int OverlayWindow::effectiveFontSize() const
{
    return qMax(6, qRound(fontSize * m_captionScale));
}

void OverlayWindow::applyLabelFont()
{
    QFont f(fontFamily, effectiveFontSize());
    f.setBold(fontBold);
    // Fall back to fonts that actually carry Indic (Devanagari) and symbol
    // glyphs so Hindi lyrics and their vowel marks render instead of vanishing.
    f.setFamilies({fontFamily, QStringLiteral("Nirmala UI"), QStringLiteral("Segoe UI"),
                   QStringLiteral("Segoe UI Symbol"), QStringLiteral("Segoe UI Emoji")});
    lyricsLabel->setFont(f);
    if (!syncedLines.isEmpty() && syncTimer->isActive())
        renderSynced();
}

void OverlayWindow::setCaptionScale(qreal scale)
{
    if (qFuzzyCompare(m_captionScale, scale)) return;
    m_captionScale = scale;
    applyLabelFont();
}

void OverlayWindow::animateCaption()
{
    if (animationType == QLatin1String("None") || !labelOpacity || !fadeAnimation) {
        if (labelOpacity) labelOpacity->setOpacity(1.0);
        return;
    }

    fadeAnimation->stop();
    labelOpacity->setOpacity(0.2);
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
    plainLyrics = result.plainLyrics;
    syncedLines.clear();
    parseSyncedLyrics(result.syncedLyrics);

    if (!syncedLines.isEmpty()) {
        elapsedOffset = QDateTime::currentMSecsSinceEpoch();
        haveSyncBase = false;
        hasLastReport = false;
        lastLineIndex = -1;
        m_captionScale = 1.0;
        if (labelOpacity) labelOpacity->setOpacity(1.0);
        syncTimer->start();
        currentLyrics = syncedLines.first().text;
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
    const int contentWidth = qMax(160, width() - 48);
    int labelHeight = lyricsLabel->heightForWidth(contentWidth);
    if (labelHeight <= 0)
        labelHeight = lyricsLabel->sizeHint().height();
    // 40px for the top/bottom margins plus 6px so tall matras are never clipped.
    const int target = qMax(minimumHeight(), labelHeight + 46);
    if (target != height())
        resize(width(), target);
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
    if (!state.valid) {
        mediaSessionActive = false;
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (state.positionMs > 0 || state.durationMs > 0)
        positionTrustworthy = true;

    if (!mediaSessionActive || !haveSyncBase) {
        syncBaseMs = state.positionMs;
        syncBaseStampMs = now;
        haveSyncBase = true;
    } else if (!hasLastReport || state.positionMs != lastReportedMs) {
        // A fresh report: trust it. Big jumps are seeks; small ones are the
        // normal tick. Either way we re-anchor so the overlay tracks exactly.
        syncBaseMs = state.positionMs;
        syncBaseStampMs = now;
    }
    // else: the player kept reporting the same position (e.g. paused or a
    // coarse timeline). Keep our own clock running instead of snapping back.

    hasLastReport = true;
    lastReportedMs = state.positionMs;
    mediaPlaying = state.isPlaying();
    mediaSessionActive = true;

    if (!syncedLines.isEmpty()) renderSynced();
}

void OverlayWindow::renderSynced()
{
    if (syncedLines.isEmpty()) return;

    const qint64 pos = currentPlaybackMs();
    int lineIndex = 0;
    for (int i = 0; i < syncedLines.size(); ++i) {
        if (syncedLines.at(i).milliseconds <= pos) lineIndex = i;
        else break;
    }
    const LyricLine &line = syncedLines.at(lineIndex);

    int wordIndex = -1;
    for (int w = 0; w < line.words.size(); ++w) {
        if (line.words.at(w).milliseconds <= pos) wordIndex = w;
        else break;
    }

    QColor base(resolvedTextColor);
    if (!base.isValid()) base = QColor(Qt::white);
    base.setAlpha(255);
    QColor dim = base;
    dim.setAlpha(95);
    const QString activeCss = base.name();
    const QString dimCss = QStringLiteral("rgba(%1,%2,%3,%4)")
                               .arg(dim.red())
                               .arg(dim.green())
                               .arg(dim.blue())
                               .arg(QString::number(dim.alphaF(), 'f', 3));
    const QString pillCss = QStringLiteral("rgba(%1,%2,%3,0.30)")
                                .arg(base.red())
                                .arg(base.green())
                                .arg(base.blue());

    const int gap = qMax(0, effectiveFontSize() * lineSpacing / 100);

    const bool hideUpcoming = (upcomingWords == QLatin1String("Hidden"));
    const bool nextOnly = (upcomingWords == QLatin1String("Next word"));

    QString lineHtml;
    if (wordByWord && !line.words.isEmpty()) {
        QStringList pieces;
        for (int w = 0; w < line.words.size(); ++w) {
            const bool sung = (w <= wordIndex);
            if (!sung) {
                if (hideUpcoming) continue;
                if (nextOnly && w != wordIndex + 1) continue;
            }
            const QString text = line.words.at(w).text.toHtmlEscaped();
            if (sung && emphasizeWord && w == wordIndex) {
                pieces << QStringLiteral("<span style=\"color:%1; background-color:%2;\">"
                                         "\u2009%3\u2009</span>")
                              .arg(activeCss, pillCss, text);
            } else {
                pieces << QStringLiteral("<span style=\"color:%1;\">%2</span>")
                              .arg(sung ? activeCss : dimCss, text);
            }
        }
        lineHtml = pieces.join(QLatin1Char(' '));
    } else if (!line.text.isEmpty()) {
        lineHtml = QStringLiteral("<span style=\"color:%1;\">%2</span>")
                       .arg(activeCss, line.text.toHtmlEscaped());
    }

    // The generous line-height reserves room for tall Indic vowel marks (matras)
    // so the first line is not clipped at the top of the overlay.
    QString html;
    if (showSongInfo) {
        const int infoSize = qMax(11, effectiveFontSize() * 45 / 100);
        html = QStringLiteral("<div style='font-size:%1px; letter-spacing:1px; opacity:0.85;'>"
                              "%2 — %3</div><div style='height:%4px'></div>"
                              "<div style='line-height:150%'>%5</div>")
                   .arg(infoSize)
                   .arg(currentSong.toHtmlEscaped(), currentArtist.toHtmlEscaped())
                   .arg(gap)
                   .arg(lineHtml);
    } else {
        html = QStringLiteral("<div style='line-height:150%'>") + lineHtml
             + QStringLiteral("</div>");
    }

    currentLyrics = line.text;
    const bool lineChanged = (lineIndex != lastLineIndex);
    lastLineIndex = lineIndex;
    if (html != lastRenderedHtml) {
        lastRenderedHtml = html;
        lyricsLabel->setText(html);
    }
    if (lineChanged) {
        fitToContent();
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

void OverlayWindow::onNowPlaying(const NowPlaying &np)
{
    if (!autoDetect) {
        updateTrayNowPlaying();
        return;
    }

    // Playback stopped / nothing detected: wipe the overlay clean.
    if (!np.valid) {
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
            m_captionScale = 1.0;
            if (labelOpacity) labelOpacity->setOpacity(1.0);
            updateText();
        }
        updateTrayNowPlaying();
        return;
    }

    if (np.query == lastAutoQuery) {
        updateTrayNowPlaying();
        return;
    }

    if (showingTip) {
        showingTip = false;
        if (tipTimer) tipTimer->stop();
    }
    songActive = true;
    lastAutoQuery = np.query;
    currentSong = np.title;
    currentArtist = np.artist.isEmpty() ? np.sourceApp : np.artist;
    syncedLines.clear();
    syncTimer->stop();
    haveSyncBase = false;
    positionTrustworthy = false;
    hasLastReport = false;
    lastLineIndex = -1;
    m_captionScale = 1.0;
    if (labelOpacity) labelOpacity->setOpacity(1.0);
    currentLyrics = QStringLiteral("Searching lyrics for \"%1\"…").arg(np.title);
    updateText();
    updateTrayNowPlaying();

    const quint64 token = ++autoRequestId;
    autoClient->searchAuto(np.title, np.artist, np.query, token);
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
    emphasizeWord = settings.value(QStringLiteral("emphasizeWord"), emphasizeWord).toBool();
    animationType = settings.value(QStringLiteral("captionAnimation"), animationType).toString();
    lineSpacing = settings.value(QStringLiteral("lineSpacing"), lineSpacing).toInt();
    showTips = settings.value(QStringLiteral("showTips"), showTips).toBool();
    startWithWindows = settings.value(QStringLiteral("startWithWindows"),
                                      startWithWindows).toBool();
    startWithWindows = windowsRunEntryPresent();

    const QByteArray geometry = settings.value(QStringLiteral("overlay/geometry")).toByteArray();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
        geometryRestored = true;
    }
}
