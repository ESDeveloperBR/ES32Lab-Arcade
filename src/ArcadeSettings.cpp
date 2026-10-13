#include "ArcadeSettings.h"

#include "ArcadeVersion.h"

namespace {

constexpr uint16_t C_BG      = 0x0008;
constexpr uint16_t C_PANEL   = 0x0862;
constexpr uint16_t C_DARK    = 0x0021;
constexpr uint16_t C_BLUE    = 0x025F;
constexpr uint16_t C_CYAN    = 0x07FF;
constexpr uint16_t C_MAGENTA = 0xF81F;
constexpr uint16_t C_GOLD    = 0xFEA0;
constexpr uint16_t C_WHITE   = 0xDEFB;
constexpr uint16_t C_MUTED   = 0x6B4D;

const char* PASSWORD_GROUPS[] = {
  "abcdefghijklmnopqrstuvwxyz",
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
  "0123456789",
  "!@#$%&*()-_=+.,;:?/\\[]{}"
};

constexpr uint8_t PASSWORD_GROUP_COUNT = 4;

String fitRawText(
  const String& text,
  uint8_t maxChars
) {
  if (text.length() <= maxChars) {
    return text;
  }

  if (maxChars <= 1) {
    return text.substring(0, maxChars);
  }

  return
    text.substring(0, maxChars - 1) +
    ".";
}

}  // namespace

void ArcadeSettings::begin(
  ArcadeWiFi& wifiRef,
  ArcadeFirmwareUpdate& updaterRef
) {
  commonBegin(wifiRef, updaterRef);

  view = View::ROOT;
  rootIndex = 0;

  renderRoot();
}

void ArcadeSettings::beginStartupProvisioning(
  ArcadeWiFi& wifiRef,
  ArcadeFirmwareUpdate& updaterRef
) {
  commonBegin(wifiRef, updaterRef);

  startupProvisioning = true;
  view = View::WIFI_SCAN;
  scanIndex = 0;

  wifi->startScan();
  lastScanReady = wifi->scanReady();

  renderWifiScan();
}

void ArcadeSettings::beginUpdatePrompt(
  ArcadeWiFi& wifiRef,
  ArcadeFirmwareUpdate& updaterRef
) {
  commonBegin(wifiRef, updaterRef);

  autoUpdatePrompt = true;
  view = View::UPDATE;
  lastUpdateState = updater->state();

  renderUpdate();
}

void ArcadeSettings::commonBegin(
  ArcadeWiFi& wifiRef,
  ArcadeFirmwareUpdate& updaterRef
) {
  wifi = &wifiRef;
  updater = &updaterRef;

  display.init();
  display.setRotation(3);
  display.setTextSize(1);

  startupProvisioning = false;
  autoUpdatePrompt = false;
  waitingCenterRelease = true;

  centerTracking = false;
  centerLongFired = false;

  startupSuccessActive = false;

  lastWifiConnected = wifi->isConnected();
  lastWifiConnecting = wifi->isConnecting();
  lastScanReady = wifi->scanReady();
  lastUpdateState = updater->state();
}

bool ArcadeSettings::update() {
  if (wifi == nullptr || updater == nullptr) {
    return true;
  }

  if (startupSuccessActive) {
    if (
      startupSuccessTimer.intervalMillis(
        1200UL
      )
    ) {
      startupSuccessActive = false;
      startupProvisioning = false;
      return true;
    }

    return false;
  }

  if (
    startupProvisioning &&
    wifi->isConnected()
  ) {
    startupSuccessActive = true;
    renderWifiConnected();
    startupSuccessTimer.resetMillis();
    return false;
  }

  if (waitingCenterRelease) {
    if (!keyboard.hold(KEY_CENTER)) {
      waitingCenterRelease = false;
    }

    return false;
  }

  if (
    view == View::WIFI_SCAN &&
    wifi->scanReady() != lastScanReady
  ) {
    lastScanReady = wifi->scanReady();
    renderWifiScan();
  }

  const bool connected =
    wifi->isConnected();

  const bool connecting =
    wifi->isConnecting();

  if (
    view == View::WIFI &&
    (
      connected != lastWifiConnected ||
      connecting != lastWifiConnecting
    )
  ) {
    lastWifiConnected = connected;
    lastWifiConnecting = connecting;
    renderWifi();
  } else {
    lastWifiConnected = connected;
    lastWifiConnecting = connecting;
  }

  if (
    view == View::UPDATE &&
    updater->state() != lastUpdateState
  ) {
    lastUpdateState = updater->state();
    renderUpdate();
  }

  const CenterEvent center =
    readCenterEvent();

  switch (view) {
    case View::ROOT:
      return handleRoot(center);

    case View::WIFI:
      return handleWifi(center);

    case View::WIFI_SCAN:
      return handleWifiScan(center);

    case View::WIFI_DELETE:
      return handleWifiDelete(center);

    case View::PASSWORD:
      return handlePassword(center);

    case View::UPDATE:
      return handleUpdate(center);
  }

  return false;
}

ArcadeSettings::CenterEvent
ArcadeSettings::readCenterEvent() {
  if (keyboard.press(KEY_CENTER)) {
    centerTracking = true;
    centerLongFired = false;
    centerHoldTimer.resetMillis();
  }

  if (
    centerTracking &&
    !centerLongFired &&
    keyboard.hold(KEY_CENTER) &&
    centerHoldTimer.intervalMillis(
      1400UL
    )
  ) {
    centerLongFired = true;
    return CenterEvent::LONG;
  }

  if (
    centerTracking &&
    keyboard.release(KEY_CENTER)
  ) {
    centerTracking = false;

    if (!centerLongFired) {
      return CenterEvent::SHORT;
    }
  }

  return CenterEvent::NONE;
}

void ArcadeSettings::drawBackground() {
  display.fillScreen(C_BG);

  display.drawLine(
    0, 19, 159, 19, C_BLUE
  );

  display.drawLine(
    0, 20, 159, 20, C_MAGENTA
  );
}

void ArcadeSettings::drawHeader(
  const char* title
) {
  drawBackground();

  display.setTextColor(
    C_CYAN,
    C_BG
  );

  display.drawString(
    "ES32Lab ARCADE",
    4,
    3,
    1
  );

  display.setTextColor(
    C_GOLD,
    C_BG
  );

  display.drawString(
    ArcadeVersion::DISPLAY_TEXT,
    121,
    3,
    1
  );

  display.setTextColor(
    C_WHITE,
    C_BG
  );

  display.drawCentreScreenString(
    title,
    11,
    1
  );
}

void ArcadeSettings::drawFooter(
  const char* text
) {
  display.fillRect(
    0, 112, 160, 16, C_DARK
  );

  display.drawLine(
    0, 112, 159, 112, C_BLUE
  );

  display.setTextColor(
    C_MUTED,
    C_DARK
  );

  display.drawCentreScreenString(
    text,
    117,
    1
  );
}

void ArcadeSettings::drawRow(
  int16_t y,
  const String& left,
  const String& right,
  bool selected
) {
  const uint16_t background =
    selected ? C_BLUE : C_DARK;

  const uint16_t border =
    selected ? C_CYAN : C_PANEL;

  display.fillRect(
    5, y, 150, 16, background
  );

  display.drawRect(
    5, y, 150, 16, border
  );

  if (selected) {
    display.fillRect(
      5, y, 3, 16, C_MAGENTA
    );
  }

  display.setTextColor(
    selected ? C_WHITE : C_MUTED,
    background
  );

  display.drawString(
    fitRawText(left, 18),
    13,
    y + 4,
    1
  );

  if (right.length() > 0) {
    display.setTextColor(
      selected ? C_GOLD : C_MUTED,
      background
    );

    const int16_t x =
      151 - right.length() * 6;

    display.drawString(
      right,
      x,
      y + 4,
      1
    );
  }
}

void ArcadeSettings::renderRoot() {
  drawHeader("CONFIGURACOES");

  const char* items[] = {
    "WI-FI",
    "ATUALIZACAO",
    "VOLTAR"
  };

  for (uint8_t i = 0; i < 3; ++i) {
    drawRow(
      31 + i * 22,
      items[i],
      "",
      i == rootIndex
    );
  }

  drawFooter(
    "CIMA/BAIXO  CENTER:ENTRA"
  );
}

void ArcadeSettings::renderWifi() {
  drawHeader("WI-FI");

  String status;

  if (wifi->isConnected()) {
    status =
      "OK: " +
      wifi->connectedSsid();
  } else if (wifi->isConnecting()) {
    status = "CONECTANDO...";
  } else {
    status = "DESCONECTADO";
  }

  display.setTextColor(
    wifi->isConnected()
      ? TFT_GREEN
      : TFT_YELLOW,
    C_BG
  );

  display.drawString(
    fitRawText(status, 25),
    4,
    24,
    1
  );

  const size_t total =
    wifi->savedCount() + 2;

  if (wifiIndex >= total) {
    wifiIndex = 0;
  }

  size_t first = 0;

  if (wifiIndex >= VISIBLE_ROWS) {
    first =
      wifiIndex - VISIBLE_ROWS + 1;
  }

  for (
    size_t row = 0;
    row < VISIBLE_ROWS;
    ++row
  ) {
    const size_t index =
      first + row;

    if (index >= total) {
      break;
    }

    String text;
    String right;

    if (index < wifi->savedCount()) {
      const auto credential =
        wifi->savedAt(index);

      text = credential.ssid;

      if (
        wifi->isConnected() &&
        wifi->connectedSsid() ==
          credential.ssid
      ) {
        right = "*";
      }
    } else if (
      index == wifi->savedCount()
    ) {
      text = "BUSCAR NOVA REDE";
    } else {
      text = "VOLTAR";
    }

    drawRow(
      37 + row * 18,
      text,
      right,
      index == wifiIndex
    );
  }

  drawFooter(
    "OK:CONECTA  DIR:APAGA"
  );
}

void ArcadeSettings::renderWifiScan() {
  drawHeader("BUSCAR WI-FI");

  if (!wifi->scanReady()) {
    display.setTextColor(
      C_GOLD,
      C_BG
    );

    display.drawCentreScreenString(
      "BUSCANDO REDES...",
      51,
      1
    );

    display.setTextColor(
      C_MUTED,
      C_BG
    );

    display.drawCentreScreenString(
      "AGUARDE",
      68,
      1
    );

    drawFooter(
      startupProvisioning
        ? "ESQ: JOGAR OFFLINE"
        : "ESQ: VOLTAR"
    );

    return;
  }

  const size_t count =
    wifi->scanCount();

  if (count == 0) {
    display.setTextColor(
      C_GOLD,
      C_BG
    );

    display.drawCentreScreenString(
      "NENHUMA REDE ENCONTRADA",
      50,
      1
    );

    display.setTextColor(
      C_MUTED,
      C_BG
    );

    display.drawCentreScreenString(
      "CENTER: BUSCAR NOVAMENTE",
      69,
      1
    );

    drawFooter(
      startupProvisioning
        ? "ESQ: JOGAR OFFLINE"
        : "ESQ: VOLTAR"
    );

    return;
  }

  if (scanIndex >= count) {
    scanIndex = 0;
  }

  size_t first = 0;

  if (scanIndex >= VISIBLE_ROWS) {
    first =
      scanIndex - VISIBLE_ROWS + 1;
  }

  for (
    size_t row = 0;
    row < VISIBLE_ROWS;
    ++row
  ) {
    const size_t index =
      first + row;

    if (index >= count) {
      break;
    }

    const auto network =
      wifi->scanAt(index);

    String right =
      String(network.rssi);

    if (network.secure) {
      right += "*";
    }

    drawRow(
      30 + row * 20,
      network.ssid,
      right,
      index == scanIndex
    );
  }

  drawFooter(
    startupProvisioning
      ? "CENTER:SELEC  ESQ:OFFLINE"
      : "CENTER:SELEC  ESQ:VOLTA"
  );
}

void ArcadeSettings::renderWifiDelete() {
  drawHeader("APAGAR REDE");

  const auto credential =
    wifi->savedAt(deleteIndex);

  display.fillRect(
    12, 37, 136, 49, C_DARK
  );

  display.drawRect(
    12, 37, 136, 49, C_MAGENTA
  );

  display.setTextColor(
    C_WHITE,
    C_DARK
  );

  display.drawCentreScreenString(
    "REMOVER ESTA REDE?",
    45,
    1
  );

  display.setTextColor(
    C_GOLD,
    C_DARK
  );

  display.drawCentreScreenString(
    fitRawText(
      credential.ssid,
      22
    ),
    62,
    1
  );

  drawFooter(
    "CENTER:APAGA  ESQ:CANCELA"
  );
}

void ArcadeSettings::renderPassword() {
  drawHeader("SENHA WI-FI");

  display.setTextColor(
    C_WHITE,
    C_BG
  );

  display.drawString(
    "SSID:",
    4,
    27,
    1
  );

  display.drawString(
    fitRawText(
      passwordSsid,
      19
    ),
    38,
    27,
    1
  );

  display.drawString(
    "SENHA:",
    4,
    45,
    1
  );

  resetPasswordCursor();
  drawPasswordEntryLine();

  display.setTextColor(
    C_GOLD,
    C_BG
  );

  const String pendingText =
    passwordHasPendingChar
      ? String(selectedPasswordChar())
      : " ";

  display.drawCentreScreenString(
    "GRUPO " +
      passwordGroupName() +
      " [" +
      pendingText +
      "]",
    70,
    1
  );

  display.setTextColor(
    C_MUTED,
    C_BG
  );

  display.drawCentreScreenString(
    "CIMA/BAIXO MUDA",
    84,
    1
  );

  display.drawCentreScreenString(
    "DIR CONFIRMA | ESQ APAGA",
    97,
    1
  );

  drawFooter(
    "CENTER:GRUPO  SEGURE:SALVA"
  );
}

void ArcadeSettings::drawPasswordEntryLine() {
  constexpr int FIELD_X = 43;
  constexpr int FIELD_Y = 40;
  constexpr int FIELD_W = 113;
  constexpr int CHAR_W = 6;
  constexpr uint8_t MAX_VISIBLE = 18;

  display.fillRoundRect(
    FIELD_X - 2,
    FIELD_Y,
    FIELD_W + 2,
    18,
    2,
    C_PANEL
  );

  display.drawRoundRect(
    FIELD_X - 2,
    FIELD_Y,
    FIELD_W + 2,
    18,
    2,
    C_CYAN
  );

  String committed;

  if (
    passwordValue.length() <=
    MAX_VISIBLE
  ) {
    committed = passwordValue;
  } else {
    committed =
      "<" +
      passwordValue.substring(
        passwordValue.length() -
        (MAX_VISIBLE - 1)
      );
  }

  display.setTextColor(
    C_WHITE,
    C_PANEL
  );

  display.drawString(
    committed,
    FIELD_X,
    43,
    1
  );

  int cursorX =
    FIELD_X +
    committed.length() * CHAR_W;

  if (
    cursorX >
    FIELD_X + FIELD_W - CHAR_W
  ) {
    cursorX =
      FIELD_X + FIELD_W - CHAR_W;
  }

  if (passwordHasPendingChar) {
    display.setTextColor(
      C_GOLD,
      C_PANEL
    );

    display.drawString(
      String(
        selectedPasswordChar()
      ),
      cursorX,
      43,
      1
    );
  }

  if (passwordCursorVisible) {
    display.drawFastHLine(
      cursorX,
      53,
      5,
      C_GOLD
    );
  }
}

void ArcadeSettings::renderUpdate() {
  drawHeader("ATUALIZACAO");

  display.setTextColor(
    C_WHITE,
    C_BG
  );

  display.drawString(
    "INSTALADA:",
    9,
    31,
    1
  );

  display.drawString(
    ArcadeVersion::TEXT,
    85,
    31,
    1
  );

  display.drawString(
    "DISPONIVEL:",
    9,
    48,
    1
  );

  const String available =
    updater->availableVersion()
      .length()
      ? updater->availableVersion()
      : "--";

  display.drawString(
    fitRawText(
      available,
      11
    ),
    85,
    48,
    1
  );

  switch (updater->state()) {
    case ArcadeFirmwareUpdate::State::CHECKING:
      display.setTextColor(
        C_CYAN,
        C_BG
      );

      display.drawCentreScreenString(
        "CONSULTANDO MANIFESTO...",
        70,
        1
      );

      drawFooter(
        "AGUARDE  ESQ:VOLTAR"
      );
      break;

    case ArcadeFirmwareUpdate::State::AVAILABLE:
      display.setTextColor(
        C_GOLD,
        C_BG
      );

      display.drawCentreScreenString(
        "ATUALIZACAO DISPONIVEL",
        67,
        1
      );

      display.setTextColor(
        updater->hasExpectedSha256()
          ? TFT_GREEN
          : TFT_ORANGE,
        C_BG
      );

      display.drawCentreScreenString(
        updater->hasExpectedSha256()
          ? "SHA-256 INFORMADO"
          : "SEM SHA-256",
        84,
        1
      );

      drawFooter(
        "CENTER:ATUALIZA  ESQ:VOLTA"
      );
      break;

    case ArcadeFirmwareUpdate::State::CURRENT:
      display.setTextColor(
        TFT_GREEN,
        C_BG
      );

      display.drawCentreScreenString(
        "SISTEMA ATUALIZADO",
        72,
        1
      );

      drawFooter(
        "CENTER:VERIFICA  ESQ:VOLTA"
      );
      break;

    case ArcadeFirmwareUpdate::State::ERROR:
      display.setTextColor(
        TFT_RED,
        C_BG
      );

      display.drawCentreScreenString(
        fitRawText(
          updater->errorMessage(),
          25
        ),
        68,
        1
      );

      drawFooter(
        "CENTER:TENTA  ESQ:VOLTAR"
      );
      break;

    case ArcadeFirmwareUpdate::State::INSTALLING:
      display.setTextColor(
        C_GOLD,
        C_BG
      );

      display.drawCentreScreenString(
        "ATUALIZANDO...",
        72,
        1
      );

      drawFooter(
        "NAO DESLIGUE A PLACA"
      );
      break;

    case ArcadeFirmwareUpdate::State::IDLE:
    default:
      display.setTextColor(
        C_CYAN,
        C_BG
      );

      display.drawCentreScreenString(
        "VERIFICAR NOVA VERSAO",
        72,
        1
      );

      drawFooter(
        "CENTER:VERIFICA  ESQ:VOLTA"
      );
      break;
  }
}

void ArcadeSettings::renderUpdateProgress(
  uint8_t percent
) {
  drawHeader("ATUALIZACAO");

  display.setTextColor(
    C_GOLD,
    C_BG
  );

  display.drawCentreScreenString(
    "INSTALANDO FIRMWARE",
    39,
    1
  );

  display.drawRect(
    17,
    61,
    126,
    14,
    C_CYAN
  );

  display.fillRect(
    19,
    63,
    122,
    10,
    C_DARK
  );

  const int16_t width =
    static_cast<int16_t>(
      (122UL * percent) / 100UL
    );

  if (width > 0) {
    display.fillRect(
      19,
      63,
      width,
      10,
      C_MAGENTA
    );
  }

  display.setTextColor(
    C_WHITE,
    C_BG
  );

  display.drawCentreScreenString(
    String(percent) + "%",
    82,
    1
  );

  drawFooter(
    "NAO DESLIGUE A PLACA"
  );
}

void ArcadeSettings::renderWifiConnected() {
  drawHeader("WI-FI");

  display.fillRect(
    15,
    40,
    130,
    45,
    C_DARK
  );

  display.drawRect(
    15,
    40,
    130,
    45,
    TFT_GREEN
  );

  display.setTextColor(
    TFT_GREEN,
    C_DARK
  );

  display.drawCentreScreenString(
    "WI-FI CONECTADO",
    48,
    1
  );

  display.setTextColor(
    C_WHITE,
    C_DARK
  );

  display.drawCentreScreenString(
    fitRawText(
      wifi->connectedSsid(),
      22
    ),
    64,
    1
  );

  drawFooter(
    "ENTRANDO NO ARCADE..."
  );
}

bool ArcadeSettings::handleRoot(
  CenterEvent center
) {
  constexpr size_t COUNT = 3;

  if (keyboard.press(KEY_UP)) {
    rootIndex =
      rootIndex == 0
        ? COUNT - 1
        : rootIndex - 1;

    renderRoot();
  }

  if (keyboard.press(KEY_DOWN)) {
    rootIndex =
      (rootIndex + 1) % COUNT;

    renderRoot();
  }

  if (keyboard.press(KEY_LEFT)) {
    return true;
  }

  if (center != CenterEvent::SHORT) {
    return false;
  }

  if (rootIndex == 0) {
    wifiIndex = 0;
    view = View::WIFI;
    renderWifi();
    return false;
  }

  if (rootIndex == 1) {
    view = View::UPDATE;
    autoUpdatePrompt = false;

    if (
      updater->state() !=
        ArcadeFirmwareUpdate::State::AVAILABLE
    ) {
      updater->startManualCheck();
    }

    lastUpdateState =
      updater->state();

    renderUpdate();
    return false;
  }

  return true;
}

bool ArcadeSettings::handleWifi(
  CenterEvent center
) {
  const size_t total =
    wifi->savedCount() + 2;

  if (keyboard.press(KEY_UP)) {
    wifiIndex =
      wifiIndex == 0
        ? total - 1
        : wifiIndex - 1;

    renderWifi();
  }

  if (keyboard.press(KEY_DOWN)) {
    wifiIndex =
      (wifiIndex + 1) % total;

    renderWifi();
  }

  if (keyboard.press(KEY_LEFT)) {
    view = View::ROOT;
    renderRoot();
    return false;
  }

  if (
    keyboard.press(KEY_RIGHT) &&
    wifiIndex < wifi->savedCount()
  ) {
    deleteIndex = wifiIndex;
    view = View::WIFI_DELETE;
    renderWifiDelete();
    return false;
  }

  if (center != CenterEvent::SHORT) {
    return false;
  }

  if (wifiIndex < wifi->savedCount()) {
    wifi->connectSaved(wifiIndex);
    renderWifi();
    return false;
  }

  if (wifiIndex == wifi->savedCount()) {
    scanIndex = 0;
    wifi->startScan();
    lastScanReady = wifi->scanReady();
    view = View::WIFI_SCAN;
    renderWifiScan();
    return false;
  }

  view = View::ROOT;
  renderRoot();
  return false;
}

bool ArcadeSettings::handleWifiScan(
  CenterEvent center
) {
  if (keyboard.press(KEY_LEFT)) {
    wifi->cancelScan();

    if (startupProvisioning) {
      startupProvisioning = false;
      return true;
    }

    view = View::WIFI;
    renderWifi();
    return false;
  }

  if (!wifi->scanReady()) {
    return false;
  }

  const size_t count =
    wifi->scanCount();

  if (count == 0) {
    if (center == CenterEvent::SHORT) {
      wifi->startScan();
      lastScanReady = false;
      renderWifiScan();
    }

    return false;
  }

  if (keyboard.press(KEY_UP)) {
    scanIndex =
      scanIndex == 0
        ? count - 1
        : scanIndex - 1;

    renderWifiScan();
  }

  if (keyboard.press(KEY_DOWN)) {
    scanIndex =
      (scanIndex + 1) % count;

    renderWifiScan();
  }

  if (
    center != CenterEvent::SHORT ||
    scanIndex >= count
  ) {
    return false;
  }

  const auto network =
    wifi->scanAt(scanIndex);

  passwordSsid =
    network.ssid;

  passwordSecure =
    network.secure;

  passwordValue = "";
  passwordGroup = 0;
  passwordCharIndex = 0;
  passwordHasPendingChar = false;

  if (!passwordSecure) {
    wifi->connectAndSave(
      passwordSsid,
      ""
    );

    if (!startupProvisioning) {
      wifiIndex = 0;
      view = View::WIFI;
      renderWifi();
    }

    return false;
  }

  view = View::PASSWORD;
  renderPassword();
  return false;
}

bool ArcadeSettings::handleWifiDelete(
  CenterEvent center
) {
  if (keyboard.press(KEY_LEFT)) {
    view = View::WIFI;
    renderWifi();
    return false;
  }

  if (center == CenterEvent::SHORT) {
    wifi->deleteSaved(deleteIndex);

    if (
      wifiIndex >=
      wifi->savedCount() + 2
    ) {
      wifiIndex = 0;
    }

    view = View::WIFI;
    renderWifi();
  }

  return false;
}

bool ArcadeSettings::handlePassword(
  CenterEvent center
) {
  servicePasswordCharacterInput();

  if (keyboard.press(KEY_RIGHT)) {
    if (
      passwordHasPendingChar &&
      passwordValue.length() < 63
    ) {
      passwordValue +=
        selectedPasswordChar();

      passwordHasPendingChar = false;
      passwordCharIndex = 0;
      resetPasswordCursor();
      renderPassword();
    }
  }

  if (keyboard.press(KEY_LEFT)) {
    if (passwordHasPendingChar) {
      passwordHasPendingChar = false;
      passwordCharIndex = 0;
      resetPasswordCursor();
      renderPassword();
      return false;
    }

    if (passwordValue.length() > 0) {
      passwordValue.remove(
        passwordValue.length() - 1
      );

      renderPassword();
      return false;
    }

    view = View::WIFI_SCAN;
    renderWifiScan();
    return false;
  }

  if (center == CenterEvent::SHORT) {
    passwordGroup =
      (
        passwordGroup + 1
      ) %
      PASSWORD_GROUP_COUNT;

    passwordCharIndex = 0;
    resetPasswordCursor();
    renderPassword();
  }

  if (center == CenterEvent::LONG) {
    String finalPassword =
      passwordValue;

    if (
      passwordHasPendingChar &&
      finalPassword.length() < 63
    ) {
      finalPassword +=
        selectedPasswordChar();
    }

    passwordValue =
      finalPassword;

    passwordHasPendingChar = false;

    wifi->connectAndSave(
      passwordSsid,
      finalPassword
    );

    if (!startupProvisioning) {
      wifiIndex = 0;
      view = View::WIFI;
      renderWifi();
    }
  }

  if (
    passwordCursorTimer.intervalMillis(
      450UL
    )
  ) {
    passwordCursorVisible =
      !passwordCursorVisible;

    drawPasswordEntryLine();
  }

  return false;
}

bool ArcadeSettings::handleUpdate(
  CenterEvent center
) {
  if (keyboard.press(KEY_LEFT)) {
    if (
      updater->state() ==
      ArcadeFirmwareUpdate::State::INSTALLING
    ) {
      return false;
    }

    updater->cancelCheck();

    if (autoUpdatePrompt) {
      autoUpdatePrompt = false;
      return true;
    }

    view = View::ROOT;
    renderRoot();
    return false;
  }

  if (center != CenterEvent::SHORT) {
    return false;
  }

  if (
    updater->state() ==
    ArcadeFirmwareUpdate::State::AVAILABLE
  ) {
    updater->install(
      updateProgressBridge,
      this
    );

    lastUpdateState =
      updater->state();

    renderUpdate();
    return false;
  }

  if (
    updater->state() !=
    ArcadeFirmwareUpdate::State::INSTALLING
  ) {
    updater->startManualCheck();
    lastUpdateState =
      updater->state();
    renderUpdate();
  }

  return false;
}

void ArcadeSettings::servicePasswordCharacterInput() {
  const bool upPress =
    keyboard.press(KEY_UP);

  const bool downPress =
    keyboard.press(KEY_DOWN);

  if (upPress) {
    changePasswordChar(+1);
    resetPasswordCursor();

    passwordHoldDelay.resetMillis();
    passwordRepeatTimer.resetMillis();

    passwordRepeatActive = false;
    passwordRepeatCount = 0;

    renderPassword();
  }

  if (downPress) {
    changePasswordChar(-1);
    resetPasswordCursor();

    passwordHoldDelay.resetMillis();
    passwordRepeatTimer.resetMillis();

    passwordRepeatActive = false;
    passwordRepeatCount = 0;

    renderPassword();
  }

  int direction = 0;

  if (keyboard.hold(KEY_UP)) {
    direction = +1;
  } else if (keyboard.hold(KEY_DOWN)) {
    direction = -1;
  }

  if (direction == 0) {
    passwordRepeatActive = false;
    passwordRepeatCount = 0;
    return;
  }

  if (!passwordRepeatActive) {
    if (
      passwordHoldDelay.intervalMillis(
        500UL
      )
    ) {
      passwordRepeatActive = true;
      passwordRepeatTimer.resetMillis();
    }

    return;
  }

  uint32_t repeatMs = 180UL;

  if (passwordRepeatCount >= 8) {
    repeatMs = 90UL;
  }

  if (passwordRepeatCount >= 20) {
    repeatMs = 45UL;
  }

  if (
    passwordRepeatTimer.intervalMillis(
      repeatMs
    )
  ) {
    changePasswordChar(direction);
    resetPasswordCursor();
    passwordRepeatCount++;
    drawPasswordEntryLine();
  }
}

void ArcadeSettings::changePasswordChar(
  int delta
) {
  const char* group =
    PASSWORD_GROUPS[passwordGroup];

  const int length =
    strlen(group);

  if (length <= 0) {
    return;
  }

  if (!passwordHasPendingChar) {
    passwordCharIndex =
      delta < 0
        ? length - 1
        : 0;

    passwordHasPendingChar = true;
    return;
  }

  int position =
    passwordCharIndex + delta;

  if (position < 0) {
    position = length - 1;
  }

  if (position >= length) {
    position = 0;
  }

  passwordCharIndex =
    static_cast<uint8_t>(position);
}

char ArcadeSettings::selectedPasswordChar() {
  const char* group =
    PASSWORD_GROUPS[passwordGroup];

  const size_t length =
    strlen(group);

  if (length == 0) {
    return 'a';
  }

  if (passwordCharIndex >= length) {
    passwordCharIndex = 0;
  }

  return group[passwordCharIndex];
}

String ArcadeSettings::passwordGroupName() const {
  if (passwordGroup == 0) {
    return "abc";
  }

  if (passwordGroup == 1) {
    return "ABC";
  }

  if (passwordGroup == 2) {
    return "123";
  }

  return "#!?";
}

void ArcadeSettings::resetPasswordCursor() {
  passwordCursorVisible = true;
  passwordCursorTimer.resetMillis();
}

void ArcadeSettings::updateProgressBridge(
  uint8_t percent,
  void* context
) {
  ArcadeSettings* self =
    static_cast<ArcadeSettings*>(
      context
    );

  if (self != nullptr) {
    self->renderUpdateProgress(
      percent
    );
  }
}
