# Capson — Automatic desktop lyrics overlay (C++20 + Qt 6)

An always-on-top floating overlay that **detects the song playing in the background
and shows its lyrics automatically**. Works with the Spotify desktop app, web
players (YouTube / YouTube Music / SoundCloud / Deezer / Tidal…) and local media
players (VLC, foobar2000, MusicBee, Media Player, AIMP…).

## Features
- **Automatic song detection** — polls the titles of known media windows and
  loads matching lyrics from [LRCLIB](https://lrclib.net) without any clicks.
- **Synchronized (LRC) lyrics** when available, with a plain-lyrics fallback.
- **Word-by-word (Instagram-style) captions** — the sung words light up as the
  song plays, using enhanced-LRC word timings when present and evenly weighting
  the line otherwise.
- **Customisable captions** (Settings → Captions): toggle word-by-word
  animation, choose how upcoming words appear (hidden, just the next word in
  light grey, or all of them), highlight the active word, pick a line animation
  (None / Fade / Pop) and tune the line spacing.
- **Real playback sync** — the overlay reads the OS media session position and
  play/pause state, so captions follow seeks (jumping forward/back), pauses and
  mid-song starts instead of drifting from a fixed timer.
- **Seven visual styles**: Floating Text, Glass Card, Instagram, Karaoke,
  Minimal Bar, Neon, Album Card.
- **Easy font customisation**
  - Mouse wheel over the overlay changes the font size instantly.
  - `Ctrl +` / `Ctrl -` grow or shrink the font, `Ctrl 0` resets it.
  - Settings dialog with a **live preview**, full font picker, bold, font size,
    opacity, custom text colour and song-info toggle.
- **System tray control** — show/hide, find lyrics, customize, style switcher,
  play/pause and next/previous track (media keys), and quit.
- Drag the overlay anywhere; position, size, style and options are remembered,
  so it reopens exactly where you left it.
- **Startup tip** — a short reminder that you can right-click or double-click the
  overlay for options (disable it under Settings → Automatic lyrics).
- **Start with Windows** — optionally launch Capson when you log in
  (Settings → Automatic lyrics).
- **Blank when idle** — nothing is drawn on screen while no song is playing.
- Manual lyric search through LRCLIB as a fallback.

## Build (MSYS2 UCRT64 + Ninja)

```cmd
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=C:/msys64/ucrt64
cmake --build build
build\Capson.exe
```

If Qt's Network module is missing, open MSYS2 UCRT64 and run:
```bash
pacman -S --needed mingw-w64-ucrt-x86_64-qt6-base
```

## Controls
- Mouse wheel over the overlay: change font size
- `Ctrl +` / `Ctrl -`: font size up / down, `Ctrl 0`: reset
- `Ctrl+Shift+L`: find lyrics · `Ctrl+,`: settings · `Ctrl+Shift+S`: style menu
- `Ctrl+Shift+H`: hide/show overlay · double-click: settings · drag: move
- Right-click overlay or tray icon: full menu
- Tray: Play/Pause, Next and Previous track

## How automatic detection works
The detector watches the window titles of media applications. It understands the
Spotify `Track • Artist` format, strips browser suffixes such as ` - YouTube`,
and removes promo tags like `(Official Video)`. The cleaned `Artist Song` text is
searched on LRCLIB and the best match (preferring synchronized lyrics) is shown.

### Honest limitations
- Detection is based on **window titles**, so if a song plays in a background
  browser tab while another window owns that browser window, it may not be seen.
- Playback position and pause state come from the Windows media session (the
  same source behind the volume-key media controls), which most players and
  browsers expose. If an app does not report them, the overlay gracefully falls
  back to a wall-clock estimate.
- Lyrics availability and provider terms vary by song and region; an internet
  connection is required.
