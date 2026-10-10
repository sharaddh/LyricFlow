#include "LyricsSearchDialog.h"

#include <QLineEdit>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QListWidgetItem>

LyricsSearchDialog::LyricsSearchDialog(QWidget *parent) : QDialog(parent), client(this)
{
    setWindowTitle("Find lyrics");
    resize(560, 430);

    trackEdit = new QLineEdit(this);
    trackEdit->setPlaceholderText("Song title");
    artistEdit = new QLineEdit(this);
    artistEdit->setPlaceholderText("Artist (optional)");

    auto *form = new QFormLayout;
    form->addRow("Song", trackEdit);
    form->addRow("Artist", artistEdit);

    searchButton = new QPushButton("Search lyrics", this);
    statusLabel = new QLabel("Search LRCLIB for available lyrics. Internet access is required.", this);
    statusLabel->setWordWrap(true);
    resultsList = new QListWidget(this);
    resultsList->setSelectionMode(QAbstractItemView::SingleSelection);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Open | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &LyricsSearchDialog::acceptSelected);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(searchButton, &QPushButton::clicked, this, &LyricsSearchDialog::startSearch);
    connect(trackEdit, &QLineEdit::returnPressed, this, &LyricsSearchDialog::startSearch);
    connect(resultsList, &QListWidget::itemDoubleClicked, this, [this]() { acceptSelected(); });

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(searchButton);
    layout->addWidget(statusLabel);
    layout->addWidget(resultsList, 1);
    layout->addWidget(buttons);

    connect(&client, &LyricsClient::searchStarted, this, [this]() {
        searchButton->setEnabled(false);
        statusLabel->setText("Searching…");
        resultsList->clear();
        results.clear();
    });
    connect(&client, &LyricsClient::searchFailed, this, [this](const QString &message) {
        searchButton->setEnabled(true);
        statusLabel->setText(message);
    });
    connect(&client, &LyricsClient::searchFinished, this, [this](const QList<LyricsResult> &found) {
        searchButton->setEnabled(true);
        results = found;
        for (const auto &result : results) {
            QString timing = result.syncedLyrics.isEmpty() ? "plain lyrics" : "synchronized lyrics";
            auto *item = new QListWidgetItem(
                QString("%1 — %2  [%3]")
                    .arg(result.trackName, result.artistName, timing), resultsList);
            item->setToolTip(result.albumName);
        }
        statusLabel->setText(QString("Found %1 result(s). Choose the correct recording.").arg(results.size()));
        if (resultsList->count() > 0) resultsList->setCurrentRow(0);
    });
}

void LyricsSearchDialog::startSearch()
{
    if (trackEdit->text().trimmed().isEmpty()) {
        statusLabel->setText("Enter a song title first.");
        return;
    }
    client.search(trackEdit->text(), artistEdit->text());
}

void LyricsSearchDialog::acceptSelected()
{
    if (resultsList->currentRow() < 0 || resultsList->currentRow() >= results.size()) {
        statusLabel->setText("Select a result first.");
        return;
    }
    accept();
}

bool LyricsSearchDialog::hasSelection() const
{
    return (resultsList->currentRow() >= 0 && resultsList->currentRow() < results.size());
}

LyricsResult LyricsSearchDialog::selectedResult() const
{
    if (!hasSelection()) return {};
    return results.at(resultsList->currentRow());
}
