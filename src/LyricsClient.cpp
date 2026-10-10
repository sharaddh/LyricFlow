#include "LyricsClient.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

LyricsClient::LyricsClient(QObject *parent) : QObject(parent) {}

void LyricsClient::search(const QString &track, const QString &artist)
{
    QUrl url("https://lrclib.net/api/search");
    QUrlQuery query;
    query.addQueryItem("track_name", track.trimmed());
    if (!artist.trimmed().isEmpty())
        query.addQueryItem("artist_name", artist.trimmed());
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "LyricFlow/1.1 (Windows desktop lyrics client)");
    request.setTransferTimeout(12000);

    emit searchStarted();
    QNetworkReply *reply = manager.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const auto finish = [reply]() { reply->deleteLater(); };
        if (reply->error() != QNetworkReply::NoError) {
            const QString error = reply->errorString();
            finish();
            emit searchFailed("Could not reach the lyrics service: " + error);
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll(), &parseError);
        finish();
        if (parseError.error != QJsonParseError::NoError || !doc.isArray()) {
            emit searchFailed("The lyrics service returned an unexpected response.");
            return;
        }

        QList<LyricsResult> results;
        for (const QJsonValue &value : doc.array()) {
            if (!value.isObject()) continue;
            const QJsonObject obj = value.toObject();
            LyricsResult result;
            result.trackName = obj.value("trackName").toString();
            result.artistName = obj.value("artistName").toString();
            result.albumName = obj.value("albumName").toString();
            result.plainLyrics = obj.value("plainLyrics").toString();
            result.syncedLyrics = obj.value("syncedLyrics").toString();
            result.sourceUrl = obj.value("url").toString();
            if (!result.syncedLyrics.isEmpty() || !result.plainLyrics.isEmpty())
                results.append(result);
        }
        if (results.isEmpty()) {
            emit searchFailed("No lyrics were found. Try a shorter song title or different artist spelling.");
            return;
        }
        emit searchFinished(results);
    });
}
