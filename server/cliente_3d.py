"""
Cliente de bin packing 3D.

A cada N caixas novas no servidor (GET /caixas), empacota e atualiza o grafico 3D.

  python cliente_3d.py --url http://127.0.0.1:5000 --n 1
  python cliente_3d.py --demo            # sem servidor, caixas aleatorias

Dependencias: pip install requests matplotlib
"""
import argparse
import json
import random
import time

import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection

from binpacking import Bin, empacota


# --------------------------------------------------------------------------- #
# estado: caixas ja empacotadas, bins, controle de reorganizacao
# --------------------------------------------------------------------------- #
class Estado:
    def __init__(self, dims, giro, folga, min_suporte, reorg_cada):
        self.dims, self.giro, self.folga = dims, giro, folga
        self.min_suporte, self.reorg_cada = min_suporte, reorg_cada
        self.reset()

    def reset(self):
        self.bins = []
        self.todas = {}            # id -> (id, x, y, z)
        self.recusadas = []
        self.ultimo_id = 0
        self.lotes = 0

    def processa_lote(self, novas):
        """novas: lista de (id, x, y, z). Retorna True se algo mudou."""
        for c in novas:
            self.todas[c[0]] = c
            self.ultimo_id = max(self.ultimo_id, c[0])
        self.lotes += 1

        reorganiza = self.reorg_cada > 0 and self.lotes % self.reorg_cada == 0
        if reorganiza:
            # reorganizacao: reempacota TUDO do zero (maiores primeiro)
            self.bins, self.recusadas = [], []
            self.recusadas += empacota(list(self.todas.values()), self.bins,
                                       self.dims, self.giro, self.min_suporte, self.folga)
        else:
            # incremental: mantem o que ja foi posicionado e so encaixa as novas
            self.recusadas += empacota(novas, self.bins, self.dims, self.giro,
                                       self.min_suporte, self.folga)
        return True

    def salva(self, caminho):
        out = {"conteiner": self.dims, "recusadas": self.recusadas, "bins": [
            {"ocupacao": b.ocupacao(),
             "itens": [{"id": i.id, "pos": i.pos, "size": i.size, "orig": i.orig}
                       for i in b.items]} for b in self.bins]}
        with open(caminho, "w") as f:
            json.dump(out, f, indent=1)


# --------------------------------------------------------------------------- #
# plot
# --------------------------------------------------------------------------- #
def _faces(pos, size):
    x, y, z = pos
    dx, dy, dz = size
    v = [(x, y, z), (x + dx, y, z), (x + dx, y + dy, z), (x, y + dy, z),
         (x, y, z + dz), (x + dx, y, z + dz), (x + dx, y + dy, z + dz), (x, y + dy, z + dz)]
    idx = [(0, 1, 2, 3), (4, 5, 6, 7), (0, 1, 5, 4), (2, 3, 7, 6), (1, 2, 6, 5), (0, 3, 7, 4)]
    return [[v[i] for i in f] for f in idx]


class Plot:
    def __init__(self):
        plt.ion()
        self.fig = plt.figure("Empacotamento 3D", figsize=(11, 6))
        self.views = {}      # guarda a camera de cada bin entre redesenhos
        self.cmap = plt.get_cmap("tab20")

    def aberta(self):
        return plt.fignum_exists(self.fig.number)

    def desenha(self, estado):
        for k, ax in enumerate(self.fig.axes):
            self.views[k] = (ax.elev, ax.azim)
        self.fig.clear()

        bins = estado.bins or [Bin(estado.dims)]
        n = len(bins)
        cols = min(n, 3)
        rows = (n + cols - 1) // cols
        X, Y, Z = estado.dims

        for k, b in enumerate(bins):
            ax = self.fig.add_subplot(rows, cols, k + 1, projection="3d")
            ax.set_box_aspect((X, Y, Z))
            ax.set_xlim(0, X); ax.set_ylim(0, Y); ax.set_zlim(0, Z)
            ax.set_xlabel("x (mm)"); ax.set_ylabel("y (mm)"); ax.set_zlabel("z (mm)")
            ax.view_init(*self.views.get(k, (22, -60)))
            ax.set_title(f"Conteiner {k + 1}: {len(b.items)} caixas, "
                         f"ocupacao {b.ocupacao():.1%}", fontsize=10)

            # arestas do conteiner
            ax.add_collection3d(Poly3DCollection(
                _faces((0, 0, 0), (X, Y, Z)), facecolors="none",
                edgecolors="gray", linewidths=0.8, linestyles="--"))
            for it in b.items:
                ax.add_collection3d(Poly3DCollection(
                    _faces(it.pos, it.size), facecolors=self.cmap(it.id % 20),
                    edgecolors="k", linewidths=0.6, alpha=0.8))
                cx, cy, cz = (it.pos[i] + it.size[i] / 2 for i in range(3))
                ax.text(cx, cy, cz, str(it.id), ha="center", va="center", fontsize=7)

        if estado.recusadas:
            self.fig.suptitle(f"Maiores que o conteiner (ids): {estado.recusadas}",
                              color="red", fontsize=9)
        self.fig.tight_layout()
        self.fig.canvas.draw_idle()


# --------------------------------------------------------------------------- #
# fonte de dados
# --------------------------------------------------------------------------- #
def busca_servidor(url):
    import requests
    r = requests.get(url.rstrip("/") + "/caixas", timeout=5)
    r.raise_for_status()
    return [(d["id"], d["x"], d["y"], d["z"]) for d in r.json()]


class Demo:
    """Simula o servidor: uma caixa nova a cada chamada."""
    def __init__(self):
        self.caixas = []

    def __call__(self, _url):
        i = len(self.caixas) + 1
        self.caixas.append((i, random.randint(80, 300), random.randint(60, 250),
                            random.randint(40, 250)))
        return list(self.caixas)


# --------------------------------------------------------------------------- #
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--url", default="http://127.0.0.1:5000")
    ap.add_argument("--n", type=int, default=1, help="empacota a cada N caixas novas")
    ap.add_argument("--intervalo", type=float, default=1.0, help="segundos entre consultas")
    ap.add_argument("--conteiner", type=float, nargs=3, default=[800, 600, 600],
                    metavar=("X", "Y", "Z"), help="dimensoes internas em mm")
    ap.add_argument("--giro", choices=["none", "yaw", "all"], default="yaw",
                    help="none: sem girar | yaw: troca x<->y | all: 6 orientacoes")
    ap.add_argument("--folga", type=float, default=5.0, help="mm somados a cada dimensao")
    ap.add_argument("--suporte", type=float, default=0.7,
                    help="fracao minima da base apoiada (0 a 1)")
    ap.add_argument("--reorg-cada", type=int, default=0,
                    help="reempacota tudo do zero a cada K lotes (0 = nunca)")
    ap.add_argument("--saida", default="empacotamento.json")
    ap.add_argument("--demo", action="store_true", help="caixas aleatorias, sem servidor")
    a = ap.parse_args()

    busca = Demo() if a.demo else busca_servidor
    estado = Estado(tuple(a.conteiner), a.giro, a.folga, a.suporte, a.reorg_cada)
    plot = Plot()
    plot.desenha(estado)

    while plot.aberta():
        try:
            caixas = busca(a.url)
        except Exception as e:
            print("servidor indisponivel:", e)
            plt.pause(a.intervalo)
            continue

        # servidor foi limpo (DELETE /caixas): zera tudo
        if (caixas and max(c[0] for c in caixas) < estado.ultimo_id) or \
           (not caixas and estado.todas):
            print("servidor reiniciado, limpando estado")
            estado.reset()
            plot.desenha(estado)

        novas = [c for c in caixas if c[0] > estado.ultimo_id]
        if len(novas) >= a.n:
            estado.processa_lote(novas)
            estado.salva(a.saida)
            print(f"+{len(novas)} caixa(s) -> {len(estado.bins)} conteiner(s), "
                  f"{len(estado.todas)} caixas no total")
            plot.desenha(estado)

        plt.pause(a.intervalo)


if __name__ == "__main__":
    main()
