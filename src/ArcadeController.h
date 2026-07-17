#pragma once

#include <Arduino.h>

#include "ArcadeMenu.h"
#include "ArcadeOTA.h"
#include "games/GameDefinition.h"

class ArcadeController {
public:
  void begin();
  void update();

private:
  ArcadeMenu menu;
  ArcadeOTA ota;
  const Arcade::GameDefinition* activeGame = nullptr;
};
