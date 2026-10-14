#include "MediaDetector.h"

#include <QList>
#include <QTimer>
#include <QRegularExpression>
#include <QStringList>
#include <QtGlobal>
#include <string>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

struct PlayerConfig {
    const char *process;
    const char *label;
    int strategy;      // 0 = Spotify, 1 = browser, 2 = generic player
    const char *suffix;
};

// Known media processes we can read a "now playing" style title from.
const PlayerConfig kPlayers[] = {
    {"spotify",             "Spotify",              0, ""},
    {"chrome",              "Chrome",               1, ""},
    {"msedge",              "Edge",                 1, ""},
    {"firefox",             "Firefox",              1, ""},
    {"brave",               "Brave",                1, ""},
    {"opera",               "Opera",                1, ""},
    {"vivaldi",             "Vivaldi",              1, ""},
    {"chromium",            "Chromium",             1, ""},
    {"librewolf",           "LibreWolf",            1, ""},
    {"musicbee",            "MusicBee",             2, "MusicBee"},
    {"foobar2000",          "foobar2000",           2, ""},
    {"vlc",                 "VLC",                  2, "VLC media player"},
    {"wmplayer",            "Windows Media Player", 2, "Windows Media Player"},
    {"itunes",              "iTunes",               2, ""},
    {"music",               "Media Player",         2, "Media Player"},
    {"music.ui",            "Groove Music",         2, "Groove Music"},
    {"mediaplayer",         "Media Player",         2, "Media Player"},
    {"aimp",                "AIMP",                 2, "AIMP"},
    {"winamp",              "Winamp",               2, "Winamp"},
    {"potplayermini64",     "PotPlayer",            2, "PotPlayer"},
    {"potplayer",           "PotPlayer",            2, "PotPlayer"},
    {"deezer",              "Deezer",               2, "Deezer"},
    {"tidal",               "TIDAL",                2, "TIDAL"},
    {"amazon music",        "Amazon Music",         2, "Amazon Music"},
    {"mpv",                 "mpv",                  2, ""},
};

[[maybe_unused]] const PlayerConfig *playerFor(const QString &processName)
{
    for (const auto &player : kPlayers) {
        if (processName.compare(QLatin1String(player.process), Qt::CaseInsensitive) == 0)
            return &player;
    }
    return nullptr;
}

const QStringList &browserSites()
{
    static const QStringList sites = {
        "YouTube Music", "YouTube", "SoundCloud", "Spotify", "Deezer",
        "Tidal", "Bandcamp", "Apple Music", "Qobuz", "Audius"
    };
    return sites;
}

const QStringList &browserNames()
{
    static const QStringList names = {
        "Google Chrome", "Chrome", "Microsoft Edge", "Edge", "Mozilla Firefox",
        "Firefox", "Brave", "Opera", "Vivaldi", "Chromium", "LibreWolf"
    };
    return names;
}

const QStringList &suffixNames()
{
    static const QStringList all = [] {
        QStringList combined = browserSites();
        combined += browserNames();
        return combined;
    }();
    return all;
}

// Strip trailing site names like " - YouTube Music" / " | YouTube".
QString stripSiteSuffix(QString title)
{
    bool changed = true;
    while (changed) {
        changed = false;
        for (const QString &site : suffixNames()) {
            for (const QString &separator : {QStringLiteral(" - "), QStringLiteral(" | "),
                                             QStringLiteral(" · "), QStringLiteral(" — ")}) {
                const QString tail = separator + site;
                if (title.endsWith(tail, Qt::CaseInsensitive)) {
                    title.chop(tail.size());
                    title = title.trimmed();
                    changed = true;
                }
            }
        }
    }
    return title;
}

// Remove promotional / quality brackets: "(Official Video)", "[HD]", "(Lyrics)"...
QString stripBracketNoise(QString title)
{
    static const QRegularExpression bracket(
        QStringLiteral(R"(\s*[\(\[][^\)\]]*(official|lyric|video|audio|visuali|hd|4k|8k|remaster|remastered|mv|music video|performance|live|explicit|color coded|prod\.|m\/v)[^\)\]]*[\)\]])"),
        QRegularExpression::CaseInsensitiveOption);
    title.remove(bracket);
    static const QRegularExpression topic(QStringLiteral(R"(\s*-\s*Topic$)"),
                                          QRegularExpression::CaseInsensitiveOption);
    title.remove(topic);
    return title.simplified();
}

// Remove list/counter/truncation noise such as a leading "(22)" track number,
// a trailing "...." ellipsis or a trailing "(22)" counter that browsers put in
// window titles. Left in place these would poison the lyrics search query.
QString stripListNoise(QString text)
{
    static const QRegularExpression leadingNumber(
        QStringLiteral(R"(^\s*(?:\(\s*\d+\s*\)|\[\s*\d+\s*\]|\d+\s*[\.\)\-:\x{2013}\x{2014}]\s*))"));
    text.remove(leadingNumber);

    static const QRegularExpression trailingCounter(
        QStringLiteral(R"(\s*[\(\[]\s*\d+\s*[\)\]]\s*$)"));
    text.remove(trailingCounter);

    static const QRegularExpression trailingNoise(
        QStringLiteral(R"([\s\.\x{2026}\x{00b7}_\-\x{2013}\x{2014}\|]+$)"));
    text.remove(trailingNoise);

    return text.simplified();
}

bool looksLikePlaceholder(const QString &title)
{
    static const QStringList blocked = {
        "youtube", "youtube music", "soundcloud", "spotify", "spotify free",
        "spotify premium", "new tab", "google", "google chrome", "microsoft edge",
        "mozilla firefox", "firefox", "brave", "opera", "vivaldi", "chromium",
        "librewolf", "facebook", "netflix", "twitch", "reddit", "gmail", "inbox",
        "twitter", "x", "instagram", "discord", "whatsapp"
    };
    const QString lower = title.trimmed().toLower();
    if (lower.isEmpty()) return true;
    return blocked.contains(lower);
}

NowPlaying parseTitle(const PlayerConfig &player, const QString &rawTitle)
{
    NowPlaying np;
    np.sourceApp = QString::fromLatin1(player.label);

    QString title = rawTitle.trimmed();

    if (player.suffix[0] != '\0') {
        const QString suffix = QString::fromLatin1(player.suffix);
        const QString dashed = QStringLiteral(" - ") + suffix;
        if (title.endsWith(dashed, Qt::CaseInsensitive))
            title.chop(dashed.size());
        else if (title.endsWith(suffix, Qt::CaseInsensitive))
            title.chop(suffix.size());
        title = title.trimmed();
    }

    if (player.strategy == 1)
        title = stripSiteSuffix(title);

    title = stripBracketNoise(title);

    if (player.strategy == 0) {
        // Spotify uses "Track • Artist".
        const int bullet = title.indexOf(QChar(0x2022));
        if (bullet > 0) {
            np.title = title.left(bullet).trimmed();
            np.artist = title.mid(bullet + 1).trimmed();
        }
    }

    if (np.title.isEmpty()) {
        // Browsers and general players typically use "Artist - Track".
        const QStringList parts = title.split(QStringLiteral(" - "), Qt::SkipEmptyParts);
        if (parts.size() >= 2) {
            np.artist = parts.at(0).trimmed();
            np.title = parts.at(1).trimmed();
            for (int i = 2; i < parts.size(); ++i)
                np.title += QStringLiteral(" - ") + parts.at(i).trimmed();
        } else {
            np.title = title.trimmed();
        }
    }

    np.title = stripListNoise(np.title);
    np.artist = stripListNoise(np.artist);

    if (np.title.isEmpty() || looksLikePlaceholder(np.title))
        return np; // invalid

    if (player.strategy == 0 && np.artist.isEmpty())
        np.artist = QStringLiteral("Spotify");

    np.query = np.artist.isEmpty() ? np.title
                                   : (np.artist + QStringLiteral(" ") + np.title);
    np.valid = true;
    return np;
}

#ifdef Q_OS_WIN

struct Candidate {
    QString label;
    int strategy = 2;
    QString suffix;
    QString rawTitle;
};

QString windowTitle(HWND hwnd)
{
    const int length = GetWindowTextLengthW(hwnd);
    if (length <= 0) return {};
    std::wstring buffer(static_cast<size_t>(length) + 1, L'\0');
    const int copied = GetWindowTextW(hwnd, buffer.data(), length + 1);
    buffer.resize(static_cast<size_t>(copied > 0 ? copied : 0));
    return QString::fromWCharArray(buffer.c_str());
}

QString processNameForPid(DWORD pid)
{
    HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!handle) return {};
    wchar_t buffer[MAX_PATH] = {0};
    DWORD size = MAX_PATH;
    QString name;
    if (QueryFullProcessImageNameW(handle, 0, buffer, &size))
        name = QString::fromWCharArray(buffer);
    CloseHandle(handle);
    if (name.isEmpty()) return {};
    const int slash = name.lastIndexOf(QLatin1Char('\\'));
    if (slash >= 0) name = name.mid(slash + 1);
    if (name.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive))
        name.chop(4);
    return name.toLower();
}

struct EnumContext {
    DWORD ownPid = 0;
    QList<Candidate> candidates;
};

BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM lParam)
{
    auto *ctx = reinterpret_cast<EnumContext *>(lParam);
    if (!IsWindowVisible(hwnd)) return TRUE;

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (!pid || pid == ctx->ownPid) return TRUE;

    const QString processName = processNameForPid(pid);
    if (processName.isEmpty()) return TRUE;

    const PlayerConfig *player = playerFor(processName);
    if (!player) return TRUE;

    const QString title = windowTitle(hwnd);
    if (title.isEmpty()) return TRUE;

    ctx->candidates.append({QString::fromLatin1(player->label), player->strategy,
                            QString::fromLatin1(player->suffix), title});
    return TRUE;
}

bool titleHasSite(const QString &title)
{
    for (const QString &site : browserSites()) {
        if (title.contains(site, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

bool candidateHasSite(const Candidate &c)
{
    return titleHasSite(c.rawTitle);
}

NowPlaying detectNowPlaying()
{
    EnumContext ctx;
    ctx.ownPid = GetCurrentProcessId();
    EnumWindows(enumWindowsProc, reinterpret_cast<LPARAM>(&ctx));

    const auto tryCandidate = [](const Candidate &c, bool allowBrowser) -> NowPlaying {
        if (c.strategy == 1 && !allowBrowser)
            return {};
        const PlayerConfig *player = nullptr;
        for (const auto &p : kPlayers) {
            if (QString::fromLatin1(p.label) == c.label) { player = &p; break; }
        }
        if (!player) return {};
        return parseTitle(*player, c.rawTitle);
    };

    // 1. Prefer a foreground media window.
    HWND foreground = GetForegroundWindow();
    if (foreground) {
        DWORD pid = 0;
        GetWindowThreadProcessId(foreground, &pid);
        if (pid && pid != ctx.ownPid) {
            const QString processName = processNameForPid(pid);
            const PlayerConfig *player = playerFor(processName);
            if (player && IsWindowVisible(foreground)) {
                const QString raw = windowTitle(foreground);
                const bool plausibleBrowser = player->strategy != 1 || titleHasSite(raw);
                if (plausibleBrowser) {
                    const NowPlaying np = parseTitle(*player, raw);
                    if (np.valid) return np;
                }
            }
        }
    }

    // 2. A browser window whose title mentions a music site.
    for (const Candidate &c : ctx.candidates) {
        if (c.strategy == 1 && candidateHasSite(c)) {
            const NowPlaying np = tryCandidate(c, true);
            if (np.valid) return np;
        }
    }

    // 3. Any native media player (Spotify, MusicBee, VLC...).
    for (const Candidate &c : ctx.candidates) {
        if (c.strategy != 1) {
            const NowPlaying np = tryCandidate(c, false);
            if (np.valid) return np;
        }
    }

    return {};
}

#endif // Q_OS_WIN

} // namespace

MediaDetector::MediaDetector(QObject *parent) : QObject(parent)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(800);
    connect(m_timer, &QTimer::timeout, this, &MediaDetector::poll);
}

void MediaDetector::start(int intervalMs)
{
    setInterval(intervalMs);
    if (!m_timer->isActive())
        m_timer->start();
    pollNow();
}

void MediaDetector::stop()
{
    m_timer->stop();
}

bool MediaDetector::isRunning() const
{
    return m_timer->isActive();
}

void MediaDetector::setInterval(int intervalMs)
{
    m_timer->setInterval(qBound(250, intervalMs, 8000));
}

void MediaDetector::pollNow()
{
    poll();
}

void MediaDetector::poll()
{
#ifdef Q_OS_WIN
    const NowPlaying np = detectNowPlaying();
#else
    NowPlaying np;
#endif

    if (np.valid != m_playbackActive) {
        m_playbackActive = np.valid;
        emit playbackActiveChanged(m_playbackActive);
    }

    if (np != m_current) {
        m_current = np;
        emit nowPlayingChanged(m_current);
    }
}

void MediaDetector::triggerMediaKey(MediaKey key)
{
#ifdef Q_OS_WIN
    WORD vk = 0;
    switch (key) {
    case MediaKey::PlayPause: vk = VK_MEDIA_PLAY_PAUSE; break;
    case MediaKey::Next:      vk = VK_MEDIA_NEXT_TRACK; break;
    case MediaKey::Previous:  vk = VK_MEDIA_PREV_TRACK; break;
    case MediaKey::Stop:      vk = VK_MEDIA_STOP; break;
    }
    if (vk) {
        keybd_event(static_cast<BYTE>(vk), 0, 0, 0);
        keybd_event(static_cast<BYTE>(vk), 0, KEYEVENTF_KEYUP, 0);
    }
#else
    Q_UNUSED(key);
#endif
}
