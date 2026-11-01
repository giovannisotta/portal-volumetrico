FQBN = esp32:esp32:esp32
TESTE_DIR = ./teste

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
