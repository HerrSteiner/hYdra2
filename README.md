# hYdra2
hetrodyne adsyn file editor for Csound by Malte Steiner
second (acutally the third version) of hYdra, written originally 1998 for Windows in C++ and MFC and 1999 as hYdraJ in Java
It was included on the CDRom of Richard Boulangers MIT Csound book, and can be still found at https://github.com/SamKomesarook/The-Csound-Book

## hYdra2:

A Qt 6.8+ / Qt Quick recreation of Malte Steiners hEdit / hYdra editor for Csound HETRO analysis files used by `adsyn`.

## Implemented in this first version

- macOS, Linux and Windows from one C++/Qt Quick code base.
- Reads current HETRO text files (`HETRO <partials>` plus comma-separated tracks).
- Reads legacy 16-bit binary adsyn/HETRO files, with or without the old partial-count header, and auto-detects little/big endian data.
- Writes current HETRO text (`.het`) and legacy binary (`.ads`). A normal Save preserves the loaded representation; Save As chooses text for `.het` and legacy binary for `.ads`.
- Large breakpoint editor with amplitude/frequency views.
- Click a point to select it and display a live value label next to it.
- Drag selected breakpoints.
- Rubber-band selection across multiple breakpoints/partials.
- Partial highlighting synchronized between the breakpoint editor and mixer.
- Partial mixer whose natural bar height represents time-weighted average amplitude.
- Mixer attenuation is non-destructive in memory. A gain of zero does not erase the source breakpoints; it is baked into amplitude values only when the file is serialized. Therefore a zeroed partial can be raised again until the saved file is reloaded.
- GitHub Actions CI matrix for Linux, Windows and macOS, including tests and downloadable workflow artifacts.

## Build

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.x/<kit>
cmake --build build --parallel
```

Or open the top-level `CMakeLists.txt` in Qt Creator and select a Qt 6.8 or later desktop kit.

## File-format notes

Current Csound HETRO defaults to a textual format beginning with `HETRO N`. Legacy HETRO/adsyn files use signed 16-bit words: `-1` introduces amplitude tracks, `-2` frequency tracks, breakpoints are time/value pairs, and `32767` terminates a track when it appears in the time position. Legacy binary files are host-endian, so hYdra2 detects byte order while loading.

Amplitude/frequency tracks are paired by encounter order. hYdra2 writes them canonically as amplitude then frequency for each partial.

## UI behavior

The main editor deliberately keeps amplitude and frequency in separate views because both tracks use the same time/value breakpoint editing model but have very different useful vertical scales. Selection is shared across views and with the partial mixer.

Modifier keys:

- click: select one point / partial
- Shift-click: add to selection
- Ctrl-click (Cmd-click on macOS): toggle selection
- drag empty editor area: rubber-band selection

## Next useful additions

Undo/redo (`QUndoStack`), zoom/pan, breakpoint insertion/deletion, optional logarithmic frequency scale, Csound audition/resynthesis, and release packaging (DMG/MSIX/AppImage) are intentionally left outside this first implementation.

