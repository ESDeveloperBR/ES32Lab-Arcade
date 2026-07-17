#pragma once

#include <Arduino.h>

#include "GameDefinition.h"

namespace ArcadeGames {

size_t count();
const Arcade::GameDefinition& at(size_t index);

}  // namespace ArcadeGames
