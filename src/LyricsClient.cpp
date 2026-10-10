#include "LyricsClient.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>

namespace {

constexpr int kMaxAttempts = 4;

QUrl searchUrl()
{
    return QUrl(QStringLiteral("https://lrclib.net/api/search"));
}

QList<LyricsResult> parseResults(const QJsonArray &array)
{
    QList<LyricsResult> results;
    for (const QJsonValue &value : array) {
        if (!value.isObject()) continue;
        const QJsonObject obj = value.toObject();
        LyricsResult result;
        result.trackName = obj.value(QStringLiteral("trackName")).toString();
        result.artistName = obj.value(QStringLiteral("artistName")).toString();
        result.albumName = obj.value(QStringLiteral("albumName")).toString();
        result.plainLyrics = obj.value(QStringLiteral("plainLyrics")).toString();
        result.syncedLyrics = obj.value(QStringLiteral("syncedLyrics")).toString();
        result.sourceUrl = obj.value(QStringLiteral("url")).toString();
        if (!result.syncedLyrics.isEmpty() || !result.plainLyrics.isEmpty())
            results.append(result);
    }
    return results;
}

// LRCLIB occasionally answers 503/429 (and friends) while under load; those are
// worth retrying instead of surfacing a hard failure to the user.
bool transientFailure(QNetworkReply *reply)
{
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 408 || status == 425 || status == 429 || status == 500 || status == 502
        || status == 503 || status == 504)
        return true;
    switch (reply->error()) {
    case QNetworkReply::TimeoutError:
    case QNetworkReply::TemporaryNetworkFailureError:
    case QNetworkReply::NetworkSessionFailedError:
    case QNetworkReply::ServiceUnavailableError:
    case QNetworkReply::UnknownNetworkError:
        return true;
    default:
        return false;
    }
}

} // namespace

LyricsClient::LyricsClient(QObject *parent) : QObject(parent) {}

void LyricsClient::search(const QString &track, const QString &artist)
{
    QUrl url = searchUrl();
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("track_name"), track.trimmed());
    if (!artist.trimmed().isEmpty())
        query.addQueryItem(QStringLiteral("artist_name"), artist.trimmed());
    url.setQuery(query);

    emit searchStarted();
    fetch(url, 1, [this](const QList<LyricsResult> &results, const QString &error) {
        if (!error.isEmpty()) {
            emit searchFailed(error);
            return;
        }
        if (results.isEmpty()) {
            emit searchFailed(QStringLiteral(
                "No lyrics were found. Try a shorter song title or a different artist spelling."));
            return;
        }
        emit searchFinished(results);
    });
}

void LyricsClient::searchQuery(const QString &queryText, quint64 token)
{
    QUrl url = searchUrl();
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("q"), queryText.trimmed());
    url.setQuery(query);

    emit searchStarted();
    fetch(url, 1, [this, token](const QList<LyricsResult> &results, const QString &error) {
        if (!error.isEmpty()) {
            emit queryFailed(token, error);
            return;
        }
        emit queryFinished(token, results);
    });
}

void LyricsClient::searchAuto(const QString &track, const QString &artist,
                              const QString &queryText, quint64 token)
{
    QList<QUrl> urls;
    const QString freeText = queryText.trimmed();
    if (!freeText.isEmpty()) {
        QUrl url = searchUrl();
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("q"), freeText);
        url.setQuery(query);
        urls.append(url);
    }
    if (!track.trimmed().isEmpty()) {
        QUrl url = searchUrl();
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("track_name"), track.trimmed());
        if (!artist.trimmed().isEmpty())
            query.addQueryItem(QStringLiteral("artist_name"), artist.trimmed());
        url.setQuery(query);
        urls.append(url);
    }

    if (urls.isEmpty()) {
        emit queryFailed(token, QStringLiteral("Nothing to search for."));
        return;
    }
    emit searchStarted();
    fetchChain(urls, 0, token);
}

void LyricsClient::fetchChain(const QList<QUrl> &urls, int index, quint64 token)
{
    if (index >= urls.size()) {
        emit queryFinished(token, {});
        return;
    }
    const bool last = (index + 1 >= urls.size());
    fetch(urls.at(index), 1,
          [this, urls, index, token, last](const QList<LyricsResult> &results, const QString &error) {
              if (!results.isEmpty()) {
                  emit queryFinished(token, results);
                  return;
              }
              // Keep trying the remaining strategies (and don't give up on a
              // transient failure while another query is still available).
              if (!last) {
                  fetchChain(urls, index + 1, token);
                  return;
              }
              if (!error.isEmpty()) {
                  emit queryFailed(token, error);
                  return;
              }
              emit queryFinished(token, {});
          });
}

void LyricsClient::fetch(const QUrl &url, int attempt, ResultsHandler handler)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Capson/1.2 (Windows desktop lyrics client)"));
    request.setTransferTimeout(12000);

    QNetworkReply *reply = manager.get(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, url, attempt, handler]() {
                if (transientFailure(reply) && attempt < kMaxAttempts) {
                    reply->deleteLater();
                    const int delay = 400 * attempt;
                    QTimer::singleShot(delay, this, [this, url, attempt, handler]() {
                        fetch(url, attempt + 1, handler);
                    });
                    return;
                }

                const auto finish = [reply]() { reply->deleteLater(); };
                if (reply->error() != QNetworkReply::NoError) {
                    const QString error = reply->errorString();
                    finish();
                    handler({}, QStringLiteral("Could not reach the lyrics service: ") + error);
                    return;
                }

                QJsonParseError parseError;
                const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll(), &parseError);
                finish();
                if (parseError.error != QJsonParseError::NoError || !doc.isArray()) {
                    handler({}, QStringLiteral("The lyrics service returned an unexpected response."));
                    return;
                }

                handler(parseResults(doc.array()), QString());
            });
}
