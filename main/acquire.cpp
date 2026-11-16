#include "acquire.h"
#include "tof.h"
#include "config.h"
#include <algorithm> 
#ifndef FILTRO_MEDIANA
#error "FILTRO_MEDIANA nao definido no config.h"
#endif

/*static int median3(int a, int b, int c) {
  if (a > b) { int t = a; a = b; b = t; }
  if (b > c) { int t = b; b = c; c = t; }
  return (a > b) ? a : b;
}

static int readFiltered(TofId id) {
#if FILTRO_MEDIANA
  int a = readTof(id), b = readTof(id), c = readTof(id);
  if (a < 0 || b < 0 || c < 0) return -1;
  return median3(a, b, c);
#else
  return readTof(id);
#endif
}*/


#ifndef N_MEDIANA
#define N_MEDIANA 3 // Define a quantidade de amostras (preferencialmente ímpar)
#endif

static int getMedian(int* samples, int n) {
    std::sort(samples, samples + n);
    return samples[n / 2];
}

static int readFiltered(TofId id) {
#if FILTRO_MEDIANA
    int samples[N_MEDIANA];
    for (int i = 0; i < N_MEDIANA; i++) {
        samples[i] = readTof(id);
        if (samples[i] < 0) return -1; // Se qualquer leitura falhar, retorna erro
    }
    return getMedian(samples, N_MEDIANA);
#else
    return readTof(id);
#endif
}

static bool readAll(Sample &s){ 
  int id, dist[TOF_COUNT];
  for ( id = 0; id < TOF_COUNT; id++){
    dist[id] = readFiltered((TofId)id);
    if (dist[id] < 0) return false; 
  }
  s.left = (dist[TOF_LEFT]);
  s.top = (dist[TOF_TOP]);
  s.right = (dist[TOF_RIGHT]);
  /*Serial.print("leitura tof\n");
  printDist(s.left);
  printDist(s.right);
  printDist(s.top);
  Serial.print("\n");*/
  return true;
}

static bool readRetry(Sample &s){
  for (int i = 0; i < MAX_FALHAS; i++) if (readAll(s)) return true;
  return false;
}

static bool present(const Sample &s, const Calib &c){
  return (
    s.left  < (c.base_left  - MARGEM_MM) ||
    s.right < (c.base_right - MARGEM_MM));
}

bool measureBaseline(Calib &c){
  int i, counter = 0;
  Sample s;
  float sum[3][20] = {};
  for (i = 0; i < 20; i++){
    if(readAll(s)){
      sum[TOF_LEFT][counter]  = s.left;
      sum[TOF_TOP][counter]   = s.top;
      sum[TOF_RIGHT][counter] = s.right;
      counter++;
    }
  }
  for(i = 0; i < 3; i++){
    std::sort(sum[i], sum[i]+counter);
  }
  
  if (counter == 0) { return false; }
  c.base_left   = sum[TOF_LEFT][counter/2];
  c.base_top    = sum[TOF_TOP][counter/2];
  c.base_right  = sum[TOF_RIGHT][counter/2];
  // Serial.print("leitura baseline\n");
  // printDist(c.base_left);
  // printDist(c.base_right);
  // printDist(c.base_top);
  // Serial.print("\n");
  return true;
}
// static void advance(Motor* m, int k){
//   for (int i = 0; i < k; i++) stepMotor(m);
// }

// BoxResult acquireBox(Motor* motor, const Calib &calib){
//   BoxResult box = {};
//   static Sample buf[MAX_SAMPLES];
//   Sample s = {};
//   do{
//     int n = 0, ausents = 0;
//     uint32_t step = 0;
//     while (true) {
//       if (!readRetry(s)) { box.status = CALC_ERR_SENSOR; return box; }
//       if (present(s, calib)) break;
//       advance(motor, K_STEPS);
//     }
//     while (true){
//       if(present(s, calib)){
//         ausents = 0;
//         s.step = step;
//         if(n == MAX_SAMPLES) {box.status = CALC_ERR_BUFFER_FULL; return box;}
//         buf[n++] = s;
//       }else if (++ausents >= N_FIM) break;
//       step += K_STEPS;
//       advance(motor, K_STEPS);
//       if (!readRetry(s)) { box.status = CALC_ERR_SENSOR; return box; }
//       //Serial.print("Labubu\n");
//     }
//     box = calcBox(buf, n, calib);
//   } while (box.status == CALC_ERR_FEW_SAMPLES);
//   return box;
// }
//
// static BoxResult gerarCaixaAleatoria() {
//   BoxResult r = {};
//
//   // Gera dimensões entre 50.0 mm e 250.0 mm (em passos de 0.1 mm)
//   float d1 = random(500, 2501) / 10.0f;
//   float d2 = random(500, 2501) / 10.0f;
//
//   r.length = max(d1, d2);
//   r.width  = min(d1, d2);
//   r.height = random(300, 2001) / 10.0f;
//   r.theta  = 0.0f;
//   r.status = CALC_OK;
//
//   return r;
// }

BoxResult acquireBox(Motor* motor, const Calib &calib){
  BoxResult box = {};
  static Sample buf[MAX_SAMPLES];
  Sample s = {};
  do{
    int n = 0, ausents = 0;
    while (true) {
      if (!readRetry(s)) { box.status = CALC_ERR_SENSOR; return box; }
      s.step = g_step;                       // NOVO
      if (present(s, calib)) break;
    }
    while (true){
      if (present(s, calib)){
        ausents = 0;
        if (n == MAX_SAMPLES) { box.status = CALC_ERR_BUFFER_FULL; return box; }
        buf[n++] = s;
      } else if (++ausents >= N_FIM) break;
      if (!readRetry(s)) { box.status = CALC_ERR_SENSOR; return box; }
      s.step = g_step;                       // NOVO
    }
    Serial.printf("n=%d de %d\n", n, MAX_SAMPLES);
    box = calcBox(buf, n, calib);
  } while (box.status == CALC_ERR_FEW_SAMPLES);
  return box;
}
