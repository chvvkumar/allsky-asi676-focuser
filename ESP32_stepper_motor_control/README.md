# ESP32-S3 Focus Controller

A feature-rich stepper motor focus controller firmware for XIAO ESP32-S3 microcontrollers, designed for telescope focusers and AllSky camera systems.

## Features

### Core Functionality
- **Precision Position Control**: Absolute position tracking with persistent storage
- **Half-Step Sequencing**: Smooth 28BYJ-48 stepper motor control (8-step sequence)
- **Variable Speed**: Adjustable from 50 to 600 steps/second
- **Safety Limits**: Configurable soft and hard position limits
- **Non-Volatile Memory**: Position and settings survive power cycles
- **Home Position**: Set and return to zero position

### Connectivity
- **WiFiManager**: Easy WiFi configuration via captive portal
- **Web Interface**: Modern, responsive browser-based control
- **WebSocket**: Real-time position updates (port 81)
- **REST API**: Complete programmatic control
- **ElegantOTA**: Over-the-air firmware updates

### Advanced Features
- **Error Logging**: Tracks last 50 errors with timestamps
- **Watchdog Timer**: Auto-recovery from hangs (10 second timeout)
- **Input Validation**: JSON parsing with overflow protection
- **Visual Feedback**: Animated motor rotation display
- **Mobile Responsive**: Works on phones, tablets, and desktops

## Hardware Requirements

### Components
- **XIAO ESP32-S3** or compatible ESP32 development board
- **28BYJ-48 Stepper Motor** (12V, typically rated at 12VDC)
- **ULN2003 Driver Board** (Darlington array driver)
- **12V Power Supply** (1A+ recommended for motor)
- **Jumper Wires**

### Pin Connections

| ULN2003 Input | ESP32-S3 GPIO | Wire Color (typical) |
|---------------|---------------|----------------------|
| IN1           | GPIO 1        | Orange               |
| IN2           | GPIO 2        | Yellow               |
| IN3           | GPIO 3        | Pink                 |
| IN4         | GPIO 4        | Blue                 |

### Circuit Diagram
![ESP32 Circuit Diagram](../images/ESP32%20Circuit%20Diagram.png)

**Power Connections:**
- ULN2003 VCC → 12V power supply
- ULN2003 GND → Power supply GND **AND** ESP32 GND (common ground required!)
- Motor connector → ULN2003 motor port (usually a white JST connector)

**Important Notes:**
- The ESP32 GPIO pins operate at 3.3V but the ULN2003 accepts this as logic high.
- Do not power the motor from the ESP32 - use a separate 12V supply.
- Common ground between ESP32 and motor power supply is critical.

## Required Arduino Libraries

Install these libraries via Arduino Library Manager (Sketch → Include Library → Manage Libraries):

1. **ArduinoJson** by Benoit Blanchon (v6.x)
2. **WebSockets** by Markus Sattler (v2.x)
3. **WiFiManager** by tzapu (v2.x)
4. **ElegantOTA** by Ayush Sharma (v3.x)

Standard ESP32 libraries (included with ESP32 board support):
- WiFi
- WebServer
- Preferences
- esp_task_wdt

## Installation & Setup

### 1. Arduino IDE Setup

1. Install **Arduino IDE** (version 2.x recommended).
2. Add ESP32 board support:
   - File → Preferences
   - Additional Board Manager URLs: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - Tools → Board → Boards Manager → Search "ESP32" → Install "esp32 by Espressif Systems"
3. Select your board:
   - Tools → Board → ESP32 Arduino → **XIAO_ESP32S3**
   - Tools → Port → Select your ESP32's COM port

### 2. Upload Firmware

1. Open `stepper_motor.ino` in Arduino IDE.
2. Install all required libraries (see above).
3. Verify/Compile (check button) to check for errors.
4. Upload (arrow button) to your ESP32.
5. Open Serial Monitor (115200 baud) to view startup messages.

### 3. WiFi Configuration

**First Boot (AP Mode):**
1. ESP32 creates an access point: **FocusController-AP**
2. Default password: `12345678`
3. Connect to this network from phone/computer.
4. Captive portal should open automatically (or go to `192.168.4.1`).
5. Select your WiFi network and enter password.
6. ESP32 will restart and connect to your network.

**After Configuration:**
- Serial Monitor shows the assigned IP address.
- Access web interface: `http://<ip-address>`
- WebSocket connects automatically on port 81.

### 4. Reset WiFi Settings

To clear saved WiFi and reconfigure:
1. Use the ElegantOTA interface.
2. Or reflash firmware with WiFiManager reset enabled.

## Web Interface

Access the controller at `http://<esp32-ip-address>`

### Main Display
- **Position Gauge**: Visual bar showing current position (bidirectional from center)
- **Animated Motor**: SVG graphic rotates based on actual shaft position
- **Status Indicators**: Connection, running state, limit warnings
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
- **Update Firmware**: Access ElegantOTA interface
- **Reboot Device**: Software restart

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
  "state": 1,
  "running": true,
  "maxSteps": 20000,
  "stepsPerRot": 4096,
  "nearLimit": false,
  "percentage": 53.75
}
```

**Fields:**
- `state`: 0=Idle, 1=Running, 2=Stopped, 3=Emergency Stop
- `nearLimit`: true when within 500 steps of soft limit
- `percentage`: Position as percentage (0-100, 50=center)

#### GET `/api/logs`
Retrieve recent error log.

**Response:**
```json
[
  {
    "timestamp": 123456789,
    "position": 1000,
    "target": 1500,
    "speed": 200,
    "state": 1,
    "error": 0
  }
]
```

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
  "message": "Moving to position"
}
```

**Error Response (hard limit):**
```json
{
  "status": "error",
  "message": "Position out of range",
  "errorCode": 7
}
```

#### POST `/api/nudge`
Relative position change.

**Request:**
```json
{"steps": 100}
```

Positive values move in positive direction, negative in reverse.

#### POST `/api/zero`
Set current position as zero (home position).

**Response:**
```json
{
  "status": "success",
  "message": "Position zeroed"
}
```

#### POST `/api/stop`
Emergency stop - immediately halt movement.

**Response:**
```json
{
  "status": "success",
  "message": "Emergency stop"
}
```

### Configuration

#### POST `/api/speed`
Set motor speed (steps per second).

**Request:**
```json
{"speed": 300}
```

Valid range: 50-600. Values outside range are constrained.

#### POST `/api/settings/max`
Set maximum travel limit (applies to both + and – directions).

**Request:**
```json
{"maxSteps": 25000}
```

#### POST `/api/settings/stepsperrot`
Set steps per full motor rotation (gear ratio dependent).

**Request:**
```json
{"stepsPerRot": 4096}
```

For 28BYJ-48 with 1/64 gearbox in half-step mode: 4096 steps

### System Control

#### POST `/api/reboot`
Reboot the ESP32 controller.

**Response:**
```json
{
  "status": "success",
  "message": "Rebooting..."
}
```

Device restarts after 500ms delay.

#### GET `/update`
Access ElegantOTA web interface for firmware updates.

## WebSocket Protocol

Connect to `ws://<esp32-ip>:81` for real-time updates.

**Server → Client Messages:**
The server broadcasts status updates every 100ms (same format as `/api/status`):

```json
{
  "position": 1500,
  "target": 2000,
  "speed": 250,
  "state": 1,
  "running": true,
  "maxSteps": 20000,
  "stepsPerRot": 4096,
  "nearLimit": false,
  "percentage": 53.75
}
```

**Client → Server Messages:**
Currently reserved for future command functionality.

**Connection Events:**
- Initial connection sends current status immediately.
- Client disconnect/reconnect handled automatically.
- Fallback to HTTP polling if WebSocket unavailable.

## Configuration Files

### Config.h
Contains all system constants:
- Pin definitions
- Timing intervals
- Default motor parameters
- Error codes and state enums
- Step sequence for ULN2003

**Key Constants:**
```cpp
#define DEFAULT_MAX_STEPS 20000
#define DEFAULT_STEPS_PER_ROTATION 4096
#define DEFAULT_SPEED 100
#define MIN_SPEED 50
#define MAX_SPEED 600
#define SOFT_LIMIT_WARNING 500
```

### StepperMotor.h
Motor control class with:
- Position tracking and validation
- Speed control with constraints
- Half-step sequence execution
- Safety limit checking
- Emergency stop functionality

### Logger.h
Error logging system:
- Circular buffer (50 entries)
- Timestamp, position, state tracking
- JSON export for web API
- Error code enumeration

### web_interface.h
Complete HTML/CSS/JavaScript web interface:
- Embedded in PROGMEM (flash storage)
- WebSocket client code
- REST API calls with error handling
- SVG motor animation
- Responsive mobile design

## Default Configuration

| Parameter | Default Value | Range | Notes |
|-----------|--------------|-------|-------|
| Max Steps | ±20,000 | Any positive int | Travel limit in both directions |
| Steps/Rotation | 4,096 | Any positive int | 28BYJ-48 half-step with 1/64 gear |
| Speed | 100 steps/sec | 50-600 | Startup speed |
| Soft Limit Zone | 500 steps | Fixed | Warning before hitting hard limit |
| WiFi AP Name | FocusController-AP | - | Default access point name |
| WiFi AP Password | 12345678 | - | Default AP password |
| HTTP Port | 80 | - | Web interface port |
| WebSocket Port | 81 | - | Real-time updates port |

## Troubleshooting

### WiFi Issues

**Problem:** Can't find FocusController-AP
- **Solution:** Ensure fresh boot, wait 30 seconds for AP to start.
- Check WiFiManager timeout hasn't expired (3 minutes).
- Reflash firmware if WiFi credentials corrupted.

**Problem:** Connected to AP but can't open config page
- **Solution:** Try manual navigation to `192.168.4.1`.
- Disable mobile data on phone.
- Clear browser cache.

**Problem:** Won't connect to home WiFi
- **Solution:** Check SSID and password (case-sensitive).
- Ensure 2.4GHz network (ESP32 doesn't support 5GHz).
- Check WiFi signal strength.

### Motor Issues

**Problem:** Motor doesn't move
- **Solution:** Verify wiring (IN1-IN4 connections).
- Check motor power supply (12V, adequate current).
- Verify common ground between ESP32 and power supply.
- Test with slow speed (50-100) first.

**Problem:** Motor vibrates but doesn't turn
- **Solution:** Wires may be swapped - try different pin order.
- Increase speed slightly (very slow can cause resonance).
- Check for mechanical binding.

**Problem:** Motor runs backwards
- **Solution:** This is normal - swap any two adjacent coil wires on ULN2003.

**Problem:** Position jumps or resets
- **Solution:** Position stored in Preferences (NVS).
- If using battery-backed RTC, check CR2032 battery.
- May occur during firmware update - recalibrate with "Set 0 Here".

### Software Issues

**Problem:** Webpage won't load
- **Solution:** Verify IP address (check Serial Monitor).
- Ping ESP32 from computer.
- Try different browser.
- Clear browser cache.

**Problem:** WebSocket shows "Disconnected"
- **Solution:** HTTP works but WebSocket doesn't - port 81 may be blocked.
- Fallback polling will activate automatically.
- Check firewall settings.

**Problem:** Device reboots unexpectedly
- **Solution:** Watchdog timer triggered (10 sec timeout).
- Check Serial Monitor for error messages.
- May indicate power supply issues.
- Review error logs via `/api/logs`.

**Problem:** Compile errors in Arduino IDE
- **Solution:** Verify all libraries installed (exact names matter).
- Check ESP32 board support package version (2.x or 3.x).
- Some libraries may need updates for ESP32 Core 3.x.

### Position/Limit Issues

**Problem:** "Position out of range" error
- **Solution:** Requested position exceeds ±maxSteps limit.
- Adjust maxSteps in settings if needed.
- Use "Set 0 Here" to recalibrate if position offset.

**Problem:** "Near soft limit" warning appears
- **Solution:** Position within 500 steps of hard limit.
- This is advisory - movement still allowed.
- Review mechanical setup if frequently hitting limits.

## Error Codes

| Code | Name | Description |
|------|------|-------------|
| 0 | ERROR_NONE | No error |
| 1 | ERROR_INVALID_POSITION | Position validation failed |
| 2 | ERROR_INVALID_SPEED | Speed out of valid range |
| 3 | ERROR_INVALID_JSON | JSON parsing error |
| 4 | ERROR_BUFFER_OVERFLOW | Input too large (>1024 bytes) |
| 5 | ERROR_POSITION_CORRUPTED | Saved position invalid on boot |
| 6 | ERROR_SOFT_LIMIT_WARNING | Near soft limit zone |
| 7 | ERROR_HARD_LIMIT | At or beyond hard limit |
| 8 | ERROR_WIFI_FAILED | WiFi connection failure |

## Security Considerations

- No authentication implemented - **do not expose to internet**.
- Use on trusted local networks only.
- WiFiManager AP is password-protected but simple.
- Consider using a VPN for remote access.
- OTA updates have no authentication in ElegantOTA.

## Advanced Usage

### Custom Step Sequences

Modify `Config.h` to change the step sequence:

```cpp
const int stepSequence[8][4] = {
  {1, 0, 0, 0},  // Step 0
  {1, 1, 0, 0},  // Step 1
  // ... modify as needed
};
```

### Adjusting Watchdog Timer

In `setup()`, modify timeout (ESP32 Core 3.x):

```cpp
esp_task_wdt_config_t wdt_config = {
  .timeout_ms = 10000,  // Change timeout here (milliseconds)
  .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
  .trigger_panic = true
};
```

### Custom Web Interface

Edit `web_interface.h` - entire HTML/CSS/JS is in `HTML_PAGE` constant.
Remember: stored in PROGMEM, keep size reasonable (<64KB recommended).

## File Descriptions

| File | Purpose |
|------|---------|
| `stepper_motor.ino` | Main program, WiFi setup, web server, API handlers |
| `Config.h` | Configuration constants, pin definitions, data structures |
| `StepperMotor.h` | Motor control class implementation |
| `Logger.h` | Error logging system |
| `web_interface.h` | Complete web UI (HTML/CSS/JavaScript) |
| `stepper_motor.ino.old` | Previous version (backup) |
| `web_interface.h.old` | Previous UI version (backup) |

## License

MIT License - Free for personal and commercial use.

## Acknowledgments

- Built for AllSky camera systems
- Uses WiFiManager by tzapu
- ElegantOTA by Ayush Sharma
- ArduinoJson by Benoit Blanchon
- WebSockets by Markus Sattler

## Support

For issues, questions, or contributions, please use the GitHub repository issue tracker.