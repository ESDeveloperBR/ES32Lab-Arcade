#include "ArcadeOTA.h"

#include <ArduinoOTA.h>

#include "ArcadeConfig.h"
#include "ArcadeVersion.h"

void ArcadeOTA::begin() {
  enabled = true;
  configured = false;
  setState(State::UNAVAILABLE);
}

void ArcadeOTA::update(bool networkConnected) {
  if (!enabled) {
    return;
  }

  if (!networkConnected) {
    if (currentState != State::UPDATING) {
      setState(State::UNAVAILABLE);
    }
    return;
  }

  if (!configured) {
    configureService();
  } else if (currentState == State::UNAVAILABLE) {
    setState(State::READY);
  }

  if (
    currentState == State::READY ||
    currentState == State::UPDATING
  ) {
    ArduinoOTA.handle();
  }
}

void ArcadeOTA::stop() {
  enabled = false;
  setState(State::IDLE);
}

bool ArcadeOTA::isReady() const {
  return currentState == State::READY;
}

bool ArcadeOTA::isUpdating() const {
  return currentState == State::UPDATING;
}

const char* ArcadeOTA::statusText() const {
  switch (currentState) {
    case State::READY:
      return "OTA:OK";

    case State::UPDATING:
      return "OTA:UPD";

    case State::IDLE:
    case State::UNAVAILABLE:
    default:
      return "";
  }
}

ArcadeOTA::State ArcadeOTA::state() const {
  return currentState;
}

void ArcadeOTA::configureService() {
  ArduinoOTA.setHostname(
    ArcadeConfig::OTA_HOSTNAME
  );

  if (
    strlen(ArcadeConfig::OTA_PASSWORD) > 0
  ) {
    ArduinoOTA.setPassword(
      ArcadeConfig::OTA_PASSWORD
    );
  }

  ArduinoOTA.onStart([this]() {
    setState(State::UPDATING);
    Serial.println(
      "OTA local: atualizacao iniciada."
    );
  });

  ArduinoOTA.onEnd([this]() {
    setState(State::UPDATING);
    Serial.println();
    Serial.println(
      "OTA local: atualizacao concluida."
    );
  });

  ArduinoOTA.onProgress([](
    unsigned int progress,
    unsigned int total
  ) {
    if (total == 0) {
      return;
    }

    const unsigned int percentage =
      static_cast<unsigned int>(
        (
          static_cast<uint64_t>(progress) *
          100ULL
        ) /
        total
      );

    Serial.printf(
      "OTA local: %u%%\r",
      percentage
    );
  });

  ArduinoOTA.onError([this](
    ota_error_t error
  ) {
    Serial.printf(
      "OTA local: erro %u.\n",
      static_cast<unsigned int>(error)
    );

    setState(State::READY);
  });

  ArduinoOTA.begin();
  configured = true;
  setState(State::READY);

  Serial.print("OTA local pronto em ");
  Serial.print(ArcadeConfig::OTA_HOSTNAME);
  Serial.println(".local");

  Serial.print("Arcade: ");
  Serial.println(ArcadeVersion::TEXT);
}

void ArcadeOTA::setState(
  State newState
) {
  currentState = newState;
}
