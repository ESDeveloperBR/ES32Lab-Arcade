#pragma once

#include <Arduino.h>

namespace ArcadeConfig {

// Wi-Fi usado somente enquanto o menu principal estiver aberto.
static constexpr char WIFI_SSID[] = "homeF";
static constexpr char WIFI_PASSWORD[] = "estouemcasa";

// Identificacao exibida pelas ferramentas de atualizacao em rede.
static constexpr char OTA_HOSTNAME[] = "es32lab-arcade";

// Senha de autorizacao do OTA.
// Mantida centralizada para facilitar uma troca futura.
static constexpr char OTA_PASSWORD[] = "estouemcasa";

// A tentativa e unica em cada inicializacao. Se falhar, o arcade segue offline.
static constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 6000UL;

}  // namespace ArcadeConfig
