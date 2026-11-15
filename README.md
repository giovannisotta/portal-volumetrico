# Portal Volumétrico

Sistema automatizado de volumetria e empacotamento 3D (Oficinas de Integração 1, UTFPR).

Um portal com três sensores ToF mede o comprimento, a largura e a altura de caixas que passam por uma esteira. O ESP32 envia cada medida por Wi-Fi para um servidor, que guarda tudo num banco de dados. O empacotamento 3D e a visualização ainda estão por fazer.

```
ESP32 (medição) --HTTP POST (JSON)--> servidor Flask --> SQLite
                                           |
                    cliente 3D <--HTTP GET-+   (a fazer)
```

## Estrutura

```
portal-volumetrico/
├── main/       firmware do ESP32 (Arduino)
│   ├── main.ino      setup, loop e envio HTTP
│   ├── acquire.*     aquisição: leitura dos sensores, baseline e gravação da passagem
│   ├── calc.*        cálculo das dimensões a partir das amostras
│   ├── tof.*         sensores VL53L0X
│   ├── motor.*       motor de passo
│   ├── config.h      pinos e constantes
│   └── secrets.h     Wi-Fi (não versionado, veja abaixo)
└── server/     API Flask + SQLite
    └── app.py
```

## Hardware

- ESP32 (DevKit)
- 3x sensor ToF VL53L0X (esquerdo, topo, direito)
- Motor de passo NEMA 17 com driver A4988
- Fonte chaveada de 12 V e regulador buck (12 V para 5 V)
- Capacitor eletrolítico de 100 µF

Pinos (em `main/config.h`):

| Função | Pino |
|---|---|
| I2C SDA / SCL | 21 / 22 |
| XSHUT esquerdo / topo / direito | 26 / 25 / 32 |
| Motor DIR / STEP | 12 / 27 |

Os três VL53L0X nascem com o mesmo endereço I2C. O firmware os liga um por vez pelo XSHUT e atribui `0x30`, `0x31` e `0x32`.

## Firmware (ESP32)

### Requisitos

- [arduino-cli](https://arduino.github.io/arduino-cli/)
- Core `esp32:esp32` (testado com a 3.3.12)
- Biblioteca `Adafruit VL53L0X`
- `WiFi`, `HTTPClient` e `Wire` já vêm no core do ESP32

```bash
arduino-cli config init   # só se ainda não existir config
arduino-cli config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32@3.3.12
arduino-cli lib install "Adafruit VL53L0X"
```

### Configuração

Crie `main/secrets.h` (está no `.gitignore`):

```cpp
#pragma once
#define WIFI_SSID  "nome_da_rede"
#define WIFI_SENHA "senha"
```

No `main/config.h`, ajuste:

- `SERVER_URL`: `http://IP_DO_PC:5000/caixas` (o IP do PC na rede do ESP32)
- `D_MM`: distância entre os dois sensores laterais, em mm
- `MM_PER_STEP`: deslocamento da esteira por passo
- `AJUSTE`, `MARGEM_MM`, `K_STEPS`: calibração (veja os comentários no arquivo)
- `CAIXA_RETA`: 1 para caixa alinhada com a esteira, 0 para caixa rotacionada

O ESP32 só conecta em redes Wi-Fi de 2,4 GHz.

### Compilar e gravar

```bash
cd main
make                  # compila
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 .
make lsp              # gera o compile_commands.json para o clangd
```

## Servidor

### Requisitos

- Python 3
- Flask (`server/requirements.txt`)

```bash
cd server
python -m venv venv
source venv/bin/activate        # no fish: source venv/bin/activate.fish
pip install -r requirements.txt
python app.py
```

O servidor escuta em `0.0.0.0:5000`. O banco `caixas.db` é criado sozinho na primeira execução. Libere a porta 5000 no firewall do PC (por exemplo `sudo ufw allow 5000/tcp`), e deixe o PC e o ESP32 na mesma rede.

### API

| Método | Rota | Descrição |
|---|---|---|
| `POST` | `/caixas` | Grava uma caixa. Corpo: `{"x": 110.0, "y": 70.0, "z": 50.0}` (mm). Responde `201` com `{"ok": true, "id": N}`, ou `400` se o JSON for inválido. |
| `GET` | `/caixas` | Lista todas as caixas, em ordem de chegada. |

`x` é o comprimento, `y` a largura e `z` a altura.

Teste sem o ESP32:

```bash
curl -X POST http://localhost:5000/caixas -H "Content-Type: application/json" -d '{"x":110,"y":70,"z":50}'
curl http://localhost:5000/caixas
```

Para limpar o banco, pare o servidor e apague o `server/caixas.db`.

## Estado atual e pendências

- Medição, envio e armazenamento funcionam.
- Falta o bin packing 3D (Pontos Extremos) e o cliente de visualização.
- O `make test` (teste do `calc.cpp` no PC) não compila no momento, porque o `calc.cpp` passou a incluir `Arduino.h` e `config.h` para depuração.
- A calibração (`D_MM`, `AJUSTE`, `MM_PER_STEP`) precisa de caixas de medidas conhecidas.

## Equipe

Arthur Kohler, Giovanni Sotta e Nicolas Bucci, Engenharia de Computação, UTFPR.
