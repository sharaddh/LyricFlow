#include "OverlayWindow.h"
#include "SettingsDialog.h"
#include "LyricsSearchDialog.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QLabel>
#include <QMenu>
#include <QStringList>
#include <QMouseEvent>
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

OverlayWindow::OverlayWindow(QWidget *parent) : QWidget(parent)
{
    setWindowTitle("LyricFlow");
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setMinimumSize(260, 90);
    resize(660, 170);
    setFocusPolicy(Qt::StrongFocus);

    lyricsLabel = new QLabel(this);
    lyricsLabel->setAlignment(Qt::AlignCenter);
    lyricsLabel->setWordWrap(true);
    lyricsLabel->setTextInteractionFlags(Qt::NoTextInteraction);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(22, 18, 22, 18);
    layout->addWidget(lyricsLabel);

    syncTimer = new QTimer(this);
    syncTimer->setInterval(100);
    connect(syncTimer, &QTimer::timeout, this, &OverlayWindow::updateSyncedLine);

    loadSettings();
    updateText();
    applyStyle();

    if (auto *screen = QApplication::primaryScreen()) {
        const QRect area = screen->availableGeometry();
        move(area.x() + (area.width() - width()) / 2, area.y() + 35);
    }

    auto *settingsShortcut = new QShortcut(QKeySequence("Ctrl+,"), this);
    connect(settingsShortcut, &QShortcut::activated, this, &OverlayWindow::openSettings);
    auto *styleShortcut = new QShortcut(QKeySequence("Ctrl+Shift+S"), this);
    connect(styleShortcut, &QShortcut::activated, this, &OverlayWindow::showStyleMenu);
    auto *hideShortcut = new QShortcut(QKeySequence("Ctrl+Shift+H"), this);
    connect(hideShortcut, &QShortcut::activated, this, [this]() { setVisible(!isVisible()); });
    auto *lyricsShortcut = new QShortcut(QKeySequence("Ctrl+Shift+L"), this);
    connect(lyricsShortcut, &QShortcut::activated, this, &OverlayWindow::findLyrics);
}

void OverlayWindow::updateText()
{
    QString text;
    if (showSongInfo) {
        text = QString("<div style='font-size:12px; letter-spacing:1px;'>%1 — %2</div>"
                       "<div style='height:8px'></div><div>%3</div>")
                   .arg(currentSong.toHtmlEscaped(), currentArtist.toHtmlEscaped(),
                        currentLyrics.toHtmlEscaped());
    } else {
        text = currentLyrics.toHtmlEscaped();
    }
    lyricsLabel->setText(text);
}

void OverlayWindow::applyStyle()
{
    const QString font = QString("font-family:'%1'; font-size:%2px; font-weight:600;")
                             .arg(fontFamily).arg(fontSize);
    const int alpha = qBound(0, opacityPercent, 100) * 255 / 100;
    QString windowCss;
    QString labelCss = "QLabel { color: white; background: transparent; " + font + " }";
    if (currentStyle == "Floating Text") {
        windowCss = "background: transparent; border: none;";
        labelCss = "QLabel { color: white; background: transparent; " + font + " }";
    } else if (currentStyle == "Instagram") {
        windowCss = QString("background-color: rgba(18,18,24,%1); border: 1px solid rgba(255,255,255,70); border-radius: 26px;").arg(alpha);
    } else if (currentStyle == "Karaoke") {
        windowCss = QString("background-color: rgba(12,12,18,%1); border: 2px solid #7c5cff; border-radius: 16px;").arg(alpha);
        labelCss = "QLabel { color: #f8f6ff; background: transparent; " + font + " }";
    } else if (currentStyle == "Neon") {
        windowCss = QString("background-color: rgba(8,10,22,%1); border: 1px solid #4deeea; border-radius: 18px;").arg(alpha);
        labelCss = "QLabel { color: #8ffff9; background: transparent; " + font + " }";
    } else if (currentStyle == "Minimal Bar") {
        windowCss = QString("background-color: rgba(20,20,20,%1); border-radius: 10px;").arg(alpha);
        labelCss = "QLabel { color: white; background: transparent; " + font + " }";
    } else if (currentStyle == "Album Card") {
        windowCss = QString("background-color: rgba(35,25,48,%1); border: 1px solid rgba(220,180,255,100); border-radius: 22px;").arg(alpha);
        labelCss = "QLabel { color: #fff7ff; background: transparent; " + font + " }";
    } else {
        windowCss = QString("background-color: rgba(35,38,52,%1); border: 1px solid rgba(255,255,255,75); border-radius: 22px;").arg(alpha);
    }
    setStyleSheet("OverlayWindow {" + windowCss + "}");
    lyricsLabel->setStyleSheet(labelCss);
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
    const QStringList styles = {"Floating Text", "Glass Card", "Instagram", "Karaoke",
                                "Minimal Bar", "Neon", "Album Card"};
    for (const QString &style : styles) {
        QAction *action = menu.addAction(style);
        action->setCheckable(true);
        action->setChecked(style == currentStyle);
        connect(action, &QAction::triggered, this, [this, style]() { setStyle(style); });
    }
    menu.addSeparator();
    menu.addAction("Find lyrics…  (Ctrl+Shift+L)", this, &OverlayWindow::findLyrics);
    menu.addAction("Customize…  (Ctrl+,)", this, &OverlayWindow::openSettings);
    menu.addAction("Quit LyricFlow", qApp, &QApplication::quit);
    menu.exec(QCursor::pos());
}

void OverlayWindow::openSettings()
{
    SettingsDialog dialog(currentStyle, fontFamily, fontSize, opacityPercent, showSongInfo, this);
    if (dialog.exec() == QDialog::Accepted) {
        currentStyle = dialog.selectedStyle();
        fontFamily = dialog.selectedFont();
        fontSize = dialog.selectedFontSize();
        opacityPercent = dialog.selectedOpacity();
        showSongInfo = dialog.showSongInfo();
        applyStyle();
        updateText();
        saveSettings();
    }
}

void OverlayWindow::findLyrics()
{
    LyricsSearchDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted && dialog.hasSelection())
        loadLyrics(dialog.selectedResult());
}

void OverlayWindow::loadLyrics(const LyricsResult &result)
{
    currentSong = result.trackName.isEmpty() ? "Unknown song" : result.trackName;
    currentArtist = result.artistName.isEmpty() ? "Unknown artist" : result.artistName;
    plainLyrics = result.plainLyrics;
    syncedLines.clear();

    static const QRegularExpression lineRegex(
        R"(^\s*\[(\d{2}):(\d{2})(?:\.(\d{1,3}))?\]\s*(.*)$)");
    const QStringList lines = result.syncedLyrics.split('\n');
    for (const QString &line : lines) {
        auto match = lineRegex.match(line);
        if (!match.hasMatch()) continue;
        const qint64 minutes = match.captured(1).toLongLong();
        const qint64 seconds = match.captured(2).toLongLong();
        QString fraction = match.captured(3);
        if (fraction.size() == 1) fraction += "00";
        else if (fraction.size() == 2) fraction += "0";
        const qint64 millis = fraction.isEmpty() ? 0 : fraction.toLongLong();
        syncedLines.append({(minutes * 60 + seconds) * 1000 + millis, match.captured(4).trimmed()});
    }
    if (!syncedLines.isEmpty()) {
        elapsedOffset = QDateTime::currentMSecsSinceEpoch();
        syncTimer->start();
        currentLyrics = syncedLines.first().text;
    } else {
        syncTimer->stop();
        currentLyrics = plainLyrics.isEmpty()
            ? "No lyrics text is available for this result."
            : plainLyrics;
    }
    updateText();
    adjustSize();
    saveSettings();
}

void OverlayWindow::updateSyncedLine()
{
    if (syncedLines.isEmpty()) return;
    const qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - elapsedOffset;
    int current = 0;
    for (int i = 0; i < syncedLines.size(); ++i) {
        if (syncedLines.at(i).milliseconds <= elapsed) current = i;
        else break;
    }
    QString next = syncedLines.at(current).text;
    if (current + 1 < syncedLines.size()) next += "\n" + syncedLines.at(current + 1).text;
    if (next != currentLyrics) {
        currentLyrics = next;
        updateText();
    }
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
void OverlayWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) { showStyleMenu(); event->accept(); return; }
    QWidget::keyPressEvent(event);
}
void OverlayWindow::saveSettings()
{
    QSettings settings;
    settings.setValue("overlay/geometry", saveGeometry());
    settings.setValue("style", currentStyle);
    settings.setValue("fontFamily", fontFamily);
    settings.setValue("fontSize", fontSize);
    settings.setValue("opacity", opacityPercent);
    settings.setValue("showSongInfo", showSongInfo);
}
void OverlayWindow::loadSettings()
{
    QSettings settings;
    currentStyle = settings.value("style", currentStyle).toString();
    fontFamily = settings.value("fontFamily", fontFamily).toString();
    fontSize = settings.value("fontSize", fontSize).toInt();
    opacityPercent = settings.value("opacity", opacityPercent).toInt();
    showSongInfo = settings.value("showSongInfo", showSongInfo).toBool();
    // Restore saved geometry after the default size is set.
    const QByteArray geometry = settings.value("overlay/geometry").toByteArray();
    if (!geometry.isEmpty()) restoreGeometry(geometry);
}
