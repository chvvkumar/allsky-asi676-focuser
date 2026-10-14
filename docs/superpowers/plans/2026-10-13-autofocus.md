# Autofocus implementation plan

Spec: in-chat design approved 2026-10-13 (single sweep + parabola fit). Pi5 version only.

## Global Constraints

- Directory: `pi5_stepper_motor_control/`. Two Flask processes already exist: `zwo.py` (camera, port 5000) and `app.py` (motor, port 8080, port from env `FOCUSER_PORT`). They stay separate processes.
- No new pip dependencies. `zwo.py` already has cv2 + numpy. `app.py` uses only stdlib (`urllib.request`, `json`, `threading`).
- Code style: match existing files (module-level globals, `logger`, plain Flask routes). No classes beyond what the task names. No docstrings longer than one line. No type-annotation sweeps of existing code.
- Do not refactor or reformat existing code outside the lines you must touch.
- Do not commit. The controller commits. Do not run git commands other than `git diff` / `git status`.
- Tests: one small self-check per task as named. No pytest fixtures, no mock libraries. Run with `python -I <file>` from `pi5_stepper_motor_control/`. On this Windows dev box `zwoasi`, `gpiozero` and the camera are absent: tests must not import the camera or GPIO.
- Comments marking a deliberate shortcut use the prefix `# ponytail:` and name the ceiling.

### API contract (binding for all tasks)

ROI is always normalized: `x, y, w, h` floats in `[0, 1]` relative to the raw captured frame, origin top-left. Default when omitted: full frame `0,0,1,1`.

`GET http://<host>:5000/focus_metric?x=&y=&w=&h=&mode=day|night&frames=N`
- Waits for `N` frames captured after the request arrived (default 1, max 10). Timeout: `N * (exposure_ms + 500) + 2000` ms, computed from `cam_state`.
- 200 `{"ok": true, "mode": "day", "score": <float>, "frames": N}` for day.
- 200 `{"ok": true, "mode": "night", "score": <float>, "frames": N, "stars": <int>, "hfr": <float|null>}` for night. `score` is `0.0` when fewer than 3 stars are found (`hfr` null).
- 503 `{"ok": false, "error": "<text>"}` when no frame arrives in time (stream not running) or camera not initialized.
- 400 `{"ok": false, "error": "<text>"}` on bad params.
- Higher `score` is always better in both modes.

`POST http://<host>:8080/api/autofocus/start` JSON body:
`{"roi": {"x":0.25,"y":0.25,"w":0.5,"h":0.5}, "mode": "day"|"night", "range": 300, "step": 50, "frames": 2, "settle_ms": 300, "backlash": 50}`
- All keys optional except `mode`. Defaults as shown. Validation: `mode` in (day, night); `step >= 1`; `range >= step`; `range <= 5000`; `1 <= frames <= 10`; `0 <= settle_ms <= 10000`; `0 <= backlash <= 2000`; ROI inside `[0,1]` with `w,h > 0`.
- 200 `{"status":"success"}` or 409 `{"status":"error","message":"autofocus already running"}` or 400 `{"status":"error","message":"<text>"}` on validation.

`POST /api/autofocus/stop` returns 200 `{"status":"success"}`. Also triggered by existing `POST /api/stop`.

`GET /api/autofocus/status` returns 200:

```
{"running": bool, "phase": "idle"|"sweeping"|"finalizing"|"done"|"error"|"aborted",
 "mode": "day"|"night"|null, "start_pos": int|null, "best_pos": int|null,
 "index": int, "total": int, "message": str,
 "points": [{"pos": int, "score": float, "stars": int|null}, ...]}
```

`points` grows as the sweep proceeds so the UI can plot live.

## Task 1: focus metric endpoint in zwo.py

File: `pi5_stepper_motor_control/zwo.py`. Add a self-check `pi5_stepper_motor_control/test_focus_metric.py`.

1. Add globals `last_gray = None`, `last_frame_seq = 0`, `frame_cond = threading.Condition()`.
2. In `generate_frames`, immediately after `color_frame` is derived from the captured frame and BEFORE `fit_resize`, store `cv2.cvtColor(color_frame, cv2.COLOR_BGR2GRAY)` into `last_gray`, increment `last_frame_seq`, and `notify_all` on `frame_cond`. Keep the grayscale at the camera's binned resolution; never the quality-downscaled one.
3. Put the metric math in pure functions that take a numpy array so the test can run without a camera:
   - `crop_roi(gray, x, y, w, h)` returns the integer-pixel slice. Clamp so the slice is at least 8x8 pixels.
   - `day_score(gray_roi) -> float`: `cv2.Laplacian(gray_roi, cv2.CV_64F).var()`.
   - `night_score(gray_roi) -> (score, stars, hfr)`: background `med = np.median(roi)`, `sigma = 1.4826 * np.median(abs(roi - med))` floored at 1.0, threshold `med + 5 * sigma`, `cv2.connectedComponentsWithStats` on the mask with 8-connectivity. Keep components with `2 <= area <= 400` pixels. For each kept component compute the flux-weighted centroid and the flux-weighted mean distance from that centroid over the component's bounding box, with flux = `roi - med` clipped at 0 and masked to the component. `hfr = median` of those radii. If stars >= 3: `score = 1.0 / hfr`; else `score = 0.0, hfr = None`. Return `(score, star_count, hfr)`.
   - `# ponytail:` comment on `night_score` noting flux-weighted mean radius stands in for HFR; replace with true half-flux radius if sweeps come out flat.
4. Route `GET /focus_metric` per the contract. It waits for `frames` frames newer than the `last_frame_seq` observed at request start, computes the score per frame, averages `score` (and `hfr`, `stars` for night) across frames. Returns 503 if `camera` is None or the wait times out.
5. `test_focus_metric.py`: builds a synthetic 200x200 float image with about 10 Gaussian stars (sigma 1.2, peak 150) on a background of 20 with Gaussian noise sigma 3, converted to uint8. Asserts: `night_score` on the sharp image reports `stars >= 5`; a blurred copy (`cv2.GaussianBlur` sigma 3) yields a lower `score`; `day_score(sharp) > day_score(blurred)`; `crop_roi(img, 0.25, 0.25, 0.5, 0.5).shape == (100, 100)`. Plain `assert` under `if __name__ == "__main__":`, prints `OK`. Importing `zwo` must not initialize the camera (today it does not; `asi.init` runs inside `generate_frames`). On the dev box `zwoasi` and `flask_cors` may be missing: the test may stub them with `sys.modules['zwoasi'] = types.SimpleNamespace()` and a stub `flask_cors` module exposing a no-op `CORS` before importing `zwo`.

## Task 2: autofocus controller in app.py

File: `pi5_stepper_motor_control/app.py`. Add a self-check `pi5_stepper_motor_control/test_autofocus.py`.

1. Add `CAM_URL = os.environ.get("FOCUSER_CAM_URL", "http://127.0.0.1:5000")`.
2. Add `class AutoFocus` constructed as `AutoFocus(motor, metric_fn)` where `metric_fn(roi, mode, frames) -> dict` returns the parsed JSON of `/focus_metric`. The production `metric_fn` (a module-level function `fetch_metric`) uses `urllib.request.urlopen` against `CAM_URL` with timeout `frames * 6 + 5` seconds; a non-200 response or `ok: false` raises `RuntimeError` carrying the error text.
3. `AutoFocus.start(params)`: validate per contract and raise `ValueError` with a message on bad input; raise `RuntimeError("autofocus already running")` if a run is active; then launch a daemon thread running `_run`.
4. `_run` sequence:
   - `start_pos = motor.current_pos`. Positions: `start_pos - range` to `start_pos + range` inclusive in `step` increments, ascending.
   - Move to `positions[0] - backlash`, wait idle. Then for each position: `motor.set_target(p)`, wait idle, `time.sleep(settle_ms / 1000)`, call `metric_fn`, append `{pos, score, stars}` to `points`, bump `index`. Check the abort flag before each move and after each metric call.
   - Wait idle: poll every 20 ms until `motor.target_pos == motor.current_pos and not motor.running`; timeout `abs(delta) * 0.6 / motor.speed + 5` seconds; on timeout raise `RuntimeError("motor did not settle")`.
   - After the sweep: if every score is `0.0`, phase `error`, message `no usable signal in ROI`, return to `start_pos` and stop.
   - Best = argmax score. If best has a neighbor on each side, fit a parabola through the three `(pos, score)` points and use its vertex, clamped to `[best - step, best + step]`, rounded to int. Otherwise use best.
   - Phase `finalizing`: move to `best_pos - backlash`, wait, move to `best_pos`, wait. Phase `done`, message `best <pos> score <score>`.
   - On abort: phase `aborted`, `motor.set_target(start_pos)`, message `aborted, returning to start`.
   - On any exception: phase `error`, message = exception text, `motor.set_target(start_pos)`.
5. `AutoFocus.stop()` sets the abort flag. `AutoFocus.status()` returns the contract dict under a lock; `points` is a copy.
6. Routes `/api/autofocus/start`, `/api/autofocus/stop`, `/api/autofocus/status` per contract. The existing `api_stop` also calls `autofocus.stop()`.
7. `test_autofocus.py`: a `FakeMotor` with `current_pos`, `target_pos`, `running`, `speed`, `set_target` (moves instantly, records every target in a list), `stop`. A fake `metric_fn` returning `{"ok": True, "score": 100 - (pos - 123) ** 2 / 100}` where `pos` is read from the fake motor. Imports `AutoFocus` from `app` with `gpiozero` absent (the module already handles that). Flask must be importable on the dev box; if not, create a venv and `pip install flask` for the test only. Case 1: `range=200, step=50, backlash=20, settle_ms=0, frames=1`, join the thread, assert phase `done`, `abs(best_pos - 123) <= 5`, `len(points) == 9`, and that the second-to-last recorded target is less than the last (final approach from below). Case 2: `metric_fn` always returns score `0.0`: phase `error`. Prints `OK`.
8. `# ponytail:` comment on the sweep: single pass + parabola; upgrade to a coarse/fine two-pass if backlash exceeds `step`.

Importing `app` must not start the Flask server (it runs only under `__main__`). It does start the motor thread; fine in simulation mode.

## Task 3: ROI + autofocus UI in index.html

File: `pi5_stepper_motor_control/templates/index.html`. No test file; the implementer verifies by opening the file in a browser with the backends absent and checking that the controls render and the ROI draw works on the placeholder (`camStatus` shows CONNECTION FAILED, which is expected).

1. Overlay: add a `<canvas id="roiCanvas">` inside the `.graphic-card` above the stream image, absolutely positioned to cover the card, `pointer-events: none` by default. Redraw the ROI box on pan, zoom, window resize, and after each draw. Map the normalized ROI to screen via `$('camStream').getBoundingClientRect()` relative to the card's rect, so CSS transform pan/zoom is handled without extra math.
2. A `SET ROI` toggle button in the View Control deck. When active: canvas gets `pointer-events: auto`, pan handlers are bypassed, mouse/touch drag on the canvas draws a rectangle; on release the ROI is stored normalized (`x,y,w,h` in `[0,1]`, clamped, minimum size 0.02) in a JS variable and `localStorage` key `af_roi`, and the toggle turns off. A `CLEAR ROI` button resets to full frame. On load, restore from `localStorage`.
3. New `AUTOFOCUS` control deck under the View Control deck in the right column: a Day/Night segmented toggle (two buttons, active one highlighted, stored in `localStorage` key `af_mode`), number inputs `Range` (default 300) and `Step` (default 50), buttons `START AF` and `STOP AF`, a status line (`phase` plus `message`), and a `<canvas id="afChart">` 100 px tall drawing score vs position as a polyline with the best point marked, redrawn from status polling.
4. `START AF` posts to `/api/autofocus/start` with `{roi, mode, range, step}` (other params use server defaults). `STOP AF` posts to `/api/autofocus/stop`. Poll `/api/autofocus/status` every 500 ms while a run is active and for 2 s after start; stop polling once `running` is false. Reuse the existing `apiCall` and `showToast` helpers where they fit; do not duplicate fetch wrappers.
5. Keep the visual language of the existing decks (`control-deck`, `deck-header`, `btn-tool`, `speed-control`, CSS variables). No new CSS framework, no external scripts.

## Task 4: README

Files: `pi5_stepper_motor_control/README.md`, and the root `README.md` API table if it lists Pi5 endpoints.

Add an `Autofocus` section after the camera/streaming material: how to draw the ROI, Day vs Night metric in two sentences each, the sweep sequence (go below range minus backlash, sweep up, parabola fit, approach best from below), the parameters with defaults, and the three endpoints with request and response shapes copied from the contract. State the limitation that the stream must be open in a browser for metrics to be available. Follow the documentation style rules: no em dashes, no emojis, no filler, active voice.
