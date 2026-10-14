import time
import threading
import json
import os
import sys
import logging
import urllib.error
import urllib.parse
import urllib.request
from datetime import datetime
from flask import Flask, render_template, request, jsonify

# Get script directory for relative paths
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler(os.path.join(SCRIPT_DIR, 'motor_control.log')),
        logging.StreamHandler(sys.stdout)
    ]
)
logger = logging.getLogger(__name__)

# --- Configuration & State Management ---
CONFIG_FILE = os.path.join(SCRIPT_DIR, 'config.json')

default_config = {
    'position': 0,
    'max_limit': 20000,
    'steps_per_rot': 4096,
    'speed': 250
}

# Global config object
config = default_config.copy()

def load_config():
    global config
    if os.path.exists(CONFIG_FILE):
        try:
            with open(CONFIG_FILE, 'r') as f:
                saved = json.load(f)
                config.update(saved)
            logger.info(f"Config loaded successfully: {config}")
        except Exception as e:
            logger.error(f"Error loading config: {e}", exc_info=True)
    else:
        logger.warning(f"Config file not found, using defaults: {config}")

def save_config():
    try:
        with open(CONFIG_FILE, 'w') as f:
            json.dump(config, f)
        logger.debug(f"Config saved: {config}")
    except Exception as e:
        logger.error(f"Error saving config: {e}", exc_info=True)

# Load initial config
load_config()

# --- GPIO Setup ---
# Try to import gpiozero, mock if not available (e.g. dev on Windows)
try:
    from gpiozero import OutputDevice
    PIN_IN1 = 26
    PIN_IN2 = 19
    PIN_IN3 = 13
    PIN_IN4 = 6
    motor_pins = [OutputDevice(PIN_IN1), OutputDevice(PIN_IN2), OutputDevice(PIN_IN3), OutputDevice(PIN_IN4)]
    GPIO_AVAILABLE = True
    logger.info(f"GPIO initialized successfully on pins: IN1={PIN_IN1}, IN2={PIN_IN2}, IN3={PIN_IN3}, IN4={PIN_IN4}")
except ImportError:
    logger.warning("gpiozero not found. Running in simulation mode.")
    motor_pins = [None, None, None, None]
    GPIO_AVAILABLE = False
except Exception as e:
    logger.error(f"GPIO Error: {e}. Running in simulation mode.", exc_info=True)
    motor_pins = [None, None, None, None]
    GPIO_AVAILABLE = False

# Half-step sequence (8 steps)
step_sequence = [
    [1, 0, 0, 0], [1, 1, 0, 0], [0, 1, 0, 0], [0, 1, 1, 0],
    [0, 0, 1, 0], [0, 0, 1, 1], [0, 0, 0, 1], [1, 0, 0, 1]
]

# --- Motor Controller Thread ---
class MotorController:
    def __init__(self):
        self.target_pos = config['position']
        self.current_pos = config['position']
        self.speed = config['speed']
        self.running = False
        self.lock = threading.Lock()
        self.stop_requested = False
        self._shutdown = False
        
        logger.info(f"MotorController initialized - Position: {self.current_pos}, Speed: {self.speed}")
        self.thread = threading.Thread(target=self._loop, daemon=True)
        self.thread.start()
        logger.info("Motor control thread started")

    def _apply_step(self, step_idx):
        if not GPIO_AVAILABLE: return
        vals = step_sequence[step_idx % 8]
        for pin, val in zip(motor_pins, vals):
            if val: pin.on()
            else: pin.off()

    def _cleanup(self):
        if not GPIO_AVAILABLE: return
        for pin in motor_pins:
            pin.off()

    def _loop(self):
        while not self._shutdown:
            with self.lock:
                target = self.target_pos
                current = self.current_pos
                speed_val = self.speed
                stop_req = self.stop_requested

            # Emergency Stop
            if stop_req:
                logger.info(f"Emergency stop requested at position {self.current_pos}")
                with self.lock:
                    self.target_pos = self.current_pos
                    self.stop_requested = False
                    self.running = False
                    config['position'] = self.current_pos
                self._cleanup()
                save_config()
                logger.info("Motor stopped and position saved")
                time.sleep(0.1)
                continue

            # Movement Logic
            if current != target:
                self.running = True
                direction = 1 if target > current else -1
                
                # Check limits
                limit = config.get('max_limit', 20000)
                # If limits are enabled in logic:
                if direction > 0 and current >= limit:
                    # Hit max
                    logger.warning(f"Hit maximum limit at position {current} (limit: {limit})")
                    with self.lock: self.target_pos = current
                    continue
                if direction < 0 and current <= -limit:
                    # Hit min
                    logger.warning(f"Hit minimum limit at position {current} (limit: -{limit})")
                    with self.lock: self.target_pos = current
                    continue

                # Calculate next position
                next_pos = current + direction
                
                # Physical Move
                self._apply_step(next_pos)
                
                # Update State
                with self.lock:
                    self.current_pos = next_pos
                
                # Dynamic Delay based on speed (50-600 from UI)
                # 600 = fast, 50 = slow
                # Base delay for max speed (600) -> ~1ms
                # Base delay for min speed (50) -> ~12ms
                # Formula: delay = 0.6 / speed (approx)
                # speed 600 -> 0.001
                # speed 50 -> 0.012
                delay = 0.6 / (speed_val if speed_val > 0 else 1)
                time.sleep(delay)
            else:
                # Idle
                if self.running:
                    logger.info(f"Movement complete - Final position: {self.current_pos}")
                    self.running = False
                    self._cleanup()
                    # Sync config on stop
                    with self.lock:
                        config['position'] = self.current_pos
                    save_config()
                time.sleep(0.05)

    def set_target(self, pos):
        with self.lock:
            old_target = self.target_pos
            self.target_pos = pos
            self.stop_requested = False
        logger.info(f"Target position changed: {old_target} -> {pos} (current: {self.current_pos})")

    def nudge(self, steps):
        with self.lock:
            old_target = self.target_pos
            self.target_pos += steps
            self.stop_requested = False
        logger.info(f"Nudge requested: {steps} steps (target: {old_target} -> {self.target_pos})")

    def stop(self):
        with self.lock:
            self.stop_requested = True
        logger.info(f"Stop requested (current: {self.current_pos}, target: {self.target_pos})")

    def set_speed(self, speed):
        with self.lock:
            old_speed = self.speed
            self.speed = speed
            config['speed'] = speed
        logger.info(f"Speed changed: {old_speed} -> {speed}")
        save_config()

    def set_zero(self):
        with self.lock:
            old_pos = self.current_pos
            self.current_pos = 0
            self.target_pos = 0
            config['position'] = 0
        logger.info(f"Position reset to zero (was: {old_pos})")
        self._cleanup()
        save_config()

    def get_status(self):
        with self.lock:
            return {
                'position': self.current_pos,
                'target': self.target_pos,
                'speed': self.speed,
                'running': self.running,
                'maxSteps': config.get('max_limit', 20000),
                'stepsPerRot': config.get('steps_per_rot', 4096),
                'state': 1 if self.running else 0, # 1=Running, 0=Idle
                'nearLimit': abs(self.current_pos) >= (config.get('max_limit', 20000) * 0.95),
                'percentage': 50 + (self.current_pos / (config.get('max_limit', 20000) * 2)) * 100 
                              if config.get('max_limit') else 50
            }

CAM_URL = os.environ.get("FOCUSER_CAM_URL", "http://127.0.0.1:5000")

def fetch_metric(roi, mode, frames):
    q = urllib.parse.urlencode({'x': roi['x'], 'y': roi['y'], 'w': roi['w'], 'h': roi['h'],
                                'mode': mode, 'frames': frames})
    try:
        with urllib.request.urlopen(f"{CAM_URL}/focus_metric?{q}", timeout=frames * 6 + 5) as r:
            data = json.loads(r.read())
    except urllib.error.HTTPError as e:
        try:
            msg = json.loads(e.read()).get('error', str(e))
        except Exception:
            msg = str(e)
        raise RuntimeError(msg)
    except urllib.error.URLError as e:
        raise RuntimeError(f"camera unreachable: {e.reason}")
    if not data.get('ok'):
        raise RuntimeError(data.get('error', 'focus metric failed'))
    return data

# --- Autofocus ---
class AutoFocus:
    def __init__(self, motor, metric_fn):
        self.motor = motor
        self.metric_fn = metric_fn
        self.lock = threading.Lock()
        self.abort = False
        self.st = self._idle()

    @staticmethod
    def _idle():
        return {'running': False, 'phase': 'idle', 'mode': None, 'start_pos': None, 'best_pos': None,
                'index': 0, 'total': 0, 'message': '', 'points': []}

    def start(self, params):
        p = {'roi': {'x': 0.25, 'y': 0.25, 'w': 0.5, 'h': 0.5}, 'range': 300, 'step': 50,
             'frames': 2, 'settle_ms': 300, 'backlash': 50}
        p.update(params)
        try:
            roi = {k: float(p['roi'][k]) for k in 'xywh'}
            for k in ('range', 'step', 'frames', 'settle_ms', 'backlash'):
                p[k] = int(p[k])
        except (KeyError, TypeError, ValueError):
            raise ValueError("invalid roi or numeric parameter")
        p['roi'] = roi
        if p.get('mode') not in ('day', 'night'):
            raise ValueError("mode must be day or night")
        if p['step'] < 1: raise ValueError("step must be >= 1")
        if not p['step'] <= p['range'] <= 5000: raise ValueError("range must be between step and 5000")
        if not 1 <= p['frames'] <= 10: raise ValueError("frames must be 1-10")
        if not 0 <= p['settle_ms'] <= 10000: raise ValueError("settle_ms must be 0-10000")
        if not 0 <= p['backlash'] <= 2000: raise ValueError("backlash must be 0-2000")
        if not (roi['w'] > 0 and roi['h'] > 0 and roi['x'] >= 0 and roi['y'] >= 0
                and roi['x'] + roi['w'] <= 1 and roi['y'] + roi['h'] <= 1):
            raise ValueError("roi must be inside 0-1 with w,h > 0")
        with self.lock:
            if self.st['running']:
                raise RuntimeError("autofocus already running")
            self.abort = False
            self.st = self._idle()
            self.st.update(running=True, phase='sweeping', mode=p['mode'])
        threading.Thread(target=self._run, args=(p,), daemon=True).start()

    def stop(self):
        self.abort = True

    def status(self):
        with self.lock:
            return dict(self.st, points=list(self.st['points']))

    def _set(self, **kw):
        with self.lock:
            self.st.update(kw)

    def _move(self, pos):
        if self.abort:
            raise InterruptedError
        m = self.motor
        m.set_target(pos)
        deadline = time.time() + abs(pos - m.current_pos) * 0.6 / max(m.speed, 1) + 5
        while not (m.target_pos == m.current_pos and not m.running):
            if time.time() > deadline:
                raise RuntimeError("motor did not settle")
            time.sleep(0.02)

    def _run(self, p):
        m = self.motor
        start_pos = m.current_pos
        try:
            # ponytail: single pass + parabola; upgrade to coarse/fine two-pass if backlash exceeds step
            positions = list(range(start_pos - p['range'], start_pos + p['range'] + 1, p['step']))
            self._set(start_pos=start_pos, total=len(positions))
            self._move(positions[0] - p['backlash'])
            for pos in positions:
                self._move(pos)
                time.sleep(p['settle_ms'] / 1000)
                r = self.metric_fn(p['roi'], p['mode'], p['frames'])
                if self.abort:
                    raise InterruptedError
                with self.lock:
                    self.st['points'].append({'pos': pos, 'score': float(r['score']), 'stars': r.get('stars')})
                    self.st['index'] += 1
            pts = self.status()['points']
            if all(pt['score'] == 0.0 for pt in pts):
                self._set(running=False, phase='error', message='no usable signal in ROI')
                m.set_target(start_pos)
                return
            i = max(range(len(pts)), key=lambda k: pts[k]['score'])
            best = pts[i]['pos']
            if 0 < i < len(pts) - 1:
                y0, y1, y2 = pts[i - 1]['score'], pts[i]['score'], pts[i + 1]['score']
                d = y0 - 2 * y1 + y2
                if d < 0:
                    best = int(round(min(max(best + p['step'] * 0.5 * (y0 - y2) / d,
                                             best - p['step']), best + p['step'])))
            self._set(phase='finalizing', best_pos=best)
            self._move(best - p['backlash'])
            self._move(best)
            self._set(running=False, phase='done', message=f"best {best} score {pts[i]['score']}")
        except InterruptedError:
            m.set_target(start_pos)
            self._set(running=False, phase='aborted', message='aborted, returning to start')
        except Exception as e:
            logger.error(f"Autofocus failed: {e}", exc_info=True)
            m.set_target(start_pos)
            self._set(running=False, phase='error', message=str(e))

motor = MotorController()
autofocus = AutoFocus(motor, fetch_metric)
app = Flask(__name__)

# --- Routes ---

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/api/status', methods=['GET'])
def get_status():
    return jsonify(motor.get_status())

@app.route('/api/stop', methods=['POST'])
def api_stop():
    logger.info(f"API: Stop requested from {request.remote_addr}")
    autofocus.stop()
    motor.stop()
    return jsonify({'status': 'success', 'message': 'Motor stopped'})

@app.route('/api/autofocus/start', methods=['POST'])
def api_autofocus_start():
    try:
        autofocus.start(request.get_json(silent=True) or {})
    except ValueError as e:
        return jsonify({'status': 'error', 'message': str(e)}), 400
    except RuntimeError as e:
        return jsonify({'status': 'error', 'message': str(e)}), 409
    return jsonify({'status': 'success'})

@app.route('/api/autofocus/stop', methods=['POST'])
def api_autofocus_stop():
    autofocus.stop()
    return jsonify({'status': 'success'})

@app.route('/api/autofocus/status', methods=['GET'])
def api_autofocus_status():
    return jsonify(autofocus.status())

@app.route('/api/nudge', methods=['POST'])
def api_nudge():
    data = request.json
    steps = int(data.get('steps', 0))
    logger.info(f"API: Nudge requested from {request.remote_addr} - steps: {steps}")
    motor.nudge(steps)
    return jsonify({'status': 'success', 'message': f'Nudging {steps}'})

@app.route('/api/position', methods=['POST'])
def api_position():
    data = request.json
    pos = int(data.get('position', 0))
    logger.info(f"API: Position change requested from {request.remote_addr} - target: {pos}")
    motor.set_target(pos)
    return jsonify({'status': 'success', 'message': f'Going to {pos}'})

@app.route('/api/speed', methods=['POST'])
def api_speed():
    data = request.json
    speed = int(data.get('speed', 250))
    logger.info(f"API: Speed change requested from {request.remote_addr} - speed: {speed}")
    motor.set_speed(speed)
    return jsonify({'status': 'success', 'message': f'Speed set to {speed}'})

@app.route('/api/zero', methods=['POST'])
def api_zero():
    logger.info(f"API: Zero position requested from {request.remote_addr}")
    motor.set_zero()
    return jsonify({'status': 'success', 'message': 'Position reset to 0'})

@app.route('/api/settings/max', methods=['POST'])
def api_set_max():
    data = request.json
    val = int(data.get('maxSteps', 20000))
    logger.info(f"API: Max limit change from {request.remote_addr} - value: {val}")
    config['max_limit'] = val
    save_config()
    return jsonify({'status': 'success', 'message': f'Max limit set to {val}'})

@app.route('/api/settings/stepsperrot', methods=['POST'])
def api_set_rot():
    data = request.json
    val = int(data.get('stepsPerRot', 4096))
    logger.info(f"API: Steps/Rot change from {request.remote_addr} - value: {val}")
    config['steps_per_rot'] = val
    save_config()
    return jsonify({'status': 'success', 'message': f'Steps/Rot set to {val}'})



if __name__ == '__main__':
    logger.info("="*50)
    logger.info("Motor Control Server Starting")
    logger.info(f"GPIO Available: {GPIO_AVAILABLE}")
    logger.info(f"Initial Config: {config}")
    port = int(os.environ.get("FOCUSER_PORT", 8080))
    logger.info(f"Server listening on http://0.0.0.0:{port}")
    logger.info("="*50)
    app.run(host='0.0.0.0', port=port, debug=False)
