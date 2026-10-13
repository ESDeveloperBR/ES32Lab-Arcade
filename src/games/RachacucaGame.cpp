#include <Arduino.h>
#include <ES32Lab.h>
#include <LittleFS.h>

#include "RachacucaGame.h"

namespace ArcadeGames {
namespace RachacucaModule {

// ============================================================
// RACHACUCA - ES32Lab
// Jogo matematico estilo arcade
// ============================================================
//
// Fluxo:
// ATTRACT (abertura + rankings)
//   -> Operacao
//   -> Dificuldade
//   -> Pre-jogo:
//        1) INICIAR JOGO
//        2) COMO E O RANKING
//   -> 10 questoes
//   -> Resultado
//   -> Se entrar no TOP 5:
//        comemoracao + SIM/DIREITA ou NAO/ESQUERDA
//   -> Nome, somente se o jogador escolher salvar
//   -> Ranking
//
// Pontuacao:
//   TC = tempo_ms * 100 / fator_percentual
//   B  = round(99 * (R - TC) / R), limitado a 0..99
//   P  = acertos * 100 + B
//
// Fatores:
//   SOM = 100%
//   SUB = 105%
//   MUL = 110%
//   DIV = 115%
//   TOD = 120%
//
// Referencia de tempo R para as 10 questoes:
//   FACIL   = 30 s
//   MEDIO   = 45 s
//   DIFICIL = 60 s
//   EXTREMO = 90 s
//
// Consequencia importante:
//   10 acertos = 1000..1099
//    9 acertos =  900.. 999
// Logo, mais acertos SEMPRE supera menos acertos.
// ============================================================

ES_TFT display;
ES_AnalogKeyboard keyboard(P_KEYBOARD, 20);
ES_Buzzer buzzer(P_BUZZER);

ES_TimeInterval inputTimer;
ES_TimeInterval stateTimer;
ES_TimeInterval attractTimer;
ES_TimeInterval openingAnimTimer;
ES_TimeInterval questionTimer;
ES_TimeInterval celebrationAnimTimer;

ES_File fileManager;

// ============================================================
// CONFIGURACOES
// ============================================================

constexpr uint8_t TOTAL_QUESTIONS = 10;
constexpr uint8_t RANK_SIZE = 5;
constexpr uint8_t DIFFICULTY_COUNT = 4;
constexpr uint8_t RANK_RULE_PAGES = 3;

constexpr unsigned long ATTRACT_OPEN_MS = 3200;
constexpr unsigned long ATTRACT_RANK_MS = 3200;
constexpr unsigned long OPENING_ANIM_MS = 110;
constexpr unsigned long CELEBRATION_ANIM_MS = 90;

constexpr unsigned long PREPARE_MS = 1400;
constexpr unsigned long FEEDBACK_MS = 650;
constexpr unsigned long RESULT_MS = 4800;
constexpr unsigned long RANK_SAVED_MS = 1500;
constexpr unsigned long POST_RANK_MS = 5500;

constexpr uint8_t MODE_FACTOR_PERCENT[5] = {
  120, // TODOS
  100, // SOMA
  105, // SUBTRACAO
  110, // MULTIPLICACAO
  115  // DIVISAO
};

constexpr uint32_t DIFFICULTY_REFERENCE_MS[DIFFICULTY_COUNT] = {
  30000UL, // FACIL
  45000UL, // MEDIO
  60000UL, // DIFICIL
  90000UL  // EXTREMO
};

const char *RANK_DIR = "/rachacuca";
const char *RANK_FILE = "/rachacuca/ranking_v2.csv";
const char *RANK_HEADER = "RACHACUCA_RANK_V2";

// ============================================================
// ESTADOS E ENUMS
// ============================================================

enum GameState : uint8_t {
  STATE_ATTRACT,
  STATE_MENU_OPERATION,
  STATE_MENU_DIFFICULTY,
  STATE_PRE_GAME_MENU,
  STATE_RANK_RULES,
  STATE_PREPARE,
  STATE_QUESTION,
  STATE_FEEDBACK,
  STATE_RESULT,
  STATE_RANK_DECISION,
  STATE_NAME_ENTRY,
  STATE_RANK_SAVED,
  STATE_POST_RANK
};

enum OperationMode : uint8_t {
  MODE_ALL,
  MODE_ADD,
  MODE_SUB,
  MODE_MUL,
  MODE_DIV
};

enum Difficulty : uint8_t {
  DIFF_EASY,
  DIFF_MEDIUM,
  DIFF_HARD,
  DIFF_EXTREME
};

enum MathOperation : uint8_t {
  MATH_ADD,
  MATH_SUB,
  MATH_MUL,
  MATH_DIV
};

enum AnswerPosition : uint8_t {
  POS_UP,
  POS_RIGHT,
  POS_DOWN,
  POS_LEFT
};

// ============================================================
// ESTRUTURAS
// ============================================================

struct Question {
  int32_t a;
  int32_t b;
  int32_t result;
  MathOperation op;
  int32_t choices[4];
  uint8_t correctPosition;
  String key;
};

struct RankEntry {
  char name[4];
  uint16_t score;
  uint8_t correct;
  uint32_t timeMs;
  OperationMode mode;
};

// ============================================================
// VARIAVEIS
// ============================================================

GameState state = STATE_ATTRACT;

OperationMode selectedMode = MODE_ALL;
Difficulty selectedDifficulty = DIFF_EASY;

Question currentQuestion;

MathOperation operationDeck[TOTAL_QUESTIONS];
String usedQuestionKeys[TOTAL_QUESTIONS];
uint8_t usedQuestionCount = 0;

uint8_t questionIndex = 0;
uint8_t correctCount = 0;
uint8_t errorCount = 0;

uint32_t totalResponseMs = 0;
uint32_t lastQuestionMs = 0;
uint16_t finalScore = 0;
uint8_t finalBonus = 0;
uint32_t finalCorrectedTimeMs = 0;

bool lastAnswerCorrect = false;

RankEntry rankings[DIFFICULTY_COUNT][RANK_SIZE];
bool storageReady = false;

// Attract:
// 0 = abertura
// 1 = ranking facil
// 2 = ranking medio
// 3 = ranking dificil
// 4 = ranking extremo
uint8_t attractPage = 0;
uint8_t openingFrame = 0;
uint8_t celebrationFrame = 0;

// Menus
uint8_t operationMenuIndex = 0;
uint8_t difficultyMenuIndex = 0;
uint8_t preGameMenuIndex = 0;
uint8_t rankRulePage = 0;

// Nome estilo arcade
char initials[4] = "AAA";
uint8_t initialPosition = 0;

// ============================================================
// NOMES E PARAMETROS
// ============================================================

const char *difficultyName(Difficulty diff) {
  switch (diff) {
    case DIFF_EASY:    return "FACIL";
    case DIFF_MEDIUM:  return "MEDIO";
    case DIFF_HARD:    return "DIFICIL";
    case DIFF_EXTREME: return "EXTREMO";
  }
  return "";
}

const char *modeName(OperationMode mode) {
  switch (mode) {
    case MODE_ALL: return "TODOS";
    case MODE_ADD: return "SOMA";
    case MODE_SUB: return "SUBTRACAO";
    case MODE_MUL: return "MULTIPLICACAO";
    case MODE_DIV: return "DIVISAO";
  }
  return "";
}

const char *modeShortName(OperationMode mode) {
  switch (mode) {
    case MODE_ALL: return "TOD";
    case MODE_ADD: return "SOM";
    case MODE_SUB: return "SUB";
    case MODE_MUL: return "MUL";
    case MODE_DIV: return "DIV";
  }
  return "---";
}

uint8_t modeFactorPercent(OperationMode mode) {
  return MODE_FACTOR_PERCENT[(uint8_t)mode];
}

uint32_t difficultyReferenceMs(Difficulty diff) {
  return DIFFICULTY_REFERENCE_MS[(uint8_t)diff];
}

String factorText(OperationMode mode) {
  uint8_t factor = modeFactorPercent(mode);

  String text = "x";
  text += String(factor / 100);
  text += ".";
  text += String((factor % 100) / 10);
  text += String(factor % 10);

  return text;
}

String formatMillis1(uint32_t ms) {
  uint32_t seconds = ms / 1000;
  uint32_t tenths = (ms % 1000) / 100;

  String text = String(seconds);
  text += ".";
  text += String(tenths);
  text += "s";

  return text;
}

// ============================================================
// HELPERS DE DISPLAY
// ============================================================

void setTextStyle(uint8_t size, uint16_t color, uint16_t background = TFT_BLACK) {
  display.setTextSize(size);
  display.setTextColor(color, background);
}

void textAt(
  const String &text,
  int16_t x,
  int16_t y,
  uint8_t size = 1,
  uint16_t color = TFT_WHITE,
  uint16_t background = TFT_BLACK
) {
  setTextStyle(size, color, background);
  display.setCursor(x, y);
  display.print(text);
}

void centerText(
  const String &text,
  int16_t y,
  uint8_t size = 1,
  uint16_t color = TFT_WHITE,
  uint16_t background = TFT_BLACK
) {
  setTextStyle(size, color, background);
  display.drawCentreScreenString(text, y, 1);
}

void rightText(
  const String &text,
  int16_t y,
  uint8_t size = 1,
  uint16_t color = TFT_WHITE,
  uint16_t background = TFT_BLACK
) {
  setTextStyle(size, color, background);
  display.drawRightScreenString(text, y, 1);
}

int16_t approxTextWidth(const String &text, uint8_t size) {
  return (int16_t)text.length() * 6 * size;
}

uint8_t fitTextSize(
  const String &text,
  int16_t maxWidth,
  uint8_t preferredSize = 2
) {
  for (int8_t size = preferredSize; size >= 1; size--) {
    if (approxTextWidth(text, (uint8_t)size) <= maxWidth) {
      return (uint8_t)size;
    }
  }

  return 1;
}

void textCenteredInBox(
  const String &text,
  int16_t x,
  int16_t y,
  int16_t w,
  int16_t h,
  uint8_t size,
  uint16_t color,
  uint16_t background
) {
  int16_t textW = approxTextWidth(text, size);
  int16_t textH = 8 * size;

  int16_t tx = x + (w - textW) / 2;
  int16_t ty = y + (h - textH) / 2;

  if (tx < x + 1) {
    tx = x + 1;
  }

  textAt(text, tx, ty, size, color, background);
}

void drawHeader(const String &title, uint16_t color = TFT_CYAN) {
  display.fillRect(0, 0, 160, 18, TFT_NAVY);
  display.drawFastHLine(0, 18, 160, color);
  centerText(title, 5, 1, color, TFT_NAVY);
}

void drawFooterHint(const String &hint) {
  display.fillRect(0, 112, 160, 16, TFT_BLACK);
  display.drawFastHLine(0, 111, 160, TFT_DARKGREY);
  centerText(hint, 117, 1, TFT_LIGHTGREY);
}

void drawMenuCard(
  int16_t y,
  int16_t h,
  const String &label,
  bool selected
) {
  uint16_t bg = selected ? TFT_NAVY : TFT_BLACK;
  uint16_t border = selected ? TFT_YELLOW : TFT_DARKGREY;
  uint16_t text = selected ? TFT_YELLOW : TFT_WHITE;

  display.fillRoundRect(12, y, 136, h, 5, bg);
  display.drawRoundRect(12, y, 136, h, 5, border);

  if (selected) {
    display.fillCircle(23, y + h / 2, 3, TFT_YELLOW);
  }

  centerText(label, y + (h - 8) / 2, 1, text, bg);
}

void drawMathSymbol(
  int16_t cx,
  int16_t cy,
  MathOperation op,
  uint16_t color,
  uint8_t radius = 8
) {
  switch (op) {
    case MATH_ADD:
      display.drawFastHLine(cx - radius, cy, radius * 2 + 1, color);
      display.drawFastVLine(cx, cy - radius, radius * 2 + 1, color);
      break;

    case MATH_SUB:
      display.drawFastHLine(cx - radius, cy, radius * 2 + 1, color);
      break;

    case MATH_MUL:
      display.drawLine(cx - radius, cy - radius, cx + radius, cy + radius, color);
      display.drawLine(cx - radius, cy + radius, cx + radius, cy - radius, color);
      break;

    case MATH_DIV:
      display.drawFastHLine(cx - radius, cy, radius * 2 + 1, color);
      display.fillCircle(cx, cy - radius + 1, 2, color);
      display.fillCircle(cx, cy + radius - 1, 2, color);
      break;
  }
}

// ============================================================
// ABERTURA DINAMICA
// ============================================================

void drawOpeningNetwork() {
  const int16_t nodeX[4] = {27, 133, 27, 133};
  const int16_t nodeY[4] = {25, 25, 57, 57};

  // As linhas ficam estaticas. Isso evita redesenhar a tela inteira
  // a cada frame e elimina o efeito de "piscar".
  for (uint8_t i = 0; i < 4; i++) {
    display.drawLine(
      80,
      41,
      nodeX[i],
      nodeY[i],
      TFT_DARKCYAN
    );
  }
}

void updateOpeningAnimation() {
  // Limpa SOMENTE a pequena area animada no centro.
  // A moldura, textos e simbolos laterais permanecem intactos.
  display.fillCircle(
    80,
    41,
    23,
    TFT_BLACK
  );

  // Recoloca as linhas que atravessam a area limpa.
  drawOpeningNetwork();

  uint8_t pulse =
    8 +
    (openingFrame % 5) * 2;

  if (pulse > 14) {
    pulse = 14;
  }

  display.drawCircle(
    80,
    41,
    pulse,
    TFT_CYAN
  );

  display.drawCircle(
    80,
    41,
    pulse + 4,
    TFT_NAVY
  );

  MathOperation centerOp =
    (MathOperation)(
      openingFrame % 4
    );

  drawMathSymbol(
    80,
    41,
    centerOp,
    TFT_YELLOW,
    7
  );

  // Atualiza somente a linha do pequeno indicador de movimento.
  // Redesenhar uma unica linha e muito mais rapido que limpar a tela.
  display.drawFastHLine(
    18,
    70,
    124,
    TFT_DARKGREY
  );

  int16_t runnerX =
    18 +
    (openingFrame * 11) % 124;

  display.fillCircle(
    runnerX,
    70,
    2,
    TFT_YELLOW
  );
}

void renderOpeningStatic() {
  display.fillScreen(
    TFT_BLACK
  );

  // Moldura dupla
  display.drawRoundRect(
    2,
    2,
    156,
    124,
    8,
    TFT_NAVY
  );

  display.drawRoundRect(
    5,
    5,
    150,
    118,
    7,
    TFT_DARKCYAN
  );

  drawOpeningNetwork();

  // Simbolos orbitais sao estaticos.
  drawMathSymbol(
    27,
    25,
    MATH_ADD,
    TFT_GREEN,
    5
  );

  drawMathSymbol(
    133,
    25,
    MATH_MUL,
    TFT_MAGENTA,
    5
  );

  drawMathSymbol(
    27,
    57,
    MATH_SUB,
    TFT_ORANGE,
    5
  );

  drawMathSymbol(
    133,
    57,
    MATH_DIV,
    TFT_SKYBLUE,
    5
  );

  display.drawFastHLine(
    18,
    70,
    124,
    TFT_DARKGREY
  );

  centerText(
    "RACHACUCA",
    78,
    2,
    TFT_CYAN
  );

  centerText(
    "PENSE  RESPONDA  SUPERE",
    99,
    1,
    TFT_WHITE
  );

  // O texto deixa de piscar propositalmente.
  centerText(
    "QUALQUER TECLA INICIA",
    115,
    1,
    TFT_GREEN
  );

  // Desenha apenas a camada animada inicial.
  updateOpeningAnimation();
}

// ============================================================
// EQUACAO
// ============================================================

String equationText(const Question &q) {
  String text = String(q.a);

  switch (q.op) {
    case MATH_ADD: text += " + "; break;
    case MATH_SUB: text += " - "; break;
    case MATH_MUL: text += " x "; break;
    case MATH_DIV: text += " / "; break;
  }

  text += String(q.b);

  return text;
}

// ============================================================
// TECLADO
// ============================================================

bool anyKeyPressed() {
  if (keyboard.press(KEY_UP)) {
    return true;
  }

  if (keyboard.press(KEY_RIGHT)) {
    return true;
  }

  if (keyboard.press(KEY_DOWN)) {
    return true;
  }

  if (keyboard.press(KEY_LEFT)) {
    return true;
  }

  if (keyboard.press(KEY_CENTER)) {
    return true;
  }

  return false;
}

// ============================================================
// PONTUACAO
// ============================================================

// ============================================================
// EFEITOS SONOROS CURTOS
// ============================================================
//
// O cronometro da questao e parado ANTES destes sons.
// Acerto: dois pulsos ascendentes, cerca de 22 ms.
// Erro: dois pulsos descendentes, cerca de 26 ms.
//

void playCorrectSound() {
  buzzer.sound(NOTE_E6, 10);
  buzzer.sound(NOTE_G6, 12);
}

void playErrorSound() {
  buzzer.sound(NOTE_E4, 12);
  buzzer.sound(NOTE_C4, 14);
}

uint32_t correctedTimeMs(uint32_t realTimeMs, OperationMode mode) {
  uint8_t factor = modeFactorPercent(mode);

  // TC = tempo * 100 / fator_percentual
  // Usa 64 bits para evitar overflow na multiplicacao.
  return (uint32_t)(
    ((uint64_t)realTimeMs * 100ULL + factor / 2)
    / factor
  );
}

uint8_t calculateSpeedBonus(
  uint32_t realTimeMs,
  OperationMode mode,
  Difficulty diff
) {
  uint32_t corrected = correctedTimeMs(realTimeMs, mode);
  uint32_t reference = difficultyReferenceMs(diff);

  if (corrected >= reference) {
    return 0;
  }

  uint32_t remaining = reference - corrected;

  // B = round(99 * remaining / reference)
  uint32_t bonus =
    (99UL * remaining + reference / 2UL)
    / reference;

  if (bonus > 99) {
    bonus = 99;
  }

  return (uint8_t)bonus;
}

uint16_t calculateScore(
  uint8_t hits,
  uint32_t realTimeMs,
  OperationMode mode,
  Difficulty diff
) {
  uint8_t bonus =
    calculateSpeedBonus(realTimeMs, mode, diff);

  return (uint16_t)hits * 100U + bonus;
}

// ============================================================
// RANKING - UTILITARIOS
// ============================================================

void clearRankings() {
  for (uint8_t d = 0; d < DIFFICULTY_COUNT; d++) {
    for (uint8_t p = 0; p < RANK_SIZE; p++) {
      strcpy(rankings[d][p].name, "---");
      rankings[d][p].score = 0;
      rankings[d][p].correct = 0;
      rankings[d][p].timeMs = 0;
      rankings[d][p].mode = MODE_ALL;
    }
  }
}

bool isEmptyRank(const RankEntry &entry) {
  return entry.score == 0;
}

bool isBetterRank(
  const RankEntry &candidate,
  const RankEntry &reference,
  Difficulty diff
) {
  if (candidate.score != reference.score) {
    return candidate.score > reference.score;
  }

  if (candidate.correct != reference.correct) {
    return candidate.correct > reference.correct;
  }

  uint32_t candidateCorrected =
    correctedTimeMs(candidate.timeMs, candidate.mode);

  uint32_t referenceCorrected =
    correctedTimeMs(reference.timeMs, reference.mode);

  if (candidateCorrected != referenceCorrected) {
    return candidateCorrected < referenceCorrected;
  }

  // Ultimo desempate: menor tempo real.
  return candidate.timeMs < reference.timeMs;
}

String csvField(const String &line, uint8_t fieldIndex) {
  int start = 0;
  uint8_t current = 0;

  while (true) {
    int comma = line.indexOf(',', start);

    if (current == fieldIndex) {
      if (comma < 0) {
        return line.substring(start);
      }

      return line.substring(start, comma);
    }

    if (comma < 0) {
      return "";
    }

    start = comma + 1;
    current++;
  }
}

// ============================================================
// RANKING - LITTLEFS
// ============================================================

bool saveRankings() {
  if (!storageReady) {
    return false;
  }

  if (!fileManager.println(RANK_FILE, RANK_HEADER, true)) {
    Serial.println("Falha ao criar arquivo de ranking.");
    return false;
  }

  for (uint8_t d = 0; d < DIFFICULTY_COUNT; d++) {
    for (uint8_t p = 0; p < RANK_SIZE; p++) {
      String line;
      line.reserve(64);

      line += String(d);
      line += ",";
      line += String(p);
      line += ",";
      line += rankings[d][p].name;
      line += ",";
      line += String(rankings[d][p].score);
      line += ",";
      line += String(rankings[d][p].correct);
      line += ",";
      line += String((unsigned long)rankings[d][p].timeMs);
      line += ",";
      line += String((uint8_t)rankings[d][p].mode);

      if (!fileManager.println(RANK_FILE, line, false)) {
        Serial.println("Falha ao gravar uma linha do ranking.");
        return false;
      }
    }
  }

  return true;
}

void loadRankings() {
  clearRankings();

  // ES_File inicializa o LittleFS conforme a implementacao oficial da LIB.
  storageReady = fileManager.begin(LittleFS);

  if (!storageReady) {
    Serial.println(
      "ES_File nao conseguiu inicializar LittleFS. "
      "Ranking funcionara somente na RAM."
    );
    return;
  }

  if (!fileManager.directoryExists(RANK_DIR)) {
    if (!fileManager.createDirectory(RANK_DIR)) {
      Serial.println("Falha ao criar diretorio /rachacuca.");
      storageReady = false;
      return;
    }
  }

  if (!fileManager.exists(RANK_FILE)) {
    Serial.println("Ranking V2 ainda nao existe. Criando...");
    saveRankings();
    return;
  }

  String content = fileManager.readFile(RANK_FILE);

  int firstBreak = content.indexOf('\n');

  // Compatibilidade com arquivos que possam terminar sem \n.
  String header;
  if (firstBreak < 0) {
    header = content;
  } else {
    header = content.substring(0, firstBreak);
  }

  header.trim();

  if (header != RANK_HEADER) {
    Serial.println(
      "Arquivo de ranking com versao incompatível. "
      "O ranking V2 sera recriado."
    );
    clearRankings();
    saveRankings();
    return;
  }

  int start = (firstBreak < 0) ? content.length() : firstBreak + 1;

  while (start < content.length()) {
    int end = content.indexOf('\n', start);

    if (end < 0) {
      end = content.length();
    }

    String line = content.substring(start, end);
    line.trim();
    start = end + 1;

    if (line.length() == 0) {
      continue;
    }

    int d = csvField(line, 0).toInt();
    int p = csvField(line, 1).toInt();
    String name = csvField(line, 2);
    uint16_t score = (uint16_t)csvField(line, 3).toInt();
    uint8_t hits = (uint8_t)csvField(line, 4).toInt();

    uint32_t timeMs =
      (uint32_t)strtoul(
        csvField(line, 5).c_str(),
        nullptr,
        10
      );

    int modeValue = csvField(line, 6).toInt();

    if (
      d < 0 || d >= DIFFICULTY_COUNT ||
      p < 0 || p >= RANK_SIZE ||
      name.length() != 3 ||
      modeValue < MODE_ALL || modeValue > MODE_DIV
    ) {
      continue;
    }

    name.toUpperCase();

    snprintf(
      rankings[d][p].name,
      sizeof(rankings[d][p].name),
      "%s",
      name.c_str()
    );

    rankings[d][p].score = score;
    rankings[d][p].correct =
      hits > TOTAL_QUESTIONS
        ? TOTAL_QUESTIONS
        : hits;

    rankings[d][p].timeMs = timeMs;
    rankings[d][p].mode = (OperationMode)modeValue;
  }
}

bool qualifiesForRanking() {
  if (correctCount == 0) {
    return false;
  }

  RankEntry candidate;
  strcpy(candidate.name, "NEW");

  candidate.score = finalScore;
  candidate.correct = correctCount;
  candidate.timeMs = totalResponseMs;
  candidate.mode = selectedMode;

  for (uint8_t p = 0; p < RANK_SIZE; p++) {
    if (isEmptyRank(rankings[selectedDifficulty][p])) {
      return true;
    }

    if (
      isBetterRank(
        candidate,
        rankings[selectedDifficulty][p],
        selectedDifficulty
      )
    ) {
      return true;
    }
  }

  return false;
}

void insertCurrentScoreInRanking() {
  RankEntry candidate;

  snprintf(
    candidate.name,
    sizeof(candidate.name),
    "%c%c%c",
    initials[0],
    initials[1],
    initials[2]
  );

  candidate.score = finalScore;
  candidate.correct = correctCount;
  candidate.timeMs = totalResponseMs;
  candidate.mode = selectedMode;

  RankEntry temp[RANK_SIZE + 1];

  for (uint8_t i = 0; i < RANK_SIZE; i++) {
    temp[i] = rankings[selectedDifficulty][i];
  }

  temp[RANK_SIZE] = candidate;

  for (uint8_t i = 0; i < RANK_SIZE; i++) {
    for (uint8_t j = i + 1; j < RANK_SIZE + 1; j++) {
      bool swapNeeded = false;

      if (isEmptyRank(temp[i])) {
        if (!isEmptyRank(temp[j])) {
          swapNeeded = true;
        }
      } else if (
        !isEmptyRank(temp[j]) &&
        isBetterRank(temp[j], temp[i], selectedDifficulty)
      ) {
        swapNeeded = true;
      }

      if (swapNeeded) {
        RankEntry aux = temp[i];
        temp[i] = temp[j];
        temp[j] = aux;
      }
    }
  }

  for (uint8_t i = 0; i < RANK_SIZE; i++) {
    rankings[selectedDifficulty][i] = temp[i];
  }

  if (!saveRankings()) {
    Serial.println(
      "Ranking atualizado na RAM, mas nao foi salvo no LittleFS."
    );
  }
}

// ============================================================
// BARALHO DE OPERACOES
// ============================================================

MathOperation operationFromMode(OperationMode mode) {
  switch (mode) {
    case MODE_ADD: return MATH_ADD;
    case MODE_SUB: return MATH_SUB;
    case MODE_MUL: return MATH_MUL;
    case MODE_DIV: return MATH_DIV;
    case MODE_ALL:
    default:
      return MATH_ADD;
  }
}

void prepareOperationDeck() {
  if (selectedMode != MODE_ALL) {
    MathOperation op = operationFromMode(selectedMode);

    for (uint8_t i = 0; i < TOTAL_QUESTIONS; i++) {
      operationDeck[i] = op;
    }

    return;
  }

  // Duas de cada operacao = 8 questoes.
  uint8_t index = 0;

  for (uint8_t repeat = 0; repeat < 2; repeat++) {
    operationDeck[index++] = MATH_ADD;
    operationDeck[index++] = MATH_SUB;
    operationDeck[index++] = MATH_MUL;
    operationDeck[index++] = MATH_DIV;
  }

  // Duas operacoes extras, diferentes entre si:
  // distribuicao final = 3/3/2/2.
  uint8_t extra1 = random(0, 4);
  uint8_t extra2;

  do {
    extra2 = random(0, 4);
  } while (extra2 == extra1);

  operationDeck[8] = (MathOperation)extra1;
  operationDeck[9] = (MathOperation)extra2;

  // Fisher-Yates
  for (int i = TOTAL_QUESTIONS - 1; i > 0; i--) {
    int j = random(0, i + 1);

    MathOperation temp = operationDeck[i];
    operationDeck[i] = operationDeck[j];
    operationDeck[j] = temp;
  }
}

// ============================================================
// GERACAO DE OPERANDOS
// ============================================================

void generateOperands(
  MathOperation op,
  Difficulty diff,
  int32_t &a,
  int32_t &b,
  int32_t &result
) {
  switch (op) {
    case MATH_ADD:
      if (diff == DIFF_EASY) {
        a = random(0, 10);
        b = random(0, 10);
      } else if (diff == DIFF_MEDIUM) {
        a = random(10, 100);
        b = random(10, 100);
      } else if (diff == DIFF_HARD) {
        a = random(100, 1000);
        b = random(100, 1000);
      } else {
        a = random(1000, 10000);
        b = random(1000, 10000);
      }

      result = a + b;
      break;

    case MATH_SUB:
      if (diff == DIFF_EASY) {
        a = random(0, 10);
        b = random(0, 10);
      } else if (diff == DIFF_MEDIUM) {
        a = random(10, 100);
        b = random(10, 100);
      } else if (diff == DIFF_HARD) {
        a = random(100, 1000);
        b = random(100, 1000);
      } else {
        a = random(1000, 10000);
        b = random(1000, 10000);
      }

      // Negativos somente no EXTREMO.
      if (diff != DIFF_EXTREME && b > a) {
        int32_t temp = a;
        a = b;
        b = temp;
      }

      result = a - b;
      break;

    case MATH_MUL:
      if (diff == DIFF_EASY) {
        a = random(0, 10);
        b = random(0, 10);
      } else if (diff == DIFF_MEDIUM) {
        a = random(10, 100);
        b = random(2, 10);
      } else if (diff == DIFF_HARD) {
        a = random(10, 100);
        b = random(10, 100);
      } else {
        a = random(100, 1000);
        b = random(10, 100);
      }

      result = a * b;
      break;

    case MATH_DIV:
      if (diff == DIFF_EASY) {
        int32_t divisor;
        int32_t quotient;

        do {
          divisor = random(1, 10);
          quotient = random(1, 10);
        } while ((divisor * quotient) > 9);

        a = divisor * quotient;
        b = divisor;
        result = quotient;
      } else if (diff == DIFF_MEDIUM) {
        b = random(2, 10);
        result = random(10, 100);
        a = b * result;
      } else if (diff == DIFF_HARD) {
        b = random(10, 100);
        result = random(10, 100);
        a = b * result;
      } else {
        b = random(10, 100);
        result = random(100, 1000);
        a = b * result;
      }
      break;
  }
}

// ============================================================
// NAO REPETIR EQUACOES
// ============================================================

String makeQuestionKey(
  MathOperation op,
  int32_t a,
  int32_t b
) {
  int32_t keyA = a;
  int32_t keyB = b;

  // Soma e multiplicacao sao comutativas:
  // 5+3 == 3+5 para efeito de repeticao.
  if (
    (op == MATH_ADD || op == MATH_MUL) &&
    keyB < keyA
  ) {
    int32_t temp = keyA;
    keyA = keyB;
    keyB = temp;
  }

  String key;
  key.reserve(32);

  key += String((uint8_t)op);
  key += ":";
  key += String(keyA);
  key += ":";
  key += String(keyB);

  return key;
}

bool questionAlreadyUsed(const String &key) {
  for (uint8_t i = 0; i < usedQuestionCount; i++) {
    if (usedQuestionKeys[i] == key) {
      return true;
    }
  }

  return false;
}

// ============================================================
// ALTERNATIVAS
// ============================================================

bool valueAlreadyExists(
  const int32_t values[4],
  uint8_t count,
  int32_t value
) {
  for (uint8_t i = 0; i < count; i++) {
    if (values[i] == value) {
      return true;
    }
  }

  return false;
}

void generateChoices(Question &q) {
  q.correctPosition = random(0, 4);

  int32_t candidates[4];
  uint8_t count = 0;

  candidates[count++] = q.result;

  int32_t magnitude = q.result >= 0 ? q.result : -q.result;
  int32_t span = magnitude / 12 + 3;

  if (span < 3) {
    span = 3;
  }

  if (span > 120) {
    span = 120;
  }

  while (count < 4) {
    int32_t delta = random(-span, span + 1);

    if (delta == 0) {
      continue;
    }

    int32_t candidate = q.result + delta;

    // Fora do extremo, mantemos alternativas nao negativas.
    if (selectedDifficulty != DIFF_EXTREME && candidate < 0) {
      continue;
    }

    if (valueAlreadyExists(candidates, count, candidate)) {
      continue;
    }

    candidates[count++] = candidate;
  }

  uint8_t distractor = 1;

  for (uint8_t pos = 0; pos < 4; pos++) {
    if (pos == q.correctPosition) {
      q.choices[pos] = q.result;
    } else {
      q.choices[pos] = candidates[distractor++];
    }
  }
}

// ============================================================
// GERAR QUESTAO
// ============================================================

void generateQuestion() {
  MathOperation op = operationDeck[questionIndex];

  bool found = false;

  for (uint16_t attempt = 0; attempt < 500; attempt++) {
    currentQuestion.op = op;

    generateOperands(
      op,
      selectedDifficulty,
      currentQuestion.a,
      currentQuestion.b,
      currentQuestion.result
    );

    currentQuestion.key =
      makeQuestionKey(
        currentQuestion.op,
        currentQuestion.a,
        currentQuestion.b
      );

    if (!questionAlreadyUsed(currentQuestion.key)) {
      found = true;
      break;
    }
  }

  // Com os espacos escolhidos, 500 tentativas sao mais que suficientes.
  // Este fallback evita travar o jogo caso as regras sejam alteradas no futuro.
  if (!found) {
    currentQuestion.key += ":F";
  }

  if (usedQuestionCount < TOTAL_QUESTIONS) {
    usedQuestionKeys[usedQuestionCount++] = currentQuestion.key;
  }

  generateChoices(currentQuestion);
}

// ============================================================
// TELA - RANKING
// ============================================================

void renderRanking(
  Difficulty diff,
  bool showStartHint = true
) {
  display.fillScreen(TFT_BLACK);

  String title = "TOP 5 - ";
  title += difficultyName(diff);

  drawHeader(title, TFT_YELLOW);

  textAt("# NOME MOD PTS", 11, 24, 1, TFT_DARKGREY);

  for (uint8_t p = 0; p < RANK_SIZE; p++) {
    int16_t y = 38 + p * 14;

    uint16_t color =
      p == 0
        ? TFT_CYAN
        : TFT_WHITE;

    String line = String(p + 1);
    line += " ";
    line += rankings[diff][p].name;
    line += " ";

    if (isEmptyRank(rankings[diff][p])) {
      line += "--- ----";
    } else {
      line += modeShortName(rankings[diff][p].mode);
      line += " ";
      line += String(rankings[diff][p].score);
    }

    textAt(line, 12, y, 1, color);
  }

  if (showStartHint) {
    drawFooterHint("QUALQUER TECLA = JOGAR");
  }
}

// ============================================================
// ATTRACT MODE
// ============================================================

void renderAttractPage() {
  if (attractPage == 0) {
    renderOpeningStatic();
  } else {
    renderRanking((Difficulty)(attractPage - 1), true);
  }
}

// ============================================================
// MENU - OPERACAO
// ============================================================

void renderOperationMenu() {
  display.fillScreen(TFT_BLACK);
  drawHeader("ESCOLHA A OPERACAO");

  const char *items[5] = {
    "TODOS",
    "SOMA",
    "SUBTRACAO",
    "MULTIPLICACAO",
    "DIVISAO"
  };

  for (uint8_t i = 0; i < 5; i++) {
    int16_t y = 24 + i * 17;
    drawMenuCard(y, 14, items[i], i == operationMenuIndex);
  }

  drawFooterHint("UP/DN  CENTRO=OK  < VOLTA");
}

// ============================================================
// MENU - DIFICULDADE
// ============================================================

void renderDifficultyMenu() {
  display.fillScreen(TFT_BLACK);
  drawHeader("ESCOLHA O NIVEL", TFT_MAGENTA);

  const char *items[4] = {
    "FACIL",
    "MEDIO",
    "DIFICIL",
    "EXTREMO"
  };

  for (uint8_t i = 0; i < 4; i++) {
    int16_t y = 26 + i * 20;
    drawMenuCard(y, 16, items[i], i == difficultyMenuIndex);
  }

  drawFooterHint("UP/DN  CENTRO=OK  < VOLTA");
}

// ============================================================
// MENU - PRE-JOGO
// ============================================================

void renderPreGameMenu() {
  display.fillScreen(TFT_BLACK);
  drawHeader("RACHACUCA - PRONTO?", TFT_GREEN);

  String config = modeShortName(selectedMode);
  config += "  ";
  config += difficultyName(selectedDifficulty);
  config += "  ";
  config += factorText(selectedMode);

  centerText(config, 26, 1, TFT_CYAN);

  drawMenuCard(
    47,
    23,
    "INICIAR JOGO",
    preGameMenuIndex == 0
  );

  drawMenuCard(
    78,
    23,
    "COMO E O RANKING",
    preGameMenuIndex == 1
  );

  drawFooterHint("UP/DN  CENTRO=OK  < VOLTA");
}

// ============================================================
// TELA - REGRAS DO RANKING
// ============================================================

void renderRankRules() {
  display.fillScreen(TFT_BLACK);

  String pageTitle = "RANKING ";
  pageTitle += String(rankRulePage + 1);
  pageTitle += "/";
  pageTitle += String(RANK_RULE_PAGES);

  drawHeader(pageTitle, TFT_YELLOW);

  if (rankRulePage == 0) {
    centerText("PONTOS = A x 100 + B", 27, 1, TFT_CYAN);

    textAt("A = ACERTOS", 13, 46, 1, TFT_WHITE);
    textAt("B = BONUS 0..99", 13, 61, 1, TFT_WHITE);

    textAt("10 ACERTOS: 1000..1099", 13, 79, 1, TFT_GREEN);
    textAt(" 9 ACERTOS:  900.. 999", 13, 93, 1, TFT_YELLOW);

    centerText("MAIS ACERTOS SEMPRE VENCE", 104, 1, TFT_GREEN);
  }

  else if (rankRulePage == 1) {
    centerText("TEMPO CORRIGIDO", 25, 1, TFT_CYAN);
    centerText("TC = TEMPO / FATOR", 39, 1, TFT_WHITE);

    textAt("SOM  x1.00   SUB  x1.05", 9, 58, 1, TFT_WHITE);
    textAt("MUL  x1.10   DIV  x1.15", 9, 72, 1, TFT_WHITE);
    textAt("TOD  x1.20", 9, 86, 1, TFT_YELLOW);

    centerText("TODOS: 3/3/2/2 OPERACOES", 102, 1, TFT_LIGHTGREY);
  }

  else {
    centerText("BONUS DE VELOCIDADE", 24, 1, TFT_CYAN);
    centerText("B = 99 x (R-TC) / R", 39, 1, TFT_WHITE);
    centerText("LIMITADO ENTRE 0 E 99", 52, 1, TFT_LIGHTGREY);

    textAt("R FACIL   = 30s", 17, 67, 1, TFT_WHITE);
    textAt("R MEDIO   = 45s", 17, 79, 1, TFT_WHITE);
    textAt("R DIFICIL = 60s", 17, 91, 1, TFT_WHITE);
    textAt("R EXTREMO = 90s", 17, 103, 1, TFT_WHITE);
  }

  drawFooterHint("UP/DN PAG  CENTRO=VOLTA");
}

// ============================================================
// TELA - PREPARE-SE
// ============================================================

void renderPrepare() {
  display.fillScreen(TFT_BLACK);

  display.drawRoundRect(8, 8, 144, 112, 10, TFT_CYAN);
  display.drawRoundRect(11, 11, 138, 106, 9, TFT_NAVY);

  centerText("RACHACUCA", 21, 2, TFT_CYAN);

  String line = modeShortName(selectedMode);
  line += "  ";
  line += difficultyName(selectedDifficulty);

  centerText(line, 51, 1, TFT_WHITE);

  // Contagem visual estatica para manter esta tela nao bloqueante.
  display.fillCircle(80, 83, 22, TFT_NAVY);
  display.drawCircle(80, 83, 22, TFT_YELLOW);
  display.drawCircle(80, 83, 18, TFT_DARKCYAN);

  centerText("VAI!", 76, 2, TFT_YELLOW);
  centerText("10 DESAFIOS", 108, 1, TFT_GREEN);
}

// ============================================================
// TELA - QUESTAO
// ============================================================

void drawAnswerChip(
  int16_t x,
  int16_t y,
  int16_t w,
  int16_t h,
  int32_t value,
  uint16_t border
) {
  String text = String(value);

  uint8_t size =
    fitTextSize(
      text,
      w - 6,
      2
    );

  display.fillRoundRect(x, y, w, h, 5, TFT_BLACK);
  display.drawRoundRect(x, y, w, h, 5, border);

  textCenteredInBox(
    text,
    x,
    y,
    w,
    h,
    size,
    TFT_WHITE,
    TFT_BLACK
  );
}

void renderQuestion() {
  display.fillScreen(TFT_BLACK);

  display.fillRect(0, 0, 160, 12, TFT_NAVY);

  String count = String(questionIndex + 1);
  count += "/";
  count += String(TOTAL_QUESTIONS);

  textAt(count, 4, 2, 1, TFT_CYAN, TFT_NAVY);

  String config = modeShortName(selectedMode);
  config += " ";
  config += difficultyName(selectedDifficulty);

  rightText(config, 2, 1, TFT_LIGHTGREY, TFT_NAVY);

  // FACIL preserva o layout grande.
  // MEDIO/DIFICIL/EXTREMO usam uma caixa central mais larga
  // e fonte 1 para garantir que a equacao inteira apareca.
  bool compactLayout =
    selectedDifficulty != DIFF_EASY;

  int16_t sideW =
    compactLayout ? 36 : 40;

  int16_t rightX =
    compactLayout ? 122 : 118;

  int16_t equationX =
    compactLayout ? 40 : 45;

  int16_t equationW =
    compactLayout ? 80 : 70;

  int16_t leftArrowX =
    compactLayout ? 38 : 42;

  int16_t rightArrowX =
    compactLayout ? 119 : 113;

  drawAnswerChip(
    48,
    16,
    64,
    24,
    currentQuestion.choices[POS_UP],
    TFT_YELLOW
  );

  drawAnswerChip(
    2,
    51,
    sideW,
    27,
    currentQuestion.choices[POS_LEFT],
    TFT_CYAN
  );

  drawAnswerChip(
    rightX,
    51,
    sideW,
    27,
    currentQuestion.choices[POS_RIGHT],
    TFT_CYAN
  );

  drawAnswerChip(
    48,
    98,
    64,
    24,
    currentQuestion.choices[POS_DOWN],
    TFT_YELLOW
  );

  String eq =
    equationText(
      currentQuestion
    );

  display.fillRoundRect(
    equationX,
    48,
    equationW,
    34,
    6,
    TFT_NAVY
  );

  display.drawRoundRect(
    equationX,
    48,
    equationW,
    34,
    6,
    TFT_DARKCYAN
  );

  uint8_t eqSize;

  if (selectedDifficulty == DIFF_EASY) {
    eqSize =
      fitTextSize(
        eq,
        equationW - 8,
        2
      );
  } else {
    eqSize = 1;
  }

  textCenteredInBox(
    eq,
    equationX,
    48,
    equationW,
    34,
    eqSize,
    TFT_WHITE,
    TFT_NAVY
  );

  textAt("^", 77, 42, 1, TFT_DARKGREY);
  textAt("<", leftArrowX, 61, 1, TFT_DARKGREY);
  textAt(">", rightArrowX, 61, 1, TFT_DARKGREY);
  textAt("v", 77, 85, 1, TFT_DARKGREY);
}

// ============================================================
// TELA - FEEDBACK
// ============================================================

void renderFeedback() {
  display.fillScreen(TFT_BLACK);

  if (lastAnswerCorrect) {
    display.drawCircle(80, 48, 27, TFT_GREEN);
    display.drawCircle(80, 48, 25, TFT_DARKGREEN);

    display.drawLine(66, 48, 76, 58, TFT_GREEN);
    display.drawLine(76, 58, 96, 36, TFT_GREEN);

    centerText("ACERTOU!", 82, 2, TFT_GREEN);
  } else {
    display.drawCircle(80, 48, 27, TFT_RED);
    display.drawCircle(80, 48, 25, TFT_MAROON);

    display.drawLine(68, 36, 92, 60, TFT_RED);
    display.drawLine(92, 36, 68, 60, TFT_RED);

    centerText("ERROU!", 80, 2, TFT_RED);

    String answer = "RESPOSTA: ";
    answer += String(currentQuestion.result);

    centerText(answer, 101, 1, TFT_YELLOW);
  }

  String time = "TEMPO ";
  time += formatMillis1(lastQuestionMs);

  centerText(time, 116, 1, TFT_LIGHTGREY);
}

// ============================================================
// TELA - RESULTADO
// ============================================================

void renderResult() {
  display.fillScreen(TFT_BLACK);
  drawHeader("RESULTADO FINAL", TFT_YELLOW);

  // Cards laterais
  display.fillRoundRect(8, 27, 68, 36, 6, TFT_DARKGREEN);
  display.fillRoundRect(84, 27, 68, 36, 6, TFT_MAROON);

  textAt("ACERTOS", 19, 31, 1, TFT_WHITE, TFT_DARKGREEN);
  textAt(
    String(correctCount),
    35,
    44,
    2,
    TFT_GREEN,
    TFT_DARKGREEN
  );

  textAt("ERROS", 101, 31, 1, TFT_WHITE, TFT_MAROON);
  textAt(
    String(errorCount),
    111,
    44,
    2,
    TFT_RED,
    TFT_MAROON
  );

  String time = "TEMPO: ";
  time += formatMillis1(totalResponseMs);

  centerText(time, 68, 1, TFT_LIGHTGREY);

  String modeLine = modeShortName(selectedMode);
  modeLine += " ";
  modeLine += factorText(selectedMode);
  modeLine += "  ";
  modeLine += difficultyName(selectedDifficulty);

  centerText(modeLine, 81, 1, TFT_CYAN);

  String points = String(finalScore);
  centerText(points, 92, 3, TFT_YELLOW);
  centerText("PONTOS", 118, 1, TFT_WHITE);
}

// ============================================================
// TELA - NOVO TOP 5 / DECISAO DE SALVAR
// ============================================================

void drawCelebrationConfetti(
  uint8_t frame,
  bool erase
) {
  const int16_t confettiX[12] = {
    8, 22, 38, 55, 72, 89,
    106, 123, 141, 151, 31, 130
  };

  const int16_t confettiY[12] = {
    22, 37, 16, 31, 20, 35,
    15, 30, 21, 40, 48, 49
  };

  const uint16_t confettiColor[4] = {
    TFT_YELLOW,
    TFT_CYAN,
    TFT_GREEN,
    TFT_MAGENTA
  };

  for (uint8_t i = 0; i < 12; i++) {
    int16_t y =
      (
        confettiY[i]
        + frame * 4
        + i * 3
      ) % 55;

    uint16_t color =
      erase
        ? TFT_BLACK
        : confettiColor[
            (i + frame) % 4
          ];

    if (i % 2 == 0) {
      display.fillCircle(
        confettiX[i],
        y,
        2,
        color
      );
    } else {
      display.drawLine(
        confettiX[i] - 2,
        y - 2,
        confettiX[i] + 2,
        y + 2,
        color
      );
    }
  }
}

void updateRankDecisionAnimation() {
  // Apaga SOMENTE os elementos do frame anterior.
  // Nenhum fillScreen() e executado durante a animacao.
  uint8_t previousFrame =
    (uint8_t)(
      celebrationFrame - 1
    );

  drawCelebrationConfetti(
    previousFrame,
    true
  );

  uint8_t previousPulse =
    17 +
    (previousFrame % 4);

  display.drawCircle(
    80,
    39,
    previousPulse,
    TFT_BLACK
  );

  display.drawCircle(
    80,
    39,
    previousPulse + 4,
    TFT_BLACK
  );

  // Desenha o novo frame.
  uint8_t pulse =
    17 +
    (celebrationFrame % 4);

  display.drawCircle(
    80,
    39,
    pulse,
    TFT_YELLOW
  );

  display.drawCircle(
    80,
    39,
    pulse + 4,
    TFT_ORANGE
  );

  drawCelebrationConfetti(
    celebrationFrame,
    false
  );

  // Estes textos ficam na regiao da animacao.
  // Sao redesenhados para permanecerem sempre nitidos,
  // sem limpar o restante da tela.
  centerText(
    "TOP",
    26,
    1,
    TFT_YELLOW
  );

  centerText(
    "5",
    35,
    2,
    TFT_WHITE
  );

  centerText(
    "NOVO TOP 5!",
    59,
    2,
    TFT_CYAN
  );
}

void renderRankDecisionStatic() {
  display.fillScreen(
    TFT_BLACK
  );

  String scoreLine =
    String(finalScore);

  scoreLine +=
    " PONTOS";

  centerText(
    scoreLine,
    79,
    1,
    TFT_WHITE
  );

  centerText(
    "SALVAR SEU NOME?",
    91,
    1,
    TFT_YELLOW
  );

  display.fillRoundRect(
    7,
    103,
    67,
    22,
    5,
    TFT_MAROON
  );

  display.drawRoundRect(
    7,
    103,
    67,
    22,
    5,
    TFT_RED
  );

  textCenteredInBox(
    "< NAO",
    7,
    103,
    67,
    22,
    1,
    TFT_WHITE,
    TFT_MAROON
  );

  display.fillRoundRect(
    86,
    103,
    67,
    22,
    5,
    TFT_DARKGREEN
  );

  display.drawRoundRect(
    86,
    103,
    67,
    22,
    5,
    TFT_GREEN
  );

  textCenteredInBox(
    "SIM >",
    86,
    103,
    67,
    22,
    1,
    TFT_WHITE,
    TFT_DARKGREEN
  );

  updateRankDecisionAnimation();
}

// ============================================================
// TELA - ENTRADA DE NOME
// ============================================================

void renderNameEntry() {
  display.fillScreen(TFT_BLACK);
  drawHeader("VOCE ENTROU NO TOP 5!", TFT_YELLOW);

  centerText("INICIAIS", 27, 1, TFT_CYAN);

  const int16_t boxY = 44;
  const int16_t boxW = 34;
  const int16_t boxH = 38;
  const int16_t startX = 25;

  for (uint8_t i = 0; i < 3; i++) {
    int16_t x = startX + i * 38;

    uint16_t border =
      i == initialPosition
        ? TFT_YELLOW
        : TFT_DARKGREY;

    uint16_t bg =
      i == initialPosition
        ? TFT_NAVY
        : TFT_BLACK;

    display.fillRoundRect(x, boxY, boxW, boxH, 5, bg);
    display.drawRoundRect(x, boxY, boxW, boxH, 5, border);

    String letter = String(initials[i]);

    textCenteredInBox(
      letter,
      x,
      boxY,
      boxW,
      boxH,
      3,
      TFT_WHITE,
      bg
    );
  }

  centerText("UP/DOWN MUDA LETRA", 91, 1, TFT_WHITE);
  centerText("LEFT/RIGHT MUDA POSICAO", 103, 1, TFT_WHITE);
  centerText("CENTRO SALVA", 117, 1, TFT_GREEN);
}

// ============================================================
// TELA - RANK SALVO
// ============================================================

void renderRankSaved() {
  display.fillScreen(TFT_BLACK);

  display.drawCircle(80, 51, 31, TFT_YELLOW);
  display.drawCircle(80, 51, 27, TFT_ORANGE);

  centerText("TOP", 33, 2, TFT_YELLOW);
  centerText("5", 51, 3, TFT_WHITE);

  String player = String(initials[0]);
  player += String(initials[1]);
  player += String(initials[2]);

  centerText(player, 88, 2, TFT_CYAN);
  centerText("RANKING SALVO", 111, 1, TFT_GREEN);
}

// ============================================================
// ENTRADA NOS ESTADOS
// ============================================================

void enterAttract(uint8_t firstPage = 0) {
  attractPage = firstPage;
  openingFrame = 0;
  state = STATE_ATTRACT;

  attractTimer.resetMillis();
  openingAnimTimer.resetMillis();

  renderAttractPage();
}

void enterOperationMenu() {
  selectedMode = MODE_ALL;
  operationMenuIndex = MODE_ALL;

  state = STATE_MENU_OPERATION;
  renderOperationMenu();
}

void enterDifficultyMenu() {
  selectedMode = (OperationMode)operationMenuIndex;

  difficultyMenuIndex = selectedDifficulty;

  state = STATE_MENU_DIFFICULTY;
  renderDifficultyMenu();
}

void enterPreGameMenu() {
  selectedDifficulty = (Difficulty)difficultyMenuIndex;
  preGameMenuIndex = 0;

  state = STATE_PRE_GAME_MENU;
  renderPreGameMenu();
}

void enterRankRules() {
  rankRulePage = 0;
  state = STATE_RANK_RULES;
  renderRankRules();
}

void enterPrepare() {
  state = STATE_PREPARE;
  stateTimer.resetMillis();
  renderPrepare();
}

void enterRankDecision() {
  celebrationFrame = 0;
  celebrationAnimTimer.resetMillis();

  state = STATE_RANK_DECISION;
  renderRankDecisionStatic();
}

// ============================================================
// PARTIDA
// ============================================================

void startQuestion() {
  generateQuestion();
  renderQuestion();

  // O cronometro comeca apenas depois de a questao estar desenhada.
  questionTimer.stopwatchStartMillis();

  state = STATE_QUESTION;
}

void beginGame() {
  questionIndex = 0;
  correctCount = 0;
  errorCount = 0;

  totalResponseMs = 0;
  lastQuestionMs = 0;

  finalScore = 0;
  finalBonus = 0;
  finalCorrectedTimeMs = 0;

  usedQuestionCount = 0;

  for (uint8_t i = 0; i < TOTAL_QUESTIONS; i++) {
    usedQuestionKeys[i] = "";
  }

  prepareOperationDeck();
  startQuestion();
}

void finishGame() {
  finalCorrectedTimeMs =
    correctedTimeMs(totalResponseMs, selectedMode);

  finalBonus =
    calculateSpeedBonus(
      totalResponseMs,
      selectedMode,
      selectedDifficulty
    );

  finalScore =
    calculateScore(
      correctCount,
      totalResponseMs,
      selectedMode,
      selectedDifficulty
    );

  state = STATE_RESULT;
  stateTimer.resetMillis();
  renderResult();
}

void processAnswer(uint8_t answerPosition) {
  // A direcao pressionada ja e a resposta.
  // O cronometro para antes de som, desenho ou animacao.
  lastQuestionMs =
    questionTimer.stopwatchStopMillis();

  totalResponseMs += lastQuestionMs;

  lastAnswerCorrect =
    answerPosition == currentQuestion.correctPosition;

  if (lastAnswerCorrect) {
    correctCount++;
  } else {
    errorCount++;
  }

  state = STATE_FEEDBACK;

  // O intervalo de feedback comeca antes do som.
  // Assim o bip curtissimo fica absorvido dentro do feedback
  // e nao aumenta a pontuacao nem atrasa a proxima questao.
  stateTimer.resetMillis();

  if (lastAnswerCorrect) {
    playCorrectSound();
  } else {
    playErrorSound();
  }

  renderFeedback();
}

void advanceAfterFeedback() {
  questionIndex++;

  if (questionIndex >= TOTAL_QUESTIONS) {
    finishGame();
  } else {
    startQuestion();
  }
}

void continueAfterResult() {
  if (qualifiesForRanking()) {
    enterRankDecision();
  } else {
    state = STATE_POST_RANK;
    stateTimer.resetMillis();

    renderRanking(
      selectedDifficulty,
      true
    );
  }
}

// ============================================================
// SETUP
// ============================================================

void begin() {
  Serial.begin(115200);

  Serial.print("ES32Lab LIB: ");
  Serial.println(ES32LAB_VERSION);

  // Semente pseudoaleatoria usando ruido de duas entradas analogicas onboard.
  uint32_t seed =
    (uint32_t)analogRead(P_POT1)
    ^ ((uint32_t)analogRead(P_POT2) << 12);

  randomSeed(seed);

  display.init();
  display.setRotation(3);
  display.fillScreen(TFT_BLACK);

  // Buzzer onboard.
  // Volume moderado; os efeitos duram somente alguns ms.
  buzzer.begin();
  buzzer.setVolume(35);

  loadRankings();

  inputTimer.resetMillis();

  enterAttract(0);
}

// ============================================================
// LOOP
// ============================================================

void update() {
  bool pollInput = inputTimer.intervalMillis(25);

  switch (state) {

    // --------------------------------------------------------
    // ATTRACT: abertura -> rankings -> abertura...
    // Qualquer tecla entra no jogo.
    // --------------------------------------------------------
    case STATE_ATTRACT:

      if (pollInput && anyKeyPressed()) {
        enterOperationMenu();
        break;
      }

      if (
        attractPage == 0 &&
        openingAnimTimer.intervalMillis(OPENING_ANIM_MS)
      ) {
        openingFrame++;
        updateOpeningAnimation();
      }

      if (
        attractTimer.intervalMillis(
          attractPage == 0
            ? ATTRACT_OPEN_MS
            : ATTRACT_RANK_MS
        )
      ) {
        attractPage = (attractPage + 1) % 5;
        openingFrame = 0;
        openingAnimTimer.resetMillis();
        renderAttractPage();
      }

      break;

    // --------------------------------------------------------
    // OPERACAO
    // --------------------------------------------------------
    case STATE_MENU_OPERATION:

      if (!pollInput) {
        break;
      }

      if (keyboard.press(KEY_UP)) {
        operationMenuIndex = (operationMenuIndex + 4) % 5;
        renderOperationMenu();
      }

      else if (keyboard.press(KEY_DOWN)) {
        operationMenuIndex = (operationMenuIndex + 1) % 5;
        renderOperationMenu();
      }

      else if (keyboard.press(KEY_CENTER)) {
        enterDifficultyMenu();
      }

      else if (keyboard.press(KEY_LEFT)) {
        enterAttract(0);
      }

      break;

    // --------------------------------------------------------
    // DIFICULDADE
    // --------------------------------------------------------
    case STATE_MENU_DIFFICULTY:

      if (!pollInput) {
        break;
      }

      if (keyboard.press(KEY_UP)) {
        difficultyMenuIndex =
          (difficultyMenuIndex + DIFFICULTY_COUNT - 1)
          % DIFFICULTY_COUNT;

        renderDifficultyMenu();
      }

      else if (keyboard.press(KEY_DOWN)) {
        difficultyMenuIndex =
          (difficultyMenuIndex + 1)
          % DIFFICULTY_COUNT;

        renderDifficultyMenu();
      }

      else if (keyboard.press(KEY_CENTER)) {
        enterPreGameMenu();
      }

      else if (keyboard.press(KEY_LEFT)) {
        state = STATE_MENU_OPERATION;
        renderOperationMenu();
      }

      break;

    // --------------------------------------------------------
    // PRE-JOGO
    // --------------------------------------------------------
    case STATE_PRE_GAME_MENU:

      if (!pollInput) {
        break;
      }

      if (
        keyboard.press(KEY_UP) ||
        keyboard.press(KEY_DOWN)
      ) {
        preGameMenuIndex = 1 - preGameMenuIndex;
        renderPreGameMenu();
      }

      else if (keyboard.press(KEY_CENTER)) {
        if (preGameMenuIndex == 0) {
          enterPrepare();
        } else {
          enterRankRules();
        }
      }

      else if (keyboard.press(KEY_LEFT)) {
        state = STATE_MENU_DIFFICULTY;
        renderDifficultyMenu();
      }

      break;

    // --------------------------------------------------------
    // COMO E O RANKING
    // --------------------------------------------------------
    case STATE_RANK_RULES:

      if (!pollInput) {
        break;
      }

      if (keyboard.press(KEY_UP)) {
        rankRulePage =
          (rankRulePage + RANK_RULE_PAGES - 1)
          % RANK_RULE_PAGES;

        renderRankRules();
      }

      else if (keyboard.press(KEY_DOWN)) {
        rankRulePage =
          (rankRulePage + 1)
          % RANK_RULE_PAGES;

        renderRankRules();
      }

      else if (
        keyboard.press(KEY_CENTER) ||
        keyboard.press(KEY_LEFT)
      ) {
        state = STATE_PRE_GAME_MENU;
        renderPreGameMenu();
      }

      break;

    // --------------------------------------------------------
    // PREPARE-SE
    // --------------------------------------------------------
    case STATE_PREPARE:

      if (stateTimer.intervalMillis(PREPARE_MS)) {
        beginGame();
      }

      break;

    // --------------------------------------------------------
    // QUESTAO
    // Centro nao confirma.
    // A tecla direcional e a resposta.
    // --------------------------------------------------------
    case STATE_QUESTION:

      if (!pollInput) {
        break;
      }

      if (keyboard.press(KEY_UP)) {
        processAnswer(POS_UP);
      }

      else if (keyboard.press(KEY_RIGHT)) {
        processAnswer(POS_RIGHT);
      }

      else if (keyboard.press(KEY_DOWN)) {
        processAnswer(POS_DOWN);
      }

      else if (keyboard.press(KEY_LEFT)) {
        processAnswer(POS_LEFT);
      }

      break;

    // --------------------------------------------------------
    // FEEDBACK
    // --------------------------------------------------------
    case STATE_FEEDBACK:

      if (stateTimer.intervalMillis(FEEDBACK_MS)) {
        advanceAfterFeedback();
      }

      break;

    // --------------------------------------------------------
    // RESULTADO
    // Nao exibe algoritmo: ele esta disponivel no pre-jogo.
    // --------------------------------------------------------
    case STATE_RESULT:

      if (
        pollInput &&
        keyboard.press(KEY_CENTER)
      ) {
        continueAfterResult();
        break;
      }

      if (stateTimer.intervalMillis(RESULT_MS)) {
        continueAfterResult();
      }

      break;

    // --------------------------------------------------------
    // NOVO TOP 5 - DECISAO DE SALVAR
    //
    // DIREITA = SIM
    // ESQUERDA = NAO
    // --------------------------------------------------------
    case STATE_RANK_DECISION:

      if (
        celebrationAnimTimer.intervalMillis(
          CELEBRATION_ANIM_MS
        )
      ) {
        celebrationFrame++;
        updateRankDecisionAnimation();
      }

      if (!pollInput) {
        break;
      }

      if (keyboard.press(KEY_RIGHT)) {
        initials[0] = 'A';
        initials[1] = 'A';
        initials[2] = 'A';
        initials[3] = '\0';

        initialPosition = 0;

        state = STATE_NAME_ENTRY;
        renderNameEntry();
      }

      else if (keyboard.press(KEY_LEFT)) {
        state = STATE_POST_RANK;
        stateTimer.resetMillis();

        renderRanking(
          selectedDifficulty,
          true
        );
      }

      break;

    // --------------------------------------------------------
    // NOME ESTILO FLIPERAMA
    // --------------------------------------------------------
    case STATE_NAME_ENTRY:

      if (!pollInput) {
        break;
      }

      if (keyboard.press(KEY_UP)) {
        initials[initialPosition]++;

        if (initials[initialPosition] > 'Z') {
          initials[initialPosition] = 'A';
        }

        renderNameEntry();
      }

      else if (keyboard.press(KEY_DOWN)) {
        if (initials[initialPosition] <= 'A') {
          initials[initialPosition] = 'Z';
        } else {
          initials[initialPosition]--;
        }

        renderNameEntry();
      }

      else if (keyboard.press(KEY_RIGHT)) {
        initialPosition = (initialPosition + 1) % 3;
        renderNameEntry();
      }

      else if (keyboard.press(KEY_LEFT)) {
        initialPosition = (initialPosition + 2) % 3;
        renderNameEntry();
      }

      else if (keyboard.press(KEY_CENTER)) {
        insertCurrentScoreInRanking();

        state = STATE_RANK_SAVED;
        stateTimer.resetMillis();

        renderRankSaved();
      }

      break;

    // --------------------------------------------------------
    // RANK SALVO
    // --------------------------------------------------------
    case STATE_RANK_SAVED:

      if (stateTimer.intervalMillis(RANK_SAVED_MS)) {
        state = STATE_POST_RANK;
        stateTimer.resetMillis();

        renderRanking(selectedDifficulty, true);
      }

      break;

    // --------------------------------------------------------
    // RANKING POS-JOGO
    // --------------------------------------------------------
    case STATE_POST_RANK:

      if (pollInput && anyKeyPressed()) {
        enterOperationMenu();
        break;
      }

      if (stateTimer.intervalMillis(POST_RANK_MS)) {
        enterAttract(0);
      }

      break;
  }
}

}  // namespace RachacucaModule

const Arcade::GameDefinition RachacucaGame = {
  "RACHACUCA",
  "Desafio matematico arcade",
  RachacucaModule::begin,
  RachacucaModule::update
};

}  // namespace ArcadeGames
