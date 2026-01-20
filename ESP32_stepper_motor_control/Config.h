/*
 * Configuration and Constants for ESP32-S3 Focus Controller
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ----------------------------------------------------------------
// Timing Constants
// ----------------------------------------------------------------
#define WIFI_CHECK_INTERVAL 30000      // Check WiFi every 30 seconds (ms)
#define WIFI_CONNECT_ATTEMPTS 20       // Max connection attempts
#define REBOOT_DELAY 500               // Delay before reboot (ms)
#define STATUS_UPDATE_INTERVAL 100     // WebSocket status update interval (ms)
#define POSITION_SAVE_INTERVAL 5000    // Save position every 5 seconds (ms)

// ----------------------------------------------------------------
// Motor Constants
// ----------------------------------------------------------------
#define DEFAULT_MAX_STEPS 20000
#define DEFAULT_STEPS_PER_ROTATION 4096
#define DEFAULT_SPEED 100
#define MIN_SPEED 50
#define MAX_SPEED 600

// Soft limit warning zone
#define SOFT_LIMIT_WARNING 500         // Warn when within 500 steps of limit

// ----------------------------------------------------------------
// Pin Configuration (ULN2003)
// ----------------------------------------------------------------
#define PIN_A GPIO_NUM_1
#define PIN_B GPIO_NUM_2
#define PIN_C GPIO_NUM_3
#define PIN_D GPIO_NUM_4

// ----------------------------------------------------------------
// ULN2003 Half-Step Sequence (8 steps per full step)
// ----------------------------------------------------------------
#define STEPS_IN_SEQUENCE 8
const int stepSequence[8][4] = {
  {1, 0, 0, 0},
  {1, 1, 0, 0},
  {0, 1, 0, 0},
  {0, 1, 1, 0},
  {0, 0, 1, 0},
  {0, 0, 1, 1},
  {0, 0, 0, 1},
  {1, 0, 0, 1}
};

// ----------------------------------------------------------------
// Error Codes
// ----------------------------------------------------------------
enum ErrorCode {
  ERROR_NONE = 0,
  ERROR_INVALID_POSITION = 1,
  ERROR_INVALID_SPEED = 2,
  ERROR_INVALID_JSON = 3,
  ERROR_BUFFER_OVERFLOW = 4,
  ERROR_POSITION_CORRUPTED = 5,
  ERROR_SOFT_LIMIT_WARNING = 6,
  ERROR_HARD_LIMIT = 7,
  ERROR_WIFI_FAILED = 8
};

// ----------------------------------------------------------------
// Motor States
// ----------------------------------------------------------------
enum MotorState {
  STATE_IDLE = 0,
  STATE_RUNNING = 1,
  STATE_STOPPED = 2,
  STATE_EMERGENCY_STOP = 3
};

// ----------------------------------------------------------------
// Configuration Structures
// ----------------------------------------------------------------
struct WiFiConfig {
  const char* ssid;
  const char* password;
  const char* apSSID;
  const char* apPassword;
  bool useAP;
};

struct MotorConfig {
  int maxSteps;
  int stepsPerRotation;
  int defaultSpeed;
  int minSpeed;
  int maxSpeed;
  int softLimitWarning;
};

struct LogEntry {
  unsigned long timestamp;
  int position;
  int targetPosition;
  int speed;
  MotorState state;
  ErrorCode error;
};

// ----------------------------------------------------------------
// Logging
// ----------------------------------------------------------------
#define LOG_BUFFER_SIZE 50

#endif // CONFIG_H
