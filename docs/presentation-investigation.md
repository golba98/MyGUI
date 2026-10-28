# Redraw-on-demand presentation investigation

## Verdict

No MyGUI presentation defect was demonstrated, and no production changes were
justified. Controlled captures establish **diagnostic/environment discrepancies**:
GL front-buffer readbacks can be black while both native captures are correct;
post-swap/idle GL reads can perturb the first resized presentation; and the live
desktop permits client capture but rejects root capture. The latter two GL effects
also occur in a minimal GLFW program containing no MyGUI code.

The originally reported intermittent **all-black Xvfb root montage was not
reproduced** with retained per-observation pixels in this investigation. Its exact
cause remains unproven. These findings must not be retroactively asserted to be
its cause. The earlier conclusion based on a back-buffer sample was insufficient.

## Controlled evidence

Baseline: HEAD `b0c42c9cad0178b88449692845ec47d8b902fb61` plus the local dialog,
popup and ComboBox work. Source fingerprints are retained in each report. The
reported 9/9 Debug/Release and 4/4 sanitizer results were treated as the baseline,
not rerun for this harness/documentation-only change.

Xvfb: X.Org server release 12101025, 1280x1024, root/client depth 24, TrueColor
visual 0x21, RGB masks ff0000/ff00/ff, 32-bit little-endian XImages. Client size is
640x520 initially and 720x560 after resize. GLFW 3.4.0 selected X11 (0x60004), GLX;
Mesa 26.2.5, llvmpipe LLVM 22.1.8, OpenGL 4.6 core. The context configuration
requested by both programs is OpenGL 3.3 core and double buffering, swap interval 1.

The eight-run matrix used ComboBox/control × demand/continuous × GL reads on/off.
All native observers agreed at every captured checkpoint. With GL reads disabled,
**all 15 ComboBox scene hashes match between continuous and demand modes**:
initial, open, highlight, commit, outside dismissal, Escape cancellation, wheel
scroll, modal popup, modal without popup, focus loss/return, resize, uncover,
unmap/remap, and idle. A static marker confirms scene extent after resizing.
No capture-induced refresh was observed in the isolated runs.

Every demand run settled to zero swaps during the final 0.5-second idle sample,
with zero refreshes during that sample. The continuous runs produced hundreds or
thousands of swaps in the same interval. Zero OS events is not an acceptance
condition; uncover/remap generated legitimate refreshes and additional frames.

Explicit GL observations used read FBO 0, original GL_BACK (1029), selected
GL_BACK before swap or GL_FRONT (1028) after swap. Logs include GLFW context,
actual GLX context/drawable and GL errors. Before-swap back images contain the
rendered scene. Front reads returned black with GL error 0 even when both native
captures showed the scene. Neither read alone establishes native presentation.
Post-swap back contents are not treated as specified/presented content.

Phase isolation on the minimal GLFW control:

| Read phase | First native resize capture |
| --- | --- |
| No GL read | Correct new marker position |
| Before-swap back only | Correct new marker position |
| After-swap front only | Old marker position |
| Idle front only | Old marker position |
| Idle back only | Old marker position |

A subsequent legitimate uncover refresh presented the correct resized scene.
This establishes an observer effect in this GLX environment, not a reason to add
swaps, synchronization or repaint timers to MyGUI. No such workaround was added.

A targeted live X11-on-Wayland desktop comparison used the NVIDIA RTX 4080,
NVIDIA 615.78.08 GLX renderer. Title matching initially found a 740x694 decoration
window (depth 32, visual 0xca). Using GLFW's native ID identified the actual
640x520 client (depth 24, visual 0x9a). Client XGetImage succeeded; root XGetImage
failed with BadMatch, request 73, on root drawable 974, despite an in-bounds crop
and a viewable live client. Root size was 11520x2160, depth 24, visual 0x9a. The
saved client image shows the control scene. A full live-desktop root comparison
therefore remains unavailable; the failure is not evidence of blank app output.

## Event-to-presentation trace

- Initial drawing and a swap return occur before the first indefinite wait.
- All exercised visible changes invalidate redraw; dispatch returns with redraw
  pending, followed by layout, drawing and a swap on the same current context.
- `UIContext::draw()` clears redraw immediately before drawing, after layout;
  no request was observed being cleared without drawing. A later request remains
  pending. Exceptions restore the flag in the existing implementation.
- Refresh events from uncover/remap set redraw and lead to presented frames.
- Drawing is followed by the existing swap; both programs then wait when settled.
- **Queue-order caveat:** the shared loop waits before draining the application
  queue. It does not drain already-queued application events before an indefinite
  wait. In the observed sequences, GLFW wait returned and events were dispatched;
  no stranded queued event caused the discrepancy. No speculative loop fix was
  made. A focused next test for that separate concern would enqueue an event
  outside polling immediately before the loop waits, with no further native event.

The probes are generated copies of current sources with opt-in trace/readback
hooks. Original UIContext, Window, renderer, example loop and controls are intact.
GL reads are off by default in the corrected harness. Native capture verifies the
actual client ID, coordinates, lifetime and viewability; it fails on mismatched
pixels/missing markers and keeps unique files rather than relying on a montage.

## Reproduction and retained evidence

See [diagnostic tool instructions](../tests/diagnostics/README.md) for dependencies,
phase filters and individual commands. Commands used for the final native pair:

```sh
python3 tests/diagnostics/prepare_presentation.py
for mode in demand continuous; do
  presentation_flags=""
  if test "$mode" = continuous; then presentation_flags="--continuous"; fi
  env -u WAYLAND_DISPLAY -u WAYLAND_SOCKET XDG_SESSION_TYPE=x11 \
    LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a -s '-screen 0 1280x1024x24' \
    python3 tests/diagnostics/presentation_capture.py \
    --program build/presentation-diagnostics/probe/combobox \
    --out "build/presentation-diagnostics/final-$mode" $presentation_flags
done
```

For the eight-way matrix, additionally select `--control` with `probe/control`,
`--continuous` for continuous mode, and `--readbacks` for GL-observer runs. To
isolate reads, add `--read-phase PHASE`. Output directories must be fresh.

Local evidence is retained under `build/presentation-diagnostics/`:
`matrix-*/report.json`, per-scene native PNGs, GL PPMs, `trace.log`, `phase-*`,
`final-*`, and desktop comparison logs/captures. Preliminary runs are separate
from the final comparisons. Python byte-compilation and warning-enabled probe
compilation pass. No package/feature checks were repeated unnecessarily.

## Limits and next experiment

The exact original all-black root symptom is unresolved, not proven harmless.
If it recurs, run the native-only harness without additional GL reads and retain
its first failing client/root images, native IDs, JSON and trace **before any
refresh or repaint**. Then reproduce that same capture order on the minimal
control. That is the missing observation required to attribute the original
symptom. Windows and macOS are outside this investigation.
