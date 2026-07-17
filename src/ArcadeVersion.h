#pragma once

#include <Arduino.h>

// Para criar uma nova versao, altere somente estes tres numeros.
#define ES32LAB_ARCADE_VERSION_MAJOR 1
#define ES32LAB_ARCADE_VERSION_MINOR 2
#define ES32LAB_ARCADE_VERSION_PATCH 0

#define ES32LAB_ARCADE_STRINGIFY_VALUE(value) #value
#define ES32LAB_ARCADE_STRINGIFY(value) \
  ES32LAB_ARCADE_STRINGIFY_VALUE(value)

#define ES32LAB_ARCADE_VERSION_TEXT \
  ES32LAB_ARCADE_STRINGIFY(ES32LAB_ARCADE_VERSION_MAJOR) "." \
  ES32LAB_ARCADE_STRINGIFY(ES32LAB_ARCADE_VERSION_MINOR) "." \
  ES32LAB_ARCADE_STRINGIFY(ES32LAB_ARCADE_VERSION_PATCH)

namespace ArcadeVersion {

static constexpr uint8_t MAJOR = ES32LAB_ARCADE_VERSION_MAJOR;
static constexpr uint8_t MINOR = ES32LAB_ARCADE_VERSION_MINOR;
static constexpr uint8_t PATCH = ES32LAB_ARCADE_VERSION_PATCH;

static constexpr char TEXT[] = ES32LAB_ARCADE_VERSION_TEXT;
static constexpr char DISPLAY_TEXT[] =
  "v" ES32LAB_ARCADE_VERSION_TEXT;

static constexpr char PROJECT_NAME[] = "ES32Lab Arcade";

// Data e hora sao preenchidas automaticamente pelo compilador.
static constexpr char BUILD_DATE[] = __DATE__;
static constexpr char BUILD_TIME[] = __TIME__;

}  // namespace ArcadeVersion
