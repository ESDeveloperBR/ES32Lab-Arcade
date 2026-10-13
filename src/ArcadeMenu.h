#pragma once

#include <Arduino.h>
#include <ES32Lab.h>

class ArcadeMenu {
public:
  static constexpr int NO_SELECTION = -1;
  static constexpr int SETTINGS_SELECTION = -2;

  void begin();
  void resume();
  int update();

  void showLaunching(const char* gameName);
  void setSystemStatus(const char* statusText);

private:
  static constexpr size_t VISIBLE_ROWS = 4;

  void draw();
  void drawMenuRow(
    size_t itemIndex,
    size_t visibleRow,
    bool selected
  );
  void keepSelectionVisible();
  size_t itemCount() const;

  ES_TFT display;
  ES_AnalogKeyboard keyboard{P_KEYBOARD};

  size_t selectedIndex = 0;
  size_t firstVisibleIndex = 0;

  bool waitingCenterRelease = false;
  bool initialized = false;

  char systemStatus[12] = "";
};
