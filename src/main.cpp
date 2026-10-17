#include <Arduino.h>
#include <ES32Lab.h>

#include "ES_WifiScanService.h"
#include "ArcadeController.h"

ES_WifiScanService wifiScanner;
ArcadeController arcade;

void setup() {
    Serial.begin(115200);

    // Janela inicial de manutencao para identificacao e scan Wi-Fi pela Serial.
    // Sem comando reconhecido, retorna automaticamente apos 500 ms.
    wifiScanner.begin(Serial);

    // Inicializacao normal do ES32Lab Arcade.
    arcade.begin();
}

void loop() {
    arcade.update();
}
