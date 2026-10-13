#pragma once

#include <Arduino.h>

class ArcadeOTA {
public:
  enum class State : uint8_t {
    IDLE,
    READY,
    UPDATING,
    UNAVAILABLE
  };

  void begin();
  void update(bool networkConnected);
  void stop();

  bool isReady() const;
  bool isUpdating() const;
  const char* statusText() const;
  State state() const;

private:
  void configureService();
  void setState(State newState);

  bool enabled = false;
  bool configured = false;
  State currentState = State::IDLE;
};
