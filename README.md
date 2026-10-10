# LyricFlow Full MVP — C++20 + Qt 6

This ZIP contains a functional desktop MVP:
- Always-on-top frameless floating overlay
- Drag to reposition
- Seven visual styles
- Settings for font, size, opacity and song info
- Preferences saved locally
- Online song/artist lyric search through LRCLIB
- Displays synchronized LRC lyrics using their timestamps when available
- Plain-lyrics fallback when timed lyrics aren't available

## Build on your existing MSYS2 UCRT64 setup

From CMD in the extracted folder:

```cmd
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=C:/msys64/ucrt64
cmake --build build
build\LyricFlow.exe
```

If Qt's Network module is missing, open MSYS2 UCRT64 and run:
```bash
pacman -S --needed mingw-w64-ucrt-x86_64-qt6-base
```

## Controls
- Right-click overlay: style menu, find lyrics, settings, quit
- `Ctrl+Shift+L`: find lyrics
- `Ctrl+,`: settings
- `Ctrl+Shift+S`: style menu
- `Ctrl+Shift+H`: hide/show overlay
- Drag with left mouse button: reposition

## Important honest limitations
This MVP has working online lyric search and timed-line display, but does not yet automatically detect the song playing in Spotify or YouTube. The sync timer starts when lyrics are loaded and is not connected to the player's current seek/pause state. Full automatic player integration needs a Windows media-session bridge and browser integration; it is not safe to claim those features are already implemented.

Lyrics availability and provider terms vary by song and region. An internet connection is required for searching.
