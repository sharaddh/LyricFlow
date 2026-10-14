#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>
#include <QColor>
#include <QBrush>

class QComboBox;
class QFontComboBox;
class QSpinBox;
class QSlider;
class QCheckBox;
class QLabel;
class QPushButton;

// Visual description of one of the seven overlay looks. Shared by the overlay
// (applyStyle / paintEvent) and the settings preview so they can never drift
// apart. The overlay paints the card with QPainter (a style-sheet background
// does not render on a per-pixel-alpha window), and surfaceCss() turns the same
// data back into declarations for the preview's QLabel.
struct StyleSkin {
    bool drawsCard = true;                 // false = Floating Text (no card)
    QBrush fill;                           // solid colour or gradient brush
    QColor borderColor = QColor(Qt::transparent);
    int borderWidth = 0;
    int radius = 22;
    QString textColor = QStringLiteral("#ffffff");
    QString accentColor = QStringLiteral("#bfe2ff");
    int padX = 30;
    int padY = 22;

    QString surfaceCss() const;            // style-sheet declarations for preview
};

struct LyricStyleOptions {
    QString style = QStringLiteral("Glass Card");
    QString font = QStringLiteral("Segoe UI");
    int fontSize = 25;
    int opacity = 88;
    bool bold = true;
    bool customColor = false;
    QColor textColor = QColor(255, 255, 255);
    bool showSongInfo = true;
    bool autoDetect = true;
    int pollIntervalMs = 1500;

    // App behaviour.
    bool showTips = true;            // show the right/double-click hint on launch
    bool startWithWindows = false;   // add Capson to the Windows startup items

    // Caption behaviour.
    bool wordByWord = true;
    QString upcomingWords = QStringLiteral("All words");   // Hidden / Next word / All words
    bool emphasizeWord = false;
    QString animation = QStringLiteral("Fade");
    int lineSpacing = 45;

    // Caption typography, independent of the card look. One of:
    // Classic / Instagram Story / Big & Small / Spotlight / Karaoke Glow.
    QString captionStyle = QStringLiteral("Classic");
};

class SettingsDialog final : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(const LyricStyleOptions &options, QWidget *parent = nullptr);

    LyricStyleOptions options() const;

    static QStringList availableStyles();
    static QStringList availableAnimations();
    static QStringList availableUpcomingModes();
    static QStringList availableCaptionStyles();
    static StyleSkin styleSkin(const QString &style, int opacityPercent);

private slots:
    void pickColor();
    void updatePreview();
    void resetDefaults();

private:
    QComboBox *styleBox = nullptr;
    QFontComboBox *fontBox = nullptr;
    QCheckBox *boldBox = nullptr;
    QSlider *sizeSlider = nullptr;
    QSpinBox *sizeBox = nullptr;
    QSlider *opacitySlider = nullptr;
    QLabel *opacityValue = nullptr;
    QCheckBox *colorCheck = nullptr;
    QPushButton *colorButton = nullptr;
    QCheckBox *songInfoBox = nullptr;
    QCheckBox *autoDetectBox = nullptr;
    QSpinBox *pollBox = nullptr;
    QCheckBox *tipsBox = nullptr;
    QCheckBox *startupBox = nullptr;
    QLabel *preview = nullptr;

    QCheckBox *wordByWordBox = nullptr;
    QComboBox *upcomingBox = nullptr;
    QComboBox *captionBox = nullptr;
    QCheckBox *emphasizeBox = nullptr;
    QComboBox *animationBox = nullptr;
    QSpinBox *spacingBox = nullptr;

    QColor chosenColor;
};
