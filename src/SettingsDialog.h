#pragma once
#include <QDialog>
#include <QString>

class QComboBox;
class QSpinBox;
class QSlider;
class QCheckBox;

class SettingsDialog final : public QDialog
{
    Q_OBJECT
public:
    SettingsDialog(const QString &style, const QString &font, int size,
                   int opacity, bool showInfo, QWidget *parent = nullptr);
    QString selectedStyle() const;
    QString selectedFont() const;
    int selectedFontSize() const;
    int selectedOpacity() const;
    bool showSongInfo() const;

private:
    QComboBox *styleBox = nullptr;
    QComboBox *fontBox = nullptr;
    QSpinBox *sizeBox = nullptr;
    QSlider *opacitySlider = nullptr;
    QCheckBox *songInfoBox = nullptr;
};
