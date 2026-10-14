#include "SettingsDialog.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
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

QStringList SettingsDialog::availableCaptionStyles()
{
    return {QStringLiteral("Classic"), QStringLiteral("Instagram Story"),
            QStringLiteral("Big & Small"), QStringLiteral("Spotlight"),
            QStringLiteral("Karaoke Glow")};
}

StyleSkin SettingsDialog::styleSkin(const QString &style, int opacityPercent)
{
    const int pct = qBound(0, opacityPercent, 100);
    const int a = pct * 255 / 100;
    const auto col = [](int r, int g, int b, int alpha) { return QColor(r, g, b, alpha); };

    StyleSkin skin;
    skin.textColor = QStringLiteral("#ffffff");
    skin.accentColor = QStringLiteral("#bfe2ff");

    if (style == QLatin1String("Floating Text")) {
        // No card at all - the text floats straight over the desktop.
        skin.drawsCard = false;
        skin.textColor = QStringLiteral("#ffffff");
        skin.accentColor = QStringLiteral("#ffffff");
        skin.padX = 18;
        skin.padY = 12;
    } else if (style == QLatin1String("Instagram")) {
        // The signature sunset gradient with a soft corner; opacity tints it.
        QLinearGradient g(0, 0, 1, 1);
        g.setColorAt(0.0, col(254, 218, 117, a));
        g.setColorAt(0.55, col(214, 41, 118, a));
        g.setColorAt(1.0, col(150, 47, 191, a));
        skin.fill = QBrush(g);
        skin.radius = 22;
        skin.textColor = QStringLiteral("#ffffff");
        skin.accentColor = QStringLiteral("#ffffff");
        skin.padX = 30;
        skin.padY = 22;
    } else if (style == QLatin1String("Karaoke")) {
        // Deep aubergine stage with a hot-pink glow for the sung word.
        skin.fill = QBrush(col(23, 19, 52, a));
        skin.borderColor = col(255, 255, 255, 28);
        skin.borderWidth = 1;
        skin.radius = 18;
        skin.textColor = QStringLiteral("#f8f6ff");
        skin.accentColor = QStringLiteral("#ff4f9a");
        skin.padX = 30;
        skin.padY = 22;
    } else if (style == QLatin1String("Minimal Bar")) {
        // Translucent frosted pill; the accent is a clean mint underline-style.
        skin.fill = QBrush(col(255, 255, 255, qBound(5, pct * 22 / 100, 65)));
        skin.borderColor = col(255, 255, 255, 90);
        skin.borderWidth = 1;
        skin.radius = 999;   // large radius clamps to a capsule
        skin.textColor = QStringLiteral("#f2f4fc");
        skin.accentColor = QStringLiteral("#16c79a");
        skin.padX = 32;
        skin.padY = 14;
    } else if (style == QLatin1String("Neon")) {
        // Near-black neon board with an electric violet frame and mint glow.
        skin.fill = QBrush(col(11, 11, 23, a));
        skin.borderColor = col(109, 91, 255, 160);
        skin.borderWidth = 1;
        skin.radius = 16;
        skin.textColor = QStringLiteral("#c9c4ff");
        skin.accentColor = QStringLiteral("#7dffc4");
        skin.padX = 30;
        skin.padY = 22;
    } else if (style == QLatin1String("Album Card")) {
        // A vinyl-album look: near-opaque ivory card with dark ink text and a
        // violet accent for the current word.
        skin.fill = QBrush(col(250, 248, 255, qBound(120, pct * 255 / 100, 245)));
        skin.borderColor = col(109, 91, 255, 110);
        skin.borderWidth = 1;
        skin.radius = 20;
        skin.textColor = QStringLiteral("#1b1430");
        skin.accentColor = QStringLiteral("#6d5bff");
        skin.padX = 30;
        skin.padY = 22;
    } else {
        // Glass Card (default): light frost, translucent white card.
        skin.fill = QBrush(col(255, 255, 255, qBound(5, pct * 38 / 100, 130)));
        skin.borderColor = col(255, 255, 255, 110);
        skin.borderWidth = 1;
        skin.radius = 22;
        skin.textColor = QStringLiteral("#ffffff");
        skin.accentColor = QStringLiteral("#bfe2ff");
        skin.padX = 30;
        skin.padY = 22;
    }
    return skin;
}

QString StyleSkin::surfaceCss() const
{
    if (!drawsCard)
        return QStringLiteral("background: transparent; border: none;");

    const auto rgba = [](const QColor &c) {
        return QStringLiteral("rgba(%1,%2,%3,%4)")
            .arg(c.red()).arg(c.green()).arg(c.blue())
            .arg(QString::number(c.alphaF(), 'f', 3));
    };

    QString css;
    if (fill.style() == Qt::LinearGradientPattern && fill.gradient()) {
        const auto *g = static_cast<const QLinearGradient *>(fill.gradient());
        css = QStringLiteral("background: qlineargradient(x1:%1, y1:%2, x2:%3, y2:%4")
                  .arg(g->start().x(), 1, 'f', 2)
                  .arg(g->start().y(), 1, 'f', 2)
                  .arg(g->finalStop().x(), 1, 'f', 2)
                  .arg(g->finalStop().y(), 1, 'f', 2);
        for (const auto &stop : g->stops())
            css += QStringLiteral(", stop:%1 %2").arg(stop.first, 1, 'f', 2).arg(rgba(stop.second));
        css += QLatin1Char(')');
    } else {
        css = QStringLiteral("background-color: %1").arg(rgba(fill.color()));
    }
    css += QLatin1Char(';');

    if (borderWidth > 0 && borderColor.alpha() > 0)
        css += QStringLiteral(" border: %1px solid %2;").arg(borderWidth).arg(rgba(borderColor));
    else
        css += QStringLiteral(" border: none;");
    css += QStringLiteral(" border-radius: %1px;").arg(radius);
    return css;
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
    captionBox = new QComboBox(this);
    captionBox->addItems(availableCaptionStyles());
    captionBox->setCurrentText(options.captionStyle);
    if (captionBox->currentIndex() < 0) captionBox->setCurrentIndex(0);
    captionBox->setToolTip(QStringLiteral(
        "How the words are typeset over any display style.\n"
        "Classic: the whole line with the sung word highlighted.\n"
        "Instagram Story: bold uppercase reveal with a highlight behind each word.\n"
        "Big & Small: the active word is large, the rest small.\n"
        "Spotlight: only the word being sung, big and centered.\n"
        "Karaoke Glow: the whole line with the active word lit up in the accent colour."));
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
    captions->addRow(QStringLiteral("Caption style"), captionBox);
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

    // Everything scrolls inside one area so the dialog never runs off a short
    // screen; the OK/Cancel row stays pinned at the bottom.
    auto *content = new QWidget(this);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(12);
    contentLayout->addWidget(appearanceGroup);
    contentLayout->addWidget(captionsGroup);
    contentLayout->addWidget(autoGroup);
    contentLayout->addWidget(previewLabel);
    contentLayout->addWidget(preview);
    contentLayout->addStretch(1);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(content);

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->addWidget(scroll, 1);
    layout->addWidget(buttons);

    // Never grow taller than the screen: cap the height and let the content scroll.
    if (const QScreen *screen = QGuiApplication::primaryScreen()) {
        const int maxHeight = screen->availableGeometry().height() * 82 / 100;
        setMaximumHeight(maxHeight);
        resize(540, qMin(maxHeight, 720));
    }

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
    connect(captionBox, &QComboBox::currentTextChanged, this, [this](const QString &) {
        const bool classic = (captionBox->currentText() == QLatin1String("Classic"));
        wordByWordBox->setEnabled(classic);
        upcomingBox->setEnabled(classic && wordByWordBox->isChecked());
        emphasizeBox->setEnabled(classic && wordByWordBox->isChecked());
        updatePreview();
    });
    connect(wordByWordBox, &QCheckBox::toggled, this, [this](bool on) {
        const bool classic = (captionBox->currentText() == QLatin1String("Classic"));
        upcomingBox->setEnabled(on && classic);
        emphasizeBox->setEnabled(on && classic);
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

    const QString style = styleBox->currentText();
    const StyleSkin skin = styleSkin(style, opacitySlider->value());
    const QString color = colorCheck->isChecked() ? chosenColor.name() : skin.textColor;

    preview->setStyleSheet(
        QStringLiteral("QLabel { %1 color:%2; padding:%3px %4px;"
                       " font-family:'%5'; font-size:%6px;"
                       " font-weight:%7; }")
            .arg(skin.surfaceCss(), color, QString::number(skin.padY),
                 QString::number(skin.padX), fontBox->currentFont().family())
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
    captionBox->setCurrentText(d.captionStyle);
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
    result.captionStyle = captionBox->currentText();
    result.emphasizeWord = emphasizeBox->isChecked();
    result.animation = animationBox->currentText();
    result.lineSpacing = spacingBox->value();
    result.showTips = tipsBox->isChecked();
    result.startWithWindows = startupBox->isChecked();
    return result;
}
