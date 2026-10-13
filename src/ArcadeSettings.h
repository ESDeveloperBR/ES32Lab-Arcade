#pragma once

#include <Arduino.h>
#include <ES32Lab.h>

#include "ArcadeFirmwareUpdate.h"
#include "ArcadeWiFi.h"

class ArcadeSettings {
public:
  void begin(
    ArcadeWiFi& wifi,
    ArcadeFirmwareUpdate& updater
  );

  void beginStartupProvisioning(
    ArcadeWiFi& wifi,
    ArcadeFirmwareUpdate& updater
  );

  void beginUpdatePrompt(
    ArcadeWiFi& wifi,
    ArcadeFirmwareUpdate& updater
  );

  // true = sair das configuracoes e voltar ao hall de jogos.
  bool update();

private:
  enum class View : uint8_t {
    ROOT,
    WIFI,
    WIFI_SCAN,
    WIFI_DELETE,
    PASSWORD,
    UPDATE
  };

  enum class CenterEvent : uint8_t {
    NONE,
    SHORT,
    LONG
  };

  static constexpr uint8_t VISIBLE_ROWS = 4;

  void commonBegin(
    ArcadeWiFi& wifi,
    ArcadeFirmwareUpdate& updater
  );

  CenterEvent readCenterEvent();

  void drawBackground();
  void drawHeader(const char* title);
  void drawFooter(const char* text);
  void drawRow(
    int16_t y,
    const String& left,
    const String& right,
    bool selected
  );

  void renderRoot();
  void renderWifi();
  void renderWifiScan();
  void renderWifiDelete();
  void renderPassword();
  void drawPasswordEntryLine();
  void renderUpdate();
  void renderUpdateProgress(uint8_t percent);
  void renderWifiConnected();

  bool handleRoot(CenterEvent center);
  bool handleWifi(CenterEvent center);
  bool handleWifiScan(CenterEvent center);
  bool handleWifiDelete(CenterEvent center);
  bool handlePassword(CenterEvent center);
  bool handleUpdate(CenterEvent center);

  void servicePasswordCharacterInput();
  void changePasswordChar(int delta);
  char selectedPasswordChar();
  String passwordGroupName() const;
  void resetPasswordCursor();

  static void updateProgressBridge(
    uint8_t percent,
    void* context
  );

  ArcadeWiFi* wifi = nullptr;
  ArcadeFirmwareUpdate* updater = nullptr;

  ES_TFT display;
  ES_AnalogKeyboard keyboard{P_KEYBOARD};

  View view = View::ROOT;

  size_t rootIndex = 0;
  size_t wifiIndex = 0;
  size_t scanIndex = 0;
  size_t deleteIndex = 0;

  bool startupProvisioning = false;
  bool autoUpdatePrompt = false;
  bool waitingCenterRelease = true;

  bool centerTracking = false;
  bool centerLongFired = false;
  ES_TimeInterval centerHoldTimer;

  String passwordSsid;
  String passwordValue;
  bool passwordSecure = true;
  uint8_t passwordGroup = 0;
  uint8_t passwordCharIndex = 0;
  bool passwordHasPendingChar = false;

  ES_TimeInterval passwordHoldDelay;
  ES_TimeInterval passwordRepeatTimer;
  ES_TimeInterval passwordCursorTimer;
  bool passwordRepeatActive = false;
  uint16_t passwordRepeatCount = 0;
  bool passwordCursorVisible = true;

  bool lastScanReady = false;
  bool lastWifiConnected = false;
  bool lastWifiConnecting = false;
  ArcadeFirmwareUpdate::State lastUpdateState =
    ArcadeFirmwareUpdate::State::IDLE;

  bool startupSuccessActive = false;
  ES_TimeInterval startupSuccessTimer;
};
