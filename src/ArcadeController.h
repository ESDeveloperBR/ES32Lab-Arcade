#pragma once

#include <Arduino.h>

#include "ArcadeFirmwareUpdate.h"
#include "ArcadeMenu.h"
#include "ArcadeOTA.h"
#include "ArcadeSettings.h"
#include "ArcadeWiFi.h"
#include "games/GameDefinition.h"

class ArcadeController {
public:
  void begin();
  void update();

private:
  void updateSystemStatus();
  void enterSettings();
  void enterStartupWifiProvisioning();
  void enterAutomaticUpdatePrompt();
  void launchGame(size_t gameIndex);

  ArcadeMenu menu;
  ArcadeSettings settings;
  ArcadeWiFi wifi;
  ArcadeOTA ota;
  ArcadeFirmwareUpdate firmwareUpdate;

  const Arcade::GameDefinition* activeGame = nullptr;

  bool settingsActive = false;
  bool startupWifiProvisioningOffered = false;
  bool automaticUpdatePrompted = false;
};
