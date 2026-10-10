#include "SettingsDialog.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

QStringList SettingsDialog::availableStyles()
{
    return {QStringLiteral("Floating Text"), QStringLiteral("Glass Card"),
            QStringLiteral("Instagram"), QStringLiteral("Karaoke"),
            QStringLiteral("Minimal Bar"), QStringLiteral("Neon"),
            QStringLiteral("Album Card")};
}

QStringList SettingsDialog::availableAnimations()
{
    return {QStringLiteral("None"), QStringLiteral("Fade"), QStringLiteral("Pop")};
}

QStringList SettingsDialog::availableUpcomingModes()
{
    return {QStringLiteral("Hidden"), QStringLiteral("Next word"), QStringLiteral("All words")};
}

SettingsDialog::SettingsDialog(const LyricStyleOptions &options, QWidget *parent)
    : QDialog(parent), chosenColor(options.textColor)
{
    setWindowTitle(QStringLiteral("Capson Settings"));
    setMinimumWidth(460);

    // ---- Appearance -------------------------------------------------------
    styleBox = new QComboBox(this);
    styleBox->addItems(availableStyles());
    styleBox->setCurrentText(options.style);
    if (styleBox->currentIndex() < 0) styleBox->setCurrentIndex(1);

    fontBox = new QFontComboBox(this);
    fontBox->setCurrentFont(QFont(options.font));

    boldBox = new QCheckBox(QStringLiteral("Bold text"), this);
    boldBox->setChecked(options.bold);

    sizeSlider = new QSlider(Qt::Horizontal, this);
    sizeSlider->setRange(10, 96);
    sizeSlider->setValue(options.fontSize);

    sizeBox = new QSpinBox(this);
    sizeBox->setRange(10, 96);
    sizeBox->setValue(options.fontSize);
    sizeBox->setSuffix(QStringLiteral(" px"));
    sizeBox->setFixedWidth(84);

    auto *sizeRow = new QWidget(this);
    auto *sizeLayout = new QHBoxLayout(sizeRow);
    sizeLayout->setContentsMargins(0, 0, 0, 0);
    sizeLayout->setSpacing(8);
    sizeLayout->addWidget(sizeSlider, 1);
    sizeLayout->addWidget(sizeBox);

    opacitySlider = new QSlider(Qt::Horizontal, this);
    opacitySlider->setRange(10, 100);
    opacitySlider->setValue(options.opacity);
    opacityValue = new QLabel(QStringLiteral("%1%").arg(options.opacity), this);
    opacityValue->setFixedWidth(46);
    opacityValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    auto *opacityRow = new QWidget(this);
    auto *opacityLayout = new QHBoxLayout(opacityRow);
    opacityLayout->setContentsMargins(0, 0, 0, 0);
    opacityLayout->setSpacing(8);
    opacityLayout->addWidget(opacitySlider, 1);
    opacityLayout->addWidget(opacityValue);

    colorCheck = new QCheckBox(QStringLiteral("Use a custom text colour"), this);
    colorCheck->setChecked(options.customColor);
    colorButton = new QPushButton(this);
    colorButton->setFixedWidth(64);

    songInfoBox = new QCheckBox(QStringLiteral("Show song and artist information"), this);
    songInfoBox->setChecked(options.showSongInfo);

    wordByWordBox = new QCheckBox(QStringLiteral("Animate captions word by word"), this);
    wordByWordBox->setChecked(options.wordByWord);
    upcomingBox = new QComboBox(this);
    upcomingBox->addItems(availableUpcomingModes());
    upcomingBox->setCurrentText(options.upcomingWords);
    if (upcomingBox->currentIndex() < 0) upcomingBox->setCurrentIndex(2);
    upcomingBox->setEnabled(options.wordByWord);
    emphasizeBox = new QCheckBox(QStringLiteral("Highlight the current word"), this);
    emphasizeBox->setChecked(options.emphasizeWord);
    emphasizeBox->setEnabled(options.wordByWord);

    animationBox = new QComboBox(this);
    animationBox->addItems(availableAnimations());
    animationBox->setCurrentText(options.animation);
    if (animationBox->currentIndex() < 0) animationBox->setCurrentIndex(1);

    spacingBox = new QSpinBox(this);
    spacingBox->setRange(0, 200);
    spacingBox->setValue(options.lineSpacing);
    spacingBox->setSuffix(QStringLiteral(" %"));
    spacingBox->setFixedWidth(84);

    // ---- Automation -------------------------------------------------------
    autoDetectBox = new QCheckBox(QStringLiteral("Detect the playing song automatically"), this);
    autoDetectBox->setChecked(options.autoDetect);

    pollBox = new QSpinBox(this);
    pollBox->setRange(1, 15);
    pollBox->setValue(qMax(1, options.pollIntervalMs / 1000));
    pollBox->setSuffix(QStringLiteral(" s"));
    pollBox->setFixedWidth(84);

    tipsBox = new QCheckBox(
        QStringLiteral("Show the right-click / double-click tip at startup"), this);
    tipsBox->setChecked(options.showTips);

    startupBox = new QCheckBox(QStringLiteral("Start Capson when Windows starts"), this);
    startupBox->setChecked(options.startWithWindows);

    // ---- Form layouts -----------------------------------------------------
    auto *appearance = new QFormLayout;
    appearance->addRow(QStringLiteral("Display style"), styleBox);
    appearance->addRow(QStringLiteral("Font"), fontBox);
    appearance->addRow(QStringLiteral("Font size"), sizeRow);
    appearance->addRow(QStringLiteral("Card opacity"), opacityRow);
    appearance->addRow(QStringLiteral("Text colour"), colorButton);
    appearance->addRow(QString(), colorCheck);
    appearance->addRow(QString(), boldBox);
    appearance->addRow(QString(), songInfoBox);

    auto *captions = new QFormLayout;
    captions->addRow(QString(), wordByWordBox);
    captions->addRow(QStringLiteral("Upcoming words"), upcomingBox);
    captions->addRow(QString(), emphasizeBox);
    captions->addRow(QStringLiteral("Line animation"), animationBox);
    captions->addRow(QStringLiteral("Line spacing"), spacingBox);

    auto *automation = new QFormLayout;
    automation->addRow(QString(), autoDetectBox);
    automation->addRow(QStringLiteral("Check for new songs every"), pollBox);
    automation->addRow(QString(), tipsBox);
    automation->addRow(QString(), startupBox);

    for (QFormLayout *form : {appearance, captions, automation}) {
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
        form->setHorizontalSpacing(14);
        form->setVerticalSpacing(8);
    }

    auto *autoGroup = new QGroupBox(QStringLiteral("Automatic lyrics"), this);
    autoGroup->setLayout(automation);
    auto *captionsGroup = new QGroupBox(QStringLiteral("Captions"), this);
    captionsGroup->setLayout(captions);
    auto *appearanceGroup = new QGroupBox(QStringLiteral("Appearance"), this);
    appearanceGroup->setLayout(appearance);

    // ---- Live preview -----------------------------------------------------
    preview = new QLabel(this);
    preview->setMinimumHeight(90);
    preview->setAlignment(Qt::AlignCenter);
    preview->setWordWrap(true);
    preview->setText(QStringLiteral("Imagine all the people\nLiving life in peace"));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    auto *resetButton = buttons->addButton(QStringLiteral("Restore defaults"),
                                           QDialogButtonBox::ResetRole);
    connect(resetButton, &QPushButton::clicked, this, &SettingsDialog::resetDefaults);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *previewLabel = new QLabel(QStringLiteral("Live preview"), this);
    previewLabel->setStyleSheet(QStringLiteral("font-weight:600;"));

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->addWidget(appearanceGroup);
    layout->addWidget(captionsGroup);
    layout->addWidget(autoGroup);
    layout->addWidget(previewLabel);
    layout->addWidget(preview);
    layout->addWidget(buttons);

    // ---- Wiring -----------------------------------------------------------
    connect(styleBox, &QComboBox::currentTextChanged, this, &SettingsDialog::updatePreview);
    connect(fontBox, &QFontComboBox::currentFontChanged, this, &SettingsDialog::updatePreview);
    connect(boldBox, &QCheckBox::toggled, this, &SettingsDialog::updatePreview);
    connect(sizeSlider, &QSlider::valueChanged, sizeBox, &QSpinBox::setValue);
    connect(sizeBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        if (sizeSlider->value() != value) sizeSlider->setValue(value);
        updatePreview();
    });
    connect(sizeSlider, &QSlider::valueChanged, this, &SettingsDialog::updatePreview);
    connect(opacitySlider, &QSlider::valueChanged, this, [this](int value) {
        opacityValue->setText(QStringLiteral("%1%").arg(value));
        updatePreview();
    });
    connect(colorCheck, &QCheckBox::toggled, this, &SettingsDialog::updatePreview);
    connect(colorButton, &QPushButton::clicked, this, &SettingsDialog::pickColor);
    connect(songInfoBox, &QCheckBox::toggled, this, &SettingsDialog::updatePreview);
    connect(wordByWordBox, &QCheckBox::toggled, this, [this](bool on) {
        upcomingBox->setEnabled(on);
        emphasizeBox->setEnabled(on);
        updatePreview();
    });

    updatePreview();
}

void SettingsDialog::pickColor()
{
    const QColor color = QColorDialog::getColor(chosenColor, this, QStringLiteral("Text colour"));
    if (color.isValid()) {
        chosenColor = color;
        colorCheck->setChecked(true);
        updatePreview();
    }
}

void SettingsDialog::updatePreview()
{
    const QString swatch = chosenColor.name();
    colorButton->setText(colorCheck->isChecked() ? swatch : QStringLiteral("Default"));
    colorButton->setStyleSheet(QStringLiteral("QPushButton { background:%1; color:%2; }")
                                   .arg(chosenColor.name(),
                                        chosenColor.lightness() > 140 ? QStringLiteral("#111")
                                                                      : QStringLiteral("#fff")));
    colorButton->setEnabled(colorCheck->isChecked());

    const QString color = colorCheck->isChecked() ? chosenColor.name() : QStringLiteral("#ffffff");
    const int alpha = qBound(10, opacitySlider->value(), 100) * 255 / 100;

    const QString style = styleBox->currentText();
    QString background = QStringLiteral("rgba(35,38,52,%1)").arg(alpha);
    if (style == QLatin1String("Floating Text"))
        background = QStringLiteral("transparent");
    else if (style == QLatin1String("Instagram"))
        background = QStringLiteral("rgba(18,18,24,%1)").arg(alpha);
    else if (style == QLatin1String("Karaoke"))
        background = QStringLiteral("rgba(12,12,18,%1)").arg(alpha);
    else if (style == QLatin1String("Neon"))
        background = QStringLiteral("rgba(8,10,22,%1)").arg(alpha);
    else if (style == QLatin1String("Minimal Bar"))
        background = QStringLiteral("rgba(20,20,20,%1)").arg(alpha);
    else if (style == QLatin1String("Album Card"))
        background = QStringLiteral("rgba(35,25,48,%1)").arg(alpha);

    preview->setStyleSheet(
        QStringLiteral("QLabel { background:%1; color:%2; border:1px solid rgba(255,255,255,60);"
                       " border-radius:14px; padding:14px; font-family:'%3'; font-size:%4px;"
                       " font-weight:%5; }")
            .arg(background, color, fontBox->currentFont().family())
            .arg(sizeBox->value())
            .arg(boldBox->isChecked() ? 700 : 400));
}

void SettingsDialog::resetDefaults()
{
    const LyricStyleOptions d;
    styleBox->setCurrentText(d.style);
    fontBox->setCurrentFont(QFont(d.font));
    boldBox->setChecked(d.bold);
    sizeSlider->setValue(d.fontSize);
    sizeBox->setValue(d.fontSize);
    opacitySlider->setValue(d.opacity);
    opacityValue->setText(QStringLiteral("%1%").arg(d.opacity));
    colorCheck->setChecked(d.customColor);
    chosenColor = d.textColor;
    songInfoBox->setChecked(d.showSongInfo);
    wordByWordBox->setChecked(d.wordByWord);
    upcomingBox->setCurrentText(d.upcomingWords);
    emphasizeBox->setChecked(d.emphasizeWord);
    animationBox->setCurrentText(d.animation);
    spacingBox->setValue(d.lineSpacing);
    autoDetectBox->setChecked(d.autoDetect);
    pollBox->setValue(qMax(1, d.pollIntervalMs / 1000));
    tipsBox->setChecked(d.showTips);
    startupBox->setChecked(d.startWithWindows);
    updatePreview();
}

LyricStyleOptions SettingsDialog::options() const
{
    LyricStyleOptions result;
    result.style = styleBox->currentText();
    result.font = fontBox->currentFont().family();
    result.fontSize = sizeBox->value();
    result.opacity = opacitySlider->value();
    result.bold = boldBox->isChecked();
    result.customColor = colorCheck->isChecked();
    result.textColor = chosenColor;
    result.showSongInfo = songInfoBox->isChecked();
    result.autoDetect = autoDetectBox->isChecked();
    result.pollIntervalMs = pollBox->value() * 1000;
    result.wordByWord = wordByWordBox->isChecked();
    result.upcomingWords = upcomingBox->currentText();
    result.emphasizeWord = emphasizeBox->isChecked();
    result.animation = animationBox->currentText();
    result.lineSpacing = spacingBox->value();
    result.showTips = tipsBox->isChecked();
    result.startWithWindows = startupBox->isChecked();
    return result;
}
