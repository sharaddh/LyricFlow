#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>
#include <QColor>

class QComboBox;
class QFontComboBox;
class QSpinBox;
class QSlider;
class QCheckBox;
class QLabel;
class QPushButton;

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
    QCheckBox *emphasizeBox = nullptr;
    QComboBox *animationBox = nullptr;
    QSpinBox *spacingBox = nullptr;

    QColor chosenColor;
};
