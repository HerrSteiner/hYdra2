# hYdra2

Heterodyne / `adsyn` analysis-file editor for Csound by Malte Steiner.

hYdra2 is the modern continuation of hYdra, originally written in 1998 for
Windows in C++/MFC, and hYdraJ, written in Java in 1998–1999. The original
programs were included on the CD-ROM accompanying Richard Boulanger's
*The Csound Book* and are preserved in the archived supplementary material at
https://github.com/SamKomesarook/The-Csound-Book.

hYdra2 is GPLv3 software and uses Qt Graphs, which is available under GPLv3 for
open-source applications.

## Current implementation

- Qt 6.9+ / Qt Quick / Qt Graphs, targeting macOS, Linux and Windows.
- GPU-backed 3D breakpoint editor using Qt Graphs `Scatter3D` and
  `Spline3DSeries`.
- X = time, Y = amplitude or frequency, Z = a fixed display-only lane for each
  partial. Partials are cascaded in depth but editing remains strictly X/Y.
- Initial orthographic front view resembles the previous 2D editor; right-drag
  rotates the graph and wheel/pinch zooms it. The **Front** button restores the
  editing view.
- GPU-backed 2D partial mixer using Qt Graphs `GraphsView` / `BarSeries`.
- Point, whole-partial and linked partial-mixer selection.
- Logic Snap (on by default) prevents breakpoints from crossing neighbors.
- The time-zero anchors are preserved when translating a complete partial in
  time; hYdra2 maintains a hold segment rather than moving the t=0 anchor.
- Global amplitude normalization.
- Non-destructive mixer gains while the document is open; gain is baked into
  amplitude values only when serializing.
- Reads current HETRO text files (`HETRO <partials>` plus comma-separated
  tracks).
- Reads legacy signed 16-bit HETRO/adsyn files, with or without the historical
  partial-count header, and auto-detects little/big endian data.
- Writes current HETRO text (`.het`) and legacy binary (`.ads`).

## Interaction

Breakpoint graph:

- left click/drag a breakpoint: select/edit time and value
- click a partial spline: select/edit the whole partial when supported by the
  Qt Graphs picker
- Shift-click: add to selection
- Ctrl-click / Cmd-click: toggle selection
- right drag: rotate camera
- wheel/pinch: zoom camera
- **Front**: return to the original orthographic front view

The Z coordinate is never edited. It only separates partials visually in 3D.
Even after rotating the graph, data dragging changes X (time) and Y
(amplitude/frequency) only.

Partial mixer:

- click a bar to select its partial
- Shift/Ctrl/Cmd selection works as in the breakpoint graph
- drag vertically to attenuate or amplify
- dragging one of several selected bars moves the selected bars together

## Build

Qt Graphs' `Spline3DSeries` is available from Qt 6.9, so hYdra2 requires Qt
6.9 or later.

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.11.2/<kit>
cmake --build build --parallel
```

Or open the top-level `CMakeLists.txt` in Qt Creator and select a desktop Qt kit
that includes the **Qt Graphs** module.

## File-format notes

Current Csound HETRO can use a textual format beginning with `HETRO N`.
Legacy HETRO/adsyn files use signed 16-bit words: `-1` introduces amplitude
tracks, `-2` frequency tracks, breakpoints are time/value pairs, and `32767`
terminates a track when it appears in the time position. hYdra2 detects byte
order while loading legacy binary files.

Amplitude/frequency tracks are paired by encounter order. hYdra2 writes them
canonically as amplitude then frequency for each partial.
