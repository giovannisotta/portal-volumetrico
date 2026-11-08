#include "config.h"
#include "tof.h"
#include "motor.h"
#include "calc.h"
#include "acquire.h"
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>

Motor* motor;
Calib calib = { MM_PER_STEP, D_MM, 0, 0, 0, DS_MIN_MM };


static void enviarCaixa(const BoxResult &r) {
  HTTPClient http;
  http.begin(SERVER_URL);
  http.addHeader("Content-Type", "application/json");

  char body[96];
  snprintf(body, sizeof body, "{\"x\":%.1f,\"y\":%.1f,\"z\":%.1f}",
           r.length, r.width, r.height);

  int code = http.POST(body);
  Serial.printf("POST -> %d\n", code);   // 201 = o servidor gravou
  http.end();
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(1);

  initAllTof();
  motor = initMotor();

  //iniciando conexão com servidor
  WiFi.begin(WIFI_SSID, WIFI_SENHA);
  Serial.print("Conectando ao Wi-Fi");
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() != WL_CONNECTED) Serial.println("\nSem Wi-Fi, so mede");
  else Serial.printf("\nIP do ESP32: %s\n", WiFi.localIP().toString().c_str());


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

  enviarCaixa(r);
}
