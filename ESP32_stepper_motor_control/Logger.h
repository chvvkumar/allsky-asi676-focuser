/*
 * Logging System - Circular Buffer for Motion History
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include "Config.h"

class Logger {
private:
  LogEntry buffer[LOG_BUFFER_SIZE];
  int writeIndex;
  int count;
  
public:
  Logger() : writeIndex(0), count(0) {}
  
  void log(int position, int target, int speed, MotorState state, ErrorCode error) {
    buffer[writeIndex].timestamp = millis();
    buffer[writeIndex].position = position;
    buffer[writeIndex].targetPosition = target;
    buffer[writeIndex].speed = speed;
    buffer[writeIndex].state = state;
    buffer[writeIndex].error = error;
    
    writeIndex = (writeIndex + 1) % LOG_BUFFER_SIZE;
    if (count < LOG_BUFFER_SIZE) count++;
  }
  
  int getCount() const { return count; }
  
  const LogEntry* getEntry(int index) const {
    if (index >= count) return nullptr;
    int actualIndex = (writeIndex - count + index + LOG_BUFFER_SIZE) % LOG_BUFFER_SIZE;
    return &buffer[actualIndex];
  }
  
  void clear() {
    writeIndex = 0;
    count = 0;
  }
  
  String getLastErrors(int maxEntries = 10) const {
    String result = "[";
    int entriesToShow = min(maxEntries, count);
    
    for (int i = count - entriesToShow; i < count; i++) {
      const LogEntry* entry = getEntry(i);
      if (entry && entry->error != ERROR_NONE) {
        if (result.length() > 1) result += ",";
        result += "{\"time\":" + String(entry->timestamp) + 
                  ",\"pos\":" + String(entry->position) +
                  ",\"error\":" + String(entry->error) + "}";
      }
    }
    result += "]";
    return result;
  }
};

#endif // LOGGER_H
