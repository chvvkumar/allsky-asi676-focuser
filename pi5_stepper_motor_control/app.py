import time
import threading
import json
import os
import sys
import logging
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

motor = MotorController()
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
    motor.stop()
    return jsonify({'status': 'success', 'message': 'Motor stopped'})

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
    logger.info("Server listening on http://0.0.0.0:8080")
    logger.info("="*50)
    app.run(host='0.0.0.0', port=8080, debug=False)
