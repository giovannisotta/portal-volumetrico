#include "calc.h"
#include "Arduino.h"
#include "config.h"

struct Faces{
  float A;
  float B;
  float theta;
  uint8_t status;
};
enum Lat { LEFT, RIGHT, LAT_COUNT};
static float dSteps (const Sample* s, int a, int b) { return float(s[b].step - s[a].step); }
static float dLeft  (const Sample* s, int a, int b) { return float(s[b].left - s[a].left); }
static void calcMin(const Sample* s, int n,int* mins){
  int i;
  mins[LEFT] = mins[RIGHT] = 0;
  for (i = 0; i < n; i++){
    mins[LEFT]  = s[mins[LEFT]].left   <=  s[i].left  ? mins[LEFT]   : i;
    mins[RIGHT] = s[mins[RIGHT]].right <=  s[i].right ? mins[RIGHT]  : i;
  }
}

static Faces fitFaces (const Sample* samples, int n, const Calib &calib){
  Faces faces = {};
  if (n < 3) { faces.status = CALC_ERR_FEW_SAMPLES; return faces;}
  
  float delta_sA, delta_sB, delta_s, delta_d, cross;
  int mins[LAT_COUNT] = {};
  
  calcMin(samples, n, mins);
  delta_sA = dSteps(samples, 0, mins[LEFT]);
  delta_sB = dSteps(samples, mins[LEFT], n - 1);

  if (delta_sA > delta_sB){
    delta_s = delta_sA;
    delta_d = dLeft(samples, 0, mins[LEFT]);
  }else{
    delta_s = delta_sB;
    delta_d = dLeft(samples, mins[LEFT], n-1);
  }

  delta_s = delta_s * calib.mm_per_step;
  delta_d = fabsf(delta_d);
  
  if (delta_s < calib.ds_min) { faces.status = CALC_ERR_ZERO_DS; return faces;}
  
  cross = calib.D - (samples[mins[LEFT]].left + samples[mins[RIGHT]].right);
  faces.A = sqrtf(delta_s*delta_s + delta_d*delta_d);
  faces.B = (cross - delta_d)*faces.A/delta_s;
  

  Serial.printf("D=%.0f cross=%.1f dd=%.1f ds=%.1f A=%.1f B=%.1f\n",
              calib.D, cross, delta_d, delta_s, faces.A, faces.B);
  //if (faces.B <= 0) { faces.status = CALC_ERR_LOW_CONF; return faces; }
  
  faces.theta = atan2f(delta_d, delta_s);
  faces.status = 0;
  
  return faces;
}

static Faces fitReta(const Sample* s, int n, const Calib &c){
  Faces f = {};
  if (n < 3) { f.status = CALC_ERR_FEW_SAMPLES; return f; }

  int a = n / 4, b = n - n / 4, m = b - a;      // miolo do buffer
  float sl = 0, sr = 0;
  for (int i = a; i < b; i++) { sl += s[i].left; sr += s[i].right; }

  f.A = dSteps(s, 0, n - 1) * c.mm_per_step + K_STEPS * c.mm_per_step;    // comprimento: trecho inteiro
  f.B = c.D - (sl + sr) / m;                    // largura: média do miolo
  f.theta = 0;
  return f;
}

static float calcHeight(const Sample* samples, int n, const Calib &calib){
  int i;
  float min_height = samples[0].top;
  for (i = 1; i < n; i++)
       min_height = (min_height < samples[i].top) ? min_height : samples[i].top;
  return (calib.base_top - min_height);
}

static float calcLength(Faces f){ return fmaxf(f.A, f.B);}
static float calcWidth (Faces f){ return fminf(f.A, f.B);}
static float calcTheta (Faces f){ return f.theta;}

BoxResult calcBox(const Sample* samples, int n, const Calib &calib){
  Faces faces = CAIXA_RETA ? fitReta(samples, n, calib) : fitFaces(samples, n, calib);
  BoxResult box = {};
  if (faces.status != CALC_OK) { box.status = faces.status; return box; }

  float length, width, height, theta;
  
  length = CAIXA_RETA ? faces.A : calcLength(faces);
  width  = CAIXA_RETA ? faces.B : calcWidth (faces);
  height = calcHeight(samples, n, calib);
  theta  = calcTheta (faces);


  box.height = height;
  box.length = length;
  box.width  = width;
  box.theta  = theta;
  box.status = faces.status;

  return box;
}
