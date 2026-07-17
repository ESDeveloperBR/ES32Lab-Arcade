#pragma once

#include <Arduino.h>
#include <ES32Lab.h>

class ArcadeOTA {
public:
  enum class State : uint8_t {
    IDLE,
    CONNECTING,
    READY,
    UPDATING,
    UNAVAILABLE
  };

  void begin();
  void update();
  void stop();

  bool isReady() const;
  bool isUpdating() const;
  const char* statusText() const;
  State state() const;

private:
  void configureService();
  void setState(State newState);
  void disableNetwork();

  ES_TimeInterval connectionTimeout;
  State currentState = State::IDLE;
};
