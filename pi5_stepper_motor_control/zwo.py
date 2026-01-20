#!/usr/bin/env python3
import sys, os, time, threading, logging
from datetime import datetime
import cv2
import numpy as np
import zwoasi as asi
from flask import Flask, Response, jsonify, request
from flask_cors import CORS

# Get script directory for relative paths
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler(os.path.join(SCRIPT_DIR, 'camera_stream.log')),
        logging.StreamHandler(sys.stdout)
    ]
)
logger = logging.getLogger(__name__)

# ================= CONFIGURATION =================
LIB_FILE = os.path.join(SCRIPT_DIR, 'libASICamera2.so') 

# Global State
cam_state = {
    'gain': 300,
    'exposure_val': 100,
    'exposure_mode': 'ms',
    'quality': 20,
    'resolution_width': 320,
    'resolution_height': 240
}
state_lock = threading.Lock()

camera = None
camera_lock = threading.Lock()
app = Flask(__name__)
CORS(app)

def create_error_frame(message):
    img = np.zeros((480, 640, 3), dtype=np.uint8)
    # Red text
    cv2.putText(img, "STREAM ERROR:", (50, 200), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 2)
    cv2.putText(img, str(message)[:40], (50, 240), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 1)
    if len(str(message)) > 40:
        cv2.putText(img, str(message)[40:80], (50, 270), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 1)
    
    ret, buffer = cv2.imencode('.jpg', img)
    return buffer.tobytes()

# ================= VIDEO STREAMING =================

def generate_frames():
    global camera
    init_error = None
    
    if not camera:
        try:
            logger.info("Initializing camera...")
            # Check if lib exists
            if not os.path.exists(LIB_FILE):
                raise FileNotFoundError(f"Library not found: {LIB_FILE}")
            logger.info(f"ZWO library found: {LIB_FILE}")

            asi.init(LIB_FILE)
            num_cams = asi.get_num_cameras()
            logger.info(f"Detected {num_cams} ZWO camera(s)")
            if num_cams > 0:
                camera = asi.Camera(0)
                # Set initial resolution before starting capture
                with state_lock:
                    res_width = cam_state.get('resolution_width', 1920)
                    res_height = cam_state.get('resolution_height', 1080)
                camera.set_roi(width=res_width, height=res_height)
                camera.set_control_value(asi.ASI_HIGH_SPEED_MODE, 1)
                camera.start_video_capture()
                logger.info(f"Camera initialized successfully - Resolution: {res_width}x{res_height}, Cameras: {num_cams}")
            else:
                init_error = "No ZWO Cameras found"
                logger.error(init_error)
        except Exception as e:
            init_error = str(e)
            logger.error(f"Camera initialization failed: {e}", exc_info=True)
            sys.stdout.flush()

    if not camera:
        # Yield error frame
        err_frame = create_error_frame(init_error or "Unknown Initialization Error")
        logger.warning(f"Streaming error frames due to: {init_error or 'Unknown Initialization Error'}")
        while True:
            yield (b'--frame\r\nContent-Type: image/jpeg\r\n\r\n' + err_frame + b'\r\n')
            time.sleep(1)

    applied_gain = -1
    applied_exp = -1
    # Get initial resolution to track changes
    with state_lock:
        applied_width = cam_state.get('resolution_width', 1920)
        applied_height = cam_state.get('resolution_height', 1080)

    while True:
        with state_lock:
            current_state = cam_state.copy()
            
        gain = current_state['gain']
        exp_val = current_state['exposure_val']
        exp_mode = current_state['exposure_mode']
        quality = current_state.get('quality', 70)
        res_width = current_state.get('resolution_width', 1920)
        res_height = current_state.get('resolution_height', 1080)
        
        try:
            # Apply resolution if changed (only when user actually changes it)
            if res_width != applied_width or res_height != applied_height:
                try:
                    logger.info(f"Changing resolution from {applied_width}x{applied_height} to {res_width}x{res_height}")
                    camera.stop_video_capture()
                    camera.set_roi(width=res_width, height=res_height)
                    camera.start_video_capture()
                    applied_width = res_width
                    applied_height = res_height
                    logger.info(f"Resolution changed successfully to {res_width}x{res_height}")
                    sys.stdout.flush()
                except Exception as e:
                    logger.error(f"Resolution change failed: {e}", exc_info=True)
                    sys.stdout.flush()
            
            if gain != applied_gain:
                logger.debug(f"Setting gain: {applied_gain} -> {gain}")
                camera.set_control_value(asi.ASI_GAIN, gain)
                applied_gain = gain
            
            target_us = exp_val * 1000 if exp_mode == 'ms' else exp_val
            target_us = max(1, target_us)
            
            if target_us != applied_exp:
                logger.debug(f"Setting exposure: {applied_exp}us -> {target_us}us (mode: {exp_mode})")
                camera.set_control_value(asi.ASI_EXPOSURE, target_us)
                applied_exp = target_us
            
            to_ms = int(target_us / 1000) + 500
            frame = camera.capture_video_frame(timeout=to_ms)
            
            if len(frame.shape) == 2: 
                color_frame = cv2.cvtColor(frame, cv2.COLOR_GRAY2RGB)
            else: 
                color_frame = cv2.cvtColor(frame, cv2.COLOR_BAYER_RG2RGB)

            # Resize based on quality setting to improve framerate
            # 100 = Original
            # 70-99 = 50% scale
            # < 70 = 25% scale
            if quality < 100:
                h, w = color_frame.shape[:2]
                scale = 0.5 if quality >= 70 else 0.25
                new_w, new_h = int(w * scale), int(h * scale)
                color_frame = cv2.resize(color_frame, (new_w, new_h), interpolation=cv2.INTER_LINEAR)

            encode_param = [int(cv2.IMWRITE_JPEG_QUALITY), quality]
            ret, buffer = cv2.imencode('.jpg', color_frame, encode_param)
            yield (b'--frame\r\nContent-Type: image/jpeg\r\n\r\n' + buffer.tobytes() + b'\r\n')
        except Exception as e:
            logger.error(f"Frame capture error: {e}", exc_info=True)
            continue

# ================= ROUTES =================

@app.route('/')
def index(): 
    return "ZWO Streamer Active"

@app.route('/video_feed')
def video_feed():
    logger.info(f"Video feed requested from {request.remote_addr}")
    return Response(generate_frames(), mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route('/update_settings', methods=['POST'])
def update_settings():
    d = request.json
    logger.info(f"Settings update from {request.remote_addr}: {d}")
    with state_lock:
        if 'gain' in d: cam_state['gain'] = int(d['gain'])
        if 'exposure_val' in d: cam_state['exposure_val'] = int(d['exposure_val'])
        if 'exposure_mode' in d: cam_state['exposure_mode'] = str(d['exposure_mode'])
        if 'quality' in d: cam_state['quality'] = int(d['quality'])
        if 'resolution_width' in d: cam_state['resolution_width'] = int(d['resolution_width'])
        if 'resolution_height' in d: cam_state['resolution_height'] = int(d['resolution_height'])
    logger.info(f"New camera state: {cam_state}")
    return jsonify({"status":"ok"})

if __name__ == '__main__':
    logger.info("="*50)
    logger.info("ZWO Camera Stream Server Starting")
    logger.info(f"Library: {LIB_FILE}")
    logger.info(f"Initial camera state: {cam_state}")
    logger.info("Server listening on http://0.0.0.0:5000")
    logger.info("="*50)
    app.run(host='0.0.0.0', port=5000, threaded=True)
