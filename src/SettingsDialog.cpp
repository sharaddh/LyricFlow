#include "SettingsDialog.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>

SettingsDialog::SettingsDialog(const QString &style, const QString &font, int size,
                               int opacity, bool showInfo, QWidget *parent) : QDialog(parent)
{
    setWindowTitle("LyricFlow Settings");
    setMinimumWidth(380);

    styleBox = new QComboBox(this);
    styleBox->addItems({"Floating Text", "Glass Card", "Instagram", "Karaoke",
                        "Minimal Bar", "Neon", "Album Card"});
    styleBox->setCurrentText(style);

    fontBox = new QComboBox(this);
    fontBox->addItems({"Segoe UI", "Arial", "Georgia", "Consolas", "Verdana", "Trebuchet MS"});
    fontBox->setCurrentText(font);

    sizeBox = new QSpinBox(this);
    sizeBox->setRange(12, 64);
    sizeBox->setValue(size);
    sizeBox->setSuffix(" px");

    opacitySlider = new QSlider(Qt::Horizontal, this);
    opacitySlider->setRange(20, 100);
    opacitySlider->setValue(opacity);
    auto *opacityValue = new QLabel(QString("%1%").arg(opacity), this);
    connect(opacitySlider, &QSlider::valueChanged, this,
            [opacityValue](int value) { opacityValue->setText(QString("%1%").arg(value)); });

    auto *opacityWidget = new QWidget(this);
    auto *opacityLayout = new QHBoxLayout(opacityWidget);
    opacityLayout->setContentsMargins(0, 0, 0, 0);
    opacityLayout->addWidget(opacitySlider, 1);
    opacityLayout->addWidget(opacityValue);

    songInfoBox = new QCheckBox("Show song and artist information", this);
    songInfoBox->setChecked(showInfo);

    auto *form = new QFormLayout;
    form->addRow("Display style", styleBox);
    form->addRow("Font", fontBox);
    form->addRow("Font size", sizeBox);
    form->addRow("Card opacity", opacityWidget);
    form->addRow("", songInfoBox);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("Customize your floating lyrics overlay.", this));
    layout->addLayout(form);
    layout->addWidget(buttons);
}
QString SettingsDialog::selectedStyle() const { return styleBox->currentText(); }
QString SettingsDialog::selectedFont() const { return fontBox->currentText(); }
int SettingsDialog::selectedFontSize() const { return sizeBox->value(); }
int SettingsDialog::selectedOpacity() const { return opacitySlider->value(); }
bool SettingsDialog::showSongInfo() const { return songInfoBox->isChecked(); }
