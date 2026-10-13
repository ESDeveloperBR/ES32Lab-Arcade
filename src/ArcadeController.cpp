#include "ArcadeController.h"

#include <ES32Lab.h>

#include "ArcadeVersion.h"
#include "games/GameRegistry.h"

void ArcadeController::begin() {
  Serial.begin(115200);

  Serial.println();

  Serial.print(
    ArcadeVersion::PROJECT_NAME
  );

  Serial.print(" v");

  Serial.println(
    ArcadeVersion::TEXT
  );

  Serial.print("Build: ");

  Serial.print(
    ArcadeVersion::BUILD_DATE
  );

  Serial.print(" ");

  Serial.println(
    ArcadeVersion::BUILD_TIME
  );

  Serial.print("ES32Lab LIB: ");

  Serial.println(
    ES32LAB_VERSION
  );

  menu.begin();

  firmwareUpdate.begin();
  wifi.begin();
  ota.begin();

  updateSystemStatus();
}

void ArcadeController::update() {
  if (activeGame != nullptr) {
    activeGame->update();
    return;
  }

  wifi.update();

  ota.update(
    wifi.isConnected()
  );

  // A consulta automatica so comeca quando realmente existe Internet.
  if (
    wifi.isConnected() &&
    !wifi.scanInProgress() &&
    !firmwareUpdate
      .automaticCheckStarted()
  ) {
    firmwareUpdate
      .startAutomaticCheck();
  }

  updateSystemStatus();

  if (settingsActive) {
    if (settings.update()) {
      settingsActive = false;
      menu.resume();
      updateSystemStatus();
    }

    return;
  }

  // Replica o comportamento do firmware de referencia:
  // se a verificacao silenciosa encontrou uma versao nova,
  // oferece a tela de atualizacao. O usuario pode simplesmente voltar.
  if (
    !automaticUpdatePrompted &&
    firmwareUpdate
      .automaticCheckCompleted() &&
    firmwareUpdate
      .updateAvailable()
  ) {
    enterAutomaticUpdatePrompt();
    return;
  }

  // Se nenhuma das redes conhecidas respondeu na inicializacao,
  // abre o scanner. ESQUERDA cancela e libera o Arcade offline.
  if (
    !startupWifiProvisioningOffered &&
    wifi.initialConnectionFailed()
  ) {
    enterStartupWifiProvisioning();
    return;
  }

  if (ota.isUpdating()) {
    return;
  }

  const int selection =
    menu.update();

  if (
    selection ==
    ArcadeMenu::NO_SELECTION
  ) {
    return;
  }

  if (
    selection ==
    ArcadeMenu::SETTINGS_SELECTION
  ) {
    enterSettings();
    return;
  }

  if (
    selection >= 0 &&
    static_cast<size_t>(selection) <
      ArcadeGames::count()
  ) {
    launchGame(
      static_cast<size_t>(
        selection
      )
    );
  }
}

void ArcadeController::updateSystemStatus() {
  if (ota.isUpdating()) {
    menu.setSystemStatus(
      ota.statusText()
    );

    return;
  }

  if (ota.isReady()) {
    menu.setSystemStatus(
      ota.statusText()
    );

    return;
  }

  menu.setSystemStatus(
    wifi.statusText()
  );
}

void ArcadeController::enterSettings() {
  settingsActive = true;

  settings.begin(
    wifi,
    firmwareUpdate
  );
}

void ArcadeController::enterStartupWifiProvisioning() {
  startupWifiProvisioningOffered = true;
  settingsActive = true;

  settings.beginStartupProvisioning(
    wifi,
    firmwareUpdate
  );
}

void ArcadeController::enterAutomaticUpdatePrompt() {
  automaticUpdatePrompted = true;
  settingsActive = true;

  settings.beginUpdatePrompt(
    wifi,
    firmwareUpdate
  );
}

void ArcadeController::launchGame(
  size_t gameIndex
) {
  activeGame =
    &ArcadeGames::at(
      gameIndex
    );

  // A consulta silenciosa, caso ainda esteja em andamento,
  // deixa de ser relevante quando o usuario decidiu jogar.
  firmwareUpdate.cancelCheck();

  ota.stop();
  wifi.stop();

  menu.showLaunching(
    activeGame->name
  );

  activeGame->begin();
}
