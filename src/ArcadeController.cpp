#include "ArcadeController.h"

#include <ES32Lab.h>

#include "ArcadeVersion.h"
#include "games/GameRegistry.h"

void ArcadeController::begin() {
  Serial.begin(115200);
  Serial.println();
  Serial.print(ArcadeVersion::PROJECT_NAME);
  Serial.print(" v");
  Serial.println(ArcadeVersion::TEXT);

  Serial.print("Build: ");
  Serial.print(ArcadeVersion::BUILD_DATE);
  Serial.print(" ");
  Serial.println(ArcadeVersion::BUILD_TIME);

  Serial.print("ES32Lab LIB: ");
  Serial.println(ES32LAB_VERSION);

  menu.begin();

  ota.begin();
  menu.setSystemStatus(ota.statusText());
}

void ArcadeController::update() {
  if (activeGame != nullptr) {
    activeGame->update();
    return;
  }

  ota.update();
  menu.setSystemStatus(ota.statusText());

  // Durante uma transferencia OTA, o menu nao inicia jogos.
  if (ota.isUpdating()) {
    return;
  }

  const int selectedGame = menu.update();

  if (selectedGame < 0) {
    return;
  }

  activeGame = &ArcadeGames::at(
    static_cast<size_t>(selectedGame)
  );

  // Jogos rodam sem Wi-Fi e sem servico OTA.
  ota.stop();

  menu.showLaunching(activeGame->name);
  activeGame->begin();
}
