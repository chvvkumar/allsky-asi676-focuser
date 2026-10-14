# Pi5 Stepper Motor Focus Controller

A Flask-based web service for controlling stepper motor focus on Raspberry Pi 5, designed for AllSky camera systems and telescope focusers.

## Features

### Core Functionality
- **Precision Position Control**: Absolute position tracking with persistent storage
- **Half-Step Sequencing**: Smooth 28BYJ-48 stepper motor control (8-step sequence)
- **Variable Speed Control**: Adjustable from 50 to 600 steps/second
- **Software Limits**: Configurable maximum travel limits
- **JSON Configuration**: Persistent settings and position storage
- **Home Position**: Set and return to zero position

### Web & API
- **Modern Web Interface**: Responsive HTML5/JavaScript control panel
- **REST API**: Complete programmatic control
- **Real-time Status**: Live position updates via polling
- **Visual Feedback**: Animated motor rotation display
- **Mobile Responsive**: Works on phones, tablets, and desktops

### System Integration
- **Systemd Service**: Runs automatically on boot
- **Comprehensive Logging**: File and console logging
- **Threaded Operation**: Non-blocking motor control
- **GPIO Control**: Native gpiozero support for Pi 5
- **Emergency Stop**: Immediate movement halt

## Hardware Requirements

### Components
- **Raspberry Pi 5** (or Pi 4/3 with compatible GPIO)
- **28BYJ-48 Stepper Motor** (12V)
- **ULN2003 Driver Board** (Darlington array driver)
- **12V Power Supply** (for motor)
- **Jumper Wires** (female-to-female)

### Pin Connections

| ULN2003 Input | Pi GPIO (BCM) | Physical Pin | Wire Color (typical) |
|---------------|---------------|--------------|----------------------|
| IN1           | GPIO 26       | Pin 37       | Orange               |
| IN2           | GPIO 19       | Pin 35       | Yellow               |
| IN3           | GPIO 13       | Pin 33       | Pink                 |
| IN4         | GPIO 6        | Pin 31       | Blue                 |

### Circuit Diagram
![Pi5 Circuit Diagram](../images/Pi%20Circuit%20Diagram.png)

**Power Connections:**
- ULN2003 VCC → 12V power supply
- ULN2003 GND → Power supply GND **AND** Pi GND (Pin 6, 9, 14, 20, 25, 30, 34, or 39)
- Motor connector → ULN2003 motor port (white 5-pin JST connector)

**Important Notes:**
- **WARNING:** Do NOT power the 12V motor from the Pi's 5V pins.
- Use a separate 12V supply for the motor with a common ground to the Pi.
- GPIO pins are 3.3V but ULN2003 accepts this as logic high.

## Installation & Setup

### Method 1: Automatic Installation (Recommended)

The installation script handles everything automatically:

```bash
# Navigate to the project directory
cd pi5_stepper_motor_control

# Make install script executable
chmod +x install.sh

# Run installer (may prompt for sudo password)
./install.sh
```

**The installer will:**
1. Install system dependencies (`python3-venv`, `python3-pip`, `python3-gpiozero`).
2. Create Python virtual environment in `venv/` folder.
3. Install Flask and gpiozero packages.
4. Create systemd service file (`pi5-focuser.service`).
5. Enable service to start on boot.
6. Start the service immediately.
7. Display access instructions.

**After installation:**
- Service runs automatically on boot.
- Access web interface at `http://<pi-ip>:8080`.
- Check status: `sudo systemctl status pi5-focuser`.

### Method 2: Manual Installation

If you prefer manual setup or need customization:

```bash
# Install system dependencies
sudo apt update
sudo apt install -y python3-venv python3-pip python3-gpiozero

# Create virtual environment
python3 -m venv venv

# Activate virtual environment
source venv/bin/activate

# Install Python packages
pip install flask gpiozero

# Test the application
python app.py
```

**To run as a service manually:**

1. Create service file: `/etc/systemd/system/pi5-focuser.service`
```ini
[Unit]
Description=Pi5 Stepper Motor Focuser
After=network.target

[Service]
Type=simple
User=pi
WorkingDirectory=/home/pi/pi5_stepper_motor_control
ExecStart=/home/pi/pi5_stepper_motor_control/venv/bin/python /home/pi/pi5_stepper_motor_control/app.py
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

2. Enable and start:
```bash
sudo systemctl daemon-reload
sudo systemctl enable pi5-focuser
sudo systemctl start pi5-focuser
```

## Web Interface

Access the controller at `http://<pi-ip>:8080`

### Main Display
- **Position Gauge**: Visual bar showing current position (bidirectional from center)
- **Animated Motor**: SVG graphic rotates based on actual shaft position
- **Status Indicators**: Running state and limit warnings
- **Emergency Stop Button**: Large red button for immediate stop

### Controls

**Focus Adjustment:**
- Fine nudge: ‹ (–1) / › (+1) steps
- Coarse nudge: « (–10) / » (+10) steps
- Large steps: –1000, –100, +100, +1000 buttons
- Speed slider: 50 to 600 steps/second

**Home Functions:**
- **Go Home (0)**: Move to zero position
- **Set 0 Here**: Set current position as new zero (recalibrates)

**Tools & Configuration:**
- **Target Position**: Enter exact position to move to
- **Max Travel Limit**: Set maximum travel in both directions (±)
- **Steps Per Rotation**: Configure motor gear ratio (default: 4096)

## REST API Reference

All endpoints return JSON. POST requests require `Content-Type: application/json`.

### Status & Information

#### GET `/api/status`
Get current controller state.

**Response:**
```json
{
  "position": 1500,
  "target": 2000,
  "speed": 250,
  "running": true,
  "maxSteps": 20000,
  "stepsPerRot": 4096,
  "state": 1,
  "nearLimit": false,
  "percentage": 53.75
}
```

**Fields:**
- `state`: 0=Idle, 1=Running
- `nearLimit`: true when within 5% of limit (19000+ steps)
- `percentage`: Position as percentage (0-100, 50=center)
- `running`: true if motor is currently moving

### Movement Control

#### POST `/api/position`
Set absolute target position.

**Request:**
```json
{"position": 5000}
```

**Response:**
```json
{
  "status": "success",
  "message": "Going to 5000"
}
```

#### POST `/api/nudge`
Relative position change.

**Request:**
```json
{"steps": 100}
```

**Response:**
```json
{
  "status": "success",
  "message": "Nudging 100"
}
```

Positive values move in positive direction, negative in reverse.

#### POST `/api/zero`
Set current position as zero (home position).

**Response:**
```json
{
  "status": "success",
  "message": "Position reset to 0"
}
```

#### POST `/api/stop`
Emergency stop - immediately halt movement.

**Response:**
```json
{
  "status": "success",
  "message": "Motor stopped"
}
```

### Configuration

#### POST `/api/speed`
Set motor speed (steps per second).

**Request:**
```json
{"speed": 300}
```

**Response:**
```json
{
  "status": "success",
  "message": "Speed set to 300"
}
```

Valid range: 50-600. Configuration is automatically saved.

#### POST `/api/settings/max`
Set maximum travel limit (applies to both + and – directions).

**Request:**
```json
{"maxSteps": 25000}
```

**Response:**
```json
{
  "status": "success",
  "message": "Max limit set to 25000"
}
```

#### POST `/api/settings/stepsperrot`
Set steps per full motor rotation (gear ratio dependent).

**Request:**
```json
{"stepsPerRot": 4096}
```

**Response:**
```json
{
  "status": "success",
  "message": "Steps/Rot set to 4096"
}
```

For 28BYJ-48 with 1/64 gearbox in half-step mode: 4096 steps

## Autofocus

The web interface sweeps the focuser through a range of positions, scores the image sharpness at each position, and moves to the best one. `zwo.py` (port 5000) computes the sharpness score. `app.py` (port 8080) runs the sweep and calls `zwo.py` over HTTP. Set the environment variable `FOCUSER_CAM_URL` to change the camera service address (default `http://127.0.0.1:5000`).

### Requirements and Limitations

- The camera stream must be open in a browser. `zwo.py` captures frames only while a stream client is connected, so `/focus_metric` returns 503 when no stream is running and the sweep ends in the `error` phase.
- The sweep runs on the Pi after it starts. Closing the control page does not stop it.

### Draw the ROI

1. Open the web interface and start the camera stream.
2. Select **SET ROI**.
3. Drag a rectangle over the part of the image to focus on (a star field in night mode, a high-contrast edge in day mode).
4. Release the pointer. The rectangle is stored in the browser and reused on the next visit.
5. Select **CLEAR ROI** to remove it. With no ROI set, the web interface uses the full frame.

The ROI is sent as normalized `x`, `y`, `w`, `h` values from 0 to 1 relative to the raw frame, with the origin at the top left. When the API receives no ROI, it uses the center half of the frame (`x=0.25, y=0.25, w=0.5, h=0.5`).

### Day and Night Metrics

**Day** scores the variance of the Laplacian of the grayscale ROI. Sharper edges produce a higher variance.

**Night** detects stars in the ROI (pixels more than 5 sigma above the median background) and scores the inverse of their median flux-weighted radius, an approximation of half-flux radius (HFR). The score is `0.0` when fewer than 3 stars are found.

A higher score is better in both modes.

### Sweep Sequence

1. Record the current position as the start position.
2. Move to the lowest sweep position minus the backlash value, then to the lowest sweep position (start position minus `range`). Approaching from below removes gear backlash from the first reading.
3. Step upward by `step` to start position plus `range`. At each position, wait `settle_ms`, then capture `frames` frames and record the score.
4. Select the position with the highest score. If it is not the first or last point, fit a parabola through it and its two neighbors and use the vertex, limited to one step either side of the peak.
5. Move to the best position minus `backlash`, then to the best position, so the final approach is from below.

If every score is `0.0`, the sweep ends in the `error` phase with the message `no usable signal in ROI`. Any sweep that ends in the `error` phase returns the motor to the start position.

A stop request (STOP AF, `POST /api/autofocus/stop`, or `POST /api/stop`) ends the sweep in the `aborted` phase with the message `aborted at <pos>` and leaves the motor where it is. `start_pos` in the status response holds the original position, so move back to it manually if needed.

If the motor settles at a position other than the requested one, for example because it reached a travel limit, the sweep ends in the `error` phase.

The motor service waits up to `frames * 65 + 5` seconds for each `/focus_metric` reply, which supports night exposures up to 60 seconds per frame.

### Parameters

| Parameter | Default | Range | Notes |
|-----------|---------|-------|-------|
| `mode` | none (required) | `day`, `night` | Metric selection |
| `roi` | `{"x":0.25,"y":0.25,"w":0.5,"h":0.5}` | 0 to 1, `w` and `h` above 0 | Normalized, must lie inside the frame |
| `range` | 300 | `step` to 5000 | Steps on each side of the start position |
| `step` | 50 | 1 or more | Steps between measurements |
| `frames` | 2 | 1 to 10 | Frames captured per position, scores averaged by the camera service |
| `settle_ms` | 300 | 0 to 10000 | Delay after each move before measuring |
| `backlash` | 50 | 0 to 2000 | Steps overshot below a target before approaching it |

The web interface exposes mode, range, and step. It sends the other values as defaults.

### Autofocus Endpoints

#### POST `/api/autofocus/start`
Start a sweep. Only `mode` is required.

**Request:**
```json
{
  "roi": {"x": 0.25, "y": 0.25, "w": 0.5, "h": 0.5},
  "mode": "night",
  "range": 300,
  "step": 50,
  "frames": 2,
  "settle_ms": 300,
  "backlash": 50
}
```

**Responses:**
- 200 `{"status": "success"}`
- 400 `{"status": "error", "message": "<text>"}` when validation fails, including when `start_pos - range - backlash` is below `-max_limit` or `start_pos + range` is above `max_limit`
- 409 `{"status": "error", "message": "autofocus already running"}`
- 409 `{"status": "error", "message": "motor busy"}` when the motor is moving or has an unfinished target

#### POST `/api/autofocus/stop`
Abort the sweep. The motor stays at its current position.

**Response:**
```json
{"status": "success"}
```

#### GET `/api/autofocus/status`
Return sweep progress. `points` grows as the sweep proceeds.

**Response:**
```json
{
  "running": true,
  "phase": "sweeping",
  "mode": "night",
  "start_pos": 1200,
  "best_pos": null,
  "index": 3,
  "total": 13,
  "message": "",
  "points": [
    {"pos": 900, "score": 0.0, "stars": 1},
    {"pos": 950, "score": 0.31, "stars": 5}
  ]
}
```

**Fields:**
- `phase`: `idle`, `sweeping`, `finalizing`, `done`, `error`, or `aborted`
- `mode`: `day`, `night`, or null before the first run
- `start_pos`, `best_pos`: integers, null until set
- `index`, `total`: measurements completed and planned
- `stars`: star count in night mode, null in day mode

### Camera Service Endpoint

`zwo.py` serves `GET /focus_metric?x=&y=&w=&h=&mode=day|night&frames=N` on port 5000. `app.py` calls it for each measurement.

- It waits for `N` frames captured after the request arrives (default 1, maximum 10).
- Day response: `{"ok": true, "mode": "day", "score": <float>, "frames": N}`
- Night response: `{"ok": true, "mode": "night", "score": <float>, "frames": N, "stars": <int>, "hfr": <float|null>}`
- 503 `{"ok": false, "error": "<text>"}` when no frame arrives in time or the camera is not initialized.
- 400 `{"ok": false, "error": "<text>"}` on bad parameters.

## Configuration

### Configuration File: `config.json`

Created automatically on first run with default settings:

```json
{
  "position": 0,
  "max_limit": 20000,
  "steps_per_rot": 4096,
  "speed": 250
}
```

**Fields:**
- `position`: Last known motor position (auto-saved)
- `max_limit`: Maximum travel in both +/– directions
- `steps_per_rot`: Steps for one complete motor shaft rotation
- `speed`: Motor speed in steps per second

### Application Settings: `app.py`

Modify these constants in `app.py` for advanced configuration:

```python
# GPIO Pin Configuration
PIN_IN1 = 26
PIN_IN2 = 19
PIN_IN3 = 13
PIN_IN4 = 6

# Server Configuration
app.run(host='0.0.0.0', port=8080, debug=False)

# Logging Configuration
logging.basicConfig(level=logging.INFO, ...)
```

### Simulation Mode

If gpiozero is not available (e.g., development on non-Pi systems), the application automatically enters simulation mode:
- Motor movements are simulated
- Position tracking works normally
- Web interface fully functional
- No GPIO hardware required

## Service Management

### View Service Status
```bash
sudo systemctl status pi5-focuser
```

### View Live Logs
```bash
# Real-time log following
sudo journalctl -u pi5-focuser -f

# Last 50 lines
sudo journalctl -u pi5-focuser -n 50 --no-pager

# Since boot
sudo journalctl -u pi5-focuser -b
```

### Control Service
```bash
# Start service
sudo systemctl start pi5-focuser

# Stop service
sudo systemctl stop pi5-focuser

# Restart service
sudo systemctl restart pi5-focuser

# Enable auto-start on boot
sudo systemctl enable pi5-focuser

# Disable auto-start
sudo systemctl disable pi5-focuser
```

### Monitor Resource Usage
```bash
# Check if service is running
systemctl is-active pi5-focuser

# View service details
systemctl show pi5-focuser
```

## Default Configuration

| Parameter | Default Value | Range | Notes |
|-----------|--------------|-------|-------|
| Position | 0 | ±max_limit | Restored from config.json |
| Max Limit | ±20,000 | Any positive int | Travel limit in both directions |
| Steps/Rotation | 4,096 | Any positive int | 28BYJ-48 half-step with 1/64 gear |
| Speed | 250 steps/sec | 50-600 | Startup speed |
| Port | 8080 | 1-65535 | Web server port, override with `FOCUSER_PORT` env var |
| Log File | motor_control.log | - | Created in app directory |

## Troubleshooting

### Service Issues

**Problem:** Service won't start
```bash
# Check detailed error logs
sudo journalctl -u pi5-focuser -n 50 --no-pager

# Common issues:
# - Python virtual environment not found
# - Permission issues
# - Port already in use
```

**Problem:** Service starts but can't access web interface
```bash
# Verify service is running
sudo systemctl status pi5-focuser

# Check if port is listening
sudo netstat -tlnp | grep 8080

# Check firewall (if enabled)
sudo ufw status
```

**Problem:** Service stops unexpectedly
```bash
# View crash logs
sudo journalctl -u pi5-focuser --since "1 hour ago"

# Service auto-restarts after 10 seconds by default
```

### GPIO & Permission Issues

**Problem:** "Permission denied" errors for GPIO
```bash
# Add user to gpio group
sudo usermod -a -G gpio $USER

# Log out and back in for changes to apply
# Verify membership
groups $USER
```

**Problem:** Module not found errors
```bash
# Verify virtual environment
ls -la venv/

# Reinstall if needed
rm -rf venv/
python3 -m venv venv
source venv/bin/activate
pip install flask gpiozero
```

### Motor Issues

**Problem:** Motor doesn't move
- **Solution:** Verify wiring (IN1-IN4 connections).
- Check motor power supply (12V, adequate current).
- Verify common ground between Pi and power supply.
- Test with slow speed (50-100) first.
- Check logs: `tail -f motor_control.log`.

**Problem:** Motor vibrates but doesn't turn
- **Solution:** Wires may be swapped - try different pin order.
- Increase speed slightly (very slow can cause resonance).
- Check for mechanical binding.

**Problem:** Motor runs backwards
- **Solution:** This is normal - reverse two adjacent coil wires on ULN2003.

**Problem:** Position resets after reboot
- **Solution:** Check `config.json` file permissions.
- Ensure application can write to config file.
- Review logs for save errors.

### Configuration Issues

**Problem:** Settings don't persist
- **Solution:** Check `config.json` file exists and is writable.
- Verify application directory permissions.
- Check disk space: `df -h`.

**Problem:** Config file corrupted
```bash
# Backup corrupted file
mv config.json config.json.backup

# Let app create new default config
sudo systemctl restart pi5-focuser
```

## Logging

### Log Locations

**Application Log:**
- File: `motor_control.log` (in application directory)
- Format: `timestamp - logger - level - message`
- Rotation: Manual (consider logrotate for production)

**System Service Log:**
- View with: `sudo journalctl -u pi5-focuser`
- Persistent across reboots
- System-managed rotation

### Log Levels

The application logs:
- **INFO**: Normal operations (startup, API calls, movements)
- **WARNING**: Potential issues (limit warnings, config issues)
- **ERROR**: Failures (GPIO errors, save failures)

**View only errors:**
```bash
tail -f motor_control.log | grep ERROR
```

**View API activity:**
```bash
tail -f motor_control.log | grep "API:"
```

## Uninstallation

### Automatic Uninstallation

```bash
chmod +x uninstall.sh
./uninstall.sh
```

**The uninstaller will:**
1. Stop the service.
2. Disable auto-start.
3. Remove systemd service file.
4. Remove virtual environment.
5. Preserve application files and configuration.

### Manual Uninstallation

```bash
# Stop and disable service
sudo systemctl stop pi5-focuser
sudo systemctl disable pi5-focuser

# Remove service file
sudo rm /etc/systemd/system/pi5-focuser.service

# Reload systemd
sudo systemctl daemon-reload

# Remove virtual environment (optional)
rm -rf venv/

# Remove logs (optional)
rm motor_control.log

# Remove configuration (optional, will reset position!)
rm config.json
```

## Security Considerations

- Service runs as `pi` user (or specified user).
- No authentication implemented - **do not expose to internet**.
- Use on trusted local networks only.
- Consider SSH tunneling or VPN for remote access.
- Log files may contain position history.

## Advanced Usage

### Custom GPIO Pins

Edit `app.py` to change pin assignments:

```python
PIN_IN1 = 26  # Change to your pin
PIN_IN2 = 19
PIN_IN3 = 13
PIN_IN4 = 6
```

After changes, restart service:
```bash
sudo systemctl restart pi5-focuser
```

### Change Web Server Port

Set the `FOCUSER_PORT` environment variable. For the systemd service, add a drop-in so `install.sh` re-runs keep it:

```bash
sudo mkdir -p /etc/systemd/system/pi5-focuser.service.d
printf "[Service]\nEnvironment=FOCUSER_PORT=8081\n" | sudo tee /etc/systemd/system/pi5-focuser.service.d/port.conf
sudo systemctl daemon-reload && sudo systemctl restart pi5-focuser
```

### Integration with AllSky Camera

Use the REST API to control focus from AllSky scripts:

```bash
# Move to position
curl -X POST http://localhost:8080/api/position \
  -H "Content-Type: application/json" \
  -d '{"position": 5000}'

# Get current position
curl http://localhost:8080/api/status | jq '.position'
```

### Custom Step Sequence

Modify the `step_sequence` array in `app.py` for different motor types:

```python
step_sequence = [
    [1, 0, 0, 0], [1, 1, 0, 0], [0, 1, 0, 0], [0, 1, 1, 0],
    [0, 0, 1, 0], [0, 0, 1, 1], [0, 0, 0, 1], [1, 0, 0, 1]
]
```

## File Descriptions

| File | Purpose |
|------|---------|
| `app.py` | Main Flask application with motor control logic |
| `install.sh` | Automated installation script |
| `uninstall.sh` | Automated uninstallation script |
| `templates/index.html` | Web interface HTML template |
| `config.json` | Persistent configuration (auto-created) |
| `motor_control.log` | Application log file (auto-created) |
| `zwo.py` | ZWO camera integration utilities |
| `libASICamera2.so` | ZWO ASI Camera SDK library |

## Dependencies

**System Packages:**
- `python3-venv`: Virtual environment support
- `python3-pip`: Package installer
- `python3-gpiozero`: GPIO control library

**Python Packages:**
- `Flask`: Web framework
- `gpiozero`: GPIO interface

## License

MIT License - Free for personal and commercial use.

## Acknowledgments

- Built for AllSky camera systems
- Uses Flask web framework
- gpiozero for GPIO control
- Compatible with Raspberry Pi 5 GPIO architecture

## Related Files

- [Main Repository README](../README.md)
- [ESP32 Version](../ESP32_stepper_motor_control/README.md)

## Support

For issues, questions, or contributions, please use the GitHub repository issue tracker.

--- 

**Enjoy precise focus control on your Raspberry Pi! 🥧🔭✨**