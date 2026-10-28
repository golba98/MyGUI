# Presentation diagnostics (Linux/X11)

These are opt-in investigation tools, not normal tests or production rendering
hooks. They require a built Debug library, a C++20 compiler, pkg-config, GLFW,
FreeType, X11/XTest, Python with Pillow and NumPy, and Xvfb for isolated runs.
No additional dependencies are imposed on MyGUI.

From the repository directory:

```sh
python3 tests/diagnostics/prepare_presentation.py
```

This compiles a traced copy of the **current** ComboBox example and a minimal
GLFW control into `build/presentation-diagnostics/probe/`. The generator copies
headers, UIContext, Window and the shared example loop into that ignored folder;
it does not edit their source files. Log hooks report redraw invalidation/clear,
layout, dispatch, frame start/end, swaps, and waiting. The original loop order is
preserved. A static magenta marker is drawn with each actual frame; it never
invalidates the UI. F12 requests diagnostic buffer reads without repainting.
`MYGUI_PRESENT_TRACE` and `MYGUI_PRESENT_CAPTURE_DIR` enable the generated hooks;
the capture runner sets these explicitly. No hooks are compiled into the library.

Use a **fresh** output directory for each run. The default is native capture only:

```sh
env -u WAYLAND_DISPLAY -u WAYLAND_SOCKET XDG_SESSION_TYPE=x11 \
  LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a -s '-screen 0 1280x1024x24' \
  python3 tests/diagnostics/presentation_capture.py \
  --program build/presentation-diagnostics/probe/combobox \
  --out build/presentation-diagnostics/review-demand
```

Repeat with `--continuous` and another output directory. Both runs use the same
scene, configuration, input sequence, capture procedure and backend. Continuous
mode uses `--frames 1000000`, verifies the process stays alive throughout, and
terminates it only after capturing. It does not use a short smoke-test lifetime.
For the GLFW control use `--program .../probe/control --control`.

The runner gets the exact GLFW native client ID from the probe, waits for
`IsViewable`, checks dimensions/lifetime at each capture, translates client origin
to root coordinates, and saves client XGetImage, cropped-root XGetImage, and
Pillow captures. It decodes XImage using its depth, byte order, stride and RGB
masks. Errors are reported with drawable/request IDs and retained in JSON.
Pixel hashes and an identifiable marker are checked, rather than trusting a
previously overwritten montage or an image viewer. `--order client`, `root`, or
`pillow` selects which observer runs first. Native-only runs fail if methods differ
or the marker is missing; readback experiments retain discrepancies for analysis.

GL readback is deliberately **not** part of the default native observation:

```sh
env -u WAYLAND_DISPLAY -u WAYLAND_SOCKET XDG_SESSION_TYPE=x11 \
  LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a -s '-screen 0 1280x1024x24' \
  python3 tests/diagnostics/presentation_capture.py \
  --program build/presentation-diagnostics/probe/control --control \
  --out build/presentation-diagnostics/review-front --readbacks \
  --read-phase after-swap-front
```

Available exact phase filters: `before-swap-back`, `after-swap-front`,
`idle-after-last-swap-front`, `idle-after-last-swap-back-undefined`, or `all`.
Logs record current GLFW/GLX contexts, GLX drawable, read FBO, original/selected
read buffer, dimensions, pack alignment and GL error. Reads synchronize rendering
and may perturb the presentation being investigated. Pack alignment/read buffer
are restored; no extra swaps, glFinish, periodic paints or application sleeps are
added. Back-buffer contents after swapping are explicitly labelled undefined.
Do not use a successful back read, a black front read, or a continuously repainting
run alone as proof of native presentation.

`report.json`, `trace.log`, uniquely named PNGs and optional GL PPMs stay in the
chosen build directory. JSON includes source fingerprints, environment metadata,
per-observer hashes, refresh counts during capture, and idle swap counts. Compare
client hashes for the same labels across demand/continuous runs. Idle sampling
and bounded waits in the harness are observation timing, not repaint workarounds.

The full investigation is in [the evidence report](../../docs/presentation-investigation.md).
On a live desktop, repeat just the control with `--order client` and no Xvfb.
Xwayland root capture may fail even when client capture succeeds. In that case,
retain the error and client image; do not describe the root result as app output.
For a manual check, open the real example, select/scroll/cancel a list, open a list
in a dialog, resize, cover/uncover, then leave it idle. Verify the last scene stays
visible and no new frame is produced except on input or legitimate refresh.
