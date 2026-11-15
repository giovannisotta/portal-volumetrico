"""
Bin packing 3D por Pontos Extremos (Extreme Points, Crainic/Perboli/Tadei 2008).

Modulo puro: nao usa rede, banco nem grafico. Pode rodar no cliente (como agora)
ou ser movido para o servidor Flask sem alteracao.

Eixos: x = comprimento, y = largura, z = altura (mesmas unidades do ESP32, mm).
"""
from dataclasses import dataclass, field
from itertools import permutations

EPS = 1e-6


@dataclass
class Placed:
    id: int
    pos: tuple          # (x, y, z) do canto minimo
    size: tuple         # (dx, dy, dz) ja na orientacao escolhida
    orig: tuple         # (x, y, z) medidos, sem folga e sem rotacao


@dataclass
class Bin:
    dims: tuple
    items: list = field(default_factory=list)
    eps: list = field(default_factory=lambda: [(0.0, 0.0, 0.0)])

    def volume_usado(self):
        return sum(i.size[0] * i.size[1] * i.size[2] for i in self.items)

    def ocupacao(self):
        d = self.dims
        return self.volume_usado() / (d[0] * d[1] * d[2])


# --------------------------------------------------------------------------- #
# geometria
# --------------------------------------------------------------------------- #
def _orientacoes(dims, giro):
    """Orientacoes distintas. giro: 'none' | 'yaw' (so troca x<->y) | 'all' (6)."""
    x, y, z = dims
    if giro == "none":
        cand = [(x, y, z)]
    elif giro == "yaw":
        cand = [(x, y, z), (y, x, z)]
    else:
        cand = list(permutations((x, y, z)))
    return list(dict.fromkeys(cand))     # tira repetidas (caixa com lados iguais)


def _overlap(p, s, q, t):
    """Interseccao com volume > 0 entre dois paralelepipedos."""
    return all(p[a] < q[a] + t[a] - EPS and q[a] < p[a] + s[a] - EPS for a in range(3))


def _suporte(bin_, pos, size):
    """Fracao da base apoiada no chao ou no topo de outras caixas."""
    if pos[2] < EPS:
        return 1.0
    base = size[0] * size[1]
    apoio = 0.0
    for it in bin_.items:
        if abs(it.pos[2] + it.size[2] - pos[2]) > EPS:
            continue
        ox = min(pos[0] + size[0], it.pos[0] + it.size[0]) - max(pos[0], it.pos[0])
        oy = min(pos[1] + size[1], it.pos[1] + it.size[1]) - max(pos[1], it.pos[1])
        if ox > 0 and oy > 0:
            apoio += ox * oy
    return apoio / base


def _cabe(bin_, pos, size, min_suporte):
    for a in range(3):
        if pos[a] + size[a] > bin_.dims[a] + EPS:
            return False
    if any(_overlap(pos, size, it.pos, it.size) for it in bin_.items):
        return False
    return _suporte(bin_, pos, size) >= min_suporte - EPS


# --------------------------------------------------------------------------- #
# pontos extremos
# --------------------------------------------------------------------------- #
def _projeta(bin_, p, eixo):
    """Desce o ponto p ao longo de 'eixo' ate a face de uma caixa (ou a parede em 0)."""
    outros = [a for a in range(3) if a != eixo]
    melhor = 0.0
    for it in bin_.items:
        face = it.pos[eixo] + it.size[eixo]
        if face > p[eixo] + EPS:
            continue
        if all(it.pos[a] - EPS <= p[a] < it.pos[a] + it.size[a] - EPS for a in outros):
            melhor = max(melhor, face)
    q = list(p)
    q[eixo] = melhor
    return tuple(q)


def _atualiza_eps(bin_, usado, item):
    x, y, z = item.pos
    dx, dy, dz = item.size
    base = [(x + dx, y, z), (x, y + dy, z), (x, y, z + dz)]
    novos = []
    for p in base:
        novos.append(p)
        for eixo in range(3):                  # projecoes nos outros eixos
            novos.append(_projeta(bin_, p, eixo))

    eps = [e for e in bin_.eps if e != usado] + novos
    vistos, saida = set(), []
    for e in eps:
        e = tuple(round(c, 4) for c in e)
        if e in vistos:
            continue
        vistos.add(e)
        if any(e[a] >= bin_.dims[a] - EPS for a in range(3)):
            continue                           # fora do conteiner
        if any(all(it.pos[a] - EPS <= e[a] < it.pos[a] + it.size[a] - EPS
                   for a in range(3)) for it in bin_.items):
            continue                           # dentro de uma caixa
        saida.append(e)
    saida.sort(key=lambda p: (p[2], p[1], p[0]))   # baixo -> fundo -> esquerda
    bin_.eps = saida


# --------------------------------------------------------------------------- #
# empacotamento
# --------------------------------------------------------------------------- #
def _melhor_posicao(bin_, dims, giro, min_suporte):
    """Escolhe (pos, size) no bin: menor (z, y, x) do ponto; empate -> menor altura."""
    melhor, chave = None, None
    for ep in bin_.eps:
        for o in _orientacoes(dims, giro):
            if _cabe(bin_, ep, o, min_suporte):
                k = (ep[2], ep[1], ep[0], o[2])
                if chave is None or k < chave:
                    melhor, chave = (ep, o), k
        # EPs estao ordenados: o primeiro EP onde algo cabe ja e o melhor
        if melhor is not None:
            break
    return melhor


def empacota(caixas, bins, dims_bin, giro="yaw", min_suporte=0.7, folga=5.0):
    """
    Coloca 'caixas' (lista de (id, x, y, z)) em 'bins' (lista, alterada no lugar).
    Abre novos bins se preciso. Retorna a lista de ids que nao cabem nem em um bin vazio.
    folga: mm somados a cada dimensao antes de empacotar (erro de medicao dos ToF).
    """
    recusadas = []
    for cid, x, y, z in sorted(caixas, key=lambda c: -(c[1] * c[2] * c[3])):
        dims = (x + folga, y + folga, z + folga)
        achou = None
        for b in bins:
            achou = _melhor_posicao(b, dims, giro, min_suporte)
            if achou:
                alvo = b
                break
        if not achou:
            novo = Bin(dims_bin)
            achou = _melhor_posicao(novo, dims, giro, min_suporte)
            if not achou:
                recusadas.append(cid)          # maior que o conteiner
                continue
            bins.append(novo)
            alvo = novo
        pos, size = achou
        item = Placed(cid, pos, size, (x, y, z))
        alvo.items.append(item)
        _atualiza_eps(alvo, pos, item)
    return recusadas


# --------------------------------------------------------------------------- #
# teste rapido: python binpacking.py
# --------------------------------------------------------------------------- #
if __name__ == "__main__":
    import random
    random.seed(1)
    dims = (800.0, 600.0, 600.0)
    caixas = [(i + 1, random.randint(80, 300), random.randint(60, 250),
               random.randint(40, 250)) for i in range(40)]
    bins = []
    rec = empacota(caixas, bins, dims)
    for n, b in enumerate(bins, 1):
        for i, a in enumerate(b.items):
            for c in b.items[i + 1:]:
                assert not _overlap(a.pos, a.size, c.pos, c.size), "sobreposicao!"
            for k in range(3):
                assert a.pos[k] + a.size[k] <= b.dims[k] + EPS, "fora do conteiner!"
        print(f"bin {n}: {len(b.items)} caixas, ocupacao {b.ocupacao():.1%}")
    print("recusadas:", rec, "| sem sobreposicao e dentro dos limites: OK")
