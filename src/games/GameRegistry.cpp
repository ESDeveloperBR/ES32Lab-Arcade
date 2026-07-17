#include "GameRegistry.h"

#include "EnduroGame.h"
#include "ES32LabBoxeGame.h"
#include "FrogAdventureGame.h"
#include "SnakeGame.h"
#include "SpaceInvaderGame.h"
#include "TetrisGame.h"

namespace ArcadeGames {
namespace {

const Arcade::GameDefinition* const GAMES[] = {
  &TetrisGame,
  &SpaceInvaderGame,
  &SnakeGame,
  &EnduroGame,
  &FrogAdventureGame,
  &ES32LabBoxeGame
};

constexpr size_t GAME_COUNT = sizeof(GAMES) / sizeof(GAMES[0]);

}  // namespace

size_t count() {
  return GAME_COUNT;
}

const Arcade::GameDefinition& at(size_t index) {
  if (index >= GAME_COUNT) {
    index = 0;
  }

  return *GAMES[index];
}

}  // namespace ArcadeGames
