#include "config.h"
#include "tof.h"
#include "motor.h"
#include "calc.h"
#include "acquire.h"
#include <WiFi.h>
#include <HTTPClient.h>

Motor* motor;
Calib calib = { MM_PER_STEP, D_MM, 0, 0, 0, DS_MIN_MM };
static QueueHandle_t qBox;

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

  // --- Wi-Fi ---
  WiFi.mode(WIFI_STA);

  int n = WiFi.scanNetworks();
  Serial.printf("%d redes encontradas\n", n);
  for (int i = 0; i < n; i++) {
    Serial.printf("  [%s] sinal=%d\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i));
  }

  WiFi.begin(WIFI_SSID, WIFI_SENHA);
  Serial.print("Conectando ao Wi-Fi");
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(500);
    Serial.printf(" status=%d", WiFi.status());
  }
  if (WiFi.status() != WL_CONNECTED) Serial.println("\nSem Wi-Fi, so mede");
  else Serial.printf("\nIP do ESP32: %s\n", WiFi.localIP().toString().c_str());

  if (!measureBaseline(calib)) {
    Serial.println("Erro: sem leitura valida para o baseline");
    while (true) delay(1000);
  }
  Serial.printf("Baseline: left=%.1f top=%.1f right=%.1f\n",
                calib.base_left, calib.base_top, calib.base_right);

  // dentro do setup(), depois do baseline:
  qBox = xQueueCreate(8, sizeof(BoxResult));
  if (qBox == NULL) {
    Serial.println("Erro: sem memoria para a fila");
    while (true) delay(1000);
  }
  xTaskCreatePinnedToCore(motorTask,  "motor",  2048, NULL, 3, NULL, 1);
  xTaskCreatePinnedToCore(sensorTask, "sensor", 8192, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(httpTask,   "http",   8192, NULL, 1, NULL, 0);
}

void motorTask(void*)  { for(;;){ stepMotor(motor); vTaskDelay(1); } }
void sensorTask(void*) {
  for (;;) {
    BoxResult r = acquireBox(motor, calib);

    if (r.status != CALC_OK) {
      Serial.printf("Erro: status=%d\n", r.status);
      vTaskDelay(pdMS_TO_TICKS(100));   // evita inundar o Serial se o erro repetir
      continue;
    }

    Serial.printf("length=%.1f width=%.1f height=%.1f theta=%.1f graus\n",
                  r.length, r.width, r.height, r.theta * 180.0f / PI);

    xQueueSend(qBox, &r, 0);
  }
}
void httpTask(void*) {
  BoxResult r;
  for(;;) if (xQueueReceive(qBox, &r, portMAX_DELAY)) enviarCaixa(r);
}


void loop() { vTaskDelay(portMAX_DELAY); }
