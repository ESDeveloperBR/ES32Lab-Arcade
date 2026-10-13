#include "ArcadeFirmwareUpdate.h"

#include <ArduinoJson.h>
#include <ES32Lab.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <mbedtls/sha256.h>

#include "ArcadeConfig.h"
#include "ArcadeVersion.h"

void ArcadeFirmwareUpdate::begin() {
  currentState = State::IDLE;
  automaticStarted = false;
  automaticCompleted = false;
  cancelRequested = false;

  remoteVersion = "";
  firmwareUrl = "";
  expectedSha256 = "";
  message = "";
}

void ArcadeFirmwareUpdate::startAutomaticCheck() {
  if (automaticStarted) {
    return;
  }

  automaticStarted = true;

  startCheck(
    true,
    ArcadeConfig::AUTO_UPDATE_TIMEOUT_MS
  );
}

void ArcadeFirmwareUpdate::startManualCheck() {
  if (
    currentState == State::CHECKING ||
    currentState == State::INSTALLING
  ) {
    return;
  }

  startCheck(
    false,
    ArcadeConfig::MANUAL_UPDATE_TIMEOUT_MS
  );
}

void ArcadeFirmwareUpdate::cancelCheck() {
  cancelRequested = true;
}

bool ArcadeFirmwareUpdate::automaticCheckStarted() const {
  return automaticStarted;
}

bool ArcadeFirmwareUpdate::automaticCheckCompleted() const {
  return automaticCompleted;
}

bool ArcadeFirmwareUpdate::isChecking() const {
  return currentState == State::CHECKING;
}

bool ArcadeFirmwareUpdate::updateAvailable() const {
  return currentState == State::AVAILABLE;
}

ArcadeFirmwareUpdate::State
ArcadeFirmwareUpdate::state() const {
  return currentState;
}

String ArcadeFirmwareUpdate::availableVersion() const {
  return remoteVersion;
}

String ArcadeFirmwareUpdate::errorMessage() const {
  return message;
}

bool ArcadeFirmwareUpdate::hasExpectedSha256() const {
  return expectedSha256.length() == 64;
}

void ArcadeFirmwareUpdate::startCheck(
  bool automatic,
  uint32_t timeoutMs
) {
  if (checkTaskHandle != nullptr) {
    return;
  }

  checkIsAutomatic = automatic;
  checkTimeoutMs = timeoutMs;
  cancelRequested = false;

  remoteVersion = "";
  firmwareUrl = "";
  expectedSha256 = "";
  message = "";

  currentState = State::CHECKING;

  BaseType_t created = xTaskCreatePinnedToCore(
    checkTaskEntry,
    "Arcade-UpdateCheck",
    10240,
    this,
    1,
    &checkTaskHandle,
    0
  );

  if (created != pdPASS) {
    checkTaskHandle = nullptr;
    currentState = State::ERROR;
    message = "FALHA AO CRIAR TAREFA";

    if (automatic) {
      automaticCompleted = true;
    }
  }
}

void ArcadeFirmwareUpdate::checkTaskEntry(
  void* parameter
) {
  ArcadeFirmwareUpdate* self =
    static_cast<ArcadeFirmwareUpdate*>(
      parameter
    );

  self->performCheck();
  self->checkTaskHandle = nullptr;

  vTaskDelete(nullptr);
}

void ArcadeFirmwareUpdate::performCheck() {
  const bool automatic =
    checkIsAutomatic;

  auto finishAutomatic = [this, automatic]() {
    if (automatic) {
      automaticCompleted = true;
    }
  };

  if (cancelRequested) {
    currentState = State::IDLE;
    finishAutomatic();
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    currentState = State::ERROR;
    message = "SEM CONEXAO WI-FI";
    finishAutomatic();
    return;
  }

  if (strlen(ArcadeConfig::MANIFEST_URL) == 0) {
    currentState = State::ERROR;
    message = "MANIFESTO NAO CONFIG";
    finishAutomatic();
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(checkTimeoutMs);

  HTTPClient http;
  http.setFollowRedirects(
    HTTPC_FORCE_FOLLOW_REDIRECTS
  );

  if (
    !http.begin(
      client,
      ArcadeConfig::MANIFEST_URL
    )
  ) {
    currentState = State::ERROR;
    message = "FALHA HTTPS";
    finishAutomatic();
    return;
  }

  const int code = http.GET();

  if (cancelRequested) {
    http.end();
    currentState = State::IDLE;
    finishAutomatic();
    return;
  }

  if (code != HTTP_CODE_OK) {
    http.end();
    currentState = State::ERROR;
    message = "HTTP " + String(code);
    finishAutomatic();
    return;
  }

  const String payload =
    http.getString();

  http.end();

  if (cancelRequested) {
    currentState = State::IDLE;
    finishAutomatic();
    return;
  }

  JsonDocument doc;

  const DeserializationError error =
    deserializeJson(doc, payload);

  if (error) {
    currentState = State::ERROR;
    message = "MANIFESTO INVALIDO";
    finishAutomatic();
    return;
  }

  remoteVersion =
    doc["version"] | "";

  firmwareUrl =
    doc["firmware_url"] | "";

  expectedSha256 =
    doc["firmware_sha256"] | "";

  remoteVersion.trim();
  firmwareUrl.trim();
  expectedSha256.trim();
  expectedSha256.toLowerCase();

  if (remoteVersion.length() == 0) {
    currentState = State::ERROR;
    message = "VERSAO AUSENTE";
    finishAutomatic();
    return;
  }

  if (firmwareUrl.length() == 0) {
    currentState = State::ERROR;
    message = "FIRMWARE AUSENTE";
    finishAutomatic();
    return;
  }

  if (
    expectedSha256.length() > 0 &&
    !isValidSha256(expectedSha256)
  ) {
    currentState = State::ERROR;
    message = "SHA256 INVALIDO";
    finishAutomatic();
    return;
  }

  currentState =
    compareVersions(
      String(ArcadeVersion::TEXT),
      remoteVersion
    ) < 0
      ? State::AVAILABLE
      : State::CURRENT;

  finishAutomatic();
}

bool ArcadeFirmwareUpdate::install(
  ProgressCallback progressCallback,
  void* context
) {
  if (WiFi.status() != WL_CONNECTED) {
    currentState = State::ERROR;
    message = "SEM CONEXAO WI-FI";
    return false;
  }

  if (firmwareUrl.length() == 0) {
    currentState = State::ERROR;
    message = "FIRMWARE AUSENTE";
    return false;
  }

  currentState = State::INSTALLING;
  message = "";

  if (progressCallback != nullptr) {
    progressCallback(0, context);
  }

  const bool verifySha256 =
    expectedSha256.length() == 64;

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(
    ArcadeConfig::MANUAL_UPDATE_TIMEOUT_MS
  );

  HTTPClient http;
  http.setFollowRedirects(
    HTTPC_FORCE_FOLLOW_REDIRECTS
  );

  if (!http.begin(client, firmwareUrl)) {
    currentState = State::ERROR;
    message = "FALHA HTTPS";
    return false;
  }

  const int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    http.end();
    currentState = State::ERROR;
    message = "HTTP " + String(httpCode);
    return false;
  }

  const int remoteLength =
    http.getSize();

  const size_t updateSize =
    remoteLength > 0
      ? static_cast<size_t>(remoteLength)
      : UPDATE_SIZE_UNKNOWN;

  if (!Update.begin(updateSize)) {
    http.end();
    currentState = State::ERROR;
    message = "SEM ESPACO OTA";
    return false;
  }

  mbedtls_sha256_context shaContext;
  bool shaActive = false;

  if (verifySha256) {
    mbedtls_sha256_init(&shaContext);

    if (
      mbedtls_sha256_starts_ret(
        &shaContext,
        0
      ) != 0
    ) {
      mbedtls_sha256_free(&shaContext);
      Update.abort();
      http.end();

      currentState = State::ERROR;
      message = "ERRO SHA256";
      return false;
    }

    shaActive = true;
  }

  WiFiClient* stream =
    http.getStreamPtr();

  uint8_t buffer[1024];
  size_t totalReceived = 0;
  bool downloadOk = true;
  String downloadError;

  ES_TimeInterval receiveTimer;
  receiveTimer.resetMillis();

  uint8_t lastProgress = 255;

  while (
    (
      remoteLength <= 0 ||
      totalReceived <
        static_cast<size_t>(remoteLength)
    ) &&
    (
      http.connected() ||
      stream->available() > 0
    )
  ) {
    size_t available =
      stream->available();

    if (available == 0) {
      if (
        receiveTimer.intervalMillis(
          ArcadeConfig::MANUAL_UPDATE_TIMEOUT_MS
        )
      ) {
        downloadOk = false;
        downloadError = "TIMEOUT DOWNLOAD";
        break;
      }

      delay(1);
      continue;
    }

    size_t toRead = available;

    if (toRead > sizeof(buffer)) {
      toRead = sizeof(buffer);
    }

    const int readCount =
      stream->readBytes(
        buffer,
        toRead
      );

    if (readCount <= 0) {
      downloadOk = false;
      downloadError = "FALHA DOWNLOAD";
      break;
    }

    receiveTimer.resetMillis();

    if (
      shaActive &&
      mbedtls_sha256_update_ret(
        &shaContext,
        buffer,
        static_cast<size_t>(readCount)
      ) != 0
    ) {
      downloadOk = false;
      downloadError = "ERRO SHA256";
      break;
    }

    const size_t written =
      Update.write(
        buffer,
        static_cast<size_t>(readCount)
      );

    if (
      written !=
      static_cast<size_t>(readCount)
    ) {
      downloadOk = false;
      downloadError = "ERRO GRAVACAO";
      break;
    }

    totalReceived +=
      static_cast<size_t>(readCount);

    if (
      remoteLength > 0 &&
      progressCallback != nullptr
    ) {
      const uint8_t progress =
        static_cast<uint8_t>(
          (
            totalReceived * 100ULL
          ) /
          static_cast<size_t>(
            remoteLength
          )
        );

      if (progress != lastProgress) {
        lastProgress = progress;
        progressCallback(
          progress,
          context
        );
      }
    }
  }

  if (
    downloadOk &&
    remoteLength > 0 &&
    totalReceived !=
      static_cast<size_t>(remoteLength)
  ) {
    downloadOk = false;
    downloadError =
      "DOWNLOAD INCOMPLETO";
  }

  String calculatedSha256;

  if (downloadOk && shaActive) {
    uint8_t digest[32];

    if (
      mbedtls_sha256_finish_ret(
        &shaContext,
        digest
      ) != 0
    ) {
      downloadOk = false;
      downloadError = "ERRO SHA256";
    } else {
      calculatedSha256 =
        sha256ToHex(digest);

      if (
        !calculatedSha256
          .equalsIgnoreCase(
            expectedSha256
          )
      ) {
        downloadOk = false;
        downloadError =
          "SHA256 DIVERGENTE";
      }
    }
  }

  if (shaActive) {
    mbedtls_sha256_free(&shaContext);
  }

  http.end();

  if (!downloadOk) {
    Update.abort();

    currentState = State::ERROR;
    message =
      downloadError.length() > 0
        ? downloadError
        : "FALHA OTA";

    return false;
  }

  const bool finalized =
    remoteLength > 0
      ? Update.end(false)
      : Update.end(true);

  if (
    !finalized ||
    !Update.isFinished()
  ) {
    currentState = State::ERROR;
    message = "FALHA AO FINALIZAR";
    return false;
  }

  if (progressCallback != nullptr) {
    progressCallback(100, context);
  }

  delay(1600);
  ESP.restart();

  return true;
}

int ArcadeFirmwareUpdate::versionPart(
  const String& version,
  uint8_t part
) {
  int values[3] = {0, 0, 0};
  uint8_t index = 0;
  int current = 0;
  bool hasDigit = false;

  for (
    size_t i = 0;
    i <= version.length() &&
    index < 3;
    ++i
  ) {
    const char c =
      i < version.length()
        ? version[i]
        : '.';

    if (c >= '0' && c <= '9') {
      current =
        current * 10 +
        (c - '0');

      hasDigit = true;
      continue;
    }

    if (c == '.' || i == version.length()) {
      values[index++] =
        hasDigit ? current : 0;

      current = 0;
      hasDigit = false;
    }
  }

  return part < 3
    ? values[part]
    : 0;
}

int ArcadeFirmwareUpdate::compareVersions(
  const String& installed,
  const String& available
) {
  for (uint8_t i = 0; i < 3; ++i) {
    const int left =
      versionPart(installed, i);

    const int right =
      versionPart(available, i);

    if (left < right) {
      return -1;
    }

    if (left > right) {
      return 1;
    }
  }

  return 0;
}

bool ArcadeFirmwareUpdate::isValidSha256(
  const String& value
) {
  if (value.length() != 64) {
    return false;
  }

  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value[i];

    const bool digit =
      c >= '0' && c <= '9';

    const bool lowerHex =
      c >= 'a' && c <= 'f';

    const bool upperHex =
      c >= 'A' && c <= 'F';

    if (!digit && !lowerHex && !upperHex) {
      return false;
    }
  }

  return true;
}

String ArcadeFirmwareUpdate::sha256ToHex(
  const uint8_t digest[32]
) {
  static const char HEX_DIGITS[] =
    "0123456789abcdef";

  String result;
  result.reserve(64);

  for (uint8_t i = 0; i < 32; ++i) {
    result +=
      HEX_DIGITS[(digest[i] >> 4) & 0x0F];

    result +=
      HEX_DIGITS[digest[i] & 0x0F];
  }

  return result;
}
