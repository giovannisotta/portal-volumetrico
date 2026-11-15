"""
Calcula a correção de cada sensor a partir do cal.csv.

  python fit_cal.py cal.csv

cal.csv = as linhas "CSV,..." copiadas do Serial (o cabeçalho pode ficar).
Para cada sensor ajusta  real = a*leitura + b  (minimos quadrados) e tambem
so offset (a = 1). Imprime os residuos e as linhas prontas para o tof.cpp.
Dependencia: pip install numpy
"""
import sys
import numpy as np

ARQ = sys.argv[1] if len(sys.argv) > 1 else "cal.csv"
ORDEM = ["left", "top", "right"]          # mesma ordem do enum TofId
TOL_MM = 3.0                              # residuo maximo aceitavel

dados = {s: [] for s in ORDEM}            # sensor -> [(real, mediana, desvio)]
with open(ARQ) as f:
    for linha in f:
        p = linha.strip().split(",")
        if len(p) < 10 or p[0] != "CSV" or p[1] not in dados:
            continue
        try:
            real, med, desv = float(p[2]), float(p[5]), float(p[7])
        except ValueError:
            continue
        dados[p[1]].append((real, med, desv))

tabela = []
for s in ORDEM:
    pts = sorted(dados[s])
    print(f"\n=== {s} ({len(pts)} pontos) ===")
    if len(pts) < 2:
        print("  precisa de pelo menos 2 pontos (ideal: 6) -> fica {1.0f, 0.0f}")
        tabela.append((0.0, 1.0, 0.0))
        continue

    real = np.array([p[0] for p in pts])
    med = np.array([p[1] for p in pts])
    desv = np.array([p[2] for p in pts])

    a, b = np.polyfit(med, real, 1)
    res_lin = real - (a * med + b)
    off = float(np.mean(real - med))
    res_off = real - (med + off)

    print(f"  {'real':>7} {'leitura':>8} {'erro cru':>9} {'desvio':>7} {'res reta':>9}")
    for r, m, d, e in zip(real, med, desv, res_lin):
        aviso = "  <- perto (<40 mm)" if r < 40 else ("  <- ruidoso" if d > 3 else "")
        print(f"  {r:7.1f} {m:8.1f} {m - r:+9.1f} {d:7.2f} {e:+9.2f}{aviso}")

    print(f"  reta:        a={a:.4f} b={b:+.2f}  maior residuo={np.abs(res_lin).max():.2f} mm")
    print(f"  so offset:   b={off:+.2f}          maior residuo={np.abs(res_off).max():.2f} mm")
    if len(pts) < 4:
        print("  ATENCAO: poucos pontos, nao da para saber se a reta serve")
    coef = (0.0, a, b)                      # (c2, c1, c0): real = c2*m^2 + c1*m + c0
    if np.abs(res_lin).max() > TOL_MM:
        if len(pts) >= 5:
            c2, c1, c0 = np.polyfit(med, real, 2)
            res_q = real - np.polyval([c2, c1, c0], med)
            print(f"  curva (grau 2): c2={c2:.3e} c1={c1:.5f} c0={c0:.3f}  "
                  f"maior residuo={np.abs(res_q).max():.2f} mm")
            if np.abs(res_q).max() < np.abs(res_lin).max() / 2:
                coef = (c2, c1, c0)
                print("  -> reta nao basta; usando a curva de grau 2 "
                      f"(valida para leituras de {med.min():.0f} a {med.max():.0f} mm)")
        else:
            print(f"  residuo > {TOL_MM} mm e menos de 5 pontos: meça mais pontos")
    tabela.append(coef)

print("\n// cole no tof.cpp (ordem: left, top, right)")
print("struct Cal { float c2, c1, c0; };   // real = c2*m*m + c1*m + c0")
print("static const Cal cal[TOF_COUNT] = {")
for nome, (c2, c1, c0) in zip(ORDEM, tabela):
    print(f"  {{ {c2:.6e}f, {c1:.6f}f, {c0:.4f}f }},   // {nome}")
print("};")
