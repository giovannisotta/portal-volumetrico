#include "config.h"
#include "tof.h"
#include "motor.h"
#include "calc.h"
#include "acquire.h"
#include <Wire.h>

Motor* motor;
Calib calib = { MM_PER_STEP, D_MM, 0, 0, 0, DS_MIN_MM };

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(1);

  initAllTof();
  motor = initMotor();

  if (!measureBaseline(calib)) {
    Serial.println("Erro: sem leitura valida para o baseline");
    while (true) delay(1000);
  }
  Serial.printf("Baseline: left=%.1f top=%.1f right=%.1f\n",
                calib.base_left, calib.base_top, calib.base_right);
  
}

void loop() {
  
  Serial.println("Esperando caixa...");
  Serial.print("TESTE\n");
  BoxResult r = acquireBox(motor, calib);

  if (r.status != CALC_OK) {
    Serial.printf("Erro: status=%d\n", r.status);
    return;
  }
  Serial.printf("length=%.1f width=%.1f height=%.1f theta=%.1f graus\n",
                r.length, r.width, r.height, r.theta * 180.0f / PI);

  /*Serial.print("LEFT: ");
  printDist(readTof(TOF_LEFT));
  Serial.print("TOP: ");
  printDist(readTof(TOF_TOP));
  Serial.print("RIGHT: ");
  printDist(readTof(TOF_RIGHT));
  Serial.print("\n");
  revolMotor(motor);*/

  while (true);
}
