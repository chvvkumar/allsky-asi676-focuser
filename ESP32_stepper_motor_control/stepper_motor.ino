/*
 * XIAO ESP32-S3 Focus Controller - Refactored
 * Features: WebSocket, WiFiManager, ArduinoJson, Acceleration, Logging
 * 
 * Required Libraries (install via Arduino Library Manager):
 * - ArduinoJson by Benoit Blanchon
 * - WebSockets by Markus Sattler
 * - WiFiManager by tzapu
 * - ElegantOTA by Ayush Sharma
 * 
 * Pin Connections (XIAO ESP32S3 and 28BYJ-48 Stepper Motor):
 * - GPIO1 -> ULN2003 IN1
 * - GPIO2 -> ULN2003 IN2
 * - GPIO3 -> ULN2003 IN3
 * - GPIO4 -> ULN2003 IN4
 */

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFiManager.h>
#include <ElegantOTA.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>
#include "Config.h"
#include "StepperMotor.h"
#include "Logger.h"
#include "web_interface.h"

// ----------------------------------------------------------------
// Global Objects
// ----------------------------------------------------------------
WebServer server(80);
WebSocketsServer webSocket(81);
Preferences preferences;
StepperMotor motor;
Logger logger;
WiFiManager wifiManager;

// ----------------------------------------------------------------
// Global State
// ----------------------------------------------------------------
bool wifiConnected = false;
unsigned long lastPositionSave = 0;
unsigned long lastStatusUpdate = 0;
unsigned long lastWiFiCheck = 0;
unsigned long lastLogEntry = 0;

// ----------------------------------------------------------------
// Function Prototypes
// ----------------------------------------------------------------
void setupWiFi();
void setupWebServer();
void setupWebSocket();
void handleWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
void broadcastStatus();
void handleRoot();
void handleGetStatus();
void handleSetPosition();
void handleSetSpeed();
void handleNudge();
void handleZero();
void handleReboot();
void handleSetMaxSteps();
void handleSetStepsPerRotation();
void handleEmergencyStop();
void handleSetProfile();
void handleGetLogs();
String createStatusJSON();
ErrorCode parseJSONRequest(const String& body, JsonDocument& doc);
void sendJSONResponse(int code, const char* status, const char* message = nullptr, ErrorCode error = ERROR_NONE);
void checkAndReconnectWiFi();
bool validateAndSavePosition();

// ----------------------------------------------------------------
// Setup
// ----------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n\n╔═══════════════════════════════════════╗");
  Serial.println("║  ESP32-S3 Focus Controller v2.0      ║");
  Serial.println("║  Enhanced with WebSocket & Safety    ║");
  Serial.println("╚═══════════════════════════════════════╝");

  // Note: Watchdog will be configured AFTER WiFi setup to avoid timeout during WiFiManager
  
  // Initialize preferences
  preferences.begin("stepper", false);
  
  // Load motor configuration
  MotorConfig motorConfig;
  motorConfig.maxSteps = preferences.getInt("maxSteps", DEFAULT_MAX_STEPS);
  motorConfig.stepsPerRotation = preferences.getInt("stepsPerRot", DEFAULT_STEPS_PER_ROTATION);
  motorConfig.defaultSpeed = preferences.getInt("speed", DEFAULT_SPEED);
  motorConfig.minSpeed = MIN_SPEED;
  motorConfig.maxSpeed = MAX_SPEED;
  motorConfig.softLimitWarning = SOFT_LIMIT_WARNING;
  
  // Initialize motor
  motor.begin(motorConfig);
  
  // Load and validate saved position
  int savedPosition = preferences.getInt("position", 0);
  ErrorCode posError = motor.validatePosition(savedPosition);
  
  if (posError == ERROR_NONE || posError == ERROR_SOFT_LIMIT_WARNING) {
    motor.setCurrentPosition(savedPosition);
    Serial.println("✓ Position restored: " + String(savedPosition));
  } else {
    Serial.println("✗ Saved position corrupted, resetting to 0");
    motor.setCurrentPosition(0);
    logger.log(0, 0, 0, STATE_IDLE, ERROR_POSITION_CORRUPTED);
  }
  
  // Setup WiFi with WiFiManager
  setupWiFi();
  
  // Setup WebSocket
  setupWebSocket();
  
  // Setup Web Server
  setupWebServer();

  // Initialize ElegantOTA
  ElegantOTA.begin(&server);
  Serial.println("✓ ElegantOTA initialized");
  
  // Start web server
  server.begin();
  Serial.println("✓ Web server started on port 80");
  Serial.println("✓ WebSocket server started on port 81");
  
  if (wifiConnected) {
    Serial.println("\n► Access at: http://" + WiFi.localIP().toString());
  } else {
    Serial.println("\n► Access at: http://192.168.4.1");
  }
  
  Serial.println("═══════════════════════════════════════\n");
  
  // Configure watchdog timer AFTER setup completes (10 seconds) - ESP32 Core 3.x API
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = 10000,
    .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
    .trigger_panic = true
  };
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);
  Serial.println("✓ Watchdog timer configured");
}

// ----------------------------------------------------------------
// Main Loop
// ----------------------------------------------------------------
void loop() {
  // Reset watchdog
  esp_task_wdt_reset();
  
  // Handle clients
  server.handleClient();
  webSocket.loop();
  ElegantOTA.loop();
  
  // Update motor
  motor.update();
  
  // Periodic tasks
  unsigned long now = millis();
  
  // Save position periodically
  if (now - lastPositionSave > POSITION_SAVE_INTERVAL) {
    lastPositionSave = now;
    if (validateAndSavePosition()) {
      // Position saved successfully
    }
  }
  
  // Broadcast status via WebSocket
  if (now - lastStatusUpdate > STATUS_UPDATE_INTERVAL) {
    lastStatusUpdate = now;
    broadcastStatus();
  }
  
  // Log state periodically (every second)
  if (now - lastLogEntry > 1000) {
    lastLogEntry = now;
    if (motor.isRunning() || motor.isNearSoftLimit()) {
      ErrorCode error = motor.isNearSoftLimit() ? ERROR_SOFT_LIMIT_WARNING : ERROR_NONE;
      logger.log(motor.getCurrentPosition(), motor.getTargetPosition(), 
                 motor.getSpeed(), motor.getState(), error);
    }
  }
  
  // Check WiFi connection
  if (now - lastWiFiCheck > WIFI_CHECK_INTERVAL) {
    lastWiFiCheck = now;
    checkAndReconnectWiFi();
  }
}

// ----------------------------------------------------------------
// WiFi Setup with WiFiManager
// ----------------------------------------------------------------
void setupWiFi() {
  // Disable watchdog during WiFi setup to prevent timeout
  esp_task_wdt_deinit();
  
  // Set custom AP name
  wifiManager.setConfigPortalTimeout(180); // 3 minute timeout
  
  Serial.println("⚡ Starting WiFi...");
  
  // Try to connect to saved WiFi or start config portal
  if (wifiManager.autoConnect("FocusController-AP", "12345678")) {
    wifiConnected = true;
    Serial.println("✓ WiFi Connected!");
    Serial.println("  IP: " + WiFi.localIP().toString());
    Serial.println("  SSID: " + WiFi.SSID());
  } else {
    wifiConnected = false;
    Serial.println("✗ WiFi connection failed");
    Serial.println("  Started AP mode: FocusController-AP");
    Serial.println("  Password: 12345678");
  }
}

// ----------------------------------------------------------------
// Non-blocking WiFi Reconnect
// ----------------------------------------------------------------
void checkAndReconnectWiFi() {
  if (WiFi.status() != WL_CONNECTED && wifiConnected) {
    Serial.println("⚠ WiFi disconnected, attempting reconnect...");
    wifiConnected = false;
    
    // Non-blocking reconnect attempt
    WiFi.reconnect();
    
    // Check after a short delay
    delay(100);
    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      Serial.println("✓ WiFi reconnected");
    }
  }
}

// ----------------------------------------------------------------
// WebSocket Setup
// ----------------------------------------------------------------
void setupWebSocket() {
  webSocket.begin();
  webSocket.onEvent(handleWebSocketEvent);
}

// ----------------------------------------------------------------
// WebSocket Event Handler
// ----------------------------------------------------------------
void handleWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("WebSocket [%u] Disconnected\n", num);
      break;
      
    case WStype_CONNECTED: {
      IPAddress ip = webSocket.remoteIP(num);
      Serial.printf("WebSocket [%u] Connected from %s\n", num, ip.toString().c_str());
      // Send initial status
      String statusJSON = createStatusJSON();
      webSocket.sendTXT(num, statusJSON);
      break;
    }
    
    case WStype_TEXT: {
      // Handle incoming WebSocket commands
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, payload, length);
      
      if (error) {
        Serial.println("WebSocket JSON parse error");
        return;
      }
      
      const char* cmd = doc["cmd"];
      if (cmd) {
        Serial.printf("WebSocket command: %s\n", cmd);
        // Commands can be added here if needed
      }
      break;
    }
    
    default:
      break;
  }
}

// ----------------------------------------------------------------
// Broadcast Status to All WebSocket Clients
// ----------------------------------------------------------------
void broadcastStatus() {
  String status = createStatusJSON();
  webSocket.broadcastTXT(status);
}

// ----------------------------------------------------------------
// Create Status JSON String
// ----------------------------------------------------------------
String createStatusJSON() {
  StaticJsonDocument<512> doc;
  
  doc["position"] = motor.getCurrentPosition();
  doc["target"] = motor.getTargetPosition();
  doc["speed"] = motor.getSpeed();
  doc["state"] = motor.getState();
  doc["running"] = motor.isRunning();
  doc["maxSteps"] = motor.getMaxSteps();
  doc["stepsPerRot"] = motor.getStepsPerRotation();
  doc["nearLimit"] = motor.isNearSoftLimit();
  
  // Calculate percentage
  float rangeWidth = 2.0 * (float)motor.getMaxSteps();
  float position = ((float)motor.getCurrentPosition() + (float)motor.getMaxSteps()) / rangeWidth;
  position = constrain(position, 0.0, 1.0);
  doc["percentage"] = position * 100.0;
  
  String output;
  serializeJson(doc, output);
  return output;
}

// ----------------------------------------------------------------
// Web Server Setup
// ----------------------------------------------------------------
void setupWebServer() {
  // Enable CORS
  server.enableCORS(true);
  
  // Routes
  server.on("/", handleRoot);
  server.on("/api/status", HTTP_GET, handleGetStatus);
  server.on("/api/position", HTTP_POST, handleSetPosition);
  server.on("/api/speed", HTTP_POST, handleSetSpeed);
  server.on("/api/nudge", HTTP_POST, handleNudge);
  server.on("/api/zero", HTTP_POST, handleZero);
  server.on("/api/stop", HTTP_POST, handleEmergencyStop);
  server.on("/api/reboot", HTTP_POST, handleReboot);
  server.on("/api/settings/max", HTTP_POST, handleSetMaxSteps);
  server.on("/api/settings/stepsperrot", HTTP_POST, handleSetStepsPerRotation);
  server.on("/api/logs", HTTP_GET, handleGetLogs);
}

// ----------------------------------------------------------------
// Web Server Handlers
// ----------------------------------------------------------------
void handleRoot() {
  server.send(200, "text/html", getHTML());
}

void handleGetStatus() {
  server.send(200, "application/json", createStatusJSON());
}

void handleSetPosition() {
  StaticJsonDocument<100> doc;
  ErrorCode error = parseJSONRequest(server.arg("plain"), doc);
  
  if (error != ERROR_NONE) {
    sendJSONResponse(400, "error", "Invalid JSON", error);
    return;
  }
  
  if (!doc.containsKey("position")) {
    sendJSONResponse(400, "error", "Missing position parameter");
    return;
  }
  
  int pos = doc["position"];
  error = motor.validatePosition(pos);
  
  if (error == ERROR_HARD_LIMIT) {
    sendJSONResponse(400, "error", "Position out of range", error);
    return;
  }
  
  motor.setTargetPosition(pos);
  
  if (error == ERROR_SOFT_LIMIT_WARNING) {
    sendJSONResponse(200, "warning", "Near soft limit", error);
  } else {
    sendJSONResponse(200, "success");
  }
}

void handleSetSpeed() {
  StaticJsonDocument<100> doc;
  ErrorCode error = parseJSONRequest(server.arg("plain"), doc);
  
  if (error != ERROR_NONE || !doc.containsKey("speed")) {
    sendJSONResponse(400, "error", "Invalid request", ERROR_INVALID_SPEED);
    return;
  }
  
  int speed = doc["speed"];
  motor.setSpeed(speed);
  preferences.putInt("speed", speed);
  
  sendJSONResponse(200, "success");
}

void handleNudge() {
  StaticJsonDocument<100> doc;
  ErrorCode error = parseJSONRequest(server.arg("plain"), doc);
  
  if (error != ERROR_NONE || !doc.containsKey("steps")) {
    sendJSONResponse(400, "error", "Invalid request");
    return;
  }
  
  int steps = doc["steps"];
  int newPos = motor.getCurrentPosition() + steps;
  
  error = motor.validatePosition(newPos);
  if (error == ERROR_HARD_LIMIT) {
    sendJSONResponse(400, "error", "Would exceed limits", error);
    return;
  }
  
  motor.setTargetPosition(newPos);
  sendJSONResponse(200, "success");
}

void handleZero() {
  motor.setCurrentPosition(0);
  preferences.putInt("position", 0);
  sendJSONResponse(200, "success", "Position zeroed");
}

void handleEmergencyStop() {
  motor.emergencyStop();
  logger.log(motor.getCurrentPosition(), motor.getTargetPosition(), 
             0, STATE_EMERGENCY_STOP, ERROR_NONE);
  sendJSONResponse(200, "success", "Emergency stop");
}

void handleReboot() {
  sendJSONResponse(200, "success", "Rebooting...");
  server.handleClient(); // Ensure response is sent
  delay(REBOOT_DELAY);
  ESP.restart();
}

void handleSetMaxSteps() {
  StaticJsonDocument<100> doc;
  ErrorCode error = parseJSONRequest(server.arg("plain"), doc);
  
  if (error != ERROR_NONE || !doc.containsKey("maxSteps")) {
    sendJSONResponse(400, "error", "Invalid request");
    return;
  }
  
  int val = doc["maxSteps"];
  if (val > 0) {
    motor.setMaxSteps(val);
    preferences.putInt("maxSteps", val);
    sendJSONResponse(200, "success");
  } else {
    sendJSONResponse(400, "error", "Invalid value");
  }
}

void handleSetStepsPerRotation() {
  StaticJsonDocument<100> doc;
  ErrorCode error = parseJSONRequest(server.arg("plain"), doc);
  
  if (error != ERROR_NONE || !doc.containsKey("stepsPerRot")) {
    sendJSONResponse(400, "error", "Invalid request");
    return;
  }
  
  int val = doc["stepsPerRot"];
  if (val > 0) {
    motor.setStepsPerRotation(val);
    preferences.putInt("stepsPerRot", val);
    sendJSONResponse(200, "success");
  } else {
    sendJSONResponse(400, "error", "Invalid value");
  }
}

void handleGetLogs() {
  String logs = logger.getLastErrors(20);
  server.send(200, "application/json", logs);
}

// ----------------------------------------------------------------
// Helper Functions
// ----------------------------------------------------------------
ErrorCode parseJSONRequest(const String& body, JsonDocument& doc) {
  // Buffer overflow protection
  if (body.length() > 1024) {
    return ERROR_BUFFER_OVERFLOW;
  }
  
  DeserializationError error = deserializeJson(doc, body);
  if (error) {
    Serial.print("JSON parse error: ");
    Serial.println(error.c_str());
    return ERROR_INVALID_JSON;
  }
  
  return ERROR_NONE;
}

void sendJSONResponse(int code, const char* status, const char* message, ErrorCode error) {
  StaticJsonDocument<200> doc;
  doc["status"] = status;
  
  if (message) {
    doc["message"] = message;
  }
  
  if (error != ERROR_NONE) {
    doc["errorCode"] = error;
  }
  
  String output;
  serializeJson(doc, output);
  server.send(code, "application/json", output);
}

bool validateAndSavePosition() {
  int pos = motor.getCurrentPosition();
  ErrorCode error = motor.validatePosition(pos);
  
  if (error == ERROR_HARD_LIMIT) {
    Serial.println("ERROR: Position validation failed!");
    return false;
  }
  
  preferences.putInt("position", pos);
  return true;
}

// ----------------------------------------------------------------
// HTML Interface
// ----------------------------------------------------------------
String getHTML() {
  return String(HTML_PAGE);
}
