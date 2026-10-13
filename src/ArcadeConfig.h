#pragma once

#include <Arduino.h>

// Valores recebidos do platformio.ini.
// Os fallbacks permitem compilar o fonte fora do PlatformIO.
#ifndef ES32LAB_ARCADE_WIFI_SSID
#define ES32LAB_ARCADE_WIFI_SSID ""
#endif

#ifndef ES32LAB_ARCADE_WIFI_PASSWORD
#define ES32LAB_ARCADE_WIFI_PASSWORD ""
#endif

#ifndef ES32LAB_ARCADE_OTA_HOSTNAME
#define ES32LAB_ARCADE_OTA_HOSTNAME "es32lab-arcade"
#endif

#ifndef ES32LAB_ARCADE_OTA_PASSWORD
#define ES32LAB_ARCADE_OTA_PASSWORD ""
#endif

// Compatibilidade com o nome usado no firmware de treinamento.
// O PlatformIO pode definir diretamente OTA_MANIFEST_URL ou
// ES32LAB_ARCADE_MANIFEST_URL.
#ifndef ES32LAB_ARCADE_MANIFEST_URL
  #ifdef OTA_MANIFEST_URL
    #define ES32LAB_ARCADE_MANIFEST_URL OTA_MANIFEST_URL
  #else
    #define ES32LAB_ARCADE_MANIFEST_URL ""
  #endif
#endif

namespace ArcadeConfig {

// Credencial inicial/fallback. Ela somente entra no armazenamento ES_Wifi
// quando ainda nao existe nenhuma rede cadastrada na NVS.
static constexpr char WIFI_SSID[] =
  ES32LAB_ARCADE_WIFI_SSID;

static constexpr char WIFI_PASSWORD[] =
  ES32LAB_ARCADE_WIFI_PASSWORD;

static constexpr char OTA_HOSTNAME[] =
  ES32LAB_ARCADE_OTA_HOSTNAME;

static constexpr char OTA_PASSWORD[] =
  ES32LAB_ARCADE_OTA_PASSWORD;

static constexpr char MANIFEST_URL[] =
  ES32LAB_ARCADE_MANIFEST_URL;

static constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 8000UL;
static constexpr uint32_t AUTO_UPDATE_TIMEOUT_MS = 5000UL;
static constexpr uint32_t MANUAL_UPDATE_TIMEOUT_MS = 15000UL;

}  // namespace ArcadeConfig
