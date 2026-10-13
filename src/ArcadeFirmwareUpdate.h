#pragma once

#include <Arduino.h>

class ArcadeFirmwareUpdate {
public:
  enum class State : uint8_t {
    IDLE,
    CHECKING,
    CURRENT,
    AVAILABLE,
    ERROR,
    INSTALLING
  };

  using ProgressCallback =
    void (*)(uint8_t percent, void* context);

  void begin();

  void startAutomaticCheck();
  void startManualCheck();
  void cancelCheck();

  bool automaticCheckStarted() const;
  bool automaticCheckCompleted() const;
  bool isChecking() const;
  bool updateAvailable() const;

  State state() const;
  String availableVersion() const;
  String errorMessage() const;
  bool hasExpectedSha256() const;

  bool install(
    ProgressCallback progressCallback,
    void* context
  );

private:
  static void checkTaskEntry(void* parameter);
  void startCheck(
    bool automatic,
    uint32_t timeoutMs
  );
  void performCheck();

  static int versionPart(
    const String& version,
    uint8_t part
  );
  static int compareVersions(
    const String& installed,
    const String& available
  );

  static bool isValidSha256(
    const String& value
  );
  static String sha256ToHex(
    const uint8_t digest[32]
  );

  volatile State currentState = State::IDLE;
  volatile bool automaticStarted = false;
  volatile bool automaticCompleted = false;
  volatile bool cancelRequested = false;

  bool checkIsAutomatic = false;
  uint32_t checkTimeoutMs = 15000UL;
  TaskHandle_t checkTaskHandle = nullptr;

  String remoteVersion;
  String firmwareUrl;
  String expectedSha256;
  String message;
};
