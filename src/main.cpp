#include <Arduino.h>
#include <ES32Lab.h>

#include "ArcadeController.h"

ArcadeController arcade;

void setup() {
  arcade.begin();
}

void loop() {
  arcade.update();
}
