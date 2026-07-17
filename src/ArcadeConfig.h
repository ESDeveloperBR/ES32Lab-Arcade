#pragma once

#include <Arduino.h>

// Os valores abaixo chegam pelo platformio.ini.
// Os fallbacks mantêm compatibilidade com compilação fora do PlatformIO.
#ifndef ES32LAB_ARCADE_WIFI_SSID
#define ES32LAB_ARCADE_WIFI_SSID "N/A"
#endif

#ifndef ES32LAB_ARCADE_WIFI_PASSWORD
#define ES32LAB_ARCADE_WIFI_PASSWORD "N/A"
#endif

#ifndef ES32LAB_ARCADE_OTA_HOSTNAME
#define ES32LAB_ARCADE_OTA_HOSTNAME "es32lab-arcade"
#endif

#ifndef ES32LAB_ARCADE_OTA_PASSWORD
#define ES32LAB_ARCADE_OTA_PASSWORD "N/A"
#endif

namespace ArcadeConfig {

// Wi-Fi usado somente enquanto o menu principal estiver aberto.
static constexpr char WIFI_SSID[] =
  ES32LAB_ARCADE_WIFI_SSID;

static constexpr char WIFI_PASSWORD[] =
  ES32LAB_ARCADE_WIFI_PASSWORD;

// Identificação exibida pelas ferramentas de atualização em rede.
static constexpr char OTA_HOSTNAME[] =
  ES32LAB_ARCADE_OTA_HOSTNAME;

// Senha de autorização do OTA.
static constexpr char OTA_PASSWORD[] =
  ES32LAB_ARCADE_OTA_PASSWORD;

// A tentativa é única em cada inicialização. Se falhar, o arcade segue offline.
static constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 6000UL;

}  // namespace ArcadeConfig