#pragma once

#include <Arduino.h>

namespace Arcade {

using GameBeginCallback = void (*)();
using GameUpdateCallback = void (*)();

struct GameDefinition {
  const char* name;
  const char* description;
  GameBeginCallback begin;
  GameUpdateCallback update;
};

}  // namespace Arcade
