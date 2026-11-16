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
static Faces fitFaces (const Sample* samples, int n, const Calib &calib, float height){
  Faces faces = {};
  if (n < 3) { faces.status = CALC_ERR_FEW_SAMPLES; return faces; }

  int mins[LAT_COUNT] = {};
  calcMin(samples, n, mins);

  float tl = 0, tr = 0;
  int loL, hiL, loR, hiR;
  bool okL = sideRange(samples, n, true,  calib.base_left,  loL, hiL)
          && sideTan(samples, loL, hiL, mins[LEFT],  true,  calib.mm_per_step, tl);
  bool okR = sideRange(samples, n, false, calib.base_right, loR, hiR)
          && sideTan(samples, loR, hiR, mins[RIGHT], false, calib.mm_per_step, tr);
  if (!okL && !okR) { faces.status = CALC_ERR_FEW_SAMPLES; return faces; }

  float tanT = (okL && okR) ? (tl + tr) / 2 : (okL ? tl : tr);
  if (tanT < 0.07f) tanT = 0;
  float theta = atanf(tanT);

  float c = cosf(theta), sn = sinf(theta), c2 = c*c - sn*sn;   // cos(2θ)
  if (c2 < 0.1f) { faces.status = CALC_ERR_LOW_CONF; return faces; }  // ~45°: sistema singular

  float sin2t = 2.0f * c * sn;   // sin(2θ): 0 quando reta, 1 quando a 45°

  float L = dSteps(samples, 0, n - 1) * calib.mm_per_step
          - (EXTRA_L0 - EXTRA_LK * height)
          + L_CORNER_K * sin2t;
  if (L < calib.ds_min) { faces.status = CALC_ERR_ZERO_DS; return faces; }

  int lv[MAX_SAMPLES], rv[MAX_SAMPLES];
  for (int i = 0; i < n; i++) { lv[i] = samples[i].left; rv[i] = samples[i].right; }
  std::sort(lv, lv + n); std::sort(rv, rv + n);

  Serial.printf("l=%d r=%d soma=%d\n", lv[0], rv[0], lv[0] + rv[0]);
  float W = calib.D - (lv[0] + rv[0]) - W_OFFSET + W_CORNER_K * sin2t;

  faces.A = (L*c - W*sn) / c2;
  faces.B = (W*c - L*sn) / c2;

  if (faces.A <= 0 || faces.B <= 0) { faces.status = CALC_ERR_LOW_CONF; return faces; }

  faces.theta  = theta;
  faces.status = CALC_OK;
  return faces;
}

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

static void dbgExt(const Sample* s, int n, const Calib& c){
  float b[3] = { c.base_left, c.base_right, c.base_top };
  int32_t lo[3] = {0,0,0}, hi[3] = {0,0,0};
  bool ok[3] = {false, false, false};
  for (int i = 0; i < n; i++){
    float d[3] = { (float)s[i].left, (float)s[i].right, (float)s[i].top };
    for (int k = 0; k < 3; k++){
      if (d[k] < b[k] - MARGEM_MM){
        if (!ok[k]) { lo[k] = (int32_t)s[i].step; ok[k] = true; }
        hi[k] = (int32_t)s[i].step;
      }
    }
  }
  if (!ok[0] || !ok[1] || !ok[2]) return;
  float mm = c.mm_per_step;
  int32_t latLo = std::max(lo[0], lo[1]), latHi = std::min(hi[0], hi[1]);
  int32_t allLo = std::max(latLo, lo[2]), allHi = std::min(latHi, hi[2]);
  int32_t anyLo = std::min(std::min(lo[0], lo[1]), lo[2]);
  int32_t anyHi = std::max(std::max(hi[0], hi[1]), hi[2]);
  Serial.printf("L=%.0f R=%.0f T=%.0f | L&&R=%.0f | todos&&=%.0f | ||=%.0f mm\n",
    (hi[0]-lo[0])*mm, (hi[1]-lo[1])*mm, (hi[2]-lo[2])*mm,
    (latHi-latLo)*mm, (allHi-allLo)*mm, (anyHi-anyLo)*mm);
  Serial.printf("inicio rel. a L: R=%.0f T=%.0f mm | fim rel. a L: R=%.0f T=%.0f mm\n",
    (lo[1]-lo[0])*mm, (lo[2]-lo[0])*mm, (hi[1]-hi[0])*mm, (hi[2]-hi[0])*mm);
}

BoxResult calcBox(const Sample* samples, int n, const Calib &calib){
  dbgExt(samples, n, calib);
  float height = calcHeight(samples, n, calib);
  Faces faces = fitFaces(samples, n, calib, height);
  BoxResult box = {};
  if (faces.status != CALC_OK) { box.status = faces.status; return box; }

  box.length = calcLength(faces);
  box.width  = calcWidth(faces);
  box.height = height;
  box.theta  = calcTheta(faces);
  box.status = faces.status;
  return box;
}
