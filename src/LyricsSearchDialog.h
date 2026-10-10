#pragma once

#include <QDialog>
#include <QList>
#include "LyricsClient.h"

class QLineEdit;
class QLabel;
class QListWidget;
class QPushButton;

class LyricsSearchDialog final : public QDialog
{
    Q_OBJECT
public:
    explicit LyricsSearchDialog(QWidget *parent = nullptr);
    bool hasSelection() const;
    LyricsResult selectedResult() const;

private:
    QLineEdit *trackEdit = nullptr;
    QLineEdit *artistEdit = nullptr;
    QLabel *statusLabel = nullptr;
    QListWidget *resultsList = nullptr;
    QPushButton *searchButton = nullptr;
    QList<LyricsResult> results;
    LyricsClient client;

    void startSearch();
    void acceptSelected();
};
