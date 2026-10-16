# MyGUI

A small C++20 GUI library and demo using GLFW, OpenGL 3.3, and FreeType.
It supports labels, buttons, horizontal and vertical layouts, nested clipping,
and UTF-8 text with mouse and keyboard interaction.

Linux is the tested platform. Windows and macOS are not currently tested.

## Build and run

### 1. Install dependencies

You need Git, a C++20 compiler, CMake 3.20 or newer, Ninja, and development files
for GLFW, OpenGL, and FreeType.

Ubuntu/Debian:

```sh
sudo apt-get update
sudo apt-get install git build-essential cmake ninja-build libglfw3-dev libgl1-mesa-dev libfreetype-dev
```

Fedora:

```sh
sudo dnf install git gcc-c++ cmake ninja-build glfw-devel mesa-libGL-devel freetype-devel
```

### 2. Get the source

```sh
git clone https://github.com/golba98/MyGUI.git
cd MyGUI
```

If you already have the source, open a terminal in the repository folder.
Run the remaining commands from that folder.

### 3. Build and launch the demo

```sh
cmake --preset debug
cmake --build --preset debug
./build/debug/MyGUI
```

The demo opens a window with buttons, a click counter, and overlapping panels.
Font assets are bundled and copied beside the executable, so you can also launch
it from another working directory.

For a Release build:

```sh
cmake --preset release
cmake --build --preset release
./build/release/MyGUI
```

## Try the demo

| Control | Action |
| --- | --- |
| Click a button | Toggle the overlay or increase the counter |
| Tab / Shift+Tab | Move keyboard focus forward / backward |
| Enter / Space | Activate the focused button |
| V | Toggle the overlay |
| Hover the overlay | Change its color |
| Resize the window | See the layout adjust |

The disabled button does not respond. Input diagnostics show mouse movement,
scrolling, and key/button state.

Optional command-line arguments:

| Option | Purpose |
| --- | --- |
| `--help` | Show available arguments |
| `--font PATH` | Use a different scalable Unicode font |
| `--width N --height N` | Set the starting window size |
| `--log-events` | Print input and window events to the terminal |
| `--frames N` | Exit after N frames for a smoke test |

For example:

```sh
./build/debug/MyGUI --width 960 --height 640 --log-events
```

## Run tests

After building Debug, run all tests:

```sh
ctest --preset debug
```

Renderer and window tests need a display. To run all tests without one on
Ubuntu/Debian, use Xvfb and software rendering:

```sh
sudo apt-get install xvfb xauth
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a ctest --preset debug
```

To run only the input, widget, layout, and font tests without a display:

```sh
ctest --preset debug -L headless
```

For a Release build, use `ctest --preset release` instead.
See [test details](docs/usage.md#test-details) for CI coverage, disabling tests,
and renderer snapshots.

## Use the library

Read the [usage reference](docs/usage.md) for widget examples, event handling,
layout, rendering, and resource lifetimes. The [demo source](src/main.cpp) shows
how to put these together in an application.

The library owns its widget trees. Public headers hide native GLFW, OpenGL, and
FreeType types. Window operations, input handling, and drawing run on the main
thread; the reference explains the required event loop and resource ordering.

## Current limits

Text editing, scrolling controls, automatic line wrapping, complex-script shaping,
bidirectional layout, color emoji, and native accessibility integration are not
included. Labels and buttons clip text that exceeds their bounds.

The bundled Noto Sans font uses the SIL Open Font License. See its
[source](assets/fonts/SOURCE.md) and [license](assets/fonts/OFL.txt).
