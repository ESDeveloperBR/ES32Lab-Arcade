#pragma once

#include <Arduino.h>

// A versao oficial deve vir do platformio.ini.
// Este fallback existe apenas para compilacoes fora do fluxo de release.
#ifndef ES32LAB_ARCADE_VERSION
#define ES32LAB_ARCADE_VERSION "1.4.0"
#endif

namespace ArcadeVersion {

static constexpr char TEXT[] =
  ES32LAB_ARCADE_VERSION;

static constexpr char DISPLAY_TEXT[] =
  "v" ES32LAB_ARCADE_VERSION;

static constexpr char PROJECT_NAME[] = "ES32Lab Arcade";

static constexpr char BUILD_DATE[] = __DATE__;
static constexpr char BUILD_TIME[] = __TIME__;

}  // namespace ArcadeVersion
