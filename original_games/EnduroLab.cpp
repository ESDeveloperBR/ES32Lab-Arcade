#include <Arduino.h>
#include <ES32Lab.h>
#include <TFT_eSPI.h>

// ============================================================
// Enduro Lab
// Versao com frame buffer e correcao da faixa preta lateral.
// ============================================================

// ------------------------------------------------------------
// Recursos da ES32Lab
// ------------------------------------------------------------

ES_TFT display;
TFT_eSprite frameBuffer(&display);

ES_AnalogKeyboard keyboard(P_KEYBOARD, 20);
ES_Buzzer buzzer(P_BUZZER);

ES_TimeInterval frameInterval;
ES_TimeInterval inputInterval;
ES_TimeInterval secondInterval;

// ------------------------------------------------------------
// Dimensoes do display
// ------------------------------------------------------------

constexpr int SCREEN_WIDTH  = 160;
constexpr int SCREEN_HEIGHT = 128;

constexpr int SCREEN_RIGHT  = SCREEN_WIDTH - 1;
constexpr int SCREEN_BOTTOM = SCREEN_HEIGHT - 1;

constexpr int HUD_HEIGHT = 14;
constexpr int HORIZON_Y  = 25;

constexpr int PLAYER_WIDTH  = 12;
constexpr int PLAYER_HEIGHT = 16;
constexpr int PLAYER_Y      = 108;

constexpr int ENEMY_COUNT = 5;

bool frameBufferReady = false;

// ------------------------------------------------------------
// Estados do jogo
// ------------------------------------------------------------

enum GameState {
  STATE_START,
  STATE_RUNNING,
  STATE_GAME_OVER
};

enum Environment {
  ENV_DAY,
  ENV_NIGHT,
  ENV_SNOW
};

struct Enemy {
  float y;
  int8_t lane;
  uint16_t color;
};

// ------------------------------------------------------------
// Variaveis do jogo
// ------------------------------------------------------------

GameState gameState = STATE_START;
Environment environment = ENV_DAY;

Enemy enemies[ENEMY_COUNT];

const uint16_t enemyColors[] = {
  TFT_RED,
  TFT_BLUE,
  TFT_YELLOW,
  TFT_MAGENTA,
  TFT_CYAN
};

constexpr int ENEMY_COLOR_COUNT =
  sizeof(enemyColors) / sizeof(enemyColors[0]);

float playerX = 74.0f;
float playerSpeed = 0.0f;

constexpr float MAX_SPEED = 7.0f;

int score = 0;
int stage = 1;
int timeLeft = 45;

float distanceToCheckpoint = 3000.0f;

int roadCurve = 0;
int targetRoadCurve = 0;
int curveHoldFrames = 0;

float roadAnimation = 0.0f;

int collisionCooldownFrames = 0;
int collisionFlashFrames = 0;
int checkpointMessageFrames = 0;

unsigned long gameFrame = 0;

// ------------------------------------------------------------
// Prototipos
// ------------------------------------------------------------

void showStartScreen();
void showGameOverScreen();
void showFrameBufferError();
void showDisplayDimensionError(int width, int height);

void startGame();
void endGame();

void handleControls();
void updateGame();
void updateRoadCurve();
void updateEnemies();
void reachCheckpoint();

void drawGame();
void drawBackground();
void drawRoad();
void drawFinishLine();
void drawEnemy(const Enemy& enemy);
void drawPlayer();
void drawHud();
void drawSnow();
void drawCheckpointMessage();

void spawnEnemy(Enemy& enemy, bool initialSpawn);

int roadCenterAt(int y);
int roadHalfWidthAt(int y);
int enemyXAt(const Enemy& enemy);
int enemyWidthAt(const Enemy& enemy);
int enemyHeightAt(const Enemy& enemy);

bool enemyCollidesWithPlayer(const Enemy& enemy);

void keepPlayerOnRoad();

void playPassSound();
void playCollisionSound();
void playCheckpointSound();
void playGameOverSound();

// ============================================================
// Inicializacao
// ============================================================

void setup() {
  Serial.begin(115200);

  Serial.println();
  Serial.println("Enduro Lab - ES32Lab");

  Serial.print("LIB ES32Lab: ");
  Serial.println(ES32LAB_VERSION);

  display.init();
  display.setRotation(3);
  display.fillScreen(TFT_BLACK);

  const int displayWidth = display.width();
  const int displayHeight = display.height();

  Serial.print("Display detectado: ");
  Serial.print(displayWidth);
  Serial.print(" x ");
  Serial.println(displayHeight);

  // Garante que a configuracao do display corresponde
  // à orientacao horizontal esperada.
  if (displayWidth != SCREEN_WIDTH ||
      displayHeight != SCREEN_HEIGHT) {
    showDisplayDimensionError(
      displayWidth,
      displayHeight
    );
    return;
  }

  // Define a profundidade antes de criar o sprite.
  frameBuffer.setColorDepth(16);

  // Usa as dimensoes reais retornadas pelo display.
  frameBufferReady =
    frameBuffer.createSprite(
      display.width(),
      display.height()
    ) != nullptr;

  if (!frameBufferReady) {
    showFrameBufferError();
    return;
  }

  frameBuffer.setTextWrap(false);

  // fillSprite() e o metodo correto para limpar o sprite.
  frameBuffer.fillSprite(TFT_BLACK);
  frameBuffer.pushSprite(0, 0);

  buzzer.begin();
  buzzer.setVolume(35);

  randomSeed(analogRead(P_POT1));

  showStartScreen();
}

// ============================================================
// Loop principal
// ============================================================

void loop() {
  if (!frameBufferReady) {
    return;
  }

  if (keyboard.press(KEY_CENTER)) {
    if (gameState == STATE_START ||
        gameState == STATE_GAME_OVER) {
      startGame();
    }
  }

  if (gameState != STATE_RUNNING) {
    return;
  }

  if (inputInterval.intervalMillis(55)) {
    handleControls();
  }

  if (secondInterval.intervalMillis(1000)) {
    timeLeft--;

    if (timeLeft <= 0) {
      timeLeft = 0;
      endGame();
      return;
    }
  }

  // Aproximadamente 20 quadros por segundo.
  if (frameInterval.intervalMillis(50)) {
    updateGame();
    drawGame();
  }
}

// ============================================================
// Telas estaticas
// ============================================================

void showStartScreen() {
  gameState = STATE_START;

  display.fillScreen(TFT_BLACK);

  display.setTextSize(2);
  display.setTextColor(TFT_YELLOW, TFT_BLACK);

  display.drawCentreScreenString(
    "ENDURO LAB",
    15,
    1
  );

  display.setTextSize(1);
  display.setTextColor(TFT_WHITE, TFT_BLACK);

  display.drawCentreScreenString(
    "Jogo de corrida 8 bits",
    43,
    1
  );

  display.drawCentreScreenString(
    "CIMA: acelerar",
    62,
    1
  );

  display.drawCentreScreenString(
    "ESQ/DIR: controlar",
    74,
    1
  );

  display.drawCentreScreenString(
    "BAIXO: frear",
    86,
    1
  );

  display.setTextColor(TFT_GREEN, TFT_BLACK);

  display.drawCentreScreenString(
    "CENTRO PARA INICIAR",
    108,
    1
  );
}

void showGameOverScreen() {
  display.fillScreen(TFT_BLACK);

  display.setTextSize(2);
  display.setTextColor(TFT_RED, TFT_BLACK);

  display.drawCentreScreenString(
    "GAME OVER",
    22,
    1
  );

  display.setTextSize(1);
  display.setTextColor(TFT_WHITE, TFT_BLACK);

  display.drawCentreScreenString(
    String("Pontuacao: ") + String(score),
    58,
    1
  );

  display.drawCentreScreenString(
    String("Checkpoint: ") + String(stage),
    73,
    1
  );

  display.setTextColor(TFT_GREEN, TFT_BLACK);

  display.drawCentreScreenString(
    "CENTRO PARA REINICIAR",
    103,
    1
  );
}

void showFrameBufferError() {
  display.fillScreen(TFT_BLACK);

  display.setTextSize(1);
  display.setTextColor(TFT_RED, TFT_BLACK);

  display.drawCentreScreenString(
    "ERRO DE MEMORIA",
    38,
    1
  );

  display.setTextColor(TFT_WHITE, TFT_BLACK);

  display.drawCentreScreenString(
    "Nao foi possivel",
    58,
    1
  );

  display.drawCentreScreenString(
    "criar o frame buffer.",
    70,
    1
  );

  Serial.println(
    "Erro: nao foi possivel criar o frame buffer."
  );
}

void showDisplayDimensionError(
  int width,
  int height
) {
  display.fillScreen(TFT_BLACK);

  display.setTextSize(1);
  display.setTextColor(TFT_RED, TFT_BLACK);

  display.drawCentreScreenString(
    "ERRO NO DISPLAY",
    32,
    1
  );

  display.setTextColor(TFT_WHITE, TFT_BLACK);

  display.drawCentreScreenString(
    String("Detectado: ") +
    String(width) +
    "x" +
    String(height),
    54,
    1
  );

  display.drawCentreScreenString(
    "Esperado: 160x128",
    68,
    1
  );

  display.drawCentreScreenString(
    "Confira a biblioteca TFT",
    88,
    1
  );

  Serial.println(
    "Erro: dimensoes inesperadas do display."
  );
}

// ============================================================
// Inicio e fim
// ============================================================

void startGame() {
  gameState = STATE_RUNNING;
  environment = ENV_DAY;

  score = 0;
  stage = 1;
  timeLeft = 45;

  playerX = 74.0f;
  playerSpeed = 0.0f;

  distanceToCheckpoint = 3000.0f;

  roadCurve = 0;
  targetRoadCurve = 0;
  curveHoldFrames = 20;

  roadAnimation = 0.0f;

  collisionCooldownFrames = 0;
  collisionFlashFrames = 0;
  checkpointMessageFrames = 0;

  gameFrame = 0;

  for (int i = 0; i < ENEMY_COUNT; i++) {
    spawnEnemy(enemies[i], true);
    enemies[i].y -= i * 23;
  }

  frameInterval.resetMillis();
  inputInterval.resetMillis();
  secondInterval.resetMillis();

  buzzer.sound(NOTE_C5, 70);
  buzzer.end();

  buzzer.sound(NOTE_E5, 70);
  buzzer.end();

  buzzer.sound(NOTE_G5, 100);
  buzzer.end();

  drawGame();
}

void endGame() {
  if (gameState != STATE_RUNNING) {
    return;
  }

  gameState = STATE_GAME_OVER;

  playGameOverSound();
  showGameOverScreen();
}

// ============================================================
// Controles
// ============================================================

void handleControls() {
  if (keyboard.hold(KEY_UP)) {
    playerSpeed += 0.16f;
  } else {
    playerSpeed -= 0.025f;
  }

  if (keyboard.hold(KEY_DOWN)) {
    playerSpeed -= 0.38f;
  }

  if (keyboard.hold(KEY_LEFT)) {
    playerX -= 3.0f;
  }

  if (keyboard.hold(KEY_RIGHT)) {
    playerX += 3.0f;
  }

  if (playerSpeed < 0.0f) {
    playerSpeed = 0.0f;
  }

  if (playerSpeed > MAX_SPEED) {
    playerSpeed = MAX_SPEED;
  }

  keepPlayerOnRoad();
}

// ============================================================
// Atualizacao do jogo
// ============================================================

void updateGame() {
  gameFrame++;

  if (collisionCooldownFrames > 0) {
    collisionCooldownFrames--;
  }

  if (collisionFlashFrames > 0) {
    collisionFlashFrames--;
  }

  if (checkpointMessageFrames > 0) {
    checkpointMessageFrames--;
  }

  updateRoadCurve();
  keepPlayerOnRoad();
  updateEnemies();

  roadAnimation +=
    0.8f + playerSpeed * 0.8f;

  while (roadAnimation >= 96.0f) {
    roadAnimation -= 96.0f;
  }

  distanceToCheckpoint -=
    playerSpeed * 0.72f;

  if (distanceToCheckpoint <= 0.0f) {
    reachCheckpoint();
  }
}

void updateRoadCurve() {
  if (curveHoldFrames <= 0) {
    targetRoadCurve = random(-28, 29);
    curveHoldFrames = random(70, 150);
  } else {
    curveHoldFrames--;
  }

  if ((gameFrame % 3) == 0) {
    if (roadCurve < targetRoadCurve) {
      roadCurve++;
    } else if (roadCurve > targetRoadCurve) {
      roadCurve--;
    }
  }
}

void updateEnemies() {
  const float enemyMovement =
    0.30f + playerSpeed * 0.27f;

  for (int i = 0; i < ENEMY_COUNT; i++) {
    enemies[i].y += enemyMovement;

    if (collisionCooldownFrames == 0 &&
        enemyCollidesWithPlayer(enemies[i])) {

      score -= 3;

      if (score < 0) {
        score = 0;
      }

      playerSpeed = 1.0f;

      collisionCooldownFrames = 28;
      collisionFlashFrames = 10;

      spawnEnemy(enemies[i], false);

      const int recoveredEnemy =
        (i + 1) % ENEMY_COUNT;

      if (enemies[recoveredEnemy].y > 45.0f) {
        enemies[recoveredEnemy].y =
          random(-100, -40);
      }

      playCollisionSound();
      continue;
    }

    if (enemies[i].y >
        SCREEN_HEIGHT + 10) {

      score++;

      playPassSound();
      spawnEnemy(enemies[i], false);
    }
  }
}

void reachCheckpoint() {
  stage++;

  timeLeft += 25;

  if (timeLeft > 99) {
    timeLeft = 99;
  }

  distanceToCheckpoint =
    3000.0f + stage * 220.0f;

  environment =
    static_cast<Environment>(
      (stage - 1) % 3
    );

  checkpointMessageFrames = 36;

  for (int i = 0; i < ENEMY_COUNT; i++) {
    spawnEnemy(enemies[i], true);
    enemies[i].y -= i * 20;
  }

  targetRoadCurve = random(-25, 26);
  curveHoldFrames = random(70, 130);

  playCheckpointSound();
}

// ============================================================
// Calculos da pista
// ============================================================

int roadCenterAt(int y) {
  if (y <= HORIZON_Y) {
    return SCREEN_WIDTH / 2;
  }

  float perspective =
    static_cast<float>(y - HORIZON_Y) /
    static_cast<float>(
      SCREEN_BOTTOM - HORIZON_Y
    );

  if (perspective < 0.0f) {
    perspective = 0.0f;
  }

  if (perspective > 1.0f) {
    perspective = 1.0f;
  }

  return
    (SCREEN_WIDTH / 2) +
    static_cast<int>(
      roadCurve * perspective
    );
}

int roadHalfWidthAt(int y) {
  if (y <= HORIZON_Y) {
    return 13;
  }

  float perspective =
    static_cast<float>(y - HORIZON_Y) /
    static_cast<float>(
      SCREEN_BOTTOM - HORIZON_Y
    );

  if (perspective < 0.0f) {
    perspective = 0.0f;
  }

  if (perspective > 1.0f) {
    perspective = 1.0f;
  }

  return
    13 +
    static_cast<int>(
      49.0f * perspective
    );
}

void keepPlayerOnRoad() {
  const int playerCenterY =
    PLAYER_Y + PLAYER_HEIGHT / 2;

  const int center =
    roadCenterAt(playerCenterY);

  const int halfWidth =
    roadHalfWidthAt(playerCenterY);

  const int leftLimit =
    center - halfWidth + 3;

  const int rightLimit =
    center +
    halfWidth -
    PLAYER_WIDTH -
    3;

  if (playerX < leftLimit) {
    playerX = leftLimit;
  }

  if (playerX > rightLimit) {
    playerX = rightLimit;
  }
}

// ============================================================
// Adversarios
// ============================================================

void spawnEnemy(
  Enemy& enemy,
  bool initialSpawn
) {
  enemy.lane = random(-1, 2);

  enemy.color =
    enemyColors[
      random(0, ENEMY_COLOR_COUNT)
    ];

  if (initialSpawn) {
    enemy.y = random(-105, -15);
  } else {
    enemy.y = random(-115, -35);
  }
}

int enemyWidthAt(const Enemy& enemy) {
  float perspective =
    (enemy.y - HORIZON_Y) /
    static_cast<float>(
      SCREEN_BOTTOM - HORIZON_Y
    );

  if (perspective < 0.0f) {
    perspective = 0.0f;
  }

  if (perspective > 1.0f) {
    perspective = 1.0f;
  }

  return
    4 +
    static_cast<int>(
      7.0f * perspective
    );
}

int enemyHeightAt(const Enemy& enemy) {
  float perspective =
    (enemy.y - HORIZON_Y) /
    static_cast<float>(
      SCREEN_BOTTOM - HORIZON_Y
    );

  if (perspective < 0.0f) {
    perspective = 0.0f;
  }

  if (perspective > 1.0f) {
    perspective = 1.0f;
  }

  return
    5 +
    static_cast<int>(
      10.0f * perspective
    );
}

int enemyXAt(const Enemy& enemy) {
  const int y =
    static_cast<int>(enemy.y);

  const int center =
    roadCenterAt(y);

  const int halfWidth =
    roadHalfWidthAt(y);

  const int laneSpacing =
    static_cast<int>(
      halfWidth * 0.50f
    );

  return
    center +
    enemy.lane * laneSpacing -
    enemyWidthAt(enemy) / 2;
}

bool enemyCollidesWithPlayer(
  const Enemy& enemy
) {
  const int enemyX =
    enemyXAt(enemy);

  const int enemyY =
    static_cast<int>(enemy.y);

  const int enemyWidth =
    enemyWidthAt(enemy);

  const int enemyHeight =
    enemyHeightAt(enemy);

  const int playerLeft =
    static_cast<int>(playerX);

  const int playerRight =
    playerLeft + PLAYER_WIDTH;

  const int playerTop =
    PLAYER_Y;

  const int playerBottom =
    PLAYER_Y + PLAYER_HEIGHT;

  const int enemyRight =
    enemyX + enemyWidth;

  const int enemyBottom =
    enemyY + enemyHeight;

  return
    playerLeft < enemyRight &&
    playerRight > enemyX &&
    playerTop < enemyBottom &&
    playerBottom > enemyY;
}

// ============================================================
// Desenho do quadro
// ============================================================

void drawGame() {
  drawBackground();
  drawRoad();

  if (distanceToCheckpoint < 190.0f) {
    drawFinishLine();
  }

  if (environment == ENV_SNOW) {
    drawSnow();
  }

  for (int i = 0; i < ENEMY_COUNT; i++) {
    drawEnemy(enemies[i]);
  }

  drawPlayer();
  drawHud();

  if (collisionFlashFrames > 0) {
    const uint16_t borderColor =
      (collisionFlashFrames % 2 == 0)
        ? TFT_RED
        : TFT_WHITE;

    frameBuffer.drawRect(
      0,
      HUD_HEIGHT,
      SCREEN_WIDTH,
      SCREEN_HEIGHT - HUD_HEIGHT,
      borderColor
    );

    frameBuffer.drawRect(
      1,
      HUD_HEIGHT + 1,
      SCREEN_WIDTH - 2,
      SCREEN_HEIGHT - HUD_HEIGHT - 2,
      borderColor
    );
  }

  if (checkpointMessageFrames > 0) {
    drawCheckpointMessage();
  }

  // O quadro completo e enviado de uma so vez.
  frameBuffer.pushSprite(0, 0);
}

// ============================================================
// Fundo
// ============================================================

void drawBackground() {
  uint16_t skyColor;
  uint16_t groundColor;

  switch (environment) {
    case ENV_NIGHT:
      skyColor = TFT_BLACK;
      groundColor = TFT_BLACK;
      break;

    case ENV_SNOW:
      skyColor = TFT_WHITE;
      groundColor = TFT_WHITE;
      break;

    case ENV_DAY:
    default:
      skyColor = TFT_CYAN;
      groundColor = TFT_GREEN;
      break;
  }

  // CORRECAO PRINCIPAL:
  // fillSprite() limpa todo o sprite usando suas dimensoes reais.
  frameBuffer.fillSprite(TFT_BLACK);

  // O ceu cobre explicitamente toda a largura.
  frameBuffer.fillRect(
    0,
    HUD_HEIGHT,
    SCREEN_WIDTH,
    HORIZON_Y - HUD_HEIGHT,
    skyColor
  );

  // O solo tambem cobre explicitamente os 160 pixels.
  // Assim nenhuma coluna permanece preta na lateral.
  frameBuffer.fillRect(
    0,
    HORIZON_Y,
    SCREEN_WIDTH,
    SCREEN_HEIGHT - HORIZON_Y,
    groundColor
  );

  if (environment == ENV_DAY) {
    frameBuffer.fillCircle(
      135,
      20,
      6,
      TFT_YELLOW
    );
  }

  if (environment == ENV_NIGHT) {
    for (int i = 0; i < 10; i++) {
      const int x =
        (i * 23 + 7) %
        SCREEN_WIDTH;

      const int y =
        HUD_HEIGHT +
        2 +
        ((i * 11) % 10);

      frameBuffer.drawPixel(
        x,
        y,
        TFT_WHITE
      );
    }
  }
}

// ============================================================
// Pista
// ============================================================

void drawRoad() {
  const int topCenter =
    roadCenterAt(HORIZON_Y);

  const int bottomCenter =
    roadCenterAt(SCREEN_BOTTOM);

  const int topHalf =
    roadHalfWidthAt(HORIZON_Y);

  const int bottomHalf =
    roadHalfWidthAt(SCREEN_BOTTOM);

  const int topLeft =
    topCenter - topHalf;

  const int topRight =
    topCenter + topHalf;

  const int bottomLeft =
    bottomCenter - bottomHalf;

  const int bottomRight =
    bottomCenter + bottomHalf;

  uint16_t roadColor;
  uint16_t edgeColor;
  uint16_t stripeColor;

  if (environment == ENV_NIGHT) {
    roadColor = TFT_BLACK;
    edgeColor = TFT_DARKGREY;
    stripeColor = TFT_DARKGREY;
  } else if (environment == ENV_SNOW) {
    roadColor = TFT_LIGHTGREY;
    edgeColor = TFT_DARKGREY;
    stripeColor = TFT_WHITE;
  } else {
    roadColor = TFT_DARKGREY;
    edgeColor = TFT_WHITE;
    stripeColor = TFT_YELLOW;
  }

  // SCREEN_BOTTOM vale 127.
  // A coordenada 128 estaria fora da tela.
  frameBuffer.fillTriangle(
    topLeft,
    HORIZON_Y,
    topRight,
    HORIZON_Y,
    bottomLeft,
    SCREEN_BOTTOM,
    roadColor
  );

  frameBuffer.fillTriangle(
    topRight,
    HORIZON_Y,
    bottomLeft,
    SCREEN_BOTTOM,
    bottomRight,
    SCREEN_BOTTOM,
    roadColor
  );

  frameBuffer.drawLine(
    topLeft,
    HORIZON_Y,
    bottomLeft,
    SCREEN_BOTTOM,
    edgeColor
  );

  frameBuffer.drawLine(
    topRight,
    HORIZON_Y,
    bottomRight,
    SCREEN_BOTTOM,
    edgeColor
  );

  for (int i = 0; i < 6; i++) {
    const int offset =
      (
        i * 22 +
        static_cast<int>(roadAnimation)
      ) % 110;

    const int y =
      HORIZON_Y + offset;

    if (y >= SCREEN_BOTTOM - 1) {
      continue;
    }

    const int center =
      roadCenterAt(y);

    const float perspective =
      static_cast<float>(
        y - HORIZON_Y
      ) /
      static_cast<float>(
        SCREEN_BOTTOM - HORIZON_Y
      );

    const int stripeWidth =
      1 +
      static_cast<int>(
        3.0f * perspective
      );

    const int stripeHeight =
      2 +
      static_cast<int>(
        6.0f * perspective
      );

    frameBuffer.fillRect(
      center - stripeWidth / 2,
      y,
      stripeWidth,
      stripeHeight,
      stripeColor
    );
  }
}

// ============================================================
// Linha de chegada
// ============================================================

void drawFinishLine() {
  float progress =
    (190.0f - distanceToCheckpoint) /
    190.0f;

  if (progress < 0.0f) {
    progress = 0.0f;
  }

  if (progress > 1.0f) {
    progress = 1.0f;
  }

  const int y =
    HORIZON_Y +
    static_cast<int>(
      progress *
      (
        SCREEN_HEIGHT -
        HORIZON_Y -
        12
      )
    );

  const int center =
    roadCenterAt(y);

  const int halfWidth =
    roadHalfWidthAt(y);

  const int left =
    center - halfWidth;

  const int totalWidth =
    halfWidth * 2;

  int squareWidth =
    totalWidth / 12;

  if (squareWidth < 1) {
    squareWidth = 1;
  }

  for (int row = 0; row < 2; row++) {
    for (int column = 0;
         column < 12;
         column++) {

      const uint16_t color =
        ((row + column) % 2 == 0)
          ? TFT_WHITE
          : TFT_BLACK;

      frameBuffer.fillRect(
        left + column * squareWidth,
        y + row * 3,
        squareWidth,
        3,
        color
      );
    }
  }
}

// ============================================================
// Desenho dos adversarios
// ============================================================

void drawEnemy(const Enemy& enemy) {
  const int y =
    static_cast<int>(enemy.y);

  if (y < HORIZON_Y - 10 ||
      y > SCREEN_BOTTOM + 5) {
    return;
  }

  const int x =
    enemyXAt(enemy);

  const int width =
    enemyWidthAt(enemy);

  const int height =
    enemyHeightAt(enemy);

  if (environment == ENV_NIGHT) {
    const int lightSize =
      width >= 8 ? 2 : 1;

    frameBuffer.fillRect(
      x,
      y + height - lightSize,
      lightSize,
      lightSize,
      TFT_YELLOW
    );

    frameBuffer.fillRect(
      x + width - lightSize,
      y + height - lightSize,
      lightSize,
      lightSize,
      TFT_YELLOW
    );

    return;
  }

  frameBuffer.fillRect(
    x,
    y,
    width,
    height,
    enemy.color
  );

  if (width >= 6 &&
      height >= 8) {

    frameBuffer.fillRect(
      x + 1,
      y + 2,
      width - 2,
      height / 3,
      TFT_BLACK
    );

    frameBuffer.fillRect(
      x + 1,
      y + height - 2,
      2,
      2,
      TFT_RED
    );

    frameBuffer.fillRect(
      x + width - 3,
      y + height - 2,
      2,
      2,
      TFT_RED
    );
  }
}

// ============================================================
// Carro do jogador
// ============================================================

void drawPlayer() {
  const int x =
    static_cast<int>(playerX);

  const uint16_t bodyColor =
    collisionFlashFrames > 0 &&
    collisionFlashFrames % 2 == 0
      ? TFT_WHITE
      : TFT_RED;

  frameBuffer.fillRect(
    x,
    PLAYER_Y,
    PLAYER_WIDTH,
    PLAYER_HEIGHT,
    bodyColor
  );

  // Vidro.
  frameBuffer.fillRect(
    x + 2,
    PLAYER_Y + 3,
    PLAYER_WIDTH - 4,
    5,
    TFT_BLACK
  );

  // Rodas.
  frameBuffer.fillRect(
    x - 1,
    PLAYER_Y + 5,
    2,
    5,
    TFT_BLACK
  );

  frameBuffer.fillRect(
    x + PLAYER_WIDTH - 1,
    PLAYER_Y + 5,
    2,
    5,
    TFT_BLACK
  );

  // Farois.
  frameBuffer.fillRect(
    x + 1,
    PLAYER_Y,
    3,
    2,
    TFT_YELLOW
  );

  frameBuffer.fillRect(
    x + PLAYER_WIDTH - 4,
    PLAYER_Y,
    3,
    2,
    TFT_YELLOW
  );

  // Lanternas.
  frameBuffer.fillRect(
    x + 1,
    PLAYER_Y + PLAYER_HEIGHT - 2,
    3,
    2,
    TFT_RED
  );

  frameBuffer.fillRect(
    x + PLAYER_WIDTH - 4,
    PLAYER_Y + PLAYER_HEIGHT - 2,
    3,
    2,
    TFT_RED
  );
}

// ============================================================
// Neve
// ============================================================

void drawSnow() {
  for (int i = 0; i < 20; i++) {
    const int x =
      (
        i * 29 +
        static_cast<int>(gameFrame * 2)
      ) % SCREEN_WIDTH;

    const int y =
      HUD_HEIGHT +
      (
        i * 17 +
        static_cast<int>(gameFrame * 3)
      ) %
      (
        SCREEN_HEIGHT - HUD_HEIGHT
      );

    frameBuffer.drawPixel(
      x,
      y,
      TFT_WHITE
    );

    if ((i % 4) == 0) {
      frameBuffer.drawPixel(
        (x + 1) % SCREEN_WIDTH,
        y,
        TFT_WHITE
      );
    }
  }
}

// ============================================================
// HUD
// ============================================================

void drawHud() {
  frameBuffer.fillRect(
    0,
    0,
    SCREEN_WIDTH,
    HUD_HEIGHT,
    TFT_BLACK
  );

  frameBuffer.setTextSize(1);

  frameBuffer.setTextColor(
    TFT_WHITE,
    TFT_BLACK
  );

  frameBuffer.setCursor(2, 3);

  frameBuffer.print("P:");
  frameBuffer.print(score);

  frameBuffer.print(" T:");
  frameBuffer.print(timeLeft);

  frameBuffer.print(" V:");
  frameBuffer.print(
    static_cast<int>(
      playerSpeed * 14.0f
    )
  );

  frameBuffer.print(" C:");
  frameBuffer.print(stage);
}

// ============================================================
// Mensagem do checkpoint
// ============================================================

void drawCheckpointMessage() {
  frameBuffer.fillRect(
    18,
    46,
    124,
    35,
    TFT_BLACK
  );

  frameBuffer.drawRect(
    18,
    46,
    124,
    35,
    TFT_YELLOW
  );

  frameBuffer.setTextColor(
    TFT_YELLOW,
    TFT_BLACK
  );

  frameBuffer.setTextSize(2);

  frameBuffer.drawCentreString(
    "CHECKPOINT",
    SCREEN_WIDTH / 2,
    51,
    1
  );

  frameBuffer.setTextColor(
    TFT_GREEN,
    TFT_BLACK
  );

  frameBuffer.setTextSize(1);

  frameBuffer.drawCentreString(
    "+25 SEGUNDOS",
    SCREEN_WIDTH / 2,
    70,
    1
  );
}

// ============================================================
// Sons
// ============================================================

void playPassSound() {
  buzzer.sound(NOTE_E6, 25);
  buzzer.end();
}

void playCollisionSound() {
  buzzer.sound(NOTE_C3, 80);
  buzzer.end();

  buzzer.sound(NOTE_G2, 120);
  buzzer.end();
}

void playCheckpointSound() {
  buzzer.sound(NOTE_C5, 60);
  buzzer.end();

  buzzer.sound(NOTE_E5, 60);
  buzzer.end();

  buzzer.sound(NOTE_G5, 100);
  buzzer.end();
}

void playGameOverSound() {
  buzzer.sound(NOTE_G4, 130);
  buzzer.end();

  buzzer.sound(NOTE_E4, 160);
  buzzer.end();

  buzzer.sound(NOTE_C4, 250);
  buzzer.end();
}