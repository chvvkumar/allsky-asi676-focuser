/*
 * Stepper Motor Controller Class
 * Handles acceleration, deceleration, and motor control
 */

#ifndef STEPPER_MOTOR_H
#define STEPPER_MOTOR_H

#include <Arduino.h>
#include "Config.h"

class StepperMotor {
private:
  int currentPosition;
  int targetPosition;
  int sequenceIndex;
  unsigned long lastStepTime;
  
  MotorState state;
  
  int currentSpeed;
  
  MotorConfig config;
  
  void setStepperPins(int a, int b, int c, int d);
  
public:
  StepperMotor();
  
  void begin(const MotorConfig& cfg);
  void update();
  void stepMotor(int direction);
  void stop();
  void emergencyStop();
  
  // Position control
  void setTargetPosition(int pos);
  void setCurrentPosition(int pos);
  int getCurrentPosition() const { return currentPosition; }
  int getTargetPosition() const { return targetPosition; }
  
  // Speed control
  void setSpeed(int speed);
  int getSpeed() const { return currentSpeed; }
  
  // State queries
  bool isRunning() const { return state != STATE_IDLE && state != STATE_STOPPED; }
  MotorState getState() const { return state; }
  
  // Configuration
  void setMaxSteps(int steps);
  void setStepsPerRotation(int steps);
  int getMaxSteps() const { return config.maxSteps; }
  int getStepsPerRotation() const { return config.stepsPerRotation; }
  
  // Safety
  ErrorCode validatePosition(int pos) const;
  bool isNearSoftLimit() const;
  int constrainPosition(int pos) const;
};

// ----------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------
StepperMotor::StepperMotor() 
  : currentPosition(0), targetPosition(0), sequenceIndex(0), 
    lastStepTime(0), state(STATE_IDLE), currentSpeed(DEFAULT_SPEED) {
}

// ----------------------------------------------------------------
// Initialize motor with configuration
// ----------------------------------------------------------------
void StepperMotor::begin(const MotorConfig& cfg) {
  config = cfg;
  currentSpeed = config.defaultSpeed;
  
  pinMode(PIN_A, OUTPUT);
  pinMode(PIN_B, OUTPUT);
  pinMode(PIN_C, OUTPUT);
  pinMode(PIN_D, OUTPUT);
  
  stop();
}

// ----------------------------------------------------------------
// Main update loop - call this frequently
// ----------------------------------------------------------------
void StepperMotor::update() {
  if (currentPosition == targetPosition) {
    if (state != STATE_IDLE && state != STATE_STOPPED) {
      stop();
    }
    return;
  }
  
  state = STATE_RUNNING;
  unsigned long stepDelay = 1000000 / currentSpeed; // microseconds
  
  if (micros() - lastStepTime >= stepDelay) {
    lastStepTime = micros();
    int direction = (targetPosition > currentPosition) ? 1 : -1;
    stepMotor(direction);
    currentPosition += direction;
  }
}

// ----------------------------------------------------------------
// Step motor one position
// ----------------------------------------------------------------
void StepperMotor::stepMotor(int direction) {
  sequenceIndex += direction;
  
  if (sequenceIndex >= STEPS_IN_SEQUENCE) {
    sequenceIndex = 0;
  } else if (sequenceIndex < 0) {
    sequenceIndex = STEPS_IN_SEQUENCE - 1;
  }
  
  setStepperPins(
    stepSequence[sequenceIndex][0],
    stepSequence[sequenceIndex][1],
    stepSequence[sequenceIndex][2],
    stepSequence[sequenceIndex][3]
  );
}

// ----------------------------------------------------------------
// Set stepper motor coil states
// ----------------------------------------------------------------
void StepperMotor::setStepperPins(int a, int b, int c, int d) {
  digitalWrite(PIN_A, a);
  digitalWrite(PIN_B, b);
  digitalWrite(PIN_C, c);
  digitalWrite(PIN_D, d);
}

// ----------------------------------------------------------------
// Normal stop - turn off coils
// ----------------------------------------------------------------
void StepperMotor::stop() {
  digitalWrite(PIN_A, LOW);
  digitalWrite(PIN_B, LOW);
  digitalWrite(PIN_C, LOW);
  digitalWrite(PIN_D, LOW);
  state = STATE_STOPPED;
}

// ----------------------------------------------------------------
// Emergency stop - immediate
// ----------------------------------------------------------------
void StepperMotor::emergencyStop() {
  targetPosition = currentPosition;
  stop();
  state = STATE_EMERGENCY_STOP;
}

// ----------------------------------------------------------------
// Position control
// ----------------------------------------------------------------
void StepperMotor::setTargetPosition(int pos) {
  int constrainedPos = constrainPosition(pos);
  targetPosition = constrainedPos;
  
  if (targetPosition != currentPosition) {
    state = STATE_RUNNING;
  }
}

void StepperMotor::setCurrentPosition(int pos) {
  currentPosition = pos;
  targetPosition = pos;
  state = STATE_IDLE;
}

// ----------------------------------------------------------------
// Speed control
// ----------------------------------------------------------------
void StepperMotor::setSpeed(int speed) {
  if (speed < config.minSpeed) speed = config.minSpeed;
  if (speed > config.maxSpeed) speed = config.maxSpeed;
  currentSpeed = speed;
}

// ----------------------------------------------------------------
// Configuration
// ----------------------------------------------------------------
void StepperMotor::setMaxSteps(int steps) {
  if (steps > 0) {
    config.maxSteps = steps;
  }
}

void StepperMotor::setStepsPerRotation(int steps) {
  if (steps > 0) {
    config.stepsPerRotation = steps;
  }
}

// ----------------------------------------------------------------
// Safety functions
// ----------------------------------------------------------------
ErrorCode StepperMotor::validatePosition(int pos) const {
  if (pos < -config.maxSteps || pos > config.maxSteps) {
    return ERROR_HARD_LIMIT;
  }
  if (abs(pos) > config.maxSteps - config.softLimitWarning) {
    return ERROR_SOFT_LIMIT_WARNING;
  }
  return ERROR_NONE;
}

bool StepperMotor::isNearSoftLimit() const {
  return abs(currentPosition) > config.maxSteps - config.softLimitWarning;
}

int StepperMotor::constrainPosition(int pos) const {
  if (pos < -config.maxSteps) return -config.maxSteps;
  if (pos > config.maxSteps) return config.maxSteps;
  return pos;
}

#endif // STEPPER_MOTOR_H
