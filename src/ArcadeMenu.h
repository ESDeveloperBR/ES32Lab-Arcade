#pragma once

#include <Arduino.h>
#include <ES32Lab.h>

class ArcadeMenu {
public:
  void begin();
  int update();
  void showLaunching(const char* gameName);
  void setSystemStatus(const char* statusText);

private:
  static constexpr size_t VISIBLE_ROWS = 4;

  void draw();
  void drawGameRow(size_t gameIndex, size_t visibleRow, bool selected);
  void keepSelectionVisible();

  ES_TFT display;
  ES_AnalogKeyboard keyboard{P_KEYBOARD};
  size_t selectedIndex = 0;
  size_t firstVisibleIndex = 0;
  bool waitingCenterRelease = false;
  bool initialized = false;
  char systemStatus[12] = "";
};
