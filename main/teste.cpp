#include <stdio.h>
#include <math.h>
#include "calc.h"

static int fails = 0;

#define N(a) ((int)(sizeof(a) / sizeof((a)[0])))

static void check(const char* nome, const Sample* buf, int n, const Calib& c,
                  uint8_t st, float len, float wid, float hei, float tol = 1.0f) {
  BoxResult r = calcBox(buf, n, c);
  bool ok = (r.status == st);
  if (st == CALC_OK)
    ok = ok && fabsf(r.length - len) < tol && fabsf(r.width - wid) < tol
            && fabsf(r.height - hei) < tol;
  printf("%-18s %-5s status=%d length=%.1f width=%.1f height=%.1f theta=%.4f\n",
         nome, ok ? "OK" : "FALHA", r.status, r.length, r.width, r.height, r.theta);
  if (!ok) fails++;
}

int main() {
  Calib c = {1.0f, 300.0f, 200.0f, 200.0f, 200.0f, 5.0f};

  // caixa 100x50x80 girada (cos=0.8, sen=0.6)
  Sample girada[] = {
    {  0, 160, 200, 200}, { 20, 145, 190, 120}, { 40, 130, 170, 120},
    { 60, 115, 150, 120}, { 80, 100, 120, 120}, { 95, 120, 100, 120},
    {110, 140, 110, 200},
  };
  check("girada", girada, N(girada), c, CALC_OK, 100, 50, 80);

  // mesma caixa alinhada (theta = 0): B tem que sair igual ao cross
  Sample alinhada[] = {
    {  0, 100, 150, 120}, { 25, 100, 150, 120}, { 50, 100, 150, 120},
    { 75, 100, 150, 120}, {100, 100, 150, 120},
  };
  check("alinhada", alinhada, N(alinhada), c, CALC_OK, 100, 50, 80);

  // face longa é a segunda (subindo)
  Sample espelho[] = {
    {  0, 140, 200, 120}, { 10, 127, 190, 120}, { 20, 113, 170, 120},
    { 30, 100, 150, 120}, { 50, 115, 130, 120}, { 70, 130, 110, 120},
    { 90, 145, 100, 120}, {110, 160, 110, 120},
  };
  check("espelho", espelho, N(espelho), c, CALC_OK, 100, 50, 80);

  // leitura repetida (115, 115) na face que sobe: não pode atrapalhar
  Sample repetida[] = {
    {  0, 140, 200, 120}, { 10, 127, 190, 120}, { 20, 113, 170, 120},
    { 30, 100, 150, 120}, { 50, 115, 130, 120}, { 70, 115, 110, 120},
    { 90, 145, 100, 120}, {110, 160, 110, 120},
  };
  check("leitura repetida", repetida, N(repetida), c, CALC_OK, 100, 50, 80);

  // girada com ruido de +-1..2 mm nas leituras (tolerancia 3 mm)
  Sample ruido[] = {
    {  0, 161, 200, 200}, { 20, 144, 191, 121}, { 40, 131, 169, 119},
    { 60, 114, 151, 120}, { 80, 101, 119, 122}, { 95, 121, 101, 120},
    {110, 139, 109, 200},
  };
  check("girada com ruido", ruido, N(ruido), c, CALC_OK, 100, 50, 80, 3.0f);

  // erros
  check("n < 3", girada, 2, c, CALC_ERR_FEW_SAMPLES, 0, 0, 0);

  Calib c_ds = c;  c_ds.ds_min = 100;      // delta_s = 80 < 100
  check("delta_s pequeno", girada, N(girada), c_ds, CALC_ERR_ZERO_DS, 0, 0, 0);

  Calib c_D = c;   c_D.D = 150;            // cross negativo -> B <= 0
  check("B <= 0", girada, N(girada), c_D, CALC_ERR_LOW_CONF, 0, 0, 0);

  printf("%d falha(s)\n", fails);
  return fails;
}
