import sqlite3
from datetime import datetime, timezone
from flask import Flask, request, jsonify

DB = "caixas.db"
app = Flask(__name__)


def db():
    con = sqlite3.connect(DB)
    con.row_factory = sqlite3.Row
    return con


def init_db():
    with db() as con:
        _ = con.execute("""CREATE TABLE IF NOT EXISTS caixas (
                id          INTEGER PRIMARY KEY AUTOINCREMENT,
                x           REAL NOT NULL,
                y           REAL NOT NULL,
                z           REAL NOT NULL,
                recebido_em TEXT NOT NULL)""")


@app.post("/caixas")
def nova_caixa():
    d: dict = request.get_json(silent=True) or {}
    try:
        x, y, z = float(d["x"]), float(d["y"]), float(d["z"])
    except (KeyError, TypeError, ValueError):
        return jsonify(erro="o JSON precisa de x, y e z numericos"), 400

    agora = datetime.now(timezone.utc).isoformat()
    with db() as con:
        cur = con.execute(
            "INSERT INTO caixas (x, y, z, recebido_em) VALUES (?, ?, ?, ?)",
            (x, y, z, agora))
    return jsonify(ok=True, id=cur.lastrowid), 201


@app.get("/caixas")
def listar():
    with db() as con:
        rows = con.execute("SELECT * FROM caixas ORDER BY id").fetchall()
    return jsonify([dict(r) for r in rows])


@app.delete("/caixas")
def limpar():
    with db() as con:
        con.execute("DELETE FROM caixas")
        con.execute("DELETE FROM sqlite_sequence WHERE name='caixas'")
    return jsonify(ok=True)

if __name__ == "__main__":
    init_db()
    app.run(host="0.0.0.0", port=5000)
