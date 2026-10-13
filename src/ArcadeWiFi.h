#pragma once

#include <Arduino.h>
#include <ES32Lab.h>
#include <ES_Wifi.h>
#include <Preferences.h>
#include <WiFi.h>

class ArcadeWiFi {
public:
  static constexpr uint8_t MAX_SAVED_NETWORKS = 6;
  static constexpr uint8_t MAX_SCAN_NETWORKS = 12;

  struct Credential {
    String ssid;
    String password;
  };

  struct ScannedNetwork {
    String ssid;
    int rssi = 0;
    bool secure = false;
  };

  enum class State : uint8_t {
    IDLE,
    CONNECTING,
    CONNECTED,
    OFFLINE
  };

  void begin();
  void update();
  void stop();

  bool isConnected() const;
  bool isConnecting() const;
  bool initialConnectionFailed() const;
  const char* statusText() const;
  String connectedSsid() const;
  String localIp() const;

  size_t savedCount() const;
  Credential savedAt(size_t index) const;
  bool connectSaved(size_t index);
  void connectAndSave(const String& ssid, const String& password);
  void deleteSaved(size_t index);

  bool startScan();
  void cancelScan();
  bool scanInProgress() const;
  bool scanReady() const;
  size_t scanCount();
  ScannedNetwork scanAt(size_t index);

private:
  static constexpr uint8_t STORAGE_VERSION = 1;

  static void scanTaskEntry(void* parameter);
  void performScan();

  void loadSaved();
  void persistSaved();
  int findSaved(const String& ssid) const;
  void promoteSaved(size_t index);
  void addOrUpdateSaved(const String& ssid, const String& password);

  void startAutoCycle();
  void startAttempt(
    const String& ssid,
    const String& password,
    bool manualAttempt
  );

  Preferences preferences;
  ES_WiFi esWifi;
  ES_TimeInterval attemptTimer;

  Credential saved[MAX_SAVED_NETWORKS];
  uint8_t savedNetworkCount = 0;

  State currentState = State::IDLE;
  bool attemptActive = false;
  bool attemptIsManual = false;
  uint8_t autoTryIndex = 0;
  bool initialFailed = false;

  ScannedNetwork scanned[MAX_SCAN_NETWORKS];
  uint8_t scannedCount = 0;
  volatile bool scanInProgressFlag = false;
  volatile bool scanReadyFlag = false;
  volatile bool scanCancelRequested = false;

  SemaphoreHandle_t scanMutex = nullptr;
  TaskHandle_t scanTaskHandle = nullptr;
};
