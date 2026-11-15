FQBN 			= esp32:esp32:esp32
TESTE_DIR = ./teste
PORT 			?= /dev/ttyUSB0

compile:
	arduino-cli compile --fqbn $(FQBN) .

lsp:
	rm -f compile_commands.json
	arduino-cli compile --fqbn $(FQBN) --only-compilation-database --build-path build .
	ln -sf build/compile_commands.json compile_commands.json

.PHONY: test
test:
	g++ -Wall -I. -o $(TESTE_DIR)/teste $(TESTE_DIR)/teste.cpp calc.cpp -lm
	$(TESTE_DIR)/teste

.PHONY: load
load:
	arduino-cli compile --fqbn $(FQBN) --upload -p $(PORT) .

monitor:
	arduino-cli monitor -p $(PORT) -c baudrate=115200
