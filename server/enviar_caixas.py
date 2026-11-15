"""
Simula o ESP32: envia caixas para POST /caixas do servidor.

  python enviar_caixas.py --qtd 60 --intervalo 0.5 --limpar
  python enviar_caixas.py --modelos modelos.json --qtd 60 --ruido 3   # 6 tamanhos fixos
  python enviar_caixas.py --arquivo caixas.json        # lista de {"x":..,"y":..,"z":..}

Dependencia: pip install requests
"""
import argparse
import json
import random
import time

import requests

ap = argparse.ArgumentParser()
ap.add_argument("--url", default="http://127.0.0.1:5000")
ap.add_argument("--qtd", type=int, default=50, help="quantas caixas aleatorias")
ap.add_argument("--intervalo", type=float, default=0.5, help="segundos entre envios")
ap.add_argument("--arquivo", help="JSON com lista de caixas (ignora --qtd)")
ap.add_argument("--modelos", help="JSON com os tamanhos fixos [{x,y,z},...]; sorteia entre eles")
ap.add_argument("--ruido", type=float, default=0.0,
                help="mm de erro (+/-) somado a cada medida, simula os ToF")
ap.add_argument("--limpar", action="store_true", help="DELETE /caixas antes de enviar")
ap.add_argument("--seed", type=int, help="semente, para repetir o mesmo conjunto")
a = ap.parse_args()

if a.seed is not None:
    random.seed(a.seed)

if a.arquivo:
    with open(a.arquivo) as f:
        caixas = json.load(f)
elif a.modelos:
    with open(a.modelos) as f:
        modelos = json.load(f)
    caixas = [dict(random.choice(modelos)) for _ in range(a.qtd)]
else:
    caixas = [{"x": round(random.uniform(80, 300), 1),
               "y": round(random.uniform(60, 250), 1),
               "z": round(random.uniform(40, 250), 1)} for _ in range(a.qtd)]

if a.ruido:
    for c in caixas:
        for k in ("x", "y", "z"):
            c[k] = round(c[k] + random.uniform(-a.ruido, a.ruido), 1)

base = a.url.rstrip("/") + "/caixas"
if a.limpar:
    print("DELETE ->", requests.delete(base, timeout=5).status_code)

for i, c in enumerate(caixas, 1):
    r = requests.post(base, json=c, timeout=5)
    print(f"{i}/{len(caixas)} {c} -> {r.status_code}")
    time.sleep(a.intervalo)

