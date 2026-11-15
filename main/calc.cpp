#include "calc.h"
#include "config.h"
#include <Arduino.h>         // Serial
#include <algorithm>         // std::sort, std::min (estava faltando, só funcionava via Arduino.h)

struct Faces{
  float A;
  float B;
  float theta;
  uint8_t status;
};
enum Lat { LEFT, RIGHT, LAT_COUNT};
static float dSteps (const Sample* s, int a, int b) { return float(s[b].step - s[a].step); }
static void calcMin(const Sample* s, int n,int* mins){
  int i;
  mins[LEFT] = mins[RIGHT] = 0;
  for (i = 0; i < n; i++){
    mins[LEFT]  = s[mins[LEFT]].left   <=  s[i].left  ? mins[LEFT]   : i;
    mins[RIGHT] = s[mins[RIGHT]].right <=  s[i].right ? mins[RIGHT]  : i;
  }
}



// NOVO: regressão linear (mínimos quadrados) de distância x passo em [a,b].
// Retorna mm de distância por passo.
static float slopeLS(const Sample* s, int a, int b, bool left){
  float sx = 0, sy = 0, sxx = 0, sxy = 0;
  int m = b - a + 1;
  for (int i = a; i <= b; i++){
    float x = float(s[i].step - s[a].step);
    float y = left ? s[i].left : s[i].right;
    sx += x; sy += y; sxx += x*x; sxy += x*y;
  }
  float den = m*sxx - sx*sx;
  return (den == 0) ? 0.0f : (m*sxy - sx*sy) / den;
}


// primeiro e último índice em que o lado enxerga a caixa (leitura abaixo do baseline)
static bool sideRange(const Sample* s, int n, bool left, float base, int &lo, int &hi){
  lo = hi = -1;
  for (int i = 0; i < n; i++){
    float d = left ? s[i].left : s[i].right;
    if (d < base - MARGEM_MM){
      if (lo < 0) lo = i;
      hi = i;
    }
  }
  return lo >= 0;
}
// NOVO: tan(theta) a partir de UM lado, usando a face mais longa em passos,
// sem as bordas (onde o feixe pega só o canto da caixa).
static bool sideTan(const Sample* s, int lo, int hi, int m, bool left, float mm_per_step, float &tanT){
  int a, b;
  if (m - lo >= hi - m) { a = lo; b = m; }    // face antes do mínimo
  else                  { a = m;  b = hi; }   // face depois do mínimo
  int t = std::max(1, (b - a) / 6);
  a += t; b -= t;
  if (b - a < 2) return false;
  tanT = fabsf(slopeLS(s, a, b, left)) / mm_per_step;
  return true;
}
static Faces fitFaces (const Sample* samples, int n, const Calib &calib){
  Faces faces = {};
  if (n < 3) { faces.status = CALC_ERR_FEW_SAMPLES; return faces;}

  int mins[LAT_COUNT] = {};
  calcMin(samples, n, mins);

  // ERA: delta_sA / delta_sB / delta_d = reta por 2 pontos (extremos), só o lado esquerdo
  // VIROU: regressão nos pontos do miolo da face, nos DOIS lados, e média
  float tl = 0, tr = 0;
  int loL, hiL, loR, hiR;
  bool okL = sideRange(samples, n, true,  calib.base_left,  loL, hiL)
          && sideTan(samples, loL, hiL, mins[LEFT],  true,  calib.mm_per_step, tl);
  bool okR = sideRange(samples, n, false, calib.base_right, loR, hiR)
          && sideTan(samples, loR, hiR, mins[RIGHT], false, calib.mm_per_step, tr);
  if (!okL && !okR) { faces.status = CALC_ERR_FEW_SAMPLES; return faces; }
  float tanT = (okL && okR) ? (tl + tr) / 2 : (okL ? tl : tr);
  float theta = atanf(tanT);

  // ERA: delta_s = passos da face * mm_per_step  (só a face usada)
  // VIROU: L = extensão total da caixa ao longo da esteira
  float L = dSteps(samples, 0, n - 1) * calib.mm_per_step;
  if (L < calib.ds_min) { faces.status = CALC_ERR_ZERO_DS; return faces; }

  // ERA: cross = D - (left do ponto mínimo + right do ponto mínimo)  -> 1 leitura de cada
  // VIROU: mesma ideia, mas com mediana das menores leituras (como na fitReta)
  int lv[MAX_SAMPLES], rv[MAX_SAMPLES];
  for (int i = 0; i < n; i++) { lv[i] = samples[i].left; rv[i] = samples[i].right; }
  std::sort(lv, lv + n); std::sort(rv, rv + n);
  int k = std::min(n, K_MENORES_MEDIANA);
  float W = calib.D - (lv[k/2] + rv[k/2]);   // extensão lateral

  // ERA: A = sqrt(ds² + dd²);  B = (cross - dd) * A / ds
  // VIROU: sistema das duas extensões
  //   L = A cosθ + B sinθ
  //   W = A sinθ + B cosθ
  float c = cosf(theta), sn = sinf(theta), c2 = c*c - sn*sn;   // cos(2θ)
  if (c2 < 0.1f) { faces.status = CALC_ERR_LOW_CONF; return faces; }  // ~45°: sistema singular
  faces.A = (L*c - W*sn) / c2;
  faces.B = (W*c - L*sn) / c2;

  // ERA: checagem de B <= 0 comentada
  // VIROU: reativada (e vale pra A também)
  if (faces.A <= 0 || faces.B <= 0) { faces.status = CALC_ERR_LOW_CONF; return faces; }

  faces.theta  = theta;
  faces.status = CALC_OK;
  return faces;
}

// static Faces fitFaces (const Sample* samples, int n, const Calib &calib){
//   Faces faces = {};
//   if (n < 3) { faces.status = CALC_ERR_FEW_SAMPLES; return faces;}
//
//   float delta_sA, delta_sB, delta_s, delta_d, cross;
//   int mins[LAT_COUNT] = {};
//
//   calcMin(samples, n, mins);
//   delta_sA = dSteps(samples, 0, mins[LEFT]);
//   delta_sB = dSteps(samples, mins[LEFT], n - 1);
//
//   if (delta_sA > delta_sB){
//     delta_s = delta_sA;
//     delta_d = dLeft(samples, 0, mins[LEFT]);
//   }else{
//     delta_s = delta_sB;
//     delta_d = dLeft(samples, mins[LEFT], n-1);
//   }
//
//   delta_s = delta_s * calib.mm_per_step;
//   delta_d = fabsf(delta_d);
//
//   if (delta_s < calib.ds_min) { faces.status = CALC_ERR_ZERO_DS; return faces;}
//
//   cross = calib.D - (samples[mins[LEFT]].left + samples[mins[RIGHT]].right);
//   faces.A = sqrtf(delta_s*delta_s + delta_d*delta_d);
//   faces.B = (cross - delta_d)*faces.A/delta_s;
//
//
//   Serial.printf("D=%.0f cross=%.1f dd=%.1f ds=%.1f A=%.1f B=%.1f\n",
//               calib.D, cross, delta_d, delta_s, faces.A, faces.B);
//   //if (faces.B <= 0) { faces.status = CALC_ERR_LOW_CONF; return faces; }
//
//   faces.theta = atan2f(delta_d, delta_s);
//   faces.status = 0;
//
//   return faces;
// }




// static float calcHeight(const Sample* samples, int n, const Calib &calib){
//   int i;
//   float min_height = samples[0].top;
//   for (i = 1; i < n; i++)
//        min_height = (min_height < samples[i].top) ? min_height : samples[i].top;
//   return (calib.base_top - min_height);
// }

static float calcHeight(const Sample* samples, int n, const Calib &calib) {
  float min_dist_top = samples[0].top;
  for (int i = 1; i < n; i++) {
    if (samples[i].top < min_dist_top) {
      min_dist_top = samples[i].top;
    }
  }
  float height = calib.base_top - min_dist_top;
  return (height < 0) ? 0.0f : height;
}

static float calcLength(Faces f){ return fmaxf(f.A, f.B);}
static float calcWidth (Faces f){ return fminf(f.A, f.B);}
static float calcTheta (Faces f){ return f.theta;}

BoxResult calcBox(const Sample* samples, int n, const Calib &calib){
  Faces faces = fitFaces(samples, n, calib);
  BoxResult box = {};
  if (faces.status != CALC_OK) { box.status = faces.status; return box; }

  box.length = calcLength(faces);
  box.width  = calcWidth(faces);
  box.height = calcHeight(samples, n, calib);
  box.theta  = calcTheta(faces);
  box.status = faces.status;
  return box;
}
