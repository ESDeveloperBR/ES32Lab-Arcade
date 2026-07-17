#include <Arduino.h>
#include <ES32Lab.h>

#include "FrogAdventureGame.h"

namespace ArcadeGames {
namespace FrogAdventureModule {

// ======================================================
// Jogo do sapo para ES32Lab
//
// Menu:
//   ESQUERDA/DIREITA: selecionar dificuldade
//   CENTRO: iniciar
//
// Durante o jogo:
//   ESQUERDA/DIREITA: movimentar
//   CIMA: pular
//   CENTRO: lançar a língua
// ======================================================

ES_TFT display;
ES_AnalogKeyboard keyboard(P_KEYBOARD);
ES_Buzzer buzzer(P_BUZZER);

ES_TimeInterval frameTimer;
ES_TimeInterval secondTimer;
ES_TimeInterval transitionTimer;
ES_TimeInterval phaseClearedAnimTimer;
ES_TimeInterval crocodileMouthTimer;
ES_TimeInterval confettiTimer;

ES_TimeInterval gameOverBlinkStartTimer;
ES_TimeInterval gameOverBlinkDurationTimer;

bool gameOverHealthyEyeClosed = false;
unsigned long gameOverNextBlinkMs = 2200;

// ======================================================
// Tela e cenário
// ======================================================

constexpr int SCREEN_W = 160;
constexpr int SCREEN_H = 128;

constexpr int HUD_H = 16;
constexpr int GROUND_Y = 108;

constexpr int PHASE_SECONDS = 30;
constexpr int SPECIAL_FLY_CHANCE = 20;

// ======================================================
// Sapo
// ======================================================

constexpr int FROG_W = 16;
constexpr int FROG_H = 12;
constexpr int FROG_GROUND_Y = GROUND_Y - FROG_H;

constexpr int TONGUE_LENGTH = 60;

int frogX = 72;
int frogY = FROG_GROUND_Y;
int jumpVelocity = 0;

bool jumping = false;

int tongueFrames = 0;
bool tongueWasDrawn = false;

// ======================================================
// Moscas
// ======================================================

constexpr int FLY_COUNT = 3;

struct Fly {
  int x;
  int y;
  int speed;
  bool special;
};

Fly flies[FLY_COUNT];

// ======================================================
// Jacarés
// ======================================================

constexpr int MAX_CROCODILES = 2;

constexpr int CROC_W = 24;
constexpr int CROC_H = 12;
constexpr int CROC_Y = GROUND_Y - CROC_H;

struct Crocodile {
  int x;
  int speed;
};

Crocodile crocodiles[MAX_CROCODILES];

bool crocodileMouthOpen = false;

// ======================================================
// Confetes
// ======================================================

constexpr int CONFETTI_COUNT = 18;

constexpr int CONFETTI_TOP = 39;
constexpr int CONFETTI_BOTTOM = 105;

struct Confetti {
  int x;
  int y;
  int speed;
  uint16_t color;
};

Confetti confetti[CONFETTI_COUNT];

bool confettiVisible = true;

// ======================================================
// Estados
// ======================================================

enum GameState {
  DIFFICULTY_MENU,
  PLAYING,
  PHASE_CLEARED,
  GAME_OVER,
  VICTORY
};

enum Difficulty {
  EASY,
  MEDIUM,
  HARD
};

GameState gameState = DIFFICULTY_MENU;
Difficulty selectedDifficulty = EASY;

// ======================================================
// Pontuação
// ======================================================

int currentPhase = 1;
int phaseScore = 0;
int totalScore = 0;
int timeLeft = PHASE_SECONDS;

bool hudDirty = true;

// ======================================================
// Configuração da dificuldade
// ======================================================

const char* difficultyName() {
  if (selectedDifficulty == EASY) {
    return "FACIL";
  }

  if (selectedDifficulty == MEDIUM) {
    return "MEDIO";
  }

  return "DIFICIL";
}

const char* difficultyShortName() {
  if (selectedDifficulty == EASY) {
    return "F";
  }

  if (selectedDifficulty == MEDIUM) {
    return "M";
  }

  return "D";
}

int jumpStartVelocity() {
  if (selectedDifficulty == EASY) {
    return -11;
  }

  if (selectedDifficulty == MEDIUM) {
    return -10;
  }

  return -8;
}

int activeCrocodileCount() {
  if (selectedDifficulty == EASY) {
    return 1;
  }

  return 2;
}

// ======================================================
// Configuração das fases
// ======================================================

const char* phaseName() {
  if (currentPhase == 1) {
    return "MANHA";
  }

  if (currentPhase == 2) {
    return "TARDE";
  }

  return "NOITE";
}

int crocodilePenalty() {
  // Manhã: -1
  // Tarde: -2
  // Noite: -3
  return currentPhase;
}

uint16_t skyColor() {
  if (currentPhase == 1) {
    return TFT_CYAN;
  }

  if (currentPhase == 2) {
    return TFT_ORANGE;
  }

  return TFT_NAVY;
}

uint16_t groundColor() {
  if (currentPhase == 1) {
    return TFT_GREEN;
  }

  return TFT_DARKGREEN;
}

// ======================================================
// Restauração parcial do fundo
// Evita apagar e redesenhar a tela inteira.
// ======================================================

void restoreBackgroundRect(
  int x,
  int y,
  int width,
  int height
) {
  if (width <= 0 || height <= 0) {
    return;
  }

  int x0 = max(0, x);
  int y0 = max(HUD_H, y);

  int x1 = min(SCREEN_W, x + width);
  int y1 = min(SCREEN_H, y + height);

  if (x0 >= x1 || y0 >= y1) {
    return;
  }

  int skyBottom = min(y1, GROUND_Y);

  if (y0 < skyBottom) {
    display.fillRect(
      x0,
      y0,
      x1 - x0,
      skyBottom - y0,
      skyColor()
    );
  }

  int groundTop = max(y0, GROUND_Y);

  if (groundTop < y1) {
    display.fillRect(
      x0,
      groundTop,
      x1 - x0,
      y1 - groundTop,
      groundColor()
    );
  }

  if (y0 <= GROUND_Y && y1 > GROUND_Y) {
    display.drawLine(
      x0,
      GROUND_Y,
      x1 - 1,
      GROUND_Y,
      TFT_DARKGREEN
    );
  }
}

// ======================================================
// Cenário
// ======================================================

void drawPhaseDecoration() {
  if (currentPhase == 1) {
    // Sol da manhã
    display.fillCircle(
      145,
      29,
      7,
      TFT_YELLOW
    );

    // Nuvem
    display.fillCircle(
      20,
      31,
      4,
      TFT_WHITE
    );

    display.fillCircle(
      26,
      29,
      6,
      TFT_WHITE
    );

    display.fillCircle(
      33,
      31,
      4,
      TFT_WHITE
    );

    return;
  }

  if (currentPhase == 2) {
    // Sol da tarde
    display.fillCircle(
      145,
      45,
      8,
      TFT_YELLOW
    );

    display.drawLine(
      0,
      88,
      SCREEN_W - 1,
      88,
      TFT_RED
    );

    return;
  }

  // Lua
  display.fillCircle(
    145,
    30,
    8,
    TFT_WHITE
  );

  display.fillCircle(
    149,
    27,
    8,
    TFT_NAVY
  );

  // Estrelas
  display.drawPixel(18, 28, TFT_WHITE);
  display.drawPixel(42, 39, TFT_WHITE);
  display.drawPixel(76, 25, TFT_WHITE);
  display.drawPixel(104, 48, TFT_WHITE);
  display.drawPixel(126, 69, TFT_WHITE);
}

void drawHUD() {
  display.fillRect(
    0,
    0,
    SCREEN_W,
    HUD_H,
    TFT_BLACK
  );

  display.drawLine(
    0,
    HUD_H - 1,
    SCREEN_W - 1,
    HUD_H - 1,
    TFT_DARKGREY
  );

  display.setTextSize(1);
  display.setTextColor(
    TFT_WHITE,
    TFT_BLACK
  );

  display.setCursor(2, 4);
  display.print("F");
  display.print(currentPhase);
  display.print("-");
  display.print(difficultyShortName());

  display.setCursor(37, 4);
  display.print(phaseName());

  display.setCursor(83, 4);
  display.print("P:");
  display.print(phaseScore);

  display.setCursor(126, 4);
  display.print("T:");
  display.print(timeLeft);

  hudDirty = false;
}

void drawStaticScene() {
  display.fillScreen(skyColor());

  display.fillRect(
    0,
    GROUND_Y,
    SCREEN_W,
    SCREEN_H - GROUND_Y,
    groundColor()
  );

  display.drawLine(
    0,
    GROUND_Y,
    SCREEN_W - 1,
    GROUND_Y,
    TFT_DARKGREEN
  );

  drawPhaseDecoration();

  hudDirty = true;
  drawHUD();
}

// ======================================================
// Sapo
// ======================================================

void drawFrog() {
  // Pernas
  display.fillRect(
    frogX,
    frogY + 8,
    6,
    4,
    TFT_DARKGREEN
  );

  display.fillRect(
    frogX + 10,
    frogY + 8,
    6,
    4,
    TFT_DARKGREEN
  );

  // Corpo
  display.fillRoundRect(
    frogX + 3,
    frogY + 4,
    10,
    8,
    3,
    TFT_DARKGREEN
  );

  // Olhos
  display.fillCircle(
    frogX + 5,
    frogY + 3,
    3,
    TFT_GREEN
  );

  display.fillCircle(
    frogX + 11,
    frogY + 3,
    3,
    TFT_GREEN
  );

  display.fillCircle(
    frogX + 5,
    frogY + 3,
    1,
    TFT_BLACK
  );

  display.fillCircle(
    frogX + 11,
    frogY + 3,
    1,
    TFT_BLACK
  );

  // Boca
  display.drawLine(
    frogX + 6,
    frogY + 8,
    frogX + 10,
    frogY + 8,
    TFT_BLACK
  );
}

void drawTongue() {
  int tongueX = frogX + FROG_W / 2;

  int tongueTop = max(
    HUD_H + 1,
    frogY - TONGUE_LENGTH
  );

  display.drawLine(
    tongueX,
    frogY + 5,
    tongueX,
    tongueTop,
    TFT_RED
  );

  display.drawLine(
    tongueX + 1,
    frogY + 5,
    tongueX + 1,
    tongueTop,
    TFT_RED
  );
}

// ======================================================
// Mosca
// ======================================================

void drawFly(const Fly& fly) {
  uint16_t wingColor =
    fly.special ? TFT_MAGENTA : TFT_WHITE;

  uint16_t bodyColor =
    fly.special ? TFT_YELLOW : TFT_BLACK;

  display.fillCircle(
    fly.x - 3,
    fly.y - 2,
    2,
    wingColor
  );

  display.fillCircle(
    fly.x + 3,
    fly.y - 2,
    2,
    wingColor
  );

  display.fillCircle(
    fly.x,
    fly.y,
    fly.special ? 3 : 2,
    bodyColor
  );

  if (fly.special) {
    display.drawPixel(
      fly.x,
      fly.y,
      TFT_RED
    );
  }
}

// ======================================================
// Jacaré com boca animada
// ======================================================

void drawCrocodile(
  const Crocodile& crocodile
) {
  int x = crocodile.x;
  int y = CROC_Y;

  // Corpo
  display.fillRect(
    x + 7,
    y + 3,
    13,
    8,
    TFT_DARKGREEN
  );

  // Cauda
  display.fillTriangle(
    x + 19,
    y + 3,
    x + 24,
    y + 7,
    x + 19,
    y + 10,
    TFT_DARKGREEN
  );

  if (crocodileMouthOpen) {
    // Parte superior da boca
    display.fillRect(
      x,
      y + 3,
      9,
      3,
      TFT_DARKGREEN
    );

    // Parte inferior da boca
    display.fillRect(
      x,
      y + 9,
      9,
      2,
      TFT_DARKGREEN
    );

    // Dentes
    display.drawPixel(
      x + 2,
      y + 6,
      TFT_WHITE
    );

    display.drawPixel(
      x + 6,
      y + 6,
      TFT_WHITE
    );

    display.drawPixel(
      x + 4,
      y + 8,
      TFT_WHITE
    );
  } else {
    // Boca fechada
    display.fillRect(
      x,
      y + 5,
      9,
      5,
      TFT_DARKGREEN
    );

    display.drawLine(
      x,
      y + 8,
      x + 8,
      y + 8,
      TFT_BLACK
    );

    display.drawPixel(
      x + 2,
      y + 8,
      TFT_WHITE
    );

    display.drawPixel(
      x + 6,
      y + 8,
      TFT_WHITE
    );
  }

  // Olho
  display.fillCircle(
    x + 8,
    y + 3,
    2,
    TFT_WHITE
  );

  display.drawPixel(
    x + 8,
    y + 3,
    TFT_BLACK
  );
}

// ======================================================
// Limpeza dos objetos móveis
// ======================================================

void eraseDynamicObjects() {
  if (tongueWasDrawn) {
    int tongueX = frogX + FROG_W / 2;

    restoreBackgroundRect(
      tongueX - 2,
      HUD_H,
      5,
      frogY - HUD_H + 8
    );

    tongueWasDrawn = false;
  }

  restoreBackgroundRect(
    frogX - 1,
    frogY - 1,
    FROG_W + 2,
    FROG_H + 2
  );

  for (int i = 0; i < FLY_COUNT; i++) {
    restoreBackgroundRect(
      flies[i].x - 6,
      flies[i].y - 6,
      12,
      12
    );
  }

  int crocodileCount =
    activeCrocodileCount();

  for (
    int i = 0;
    i < crocodileCount;
    i++
  ) {
    restoreBackgroundRect(
      crocodiles[i].x - 2,
      CROC_Y - 2,
      CROC_W + 5,
      CROC_H + 4
    );
  }
}

// ======================================================
// Surgimento dos objetos
// ======================================================

void respawnFly(
  int index,
  bool initialPosition = false
) {
  int extraDistance =
    initialPosition ? index * 48 : 0;

  flies[index].x =
    SCREEN_W +
    random(25, 120) +
    extraDistance;

  flies[index].y =
    random(34, 79);

  flies[index].speed =
    random(1, 4);

  flies[index].special =
    random(100) < SPECIAL_FLY_CHANCE;
}

void respawnCrocodile(
  int index,
  bool initialPosition = false
) {
  int extraDistance = 0;

  if (
    initialPosition &&
    selectedDifficulty != EASY
  ) {
    extraDistance = index * 100;
  }

  crocodiles[index].x =
    SCREEN_W +
    random(60, 155) +
    extraDistance;

  crocodiles[index].speed =
    random(2, 4);
}

void prepareObjects() {
  for (int i = 0; i < FLY_COUNT; i++) {
    respawnFly(i, true);
  }

  for (
    int i = 0;
    i < MAX_CROCODILES;
    i++
  ) {
    respawnCrocodile(i, true);
  }
}

// ======================================================
// Colisões
// ======================================================

bool rectanglesCollide(
  int ax,
  int ay,
  int aw,
  int ah,
  int bx,
  int by,
  int bw,
  int bh
) {
  return ax < bx + bw &&
         ax + aw > bx &&
         ay < by + bh &&
         ay + ah > by;
}

void playCrocodileHitSound() {
  buzzer.sound(
    NOTE_G3,
    45
  );

  buzzer.end(10);

  buzzer.sound(
    NOTE_C3,
    75
  );

  buzzer.end(0);
}

void playVictoryMelody() {
  const int notes[] = {
    NOTE_C5,
    NOTE_E5,
    NOTE_G5,
    NOTE_C6,
    NOTE_E6
  };

  const int durations[] = {
    80,
    80,
    90,
    110,
    190
  };

  constexpr int NOTE_COUNT =
    sizeof(notes) / sizeof(notes[0]);

  for (int i = 0; i < NOTE_COUNT; i++) {
    buzzer.sound(
      notes[i],
      durations[i]
    );

    buzzer.end(20);
  }
}

// ======================================================
// Controles
// ======================================================

void readControls() {
  if (keyboard.hold(KEY_LEFT)) {
    frogX -= 3;
    frogX = max(1, frogX);
  }

  if (keyboard.hold(KEY_RIGHT)) {
    frogX += 3;

    frogX = min(
      SCREEN_W - FROG_W - 1,
      frogX
    );
  }

  // Botão de cima: pular
  if (
    keyboard.press(KEY_UP) &&
    !jumping
  ) {
    jumping = true;
    jumpVelocity = jumpStartVelocity();
  }

  // Botão central: lançar língua
  if (keyboard.press(KEY_CENTER)) {
    tongueFrames = 5;
  }
}

// ======================================================
// Atualizações do jogo
// ======================================================

void updateJump() {
  if (!jumping) {
    return;
  }

  frogY += jumpVelocity;
  jumpVelocity++;

  if (frogY >= FROG_GROUND_Y) {
    frogY = FROG_GROUND_Y;
    jumpVelocity = 0;
    jumping = false;
  }
}

void updateObjects() {
  for (int i = 0; i < FLY_COUNT; i++) {
    flies[i].x -= flies[i].speed;

    if (flies[i].x < -8) {
      respawnFly(i);
    }
  }

  int crocodileCount =
    activeCrocodileCount();

  for (
    int i = 0;
    i < crocodileCount;
    i++
  ) {
    crocodiles[i].x -=
      crocodiles[i].speed;

    if (
      crocodiles[i].x <
      -CROC_W
    ) {
      respawnCrocodile(i);
    }
  }
}

void updateCrocodileMouth() {
  if (
    crocodileMouthTimer.intervalMillis(
      180
    )
  ) {
    crocodileMouthOpen =
      !crocodileMouthOpen;
  }
}

void checkFlies() {
  if (tongueFrames <= 0) {
    return;
  }

  int tongueX =
    frogX + FROG_W / 2;

  int tongueTop = max(
    HUD_H + 1,
    frogY - TONGUE_LENGTH
  );

  for (int i = 0; i < FLY_COUNT; i++) {
    bool alignedHorizontally =
      flies[i].x >= tongueX - 9 &&
      flies[i].x <= tongueX + 9;

    bool insideTongueHeight =
      flies[i].y >= tongueTop &&
      flies[i].y <= frogY + 5;

    if (
      alignedHorizontally &&
      insideTongueHeight
    ) {
      phaseScore +=
        flies[i].special ? 2 : 1;

      hudDirty = true;
      respawnFly(i);
    }
  }
}

void checkCrocodiles() {
  int crocodileCount =
    activeCrocodileCount();

  for (
    int i = 0;
    i < crocodileCount;
    i++
  ) {
    bool collided = rectanglesCollide(
      frogX,
      frogY,
      FROG_W,
      FROG_H,
      crocodiles[i].x,
      CROC_Y,
      CROC_W,
      CROC_H
    );

    if (!collided) {
      continue;
    }

    phaseScore -=
      crocodilePenalty();

    hudDirty = true;

    playCrocodileHitSound();
    respawnCrocodile(i);

    // Impede dois sons no mesmo quadro
    break;
  }
}

// ======================================================
// Menu de dificuldade
// ======================================================

// Temporizadores exclusivos das animações do menu.
ES_TimeInterval menuFrameTimer;
ES_TimeInterval menuBlinkStartTimer;
ES_TimeInterval menuBlinkDurationTimer;
ES_TimeInterval menuFlyStartTimer;
ES_TimeInterval menuTongueTimer;

constexpr int MENU_FLY_Y = 39;

bool menuEyeClosed = false;
bool menuFlyActive = false;
bool menuTongueActive = false;

int menuFlyX = 164;

unsigned long menuNextBlinkMs = 2200;
unsigned long menuNextFlyMs = 4500;

// ------------------------------------------------------
// Fundo decorativo do menu
// ------------------------------------------------------

void drawMenuBackground() {
  // Céu
  display.fillScreen(TFT_CYAN);

  // Lago
  display.fillRect(
    0,
    70,
    SCREEN_W,
    SCREEN_H - 70,
    TFT_BLUE
  );

  // Margem de grama
  display.fillRect(
    0,
    66,
    SCREEN_W,
    9,
    TFT_GREEN
  );

  display.drawLine(
    0,
    74,
    SCREEN_W - 1,
    74,
    TFT_DARKGREEN
  );

  // Nuvem esquerda
  display.fillCircle(15, 21, 4, TFT_WHITE);
  display.fillCircle(21, 18, 6, TFT_WHITE);
  display.fillCircle(28, 21, 4, TFT_WHITE);

  // Nuvem direita
  display.fillCircle(131, 23, 4, TFT_WHITE);
  display.fillCircle(138, 19, 7, TFT_WHITE);
  display.fillCircle(146, 23, 5, TFT_WHITE);

  // Ondas
  display.drawLine(4, 89, 34, 89, TFT_CYAN);
  display.drawLine(120, 91, 153, 91, TFT_CYAN);
  display.drawLine(7, 108, 31, 108, TFT_CYAN);
  display.drawLine(126, 110, 154, 110, TFT_CYAN);

  // Vitória-régia esquerda
  display.fillCircle(
    18,
    99,
    9,
    TFT_GREEN
  );

  display.fillTriangle(
    18,
    99,
    27,
    94,
    27,
    104,
    TFT_BLUE
  );

  // Vitória-régia direita
  display.fillCircle(
    143,
    100,
    8,
    TFT_DARKGREEN
  );

  display.fillTriangle(
    143,
    100,
    151,
    96,
    151,
    104,
    TFT_BLUE
  );

  // Plantas à esquerda
  display.drawLine(4, 70, 4, 56, TFT_DARKGREEN);
  display.drawLine(8, 70, 10, 53, TFT_DARKGREEN);
  display.drawLine(12, 70, 15, 58, TFT_DARKGREEN);

  // Plantas à direita
  display.drawLine(155, 70, 155, 54, TFT_DARKGREEN);
  display.drawLine(151, 70, 148, 57, TFT_DARKGREEN);
}

// ------------------------------------------------------
// Olhos do sapo do menu
// ------------------------------------------------------

void drawMenuFrogEyes(bool closed) {
  const int x = 58;
  const int y = 31;

  // Refaz as bases verdes dos olhos.
  display.fillCircle(
    x + 13,
    y + 12,
    10,
    TFT_GREEN
  );

  display.fillCircle(
    x + 35,
    y + 12,
    10,
    TFT_GREEN
  );

  if (closed) {
    // Olho esquerdo fechado
    display.drawLine(
      x + 8,
      y + 12,
      x + 18,
      y + 12,
      TFT_BLACK
    );

    display.drawLine(
      x + 9,
      y + 13,
      x + 17,
      y + 13,
      TFT_BLACK
    );

    // Olho direito permanece aberto
    display.fillCircle(
      x + 35,
      y + 11,
      6,
      TFT_WHITE
    );

    display.fillCircle(
      x + 34,
      y + 12,
      2,
      TFT_BLACK
    );

    return;
  }

  // Dois olhos abertos
  display.fillCircle(
    x + 13,
    y + 11,
    6,
    TFT_WHITE
  );

  display.fillCircle(
    x + 35,
    y + 11,
    6,
    TFT_WHITE
  );

  display.fillCircle(
    x + 14,
    y + 12,
    2,
    TFT_BLACK
  );

  display.fillCircle(
    x + 34,
    y + 12,
    2,
    TFT_BLACK
  );
}

// ------------------------------------------------------
// Sapinho simpático do menu
// ------------------------------------------------------

void drawFriendlyMenuFrog() {
  const int x = 58;
  const int y = 31;

  // Sombra sobre a água
  display.fillRoundRect(
    x - 4,
    y + 38,
    52,
    7,
    4,
    TFT_DARKGREEN
  );

  // Pernas
  display.fillRoundRect(
    x - 5,
    y + 29,
    18,
    10,
    4,
    TFT_DARKGREEN
  );

  display.fillRoundRect(
    x + 35,
    y + 29,
    18,
    10,
    4,
    TFT_DARKGREEN
  );

  // Corpo
  display.fillRoundRect(
    x + 5,
    y + 14,
    38,
    29,
    10,
    TFT_GREEN
  );

  // Barriga
  display.fillRoundRect(
    x + 13,
    y + 24,
    22,
    16,
    7,
    TFT_YELLOW
  );

  drawMenuFrogEyes(false);

  // Nariz
  display.drawPixel(
    x + 21,
    y + 22,
    TFT_DARKGREEN
  );

  display.drawPixel(
    x + 27,
    y + 22,
    TFT_DARKGREEN
  );

  // Sorriso
  display.drawLine(
    x + 17,
    y + 28,
    x + 21,
    y + 31,
    TFT_BLACK
  );

  display.drawLine(
    x + 21,
    y + 31,
    x + 27,
    y + 31,
    TFT_BLACK
  );

  display.drawLine(
    x + 27,
    y + 31,
    x + 31,
    y + 27,
    TFT_BLACK
  );

  // Bochechas
  display.fillCircle(
    x + 12,
    y + 26,
    2,
    TFT_YELLOW
  );

  display.fillCircle(
    x + 37,
    y + 26,
    2,
    TFT_YELLOW
  );
}

// ------------------------------------------------------
// Mosquinha da animação inicial
// ------------------------------------------------------

void drawMenuFly(int x, int y) {
  // Asas
  display.fillCircle(
    x - 3,
    y - 2,
    2,
    TFT_WHITE
  );

  display.fillCircle(
    x + 3,
    y - 2,
    2,
    TFT_WHITE
  );

  // Corpo
  display.fillCircle(
    x,
    y,
    2,
    TFT_BLACK
  );

  // Olhinhos
  display.drawPixel(
    x - 1,
    y - 1,
    TFT_RED
  );

  display.drawPixel(
    x + 1,
    y - 1,
    TFT_RED
  );
}

void eraseMenuFly(int x, int y) {
  display.fillRect(
    x - 6,
    y - 5,
    12,
    10,
    TFT_CYAN
  );
}

// ------------------------------------------------------
// Língua da animação inicial
// ------------------------------------------------------

void drawMenuTongue(int targetX, int targetY) {
  constexpr int frogMouthX = 82;
  constexpr int frogMouthY = 60;

  display.drawLine(
    frogMouthX,
    frogMouthY,
    targetX,
    targetY,
    TFT_RED
  );

  display.drawLine(
    frogMouthX + 1,
    frogMouthY,
    targetX + 1,
    targetY,
    TFT_RED
  );
}

void eraseMenuTongue(int targetX, int targetY) {
  constexpr int frogMouthX = 82;
  constexpr int frogMouthY = 60;

  // O caminho da língua está na área azul-clara do céu.
  display.drawLine(
    frogMouthX,
    frogMouthY,
    targetX,
    targetY,
    TFT_CYAN
  );

  display.drawLine(
    frogMouthX + 1,
    frogMouthY,
    targetX + 1,
    targetY,
    TFT_CYAN
  );

  // Refaz o sapo para restaurar a boca e o rosto.
  drawFriendlyMenuFrog();

  // Preserva o estado da piscada caso ela esteja ocorrendo.
  if (menuEyeClosed) {
    drawMenuFrogEyes(true);
  }
}

// ------------------------------------------------------
// Reinicia as animações do menu
// ------------------------------------------------------

void resetMenuAnimations() {
  menuEyeClosed = false;
  menuFlyActive = false;
  menuTongueActive = false;

  menuFlyX = SCREEN_W + 4;

  menuNextBlinkMs = random(1800, 3400);
  menuNextFlyMs = random(3500, 6500);

  menuFrameTimer.resetMillis();
  menuBlinkStartTimer.resetMillis();
  menuBlinkDurationTimer.resetMillis();
  menuFlyStartTimer.resetMillis();
  menuTongueTimer.resetMillis();
}

// ------------------------------------------------------
// Atualiza a piscada do sapo
// ------------------------------------------------------

void updateMenuBlink() {
  if (!menuEyeClosed) {
    if (
      menuBlinkStartTimer.intervalMillis(
        menuNextBlinkMs
      )
    ) {
      menuEyeClosed = true;

      drawMenuFrogEyes(true);
      menuBlinkDurationTimer.resetMillis();
    }

    return;
  }

  if (
    menuBlinkDurationTimer.intervalMillis(150)
  ) {
    menuEyeClosed = false;

    drawMenuFrogEyes(false);

    menuNextBlinkMs = random(1800, 3400);
    menuBlinkStartTimer.resetMillis();
  }
}

// ------------------------------------------------------
// Atualiza a mosquinha e a língua
// ------------------------------------------------------

void updateMenuFlyAnimation() {
  // Aguarda alguns segundos antes de soltar outra mosca.
  if (
    !menuFlyActive &&
    !menuTongueActive
  ) {
    if (
      menuFlyStartTimer.intervalMillis(
        menuNextFlyMs
      )
    ) {
      menuFlyActive = true;
      menuFlyX = SCREEN_W + 4;

      menuFrameTimer.resetMillis();
    }
  }

  // Mosca voando da direita em direção ao sapo.
  if (
    menuFlyActive &&
    menuFrameTimer.intervalMillis(70)
  ) {
    eraseMenuFly(
      menuFlyX,
      MENU_FLY_Y
    );

    menuFlyX -= 4;

    // Quando chega perto do sapo, ele lança a língua.
    if (menuFlyX <= 106) {
      menuFlyActive = false;
      menuTongueActive = true;

      drawMenuFly(
        menuFlyX,
        MENU_FLY_Y
      );

      drawMenuTongue(
        menuFlyX,
        MENU_FLY_Y
      );

      menuTongueTimer.resetMillis();

      return;
    }

    drawMenuFly(
      menuFlyX,
      MENU_FLY_Y
    );
  }

  // Mantém a língua visível por um pequeno período.
  if (
    menuTongueActive &&
    menuTongueTimer.intervalMillis(230)
  ) {
    eraseMenuFly(
      menuFlyX,
      MENU_FLY_Y
    );

    eraseMenuTongue(
      menuFlyX,
      MENU_FLY_Y
    );

    menuTongueActive = false;

    menuNextFlyMs = random(3500, 6500);
    menuFlyStartTimer.resetMillis();
  }
}

void updateMenuAnimations() {
  updateMenuBlink();
  updateMenuFlyAnimation();
}

// ------------------------------------------------------
// Cartão de cada dificuldade
// ------------------------------------------------------

void drawDifficultyOption(
  int x,
  int y,
  int width,
  const char* label,
  Difficulty option
) {
  bool selected =
    selectedDifficulty == option;

  uint16_t borderColor =
    selected ? TFT_YELLOW : TFT_WHITE;

  uint16_t backgroundColor =
    selected ? TFT_DARKGREEN : TFT_BLUE;

  uint16_t textColor =
    selected ? TFT_YELLOW : TFT_WHITE;

  // Sombra
  display.fillRoundRect(
    x + 2,
    y + 2,
    width,
    20,
    4,
    TFT_BLACK
  );

  // Fundo
  display.fillRoundRect(
    x,
    y,
    width,
    20,
    4,
    backgroundColor
  );

  // Contorno
  display.drawRoundRect(
    x,
    y,
    width,
    20,
    4,
    borderColor
  );

  // Indicador da opção selecionada
  if (selected) {
    display.fillTriangle(
      x + width / 2 - 4,
      y - 6,
      x + width / 2 + 4,
      y - 6,
      x + width / 2,
      y - 1,
      TFT_YELLOW
    );
  }

  display.setTextSize(1);

  display.setTextColor(
    textColor,
    backgroundColor
  );

  int textWidth =
    strlen(label) * 6;

  display.setCursor(
    x + (width - textWidth) / 2,
    y + 7
  );

  display.print(label);
}

// ------------------------------------------------------
// Desenha a tela principal
// ------------------------------------------------------

void showDifficultyMenu() {
  gameState = DIFFICULTY_MENU;

  drawMenuBackground();

  // Painel do título
  display.fillRoundRect(
    41,
    2,
    78,
    27,
    6,
    TFT_DARKGREEN
  );

  display.drawRoundRect(
    41,
    2,
    78,
    27,
    6,
    TFT_YELLOW
  );

  display.setTextColor(
    TFT_YELLOW,
    TFT_DARKGREEN
  );

  display.drawCentreScreenString(
    "FROG",
    3,
    2
  );

  display.setTextColor(
    TFT_WHITE,
    TFT_DARKGREEN
  );

  display.drawCentreScreenString(
    "ADVENTURE",
    19,
    1
  );

  // Sapinho central
  drawFriendlyMenuFrog();

  // Opções de dificuldade
  drawDifficultyOption(
    3,
    81,
    48,
    "FACIL",
    EASY
  );

  drawDifficultyOption(
    56,
    81,
    48,
    "MEDIO",
    MEDIUM
  );

  drawDifficultyOption(
    109,
    81,
    48,
    "DIFICIL",
    HARD
  );

  // Instrução
  display.fillRoundRect(
    2,
    104,
    156,
    13,
    4,
    TFT_BLACK
  );

  display.setTextColor(
    TFT_WHITE,
    TFT_BLACK
  );

  display.drawCentreScreenString(
    "< > ESCOLHE  CENTRO INICIA",
    107,
    1
  );

  // Assinatura
  display.fillRect(
    0,
    119,
    SCREEN_W,
    9,
    TFT_BLACK
  );

  display.setTextColor(
    TFT_DARKGREY,
    TFT_BLACK
  );

  display.drawCentreScreenString(
    "By Roberta Santini",
    120,
    1
  );

  resetMenuAnimations();
}

// ------------------------------------------------------
// Abre o menu sempre no nível fácil
// ------------------------------------------------------

void openDifficultyMenu() {
  selectedDifficulty = EASY;
  showDifficultyMenu();
}

// ------------------------------------------------------
// Controle do menu
// ------------------------------------------------------

void updateDifficultyMenu() {
  bool changed = false;

  if (keyboard.press(KEY_LEFT)) {
    int value =
      static_cast<int>(
        selectedDifficulty
      );

    value--;

    if (value < 0) {
      value = 2;
    }

    selectedDifficulty =
      static_cast<Difficulty>(value);

    changed = true;
  }

  if (keyboard.press(KEY_RIGHT)) {
    int value =
      static_cast<int>(
        selectedDifficulty
      );

    value++;

    if (value > 2) {
      value = 0;
    }

    selectedDifficulty =
      static_cast<Difficulty>(value);

    changed = true;
  }

  if (changed) {
    showDifficultyMenu();
    return;
  }

  updateMenuAnimations();
}

// ======================================================
// Telas intermediárias
// ======================================================

// Estado da animação da passagem de fase.
bool phaseClearedFrogWinking = false;
bool phaseClearedSparklesBright = true;

int phaseClearedBlinkTicks = 0;
int phaseClearedNextBlinkTicks = 7;

// ------------------------------------------------------
// Informações da próxima fase
// ------------------------------------------------------

const char* nextPhaseName() {
  if (currentPhase == 1) {
    return "TARDE";
  }

  return "NOITE";
}

uint16_t nextPhaseSkyColor() {
  if (currentPhase == 1) {
    return TFT_ORANGE;
  }

  return TFT_NAVY;
}

// ------------------------------------------------------
// Fundo da passagem de fase
// ------------------------------------------------------

void drawPhaseClearedBackground() {
  uint16_t backgroundColor =
    nextPhaseSkyColor();

  display.fillScreen(backgroundColor);

  // Lago
  display.fillRect(
    0,
    73,
    SCREEN_W,
    SCREEN_H - 73,
    TFT_BLUE
  );

  // Margem de grama
  display.fillRect(
    0,
    68,
    SCREEN_W,
    9,
    TFT_GREEN
  );

  display.drawLine(
    0,
    76,
    SCREEN_W - 1,
    76,
    TFT_DARKGREEN
  );

  // Prévia visual da próxima fase.
  if (currentPhase == 1) {
    // Sol da tarde
    display.fillCircle(
      146,
      30,
      8,
      TFT_YELLOW
    );

    display.drawLine(
      132,
      30,
      126,
      30,
      TFT_YELLOW
    );

    display.drawLine(
      146,
      16,
      146,
      11,
      TFT_YELLOW
    );
  } else {
    // Lua da noite
    display.fillCircle(
      146,
      29,
      8,
      TFT_WHITE
    );

    display.fillCircle(
      150,
      26,
      8,
      TFT_NAVY
    );

    display.drawPixel(126, 31, TFT_WHITE);
    display.drawPixel(137, 17, TFT_WHITE);
    display.drawPixel(155, 43, TFT_WHITE);
  }

  // Ondas discretas
  display.drawLine(4, 88, 32, 88, TFT_CYAN);
  display.drawLine(126, 91, 154, 91, TFT_CYAN);
  display.drawLine(7, 106, 30, 106, TFT_CYAN);
  display.drawLine(130, 108, 154, 108, TFT_CYAN);
}

// ------------------------------------------------------
// Olhos do sapinho da passagem de fase
// ------------------------------------------------------

void drawPhaseClearedFrogEyes(bool winking) {
  const int x = 12;
  const int y = 31;

  // Bases verdes
  display.fillCircle(
    x + 13,
    y + 12,
    9,
    TFT_GREEN
  );

  display.fillCircle(
    x + 32,
    y + 12,
    9,
    TFT_GREEN
  );

  if (winking) {
    // Olho esquerdo piscando
    display.drawLine(
      x + 8,
      y + 12,
      x + 18,
      y + 12,
      TFT_BLACK
    );

    display.drawLine(
      x + 9,
      y + 13,
      x + 17,
      y + 13,
      TFT_BLACK
    );
  } else {
    display.fillCircle(
      x + 13,
      y + 11,
      5,
      TFT_WHITE
    );

    display.fillCircle(
      x + 14,
      y + 12,
      2,
      TFT_BLACK
    );
  }

  // Olho direito aberto
  display.fillCircle(
    x + 32,
    y + 11,
    5,
    TFT_WHITE
  );

  display.fillCircle(
    x + 31,
    y + 12,
    2,
    TFT_BLACK
  );
}

// ------------------------------------------------------
// Sapinho feliz da passagem de fase
// ------------------------------------------------------

void drawPhaseClearedFrog() {
  const int x = 12;
  const int y = 31;

  // Vitória-régia
  display.fillCircle(
    x + 24,
    y + 43,
    22,
    TFT_DARKGREEN
  );

  display.fillTriangle(
    x + 24,
    y + 43,
    x + 46,
    y + 35,
    x + 46,
    y + 51,
    TFT_BLUE
  );

  // Pernas
  display.fillRoundRect(
    x - 1,
    y + 31,
    17,
    9,
    4,
    TFT_DARKGREEN
  );

  display.fillRoundRect(
    x + 30,
    y + 31,
    17,
    9,
    4,
    TFT_DARKGREEN
  );

  // Braço levantado acenando
  display.fillRoundRect(
    x + 37,
    y + 17,
    7,
    17,
    3,
    TFT_GREEN
  );

  display.fillCircle(
    x + 42,
    y + 15,
    4,
    TFT_GREEN
  );

  // Corpo
  display.fillRoundRect(
    x + 5,
    y + 15,
    35,
    29,
    10,
    TFT_GREEN
  );

  // Barriga
  display.fillRoundRect(
    x + 12,
    y + 25,
    21,
    16,
    7,
    TFT_YELLOW
  );

  drawPhaseClearedFrogEyes(false);

  // Nariz
  display.drawPixel(
    x + 20,
    y + 23,
    TFT_DARKGREEN
  );

  display.drawPixel(
    x + 26,
    y + 23,
    TFT_DARKGREEN
  );

  // Sorriso
  display.drawLine(
    x + 16,
    y + 29,
    x + 20,
    y + 33,
    TFT_BLACK
  );

  display.drawLine(
    x + 20,
    y + 33,
    x + 27,
    y + 33,
    TFT_BLACK
  );

  display.drawLine(
    x + 27,
    y + 33,
    x + 31,
    y + 29,
    TFT_BLACK
  );

  // Bochechas
  display.fillCircle(
    x + 11,
    y + 27,
    2,
    TFT_YELLOW
  );

  display.fillCircle(
    x + 35,
    y + 27,
    2,
    TFT_YELLOW
  );
}

// ------------------------------------------------------
// Placa indicando a próxima fase
// ------------------------------------------------------

void drawNextPhaseSign(bool brightArrow) {
  // Poste
  display.fillRect(
    109,
    66,
    5,
    15,
    TFT_DARKGREY
  );

  // Placa
  display.fillRoundRect(
    68,
    30,
    87,
    42,
    6,
    TFT_BLACK
  );

  display.drawRoundRect(
    68,
    30,
    87,
    42,
    6,
    TFT_WHITE
  );

  display.setTextColor(
    TFT_WHITE,
    TFT_BLACK
  );

  display.setTextSize(1);
  display.setCursor(91, 36);
  display.print("PROXIMA");

  display.setTextColor(
    TFT_YELLOW,
    TFT_BLACK
  );

  display.setTextSize(2);
  display.setCursor(82, 49);
  display.print(nextPhaseName());

  // Seta pequena e pulsante
  uint16_t arrowColor =
    brightArrow ? TFT_YELLOW : TFT_WHITE;

  display.drawLine(
    61,
    49,
    67,
    49,
    arrowColor
  );

  display.fillTriangle(
    67,
    45,
    67,
    53,
    72,
    49,
    arrowColor
  );
}

// ------------------------------------------------------
// Indicador de progresso pelas três fases
// ------------------------------------------------------

void drawPhaseProgress() {
  const int padY = 115;
  const int padX[3] = {
    34,
    80,
    126
  };

  // Caminho entre as vitórias-régias
  display.drawLine(
    padX[0] + 10,
    padY,
    padX[1] - 10,
    padY,
    TFT_WHITE
  );

  display.drawLine(
    padX[1] + 10,
    padY,
    padX[2] - 10,
    padY,
    TFT_WHITE
  );

  for (int i = 0; i < 3; i++) {
    int phaseNumber = i + 1;

    uint16_t padColor =
      TFT_DARKGREEN;

    uint16_t numberColor =
      TFT_WHITE;

    if (phaseNumber <= currentPhase) {
      padColor = TFT_GREEN;
      numberColor = TFT_BLACK;
    } else if (
      phaseNumber == currentPhase + 1
    ) {
      padColor = TFT_YELLOW;
      numberColor = TFT_BLACK;
    }

    display.fillCircle(
      padX[i],
      padY,
      10,
      padColor
    );

    display.fillTriangle(
      padX[i],
      padY,
      padX[i] + 10,
      padY - 5,
      padX[i] + 10,
      padY + 5,
      TFT_BLUE
    );

    display.setTextSize(1);
    display.setTextColor(
      numberColor,
      padColor
    );

    display.setCursor(
      padX[i] - 3,
      padY - 3
    );

    display.print(phaseNumber);
  }
}

// ------------------------------------------------------
// Pequenos brilhos discretos
// ------------------------------------------------------

void drawPhaseClearedSparkles(bool bright) {
  uint16_t sparkleColor =
    bright
      ? TFT_YELLOW
      : nextPhaseSkyColor();

  // Brilho próximo ao sapinho
  display.drawLine(
    61,
    27,
    67,
    27,
    sparkleColor
  );

  display.drawLine(
    64,
    24,
    64,
    30,
    sparkleColor
  );

  // Brilho próximo à placa
  display.drawLine(
    151,
    78,
    157,
    78,
    sparkleColor
  );

  display.drawLine(
    154,
    75,
    154,
    81,
    sparkleColor
  );
}

// ------------------------------------------------------
// Tela completa de passagem de fase
// ------------------------------------------------------

void showPhaseClearedScreen() {
  drawPhaseClearedBackground();

  // Título
  display.fillRoundRect(
    8,
    2,
    144,
    22,
    7,
    TFT_DARKGREEN
  );

  display.drawRoundRect(
    8,
    2,
    144,
    22,
    7,
    TFT_WHITE
  );

  String title = "FASE ";
  title += currentPhase;
  title += " CONCLUIDA!";

  display.setTextColor(
    TFT_YELLOW,
    TFT_DARKGREEN
  );

  display.drawCentreScreenString(
    title,
    8,
    1
  );

  drawPhaseClearedFrog();
  drawNextPhaseSign(true);

  // Painel de pontuação
  display.fillRoundRect(
    5,
    82,
    150,
    20,
    6,
    TFT_BLACK
  );

  display.drawRoundRect(
    5,
    82,
    150,
    20,
    6,
    TFT_WHITE
  );

  String scoreLine = "Fase: +";
  scoreLine += phaseScore;
  scoreLine += "   Total: ";
  scoreLine += totalScore;

  display.setTextColor(
    TFT_WHITE,
    TFT_BLACK
  );

  display.drawCentreScreenString(
    scoreLine,
    88,
    1
  );

  drawPhaseProgress();
  drawPhaseClearedSparkles(true);

  phaseClearedFrogWinking = false;
  phaseClearedSparklesBright = true;

  phaseClearedBlinkTicks = 0;
  phaseClearedNextBlinkTicks =
    random(6, 10);

  transitionTimer.resetMillis();
  phaseClearedAnimTimer.resetMillis();
}

// ------------------------------------------------------
// Animação discreta da passagem de fase
// ------------------------------------------------------

void updatePhaseClearedAnimation() {
  if (
    !phaseClearedAnimTimer.intervalMillis(
      160
    )
  ) {
    return;
  }

  phaseClearedSparklesBright =
    !phaseClearedSparklesBright;

  drawPhaseClearedSparkles(
    phaseClearedSparklesBright
  );

  drawNextPhaseSign(
    phaseClearedSparklesBright
  );

  phaseClearedBlinkTicks++;

  if (!phaseClearedFrogWinking) {
    if (
      phaseClearedBlinkTicks >=
      phaseClearedNextBlinkTicks
    ) {
      phaseClearedFrogWinking = true;
      phaseClearedBlinkTicks = 0;

      drawPhaseClearedFrogEyes(true);
    }

    return;
  }

  // Aproximadamente 160 ms com o olho fechado.
  if (phaseClearedBlinkTicks >= 1) {
    phaseClearedFrogWinking = false;
    phaseClearedBlinkTicks = 0;

    drawPhaseClearedFrogEyes(false);

    phaseClearedNextBlinkTicks =
      random(6, 10);
  }
}

// ======================================================
// Fundo da tela de Game Over
// ======================================================

void drawGameOverBackground() {
  // Céu mais escuro, mantendo o estilo da abertura.
  display.fillScreen(TFT_NAVY);

  // Nuvens discretas
  display.fillCircle(
    15,
    27,
    4,
    TFT_DARKGREY
  );

  display.fillCircle(
    21,
    24,
    6,
    TFT_DARKGREY
  );

  display.fillCircle(
    28,
    27,
    4,
    TFT_DARKGREY
  );

  display.fillCircle(
    133,
    30,
    4,
    TFT_DARKGREY
  );

  display.fillCircle(
    140,
    27,
    6,
    TFT_DARKGREY
  );

  display.fillCircle(
    147,
    30,
    4,
    TFT_DARKGREY
  );

  // Lago
  display.fillRect(
    0,
    72,
    SCREEN_W,
    SCREEN_H - 72,
    TFT_BLUE
  );

  // Margem de grama
  display.fillRect(
    0,
    68,
    SCREEN_W,
    8,
    TFT_DARKGREEN
  );

  display.drawLine(
    0,
    75,
    SCREEN_W - 1,
    75,
    TFT_GREEN
  );

  // Ondas
  display.drawLine(
    4,
    88,
    34,
    88,
    TFT_CYAN
  );

  display.drawLine(
    125,
    91,
    154,
    91,
    TFT_CYAN
  );

  display.drawLine(
    8,
    105,
    31,
    105,
    TFT_CYAN
  );

  display.drawLine(
    127,
    108,
    153,
    108,
    TFT_CYAN
  );

  // Vitória-régia esquerda
  display.fillCircle(
    18,
    97,
    9,
    TFT_DARKGREEN
  );

  display.fillTriangle(
    18,
    97,
    27,
    92,
    27,
    102,
    TFT_BLUE
  );

  // Vitória-régia direita
  display.fillCircle(
    143,
    99,
    8,
    TFT_DARKGREEN
  );

  display.fillTriangle(
    143,
    99,
    151,
    95,
    151,
    103,
    TFT_BLUE
  );
}

// ======================================================
// Olho saudável do sapo na tela de Game Over
// ======================================================

void drawGameOverHealthyEye(bool closed) {
  const int x = 58;
  const int y = 29;

  // Refaz a base verde do olho direito.
  display.fillCircle(
    x + 35,
    y + 12,
    10,
    TFT_GREEN
  );

  if (closed) {
    // Olho piscando.
    display.drawLine(
      x + 30,
      y + 12,
      x + 40,
      y + 12,
      TFT_BLACK
    );

    display.drawLine(
      x + 31,
      y + 13,
      x + 39,
      y + 13,
      TFT_BLACK
    );
  } else {
    // Olho aberto.
    display.fillCircle(
      x + 35,
      y + 12,
      6,
      TFT_WHITE
    );

    display.fillCircle(
      x + 34,
      y + 14,
      2,
      TFT_BLACK
    );
  }

  // Redesenha a lágrima, pois ela fica parcialmente
  // dentro da área do olho que foi restaurada.
  display.drawPixel(
    x + 39,
    y + 18,
    TFT_CYAN
  );

  display.drawLine(
    x + 40,
    y + 19,
    x + 40,
    y + 21,
    TFT_CYAN
  );

  display.drawPixel(
    x + 39,
    y + 22,
    TFT_CYAN
  );
}

// ======================================================
// Controle da piscada no Game Over
// ======================================================

void resetGameOverBlink() {
  gameOverHealthyEyeClosed = false;

  gameOverNextBlinkMs =
    random(1800, 3400);

  gameOverBlinkStartTimer.resetMillis();
  gameOverBlinkDurationTimer.resetMillis();

  drawGameOverHealthyEye(false);
}


void updateGameOverBlink() {
  if (!gameOverHealthyEyeClosed) {
    if (
      gameOverBlinkStartTimer.intervalMillis(
        gameOverNextBlinkMs
      )
    ) {
      gameOverHealthyEyeClosed = true;

      drawGameOverHealthyEye(true);

      gameOverBlinkDurationTimer.resetMillis();
    }

    return;
  }

  // Mantém o olho fechado por 150 ms.
  if (
    gameOverBlinkDurationTimer.intervalMillis(150)
  ) {
    gameOverHealthyEyeClosed = false;

    drawGameOverHealthyEye(false);

    gameOverNextBlinkMs =
      random(1800, 3400);

    gameOverBlinkStartTimer.resetMillis();
  }
}

// ======================================================
// Sapinho derrotado, mas com aparência infantil
// ======================================================

void drawDefeatedGameOverFrog() {
  const int x = 58;
  const int y = 29;

  // Sombra sobre a água
  display.fillRoundRect(
    x - 5,
    y + 41,
    54,
    7,
    4,
    TFT_DARKGREEN
  );

  // Pernas caídas
  display.fillRoundRect(
    x - 6,
    y + 31,
    19,
    9,
    4,
    TFT_DARKGREEN
  );

  display.fillRoundRect(
    x + 35,
    y + 31,
    19,
    9,
    4,
    TFT_DARKGREEN
  );

  // Corpo
  display.fillRoundRect(
    x + 5,
    y + 15,
    38,
    29,
    10,
    TFT_GREEN
  );

  // Barriga
  display.fillRoundRect(
    x + 13,
    y + 25,
    22,
    16,
    7,
    TFT_YELLOW
  );

  // Bases verdes dos olhos
  display.fillCircle(
    x + 13,
    y + 12,
    10,
    TFT_GREEN
  );

  display.fillCircle(
    x + 35,
    y + 12,
    10,
    TFT_GREEN
  );

  // Olho esquerdo semicerrado
  display.drawLine(
    x + 8,
    y + 12,
    x + 18,
    y + 12,
    TFT_BLACK
  );

  display.drawLine(
    x + 9,
    y + 13,
    x + 17,
    y + 13,
    TFT_BLACK
  );

  // Olho direito saudável, aberto inicialmente.
  drawGameOverHealthyEye(false);

  // Nariz
  display.drawPixel(
    x + 21,
    y + 23,
    TFT_DARKGREEN
  );

  display.drawPixel(
    x + 27,
    y + 23,
    TFT_DARKGREEN
  );

  // Boca triste
  display.drawLine(
    x + 17,
    y + 33,
    x + 21,
    y + 29,
    TFT_BLACK
  );

  display.drawLine(
    x + 21,
    y + 29,
    x + 27,
    y + 29,
    TFT_BLACK
  );

  display.drawLine(
    x + 27,
    y + 29,
    x + 31,
    y + 33,
    TFT_BLACK
  );

  // Bochechas discretas
  display.fillCircle(
    x + 12,
    y + 27,
    2,
    TFT_YELLOW
  );

  display.fillCircle(
    x + 37,
    y + 27,
    2,
    TFT_YELLOW
  );

  // Curativo infantil na testa
  display.fillRoundRect(
    x + 18,
    y + 7,
    13,
    6,
    2,
    TFT_WHITE
  );

  display.drawRoundRect(
    x + 18,
    y + 7,
    13,
    6,
    2,
    TFT_DARKGREY
  );

  // Detalhe central do curativo
  display.fillRect(
    x + 23,
    y + 8,
    3,
    4,
    TFT_YELLOW
  );

  // Pequenas estrelinhas de tontura
  display.drawLine(
    x - 8,
    y + 8,
    x - 2,
    y + 8,
    TFT_YELLOW
  );

  display.drawLine(
    x - 5,
    y + 5,
    x - 5,
    y + 11,
    TFT_YELLOW
  );

  display.drawLine(
    x + 48,
    y + 5,
    x + 54,
    y + 5,
    TFT_YELLOW
  );

  display.drawLine(
    x + 51,
    y + 2,
    x + 51,
    y + 8,
    TFT_YELLOW
  );
}


// ======================================================
// Nova tela de Game Over
// ======================================================

void showGameOverScreen() {
  drawGameOverBackground();

  // Painel do título
  display.fillRoundRect(
    31,
    2,
    98,
    23,
    7,
    TFT_BLACK
  );

  display.drawRoundRect(
    31,
    2,
    98,
    23,
    7,
    TFT_RED
  );

  display.setTextColor(
    TFT_RED,
    TFT_BLACK
  );

  display.drawCentreScreenString(
    "GAME OVER",
    6,
    2
  );

  // Sapinho derrotado
  drawDefeatedGameOverFrog();

  /*
  // Painel das informações
  display.fillRoundRect(
    31,
    78,
    98,
    31,
    6,
    TFT_BLACK
  );

  display.drawRoundRect(
    31,
    78,
    98,
    31,
    6,
    TFT_WHITE
  );

  String gameOverLine1 = "Fase: ";
  gameOverLine1 += currentPhase;
  gameOverLine1 += "  Nivel: ";
  gameOverLine1 += difficultyName();

  String gameOverLine2 = "Pontos: ";
  gameOverLine2 += phaseScore;
  gameOverLine2 += "  Total: ";
  gameOverLine2 += totalScore;

  display.setTextColor(
    TFT_WHITE,
    TFT_BLACK
  );

  display.drawCentreScreenString(
    gameOverLine1,
    83,
    1
  );

  display.drawCentreScreenString(
    gameOverLine2,
    95,
    1
  );
*/

// Painel das informações
display.fillRoundRect(
  4,
  77,
  152,
  32,
  6,
  TFT_BLACK
);

display.drawRoundRect(
  4,
  77,
  152,
  32,
  6,
  TFT_WHITE
);

String gameOverLine1 = "Fase: ";
gameOverLine1 += currentPhase;
gameOverLine1 += "  Nivel: ";
gameOverLine1 += difficultyName();

String gameOverLine2 = "Pontos: ";
gameOverLine2 += phaseScore;
gameOverLine2 += "  Total: ";
gameOverLine2 += totalScore;

display.setTextColor(
  TFT_WHITE,
  TFT_BLACK
);

display.drawCentreScreenString(
  gameOverLine1,
  82,
  1
);

display.drawCentreScreenString(
  gameOverLine2,
  95,
  1
);

  // Mensagem positiva para o público infantil
  display.fillRoundRect(
    37,
    111,
    86,
    15,
    5,
    TFT_DARKGREEN
  );

  display.setTextColor(
    TFT_YELLOW,
    TFT_DARKGREEN
  );

  display.drawCentreScreenString(
    "CENTRO: TENTAR DE NOVO",
    115,
    1
  );
  resetGameOverBlink();
}

// ======================================================
// Tela de vitória
// ======================================================

// Estado das animações da tela de vitória.
bool victoryFrogWinking = false;
bool victorySparklesBright = true;

int victoryBlinkTicks = 0;
int victoryNextBlinkTicks = 20;

// ------------------------------------------------------
// Fundo comemorativo
// ------------------------------------------------------

void drawVictoryBackground() {
  // Céu claro
  display.fillScreen(TFT_CYAN);

  // Lago
  display.fillRect(
    0,
    72,
    SCREEN_W,
    SCREEN_H - 72,
    TFT_BLUE
  );

  // Margem de grama
  display.fillRect(
    0,
    68,
    SCREEN_W,
    9,
    TFT_GREEN
  );

  display.drawLine(
    0,
    76,
    SCREEN_W - 1,
    76,
    TFT_DARKGREEN
  );

  // Ondas
  display.drawLine(5, 89, 34, 89, TFT_WHITE);
  display.drawLine(125, 91, 154, 91, TFT_WHITE);
  display.drawLine(8, 105, 34, 105, TFT_CYAN);
  display.drawLine(126, 107, 153, 107, TFT_CYAN);

  // Vitórias-régias
  display.fillCircle(
    18,
    98,
    9,
    TFT_GREEN
  );

  display.fillTriangle(
    18,
    98,
    27,
    93,
    27,
    103,
    TFT_BLUE
  );

  display.fillCircle(
    143,
    99,
    8,
    TFT_DARKGREEN
  );

  display.fillTriangle(
    143,
    99,
    151,
    95,
    151,
    103,
    TFT_BLUE
  );
}

// ------------------------------------------------------
// Olhos do sapo vencedor
// ------------------------------------------------------

void drawVictoryFrogEyes(bool winking) {
  const int x = 58;
  const int y = 30;

  // Bases verdes
  display.fillCircle(
    x + 13,
    y + 13,
    10,
    TFT_GREEN
  );

  display.fillCircle(
    x + 35,
    y + 13,
    10,
    TFT_GREEN
  );

  if (winking) {
    // Olho esquerdo piscando
    display.drawLine(
      x + 8,
      y + 13,
      x + 18,
      y + 13,
      TFT_BLACK
    );

    display.drawLine(
      x + 9,
      y + 14,
      x + 17,
      y + 14,
      TFT_BLACK
    );
  } else {
    // Olho esquerdo aberto
    display.fillCircle(
      x + 13,
      y + 12,
      6,
      TFT_WHITE
    );

    display.fillCircle(
      x + 14,
      y + 13,
      2,
      TFT_BLACK
    );
  }

  // Olho direito sempre aberto
  display.fillCircle(
    x + 35,
    y + 12,
    6,
    TFT_WHITE
  );

  display.fillCircle(
    x + 34,
    y + 13,
    2,
    TFT_BLACK
  );
}

// ------------------------------------------------------
// Sapinho vencedor
// ------------------------------------------------------

void drawVictoryFrog() {
  const int x = 58;
  const int y = 30;

  // Sombra sobre a água
  display.fillRoundRect(
    x - 6,
    y + 42,
    56,
    7,
    4,
    TFT_DARKGREEN
  );

  // Pernas
  display.fillRoundRect(
    x - 6,
    y + 32,
    19,
    10,
    4,
    TFT_DARKGREEN
  );

  display.fillRoundRect(
    x + 35,
    y + 32,
    19,
    10,
    4,
    TFT_DARKGREEN
  );

  // Braços levantados
  display.fillRoundRect(
    x + 1,
    y + 18,
    7,
    18,
    3,
    TFT_GREEN
  );

  display.fillRoundRect(
    x + 40,
    y + 18,
    7,
    18,
    3,
    TFT_GREEN
  );

  // Corpo
  display.fillRoundRect(
    x + 5,
    y + 15,
    38,
    30,
    10,
    TFT_GREEN
  );

  // Barriga
  display.fillRoundRect(
    x + 13,
    y + 25,
    22,
    17,
    7,
    TFT_YELLOW
  );

  drawVictoryFrogEyes(false);

  // Nariz
  display.drawPixel(
    x + 21,
    y + 23,
    TFT_DARKGREEN
  );

  display.drawPixel(
    x + 27,
    y + 23,
    TFT_DARKGREEN
  );

  // Sorriso aberto
  display.drawLine(
    x + 17,
    y + 29,
    x + 21,
    y + 33,
    TFT_BLACK
  );

  display.drawLine(
    x + 21,
    y + 33,
    x + 27,
    y + 33,
    TFT_BLACK
  );

  display.drawLine(
    x + 27,
    y + 33,
    x + 31,
    y + 29,
    TFT_BLACK
  );

  display.fillRect(
    x + 22,
    y + 33,
    5,
    2,
    TFT_RED
  );

  // Bochechas
  display.fillCircle(
    x + 12,
    y + 27,
    2,
    TFT_YELLOW
  );

  display.fillCircle(
    x + 37,
    y + 27,
    2,
    TFT_YELLOW
  );

  // Coroa
  display.fillRect(
    x + 16,
    y + 4,
    17,
    6,
    TFT_YELLOW
  );

  display.fillTriangle(
    x + 16,
    y + 4,
    x + 20,
    y - 2,
    x + 23,
    y + 4,
    TFT_YELLOW
  );

  display.fillTriangle(
    x + 21,
    y + 4,
    x + 25,
    y - 4,
    x + 28,
    y + 4,
    TFT_YELLOW
  );

  display.fillTriangle(
    x + 27,
    y + 4,
    x + 31,
    y - 2,
    x + 33,
    y + 4,
    TFT_YELLOW
  );

  display.drawLine(
    x + 16,
    y + 9,
    x + 32,
    y + 9,
    TFT_ORANGE
  );

  // Medalha na barriga
  display.fillCircle(
    x + 24,
    y + 33,
    4,
    TFT_ORANGE
  );

  display.drawPixel(
    x + 24,
    y + 33,
    TFT_WHITE
  );
}

// ------------------------------------------------------
// Estrelinhas ao redor do sapo
// ------------------------------------------------------

void drawVictorySparkles(bool bright) {
  uint16_t color =
    bright ? TFT_YELLOW : TFT_CYAN;

  // Esquerda
  display.drawLine(45, 39, 51, 39, color);
  display.drawLine(48, 36, 48, 42, color);

  display.drawLine(39, 57, 45, 57, color);
  display.drawLine(42, 54, 42, 60, color);

  // Direita
  display.drawLine(109, 40, 115, 40, color);
  display.drawLine(112, 37, 112, 43, color);

  display.drawLine(116, 58, 122, 58, color);
  display.drawLine(119, 55, 119, 61, color);
}

// ------------------------------------------------------
// Painéis e textos da vitória
// ------------------------------------------------------

void drawVictoryLabels() {
  // Painel do título
  display.fillRoundRect(
    15,
    2,
    130,
    24,
    7,
    TFT_DARKGREEN
  );

  display.drawRoundRect(
    15,
    2,
    130,
    24,
    7,
    TFT_YELLOW
  );

  display.setTextColor(
    TFT_YELLOW,
    TFT_DARKGREEN
  );

  display.drawCentreScreenString(
    "VOCE VENCEU!",
    6,
    2
  );

  // Painel das informações
  display.fillRoundRect(
    4,
    79,
    152,
    30,
    6,
    TFT_BLACK
  );

  display.drawRoundRect(
    4,
    79,
    152,
    30,
    6,
    TFT_WHITE
  );

  String victoryLine1 = "Nivel: ";
  victoryLine1 += difficultyName();

  String victoryLine2 = "Pontuacao total: ";
  victoryLine2 += totalScore;

  display.setTextColor(
    TFT_WHITE,
    TFT_BLACK
  );

  display.drawCentreScreenString(
    victoryLine1,
    84,
    1
  );

  display.drawCentreScreenString(
    victoryLine2,
    97,
    1
  );

  // Botão de retorno
  display.fillRoundRect(
    2,
    111,
    156,
    15,
    5,
    TFT_DARKGREEN
  );

  display.drawRoundRect(
    2,
    111,
    156,
    15,
    5,
    TFT_YELLOW
  );

  display.setTextColor(
    TFT_YELLOW,
    TFT_DARKGREEN
  );

  display.drawCentreScreenString(
    "CENTRO: VOLTAR AO MENU",
    115,
    1
  );
}

// ------------------------------------------------------
// Confetes laterais
// ------------------------------------------------------

void generateConfetti() {
  const uint16_t colors[] = {
    TFT_RED,
    TFT_YELLOW,
    TFT_MAGENTA,
    TFT_GREEN,
    TFT_WHITE,
    TFT_ORANGE
  };

  for (int i = 0; i < CONFETTI_COUNT; i++) {
    bool leftSide =
      random(2) == 0;

    if (leftSide) {
      confetti[i].x =
        random(3, 40);
    } else {
      confetti[i].x =
        random(120, 157);
    }

    // Os confetes ficam somente na área do céu.
    // Assim, nunca passam sobre títulos ou informações.
    confetti[i].y =
      random(29, 68);

    confetti[i].speed =
      random(2, 5);

    confetti[i].color =
      colors[random(0, 6)];
  }
}

void eraseConfetti() {
  for (int i = 0; i < CONFETTI_COUNT; i++) {
    display.fillRect(
      confetti[i].x,
      confetti[i].y,
      2,
      3,
      TFT_CYAN
    );
  }
}

void drawConfetti() {
  for (int i = 0; i < CONFETTI_COUNT; i++) {
    display.fillRect(
      confetti[i].x,
      confetti[i].y,
      2,
      3,
      confetti[i].color
    );
  }
}

void moveConfetti() {
  for (int i = 0; i < CONFETTI_COUNT; i++) {
    confetti[i].y +=
      confetti[i].speed;

    if (confetti[i].y > 68) {
      confetti[i].y = 29;

      bool leftSide =
        random(2) == 0;

      if (leftSide) {
        confetti[i].x =
          random(3, 40);
      } else {
        confetti[i].x =
          random(120, 157);
      }
    }
  }
}

// ------------------------------------------------------
// Abre a tela de vitória
// ------------------------------------------------------

void showVictoryScreen() {
  drawVictoryBackground();
  drawVictoryFrog();
  drawVictorySparkles(true);
  drawVictoryLabels();

  generateConfetti();
  drawConfetti();

  victoryFrogWinking = false;
  victorySparklesBright = true;

  victoryBlinkTicks = 0;
  victoryNextBlinkTicks =
    random(16, 29);

  confettiVisible = true;

  // A tela aparece antes da melodia.
  playVictoryMelody();

  confettiTimer.resetMillis();
}

// ------------------------------------------------------
// Atualiza confetes, estrelinhas e piscada
// ------------------------------------------------------

void updateVictoryConfetti() {
  if (
    !confettiTimer.intervalMillis(120)
  ) {
    return;
  }

  eraseConfetti();
  moveConfetti();
  drawConfetti();

  // Faz as estrelinhas brilharem.
  victorySparklesBright =
    !victorySparklesBright;

  drawVictorySparkles(
    victorySparklesBright
  );

  // Controla a piscada do sapinho vencedor.
  victoryBlinkTicks++;

  if (!victoryFrogWinking) {
    if (
      victoryBlinkTicks >=
      victoryNextBlinkTicks
    ) {
      victoryFrogWinking = true;
      victoryBlinkTicks = 0;

      drawVictoryFrogEyes(true);
    }

    return;
  }

  // Aproximadamente 240 ms com o olho fechado.
  if (victoryBlinkTicks >= 2) {
    victoryFrogWinking = false;
    victoryBlinkTicks = 0;

    drawVictoryFrogEyes(false);

    victoryNextBlinkTicks =
      random(16, 29);
  }
}

// ======================================================
// Desenho dos objetos móveis
// ======================================================

void drawAllDynamicObjects() {
  // Repara decorações que possam ter sido
  // atravessadas pelos objetos móveis.
  drawPhaseDecoration();

  for (int i = 0; i < FLY_COUNT; i++) {
    drawFly(flies[i]);
  }

  int crocodileCount =
    activeCrocodileCount();

  for (
    int i = 0;
    i < crocodileCount;
    i++
  ) {
    drawCrocodile(crocodiles[i]);
  }

  drawFrog();

  if (tongueFrames > 0) {
    drawTongue();

    tongueWasDrawn = true;
    tongueFrames--;
  }

  if (hudDirty) {
    drawHUD();
  }
}

// ======================================================
// Controle das fases
// ======================================================

void startPhase() {
  phaseScore = 0;
  timeLeft = PHASE_SECONDS;

  frogX = 72;
  frogY = FROG_GROUND_Y;

  jumpVelocity = 0;
  jumping = false;

  tongueFrames = 0;
  tongueWasDrawn = false;

  crocodileMouthOpen = false;

  prepareObjects();

  frameTimer.resetMillis();
  secondTimer.resetMillis();
  crocodileMouthTimer.resetMillis();

  gameState = PLAYING;

  drawStaticScene();
  drawAllDynamicObjects();
}

void startNewGame() {
  currentPhase = 1;
  totalScore = 0;

  startPhase();
}

void finishPhase() {
  if (phaseScore <= 0) {
    gameState = GAME_OVER;
    showGameOverScreen();

    return;
  }

  totalScore += phaseScore;

  if (currentPhase >= 3) {
    gameState = VICTORY;
    showVictoryScreen();

    return;
  }

  gameState = PHASE_CLEARED;
  showPhaseClearedScreen();
}

void updatePlayingFrame() {
  // Apaga somente os objetos antigos.
  eraseDynamicObjects();

  readControls();
  updateJump();
  updateObjects();
  updateCrocodileMouth();

  checkFlies();
  checkCrocodiles();

  drawAllDynamicObjects();
}

// ======================================================
// Setup
// ======================================================

void begin() {
  Serial.begin(115200);

  Serial.print("ES32Lab LIB: ");
  Serial.println(ES32LAB_VERSION);

  randomSeed(
    analogRead(P_POT1)
  );

  buzzer.begin();
  buzzer.setVolume(55);

  display.init();
  display.setRotation(3);
  display.fillScreen(TFT_BLACK);

  //EDER showDifficultyMenu();
  openDifficultyMenu();
}

// ======================================================
// Loop
// ======================================================

void update() {
  if (gameState == DIFFICULTY_MENU) {
    updateDifficultyMenu();

    if (keyboard.press(KEY_CENTER)) {
      startNewGame();
    }

    return;
  }

  if (gameState == GAME_OVER) {
    updateGameOverBlink();

    if (keyboard.press(KEY_CENTER)) {
      openDifficultyMenu();
    }

    return;
  }

  if (gameState == PHASE_CLEARED) {
    updatePhaseClearedAnimation();

    if (
      transitionTimer.intervalMillis(
        2600
      )
    ) {
      currentPhase++;
      startPhase();
    }

    return;
  }

  if (gameState == VICTORY) {
    updateVictoryConfetti();

    if (keyboard.press(KEY_CENTER)) {
      //EDER showDifficultyMenu();
      openDifficultyMenu();
    }

    return;
  }

  if (
    secondTimer.intervalMillis(1000)
  ) {
    timeLeft--;
    hudDirty = true;

    if (timeLeft <= 0) {
      timeLeft = 0;

      drawHUD();
      finishPhase();

      return;
    }
  }

  if (
    frameTimer.intervalMillis(50)
  ) {
    updatePlayingFrame();
  }
}

}  // namespace FrogAdventureModule

const Arcade::GameDefinition FrogAdventureGame = {
  "FROG ADVENTURE",
  "Sapo, moscas e jacares",
  FrogAdventureModule::begin,
  FrogAdventureModule::update
};

}  // namespace ArcadeGames
