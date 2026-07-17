#pragma once

#include <Arduino.h>

// No PlatformIO, a versão é definida na seção [arcade] do platformio.ini.
// Este fallback mantém o projeto utilizável em outros ambientes.
#ifndef ES32LAB_ARCADE_VERSION
#define ES32LAB_ARCADE_VERSION "N/A"
#endif

namespace ArcadeVersion {

static constexpr char TEXT[] =
  ES32LAB_ARCADE_VERSION;

static constexpr char DISPLAY_TEXT[] =
  "v" ES32LAB_ARCADE_VERSION;

static constexpr char PROJECT_NAME[] = "ES32Lab Arcade";

// Data e hora são preenchidas automaticamente pelo compilador.
static constexpr char BUILD_DATE[] = __DATE__;
static constexpr char BUILD_TIME[] = __TIME__;

}  // namespace ArcadeVersion