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
    force=True,  # zwoasi logs at import time, which would otherwise make this a no-op
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
cam_info = {}
cam_bin = 1  # hardware bin the camera is currently configured with
camera_lock = threading.Lock()

def bin_for(width, height):
    """Largest supported hardware bin whose full-frame output still covers width x height."""
    best = 1
    for b in sorted(cam_info.get('SupportedBins', [1])):
        if cam_info['MaxWidth'] // b >= width and cam_info['MaxHeight'] // b >= height:
            best = b
    return best

def fit_resize(img, width, height):
    """Scale img to fit inside width x height, preserving aspect ratio."""
    h, w = img.shape[:2]
    scale = min(width / w, height / h)
    if scale >= 1.0:
        return img
    return cv2.resize(img, (max(1, int(w * scale)), max(1, int(h * scale))), interpolation=cv2.INTER_AREA)
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
    global camera, cam_info, cam_bin
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
                cam_info = camera.get_camera_property()
                with state_lock:
                    res_width = cam_state.get('resolution_width', 1920)
                    res_height = cam_state.get('resolution_height', 1080)
                # Full sensor field, binned down; output is resized in software, never cropped.
                # RGB24 so the SDK debayers: binning a RAW bayer frame loses color.
                cam_bin = bin_for(res_width, res_height)
                camera.set_roi(bins=cam_bin, image_type=asi.ASI_IMG_RGB24)
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
            new_bin = bin_for(res_width, res_height)
            if new_bin != cam_bin:
                with camera_lock:
                    try:
                        logger.info(f"Output {res_width}x{res_height}: switching to bin {new_bin}")
                        camera.stop_video_capture()
                        camera.set_roi(bins=new_bin, image_type=asi.ASI_IMG_RGB24)
                    except Exception as e:
                        logger.error(f"Bin change to {new_bin} failed: {e}")
                    finally:
                        # set_roi can raise after stop; capture must restart either way.
                        # Record the bin regardless so a failure is not retried every frame.
                        cam_bin = new_bin
                        camera.start_video_capture()
            
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
                color_frame = cv2.cvtColor(frame, cv2.COLOR_GRAY2BGR)
            else:
                color_frame = frame  # ASI_IMG_RGB24 is already BGR
            color_frame = fit_resize(color_frame, res_width, res_height)

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
            logger.error(f"Frame capture error: {e}")
            # Yield so a disconnected client raises GeneratorExit instead of looping forever
            yield (b'--frame\r\nContent-Type: image/jpeg\r\n\r\n' + create_error_frame(e) + b'\r\n')
            time.sleep(0.5)

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
