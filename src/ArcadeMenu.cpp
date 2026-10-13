#include "ArcadeMenu.h"

#include <cstring>

#include "ArcadeVersion.h"
#include "games/GameRegistry.h"

namespace {

constexpr uint16_t COLOR_BACKGROUND = 0x0008;
constexpr uint16_t COLOR_PANEL      = 0x0862;
constexpr uint16_t COLOR_PANEL_DARK = 0x0021;
constexpr uint16_t COLOR_BLUE       = 0x025F;
constexpr uint16_t COLOR_CYAN       = 0x07FF;
constexpr uint16_t COLOR_MAGENTA    = 0xF81F;
constexpr uint16_t COLOR_PURPLE     = 0x8010;
constexpr uint16_t COLOR_GOLD       = 0xFEA0;
constexpr uint16_t COLOR_SOFT_WHITE = 0xDEFB;
constexpr uint16_t COLOR_MUTED      = 0x6B4D;

void copyTruncated(
  char* destination,
  size_t destinationSize,
  const char* source,
  size_t maxCharacters
) {
  if (destinationSize == 0) {
    return;
  }

  if (source == nullptr) {
    destination[0] = '\0';
    return;
  }

  const size_t sourceLength =
    strlen(source);

  const size_t copyLength =
    sourceLength < maxCharacters
      ? sourceLength
      : maxCharacters;

  const size_t safeLength =
    copyLength < destinationSize - 1
      ? copyLength
      : destinationSize - 1;

  memcpy(
    destination,
    source,
    safeLength
  );

  destination[safeLength] = '\0';
}

void drawPixelStar(
  ES_TFT& display,
  int16_t x,
  int16_t y,
  uint16_t color,
  bool large = false
) {
  display.drawPixel(x, y, color);

  if (!large) {
    return;
  }

  display.drawPixel(x - 1, y, color);
  display.drawPixel(x + 1, y, color);
  display.drawPixel(x, y - 1, color);
  display.drawPixel(x, y + 1, color);
}

void drawBackgroundArt(
  ES_TFT& display
) {
  display.fillScreen(
    COLOR_BACKGROUND
  );

  drawPixelStar(
    display, 4, 7,
    COLOR_PURPLE, true
  );

  drawPixelStar(
    display, 23, 3,
    COLOR_CYAN
  );

  drawPixelStar(
    display, 49, 6,
    COLOR_MAGENTA
  );

  drawPixelStar(
    display, 78, 2,
    COLOR_CYAN
  );

  drawPixelStar(
    display, 111, 7,
    COLOR_PURPLE, true
  );

  drawPixelStar(
    display, 145, 3,
    COLOR_MAGENTA
  );

  drawPixelStar(
    display, 156, 15,
    COLOR_CYAN
  );

  drawPixelStar(
    display, 2, 27,
    COLOR_MAGENTA
  );

  display.drawLine(
    0, 30, 159, 30,
    COLOR_PURPLE
  );

  display.drawLine(
    0, 31, 159, 31,
    COLOR_BLUE
  );

  for (
    int16_t x = 0;
    x < 160;
    x += 16
  ) {
    display.drawPixel(
      x,
      29,
      COLOR_CYAN
    );
  }
}

void drawArcadeCabinet(
  ES_TFT& display,
  int16_t x,
  int16_t y
) {
  display.fillRect(
    x + 2,
    y,
    22,
    24,
    COLOR_PURPLE
  );

  display.fillRect(
    x,
    y + 3,
    26,
    15,
    COLOR_MAGENTA
  );

  display.fillRect(
    x + 2,
    y + 5,
    22,
    11,
    COLOR_PANEL_DARK
  );

  display.fillRect(
    x + 5,
    y + 7,
    16,
    7,
    COLOR_BLUE
  );

  display.drawPixel(
    x + 8,
    y + 9,
    COLOR_CYAN
  );

  display.drawPixel(
    x + 15,
    y + 8,
    COLOR_SOFT_WHITE
  );

  display.drawPixel(
    x + 18,
    y + 11,
    COLOR_MAGENTA
  );

  display.fillRect(
    x + 2,
    y + 18,
    22,
    4,
    COLOR_GOLD
  );

  display.fillRect(
    x + 6,
    y + 22,
    4,
    4,
    COLOR_PURPLE
  );

  display.fillRect(
    x + 17,
    y + 22,
    4,
    4,
    COLOR_PURPLE
  );

  display.drawLine(
    x + 8,
    y + 17,
    x + 8,
    y + 19,
    COLOR_SOFT_WHITE
  );

  display.fillRect(
    x + 7,
    y + 16,
    3,
    2,
    COLOR_CYAN
  );

  display.fillRect(
    x + 16,
    y + 18,
    2,
    2,
    COLOR_MAGENTA
  );
}

void drawHeader(
  ES_TFT& display,
  const char* systemStatus
) {
  drawArcadeCabinet(
    display,
    6,
    3
  );

  display.setTextSize(1);

  display.setTextColor(
    COLOR_CYAN,
    COLOR_BACKGROUND
  );

  display.drawString(
    "ES32Lab",
    38,
    3,
    1
  );

  // Mantem ARCADE compacto para nunca invadir a lista.
  display.setTextColor(
    COLOR_SOFT_WHITE,
    COLOR_BACKGROUND
  );

  display.drawString(
    "ARCADE",
    38,
    11,
    2
  );

  display.fillRect(
    94, 14, 3, 3,
    COLOR_MAGENTA
  );

  display.fillRect(
    100, 14, 3, 3,
    COLOR_CYAN
  );

  display.fillRect(
    106, 14, 3, 3,
    COLOR_GOLD
  );

  display.setTextColor(
    COLOR_MUTED,
    COLOR_BACKGROUND
  );

  display.drawString(
    ArcadeVersion::DISPLAY_TEXT,
    121,
    3,
    1
  );

  if (
    systemStatus != nullptr &&
    systemStatus[0] != '\0'
  ) {
    display.setTextColor(
      COLOR_GOLD,
      COLOR_BACKGROUND
    );

    display.drawString(
      systemStatus,
      112,
      22,
      1
    );
  } else {
    display.setTextColor(
      COLOR_MUTED,
      COLOR_BACKGROUND
    );

    display.drawString(
      "SELECT",
      124,
      22,
      1
    );
  }
}

void drawScrollArrow(
  ES_TFT& display,
  int16_t centerX,
  int16_t centerY,
  bool pointsUp
) {
  if (pointsUp) {
    display.fillTriangle(
      centerX,
      centerY - 3,
      centerX - 3,
      centerY + 2,
      centerX + 3,
      centerY + 2,
      COLOR_GOLD
    );
  } else {
    display.fillTriangle(
      centerX,
      centerY + 3,
      centerX - 3,
      centerY - 2,
      centerX + 3,
      centerY - 2,
      COLOR_GOLD
    );
  }
}

}  // namespace

void ArcadeMenu::begin() {
  display.init();
  display.setRotation(3);

  selectedIndex = 0;
  firstVisibleIndex = 0;
  waitingCenterRelease = false;
  initialized = true;

  draw();
}

void ArcadeMenu::resume() {
  initialized = true;
  waitingCenterRelease = false;

  if (selectedIndex >= itemCount()) {
    selectedIndex = 0;
  }

  keepSelectionVisible();
  draw();
}

int ArcadeMenu::update() {
  const size_t total =
    itemCount();

  if (total == 0) {
    return NO_SELECTION;
  }

  if (waitingCenterRelease) {
    if (keyboard.release(KEY_CENTER)) {
      waitingCenterRelease = false;

      if (
        selectedIndex ==
        ArcadeGames::count()
      ) {
        return SETTINGS_SELECTION;
      }

      return
        static_cast<int>(
          selectedIndex
        );
    }

    return NO_SELECTION;
  }

  if (keyboard.press(KEY_UP)) {
    selectedIndex =
      (
        selectedIndex +
        total -
        1
      ) %
      total;

    keepSelectionVisible();
    draw();
  }

  if (keyboard.press(KEY_DOWN)) {
    selectedIndex =
      (
        selectedIndex + 1
      ) %
      total;

    keepSelectionVisible();
    draw();
  }

  if (keyboard.press(KEY_CENTER)) {
    waitingCenterRelease = true;

    display.fillRect(
      28,
      43,
      104,
      42,
      COLOR_PANEL_DARK
    );

    display.drawRect(
      28,
      43,
      104,
      42,
      COLOR_MAGENTA
    );

    display.drawRect(
      30,
      45,
      100,
      38,
      COLOR_CYAN
    );

    display.setTextSize(1);

    display.setTextColor(
      COLOR_GOLD,
      COLOR_PANEL_DARK
    );

    display.drawCentreScreenString(
      "SOLTE CENTER",
      53,
      1
    );

    display.setTextColor(
      COLOR_SOFT_WHITE,
      COLOR_PANEL_DARK
    );

    display.drawCentreScreenString(
      selectedIndex == ArcadeGames::count()
        ? "CONFIGURAR"
        : "PARA JOGAR",
      67,
      1
    );
  }

  return NO_SELECTION;
}

void ArcadeMenu::setSystemStatus(
  const char* statusText
) {
  char nextStatus[
    sizeof(systemStatus)
  ];

  copyTruncated(
    nextStatus,
    sizeof(nextStatus),
    statusText,
    sizeof(nextStatus) - 1
  );

  if (
    strcmp(
      systemStatus,
      nextStatus
    ) == 0
  ) {
    return;
  }

  memcpy(
    systemStatus,
    nextStatus,
    sizeof(systemStatus)
  );

  systemStatus[
    sizeof(systemStatus) - 1
  ] = '\0';

  if (initialized) {
    draw();
  }
}

void ArcadeMenu::showLaunching(
  const char* gameName
) {
  drawBackgroundArt(display);

  display.fillRect(
    20,
    40,
    120,
    50,
    COLOR_PANEL_DARK
  );

  display.drawRect(
    20,
    40,
    120,
    50,
    COLOR_PURPLE
  );

  display.drawRect(
    22,
    42,
    116,
    46,
    COLOR_BLUE
  );

  display.fillRect(
    31,
    51,
    4,
    28,
    COLOR_MAGENTA
  );

  display.fillRect(
    36,
    47,
    4,
    36,
    COLOR_PURPLE
  );

  display.fillRect(
    41,
    44,
    4,
    42,
    COLOR_BLUE
  );

  display.fillRect(
    46,
    48,
    4,
    34,
    COLOR_CYAN
  );

  display.fillTriangle(
    51,
    58,
    51,
    72,
    59,
    65,
    COLOR_GOLD
  );

  display.setTextSize(1);

  display.setTextColor(
    COLOR_CYAN,
    COLOR_PANEL_DARK
  );

  display.drawString(
    "INICIANDO",
    66,
    49,
    1
  );

  char safeName[15];

  copyTruncated(
    safeName,
    sizeof(safeName),
    gameName,
    14
  );

  display.setTextColor(
    COLOR_SOFT_WHITE,
    COLOR_PANEL_DARK
  );

  display.drawString(
    safeName,
    66,
    64,
    1
  );

  display.setTextColor(
    COLOR_GOLD,
    COLOR_PANEL_DARK
  );

  display.drawString(
    "READY!",
    66,
    77,
    1
  );

  display.setTextColor(
    COLOR_MUTED,
    COLOR_BACKGROUND
  );

  display.drawCentreScreenString(
    "RESET FISICO = MENU",
    111,
    1
  );
}

void ArcadeMenu::draw() {
  drawBackgroundArt(display);
  drawHeader(
    display,
    systemStatus
  );

  const size_t total =
    itemCount();

  const size_t remaining =
    total > firstVisibleIndex
      ? total - firstVisibleIndex
      : 0;

  const size_t rows =
    remaining < VISIBLE_ROWS
      ? remaining
      : VISIBLE_ROWS;

  for (
    size_t row = 0;
    row < rows;
    ++row
  ) {
    const size_t itemIndex =
      firstVisibleIndex + row;

    drawMenuRow(
      itemIndex,
      row,
      itemIndex == selectedIndex
    );
  }

  if (firstVisibleIndex > 0) {
    drawScrollArrow(
      display,
      154,
      40,
      true
    );
  }

  if (
    firstVisibleIndex + rows <
    total
  ) {
    drawScrollArrow(
      display,
      154,
      97,
      false
    );
  }

  display.fillRect(
    0,
    103,
    160,
    25,
    COLOR_PANEL_DARK
  );

  display.drawLine(
    0,
    103,
    159,
    103,
    COLOR_BLUE
  );

  display.setTextSize(1);

  if (
    selectedIndex ==
    ArcadeGames::count()
  ) {
    display.setTextColor(
      COLOR_GOLD,
      COLOR_PANEL_DARK
    );

    display.drawCentreScreenString(
      "WI-FI / ATUALIZACAO",
      107,
      1
    );

    display.setTextColor(
      COLOR_MUTED,
      COLOR_PANEL_DARK
    );

    display.drawCentreScreenString(
      "CIMA/BAIXO  CENTER:ENTRA",
      119,
      1
    );

    return;
  }

  const auto& selectedGame =
    ArcadeGames::at(
      selectedIndex
    );

  char description[25];

  copyTruncated(
    description,
    sizeof(description),
    selectedGame.description,
    24
  );

  display.setTextColor(
    COLOR_GOLD,
    COLOR_PANEL_DARK
  );

  display.drawCentreScreenString(
    description,
    107,
    1
  );

  display.setTextColor(
    COLOR_MUTED,
    COLOR_PANEL_DARK
  );

  display.drawCentreScreenString(
    "CIMA/BAIXO  CENTER:JOGAR",
    119,
    1
  );
}

void ArcadeMenu::drawMenuRow(
  size_t itemIndex,
  size_t visibleRow,
  bool selected
) {
  constexpr int16_t ROW_X = 6;
  constexpr int16_t ROW_W = 146;
  constexpr int16_t ROW_H = 17;
  constexpr int16_t FIRST_ROW_Y = 34;

  const int16_t y =
    FIRST_ROW_Y +
    static_cast<int16_t>(
      visibleRow
    ) *
    ROW_H;

  const uint16_t background =
    selected
      ? COLOR_BLUE
      : COLOR_PANEL_DARK;

  const uint16_t border =
    selected
      ? COLOR_CYAN
      : COLOR_PANEL;

  display.fillRect(
    ROW_X,
    y,
    ROW_W,
    ROW_H - 2,
    background
  );

  display.drawRect(
    ROW_X,
    y,
    ROW_W,
    ROW_H - 2,
    border
  );

  if (selected) {
    display.fillRect(
      ROW_X,
      y,
      3,
      ROW_H - 2,
      COLOR_MAGENTA
    );

    display.fillRect(
      ROW_X + 3,
      y,
      2,
      ROW_H - 2,
      COLOR_PURPLE
    );

    display.fillTriangle(
      ROW_X + 9,
      y + 4,
      ROW_X + 9,
      y + 10,
      ROW_X + 14,
      y + 7,
      COLOR_GOLD
    );
  } else {
    display.drawRect(
      ROW_X + 5,
      y + 5,
      4,
      4,
      COLOR_PURPLE
    );

    display.drawPixel(
      ROW_X + 6,
      y + 6,
      COLOR_CYAN
    );
  }

  const bool isSettings =
    itemIndex ==
    ArcadeGames::count();

  char safeName[18];

  if (isSettings) {
    copyTruncated(
      safeName,
      sizeof(safeName),
      "CONFIGURACOES",
      17
    );
  } else {
    const auto& game =
      ArcadeGames::at(itemIndex);

    copyTruncated(
      safeName,
      sizeof(safeName),
      game.name,
      17
    );
  }

  display.setTextColor(
    selected
      ? COLOR_SOFT_WHITE
      : COLOR_MUTED,
    background
  );

  display.drawString(
    safeName,
    ROW_X + 18,
    y + 4,
    1
  );

  char marker[4];

  if (isSettings) {
    snprintf(
      marker,
      sizeof(marker),
      "CFG"
    );
  } else {
    snprintf(
      marker,
      sizeof(marker),
      "%02u",
      static_cast<unsigned int>(
        itemIndex + 1
      )
    );
  }

  display.setTextColor(
    selected
      ? COLOR_GOLD
      : COLOR_PURPLE,
    background
  );

  display.drawString(
    marker,
    ROW_X + 128,
    y + 4,
    1
  );
}

void ArcadeMenu::keepSelectionVisible() {
  if (
    selectedIndex <
    firstVisibleIndex
  ) {
    firstVisibleIndex =
      selectedIndex;

    return;
  }

  if (
    selectedIndex >=
    firstVisibleIndex +
    VISIBLE_ROWS
  ) {
    firstVisibleIndex =
      selectedIndex -
      VISIBLE_ROWS +
      1;
  }
}

size_t ArcadeMenu::itemCount() const {
  // Configuracoes sempre ocupa a ultima posicao.
  return ArcadeGames::count() + 1;
}
