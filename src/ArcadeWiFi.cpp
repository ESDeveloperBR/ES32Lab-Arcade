#include "ArcadeWiFi.h"

#include <ArduinoJson.h>

#include "ArcadeConfig.h"

namespace {
constexpr char WIFI_PREFS_NAMESPACE[] = "ES_Wifi";
}

void ArcadeWiFi::begin() {
  scanMutex = xSemaphoreCreateMutex();

  preferences.begin(WIFI_PREFS_NAMESPACE, false);
  loadSaved();

  // O valor do platformio.ini funciona apenas como credencial inicial.
  // Se a loja ja gravou ES_Wifi na NVS, os dados do usuario prevalecem.
  if (
    savedNetworkCount == 0 &&
    strlen(ArcadeConfig::WIFI_SSID) > 0
  ) {
    addOrUpdateSaved(
      ArcadeConfig::WIFI_SSID,
      ArcadeConfig::WIFI_PASSWORD
    );
  }

  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);

  if (savedNetworkCount > 0) {
    startAutoCycle();
  } else {
    currentState = State::OFFLINE;
    initialFailed = true;
  }
}

void ArcadeWiFi::update() {
  // A classe ES_WiFi executa o scan no worker. Durante esse periodo,
  // nao iniciamos ou trocamos conexoes.
  if (scanInProgressFlag) {
    return;
  }

  const bool connected = esWifi.isConnected();

  if (connected) {
    if (currentState != State::CONNECTED) {
      attemptActive = false;
      currentState = State::CONNECTED;
      initialFailed = false;

      const int index = findSaved(WiFi.SSID());
      if (index > 0) {
        promoteSaved(static_cast<size_t>(index));
      }

      Serial.print("Wi-Fi conectado: ");
      Serial.println(WiFi.SSID());
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
    }

    return;
  }

  if (currentState == State::CONNECTED) {
    currentState = State::OFFLINE;
    attemptActive = false;
  }

  if (!attemptActive) {
    return;
  }

  if (
    !attemptTimer.intervalMillis(
      ArcadeConfig::WIFI_CONNECT_TIMEOUT_MS
    )
  ) {
    return;
  }

  attemptActive = false;

  if (attemptIsManual) {
    attemptIsManual = false;
    currentState = State::OFFLINE;
    return;
  }

  autoTryIndex++;

  if (autoTryIndex < savedNetworkCount) {
    startAttempt(
      saved[autoTryIndex].ssid,
      saved[autoTryIndex].password,
      false
    );
    return;
  }

  // Todas as redes armazenadas foram testadas.
  currentState = State::OFFLINE;
  initialFailed = true;
  WiFi.disconnect();

  Serial.println(
    "Wi-Fi: nenhuma rede cadastrada respondeu; Arcade segue offline."
  );
}

void ArcadeWiFi::stop() {
  scanCancelRequested = true;
  attemptActive = false;
  attemptIsManual = false;

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  currentState = State::IDLE;
}

bool ArcadeWiFi::isConnected() const {
  return currentState == State::CONNECTED &&
         WiFi.status() == WL_CONNECTED;
}

bool ArcadeWiFi::isConnecting() const {
  return currentState == State::CONNECTING;
}

bool ArcadeWiFi::initialConnectionFailed() const {
  return initialFailed;
}

const char* ArcadeWiFi::statusText() const {
  if (scanInProgressFlag) {
    return "WIFI:SCAN";
  }

  switch (currentState) {
    case State::CONNECTING:
      return "WIFI:...";

    case State::CONNECTED:
      return "WIFI:OK";

    case State::IDLE:
    case State::OFFLINE:
    default:
      return "";
  }
}

String ArcadeWiFi::connectedSsid() const {
  return isConnected() ? WiFi.SSID() : String();
}

String ArcadeWiFi::localIp() const {
  return isConnected() ? WiFi.localIP().toString() : String();
}

size_t ArcadeWiFi::savedCount() const {
  return savedNetworkCount;
}

ArcadeWiFi::Credential ArcadeWiFi::savedAt(size_t index) const {
  if (index >= savedNetworkCount) {
    return Credential();
  }

  return saved[index];
}

bool ArcadeWiFi::connectSaved(size_t index) {
  if (
    index >= savedNetworkCount ||
    scanInProgressFlag
  ) {
    return false;
  }

  startAttempt(
    saved[index].ssid,
    saved[index].password,
    true
  );

  return true;
}

void ArcadeWiFi::connectAndSave(
  const String& ssid,
  const String& password
) {
  if (ssid.length() == 0) {
    return;
  }

  addOrUpdateSaved(ssid, password);
  startAttempt(ssid, password, true);
}

void ArcadeWiFi::deleteSaved(size_t index) {
  if (index >= savedNetworkCount) {
    return;
  }

  for (
    size_t i = index;
    i + 1 < savedNetworkCount;
    ++i
  ) {
    saved[i] = saved[i + 1];
  }

  if (savedNetworkCount > 0) {
    savedNetworkCount--;
  }

  persistSaved();
}

bool ArcadeWiFi::startScan() {
  if (scanInProgressFlag) {
    return false;
  }

  scanCancelRequested = false;
  scanReadyFlag = false;
  scanInProgressFlag = true;

  if (scanMutex != nullptr) {
    xSemaphoreTake(scanMutex, portMAX_DELAY);
    scannedCount = 0;
    xSemaphoreGive(scanMutex);
  } else {
    scannedCount = 0;
  }

  BaseType_t created = xTaskCreatePinnedToCore(
    scanTaskEntry,
    "Arcade-WiFiScan",
    7168,
    this,
    1,
    &scanTaskHandle,
    0
  );

  if (created != pdPASS) {
    scanTaskHandle = nullptr;
    scanInProgressFlag = false;
    scanReadyFlag = true;
    return false;
  }

  return true;
}

void ArcadeWiFi::cancelScan() {
  scanCancelRequested = true;
}

bool ArcadeWiFi::scanInProgress() const {
  return scanInProgressFlag;
}

bool ArcadeWiFi::scanReady() const {
  return scanReadyFlag;
}

size_t ArcadeWiFi::scanCount() {
  if (scanMutex == nullptr) {
    return scannedCount;
  }

  xSemaphoreTake(scanMutex, portMAX_DELAY);
  const size_t count = scannedCount;
  xSemaphoreGive(scanMutex);

  return count;
}

ArcadeWiFi::ScannedNetwork ArcadeWiFi::scanAt(size_t index) {
  ScannedNetwork result;

  if (scanMutex == nullptr) {
    if (index < scannedCount) {
      result = scanned[index];
    }
    return result;
  }

  xSemaphoreTake(scanMutex, portMAX_DELAY);

  if (index < scannedCount) {
    result = scanned[index];
  }

  xSemaphoreGive(scanMutex);

  return result;
}

void ArcadeWiFi::scanTaskEntry(void* parameter) {
  ArcadeWiFi* self =
    static_cast<ArcadeWiFi*>(parameter);

  self->performScan();
  self->scanTaskHandle = nullptr;

  vTaskDelete(nullptr);
}

void ArcadeWiFi::performScan() {
  String json = esWifi.scanWifi(true);

  ScannedNetwork temporary[MAX_SCAN_NETWORKS];
  uint8_t count = 0;

  JsonDocument doc;
  const DeserializationError error =
    deserializeJson(doc, json);

  if (!error && !scanCancelRequested) {
    for (
      JsonObject network :
      doc["scanwifi"].as<JsonArray>()
    ) {
      String ssid =
        network["ssid"] | "";

      if (ssid.length() == 0) {
        continue;
      }

      bool duplicate = false;

      for (uint8_t i = 0; i < count; ++i) {
        if (temporary[i].ssid == ssid) {
          duplicate = true;
          break;
        }
      }

      if (duplicate) {
        continue;
      }

      temporary[count].ssid = ssid;
      temporary[count].rssi =
        network["rssi"] | 0;
      temporary[count].secure =
        network["secure"] | false;

      count++;

      if (count >= MAX_SCAN_NETWORKS) {
        break;
      }
    }
  }

  if (!scanCancelRequested) {
    if (scanMutex != nullptr) {
      xSemaphoreTake(scanMutex, portMAX_DELAY);
    }

    scannedCount = count;

    for (uint8_t i = 0; i < count; ++i) {
      scanned[i] = temporary[i];
    }

    if (scanMutex != nullptr) {
      xSemaphoreGive(scanMutex);
    }

    scanReadyFlag = true;
  }

  scanInProgressFlag = false;
}

void ArcadeWiFi::loadSaved() {
  savedNetworkCount = 0;

  const uint8_t storageVersion =
    preferences.getUChar("version", 0);

  if (storageVersion == 0) {
    persistSaved();
  }

  uint8_t storedCount =
    preferences.getUChar("wcount", 0);

  if (storedCount > MAX_SAVED_NETWORKS) {
    storedCount = MAX_SAVED_NETWORKS;
  }

  for (uint8_t i = 0; i < storedCount; ++i) {
    String ssid =
      preferences.getString(
        ("ws" + String(i)).c_str(),
        ""
      );

    if (ssid.length() == 0) {
      continue;
    }

    saved[savedNetworkCount].ssid = ssid;
    saved[savedNetworkCount].password =
      preferences.getString(
        ("wp" + String(i)).c_str(),
        ""
      );

    savedNetworkCount++;

    if (savedNetworkCount >= MAX_SAVED_NETWORKS) {
      break;
    }
  }

  // Compacta entradas vazias e garante que o armazenamento permaneça
  // no contrato oficial ES_Wifi v1.
  persistSaved();
}

void ArcadeWiFi::persistSaved() {
  preferences.putUChar(
    "version",
    STORAGE_VERSION
  );

  preferences.putUChar(
    "wcount",
    savedNetworkCount
  );

  for (
    uint8_t i = 0;
    i < MAX_SAVED_NETWORKS;
    ++i
  ) {
    const String ssidKey =
      "ws" + String(i);
    const String passwordKey =
      "wp" + String(i);

    if (i < savedNetworkCount) {
      preferences.putString(
        ssidKey.c_str(),
        saved[i].ssid
      );

      preferences.putString(
        passwordKey.c_str(),
        saved[i].password
      );
    } else {
      preferences.remove(
        ssidKey.c_str()
      );

      preferences.remove(
        passwordKey.c_str()
      );
    }
  }
}

int ArcadeWiFi::findSaved(
  const String& ssid
) const {
  for (
    uint8_t i = 0;
    i < savedNetworkCount;
    ++i
  ) {
    if (saved[i].ssid == ssid) {
      return i;
    }
  }

  return -1;
}

void ArcadeWiFi::promoteSaved(size_t index) {
  if (
    index == 0 ||
    index >= savedNetworkCount
  ) {
    return;
  }

  const Credential credential =
    saved[index];

  for (
    size_t i = index;
    i > 0;
    --i
  ) {
    saved[i] = saved[i - 1];
  }

  saved[0] = credential;
  persistSaved();
}

void ArcadeWiFi::addOrUpdateSaved(
  const String& ssid,
  const String& password
) {
  const int existing =
    findSaved(ssid);

  if (existing >= 0) {
    saved[existing].password = password;
    promoteSaved(
      static_cast<size_t>(existing)
    );
    persistSaved();
    return;
  }

  const uint8_t newCount =
    savedNetworkCount < MAX_SAVED_NETWORKS
      ? savedNetworkCount + 1
      : MAX_SAVED_NETWORKS;

  for (
    int i = newCount - 1;
    i > 0;
    --i
  ) {
    saved[i] = saved[i - 1];
  }

  saved[0].ssid = ssid;
  saved[0].password = password;
  savedNetworkCount = newCount;

  persistSaved();
}

void ArcadeWiFi::startAutoCycle() {
  initialFailed = false;
  autoTryIndex = 0;

  startAttempt(
    saved[0].ssid,
    saved[0].password,
    false
  );
}

void ArcadeWiFi::startAttempt(
  const String& ssid,
  const String& password,
  bool manualAttempt
) {
  if (ssid.length() == 0) {
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  WiFi.begin(
    ssid.c_str(),
    password.c_str()
  );

  attemptIsManual = manualAttempt;
  attemptActive = true;
  currentState = State::CONNECTING;
  attemptTimer.resetMillis();

  Serial.print("Wi-Fi tentando: ");
  Serial.println(ssid);
}
