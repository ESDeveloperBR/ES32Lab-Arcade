#include "ArcadeOTA.h"

#include <ArduinoOTA.h>
#include <WiFi.h>

#include "ArcadeConfig.h"
#include "ArcadeVersion.h"

void ArcadeOTA::begin() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  WiFi.begin(
    ArcadeConfig::WIFI_SSID,
    ArcadeConfig::WIFI_PASSWORD
  );

  connectionTimeout.resetMillis();
  setState(State::CONNECTING);

  Serial.print("OTA: procurando Wi-Fi ");
  Serial.println(ArcadeConfig::WIFI_SSID);
}

void ArcadeOTA::update() {
  if (currentState == State::CONNECTING) {
    if (WiFi.status() == WL_CONNECTED) {
      configureService();
      return;
    }

    if (
      connectionTimeout.intervalMillis(
        ArcadeConfig::WIFI_CONNECT_TIMEOUT_MS
      )
    ) {
      Serial.println("OTA: rede indisponivel; seguindo offline.");
      disableNetwork();
      setState(State::UNAVAILABLE);
    }

    return;
  }

  if (
    currentState != State::READY &&
    currentState != State::UPDATING
  ) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("OTA: conexao Wi-Fi encerrada.");
    disableNetwork();
    setState(State::UNAVAILABLE);
    return;
  }

  ArduinoOTA.handle();
}

void ArcadeOTA::stop() {
  if (currentState == State::IDLE) {
    return;
  }

  // O servico OTA deixa de ser processado e o radio e desligado.
  // A proxima inicializacao fisica reativa a tentativa de conexao.
  disableNetwork();
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
    case State::CONNECTING:
      return "OTA:...";

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
  ArduinoOTA.setHostname(ArcadeConfig::OTA_HOSTNAME);
  ArduinoOTA.setPassword(ArcadeConfig::OTA_PASSWORD);

  ArduinoOTA.onStart([this]() {
    setState(State::UPDATING);
    Serial.println("OTA: atualizacao iniciada.");
  });

  ArduinoOTA.onEnd([this]() {
    setState(State::UPDATING);
    Serial.println();
    Serial.println("OTA: atualizacao concluida.");
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
        (static_cast<uint64_t>(progress) * 100ULL) / total
      );

    Serial.printf("OTA: %u%%\r", percentage);
  });

  ArduinoOTA.onError([this](ota_error_t error) {
    Serial.printf("OTA: erro %u.\n", static_cast<unsigned int>(error));
    setState(State::READY);
  });

  ArduinoOTA.begin();
  setState(State::READY);

  Serial.print("OTA: pronto em ");
  Serial.print(ArcadeConfig::OTA_HOSTNAME);
  Serial.println(".local");

  Serial.print("OTA: IP ");
  Serial.println(WiFi.localIP());

  Serial.print("Arcade: ");
  Serial.println(ArcadeVersion::TEXT);
}

void ArcadeOTA::setState(State newState) {
  currentState = newState;
}

void ArcadeOTA::disableNetwork() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}
