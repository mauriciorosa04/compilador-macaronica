# make                     compila bin/macaronica e copia como ./compilador
# make test                front-end, execucao (lli e clang) e placa (simavr)
# make bench               mede os nucleos vetoriais
# make roda PROG=arq.mac   compila pro PC e executa
# make placa PROG=arq.mac  gera placa/arq.hex pro Arduino Uno
# make simula / gravar     roda no simavr / grava na placa

CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -O2
SRCDIR  = src
BINDIR  = bin

SRCS = $(SRCDIR)/main.c $(SRCDIR)/lexer.c $(SRCDIR)/parser.c \
       $(SRCDIR)/ast.c $(SRCDIR)/symtab.c $(SRCDIR)/semantic.c \
       $(SRCDIR)/codegen_llvm.c $(SRCDIR)/codegen_mlir.c $(SRCDIR)/erros.c

HDRS = $(SRCDIR)/lexer.h $(SRCDIR)/parser.h $(SRCDIR)/ast.h \
       $(SRCDIR)/symtab.h $(SRCDIR)/semantic.h $(SRCDIR)/codegen_llvm.h \
       $(SRCDIR)/codegen_mlir.h $(SRCDIR)/erros.h

ALVO = $(BINDIR)/macaronica$(EXE)

# no Windows a variavel SHELL diz sh.exe mesmo sem sh, entao testa rodando
TEM_SH := $(shell sh -c "echo posixok" 2>&1)

ifeq ($(OS),Windows_NT)
  EXE = .exe
  # pilha de 16 MB (o padrao do Windows e 2)
  LDFLAGS = -Wl,--stack,16777216
else
  EXE =
  LDFLAGS =
endif

ifeq ($(TEM_SH),posixok)
  CRIAR_BIN = mkdir -p $(BINDIR)
  APAGAR_BIN = rm -rf $(BINDIR) bin-asan bin-linux build compilador compilador.exe
  COPIAR = cp $(ALVO) compilador$(EXE)
else
  CRIAR_BIN = if not exist $(BINDIR) mkdir $(BINDIR)
  APAGAR_BIN = if exist $(BINDIR) rmdir /s /q $(BINDIR)
  COPIAR = copy /y $(BINDIR)\macaronica.exe compilador.exe >nul
endif

all: $(ALVO) compilador$(EXE)

compilador$(EXE): $(ALVO)
	@$(COPIAR)

$(ALVO): $(SRCS) $(HDRS)
	@$(CRIAR_BIN)
	$(CC) $(CFLAGS) -o $(ALVO) $(SRCS) $(LDFLAGS)

clean:
	@$(APAGAR_BIN)

ifeq ($(TEM_SH),posixok)

test: $(ALVO)
	@ok=0; falhas=0; \
	echo "===================== CASOS VALIDOS ====================="; \
	for f in tests/validos/*.mac; do \
		echo; echo ">>> $$f"; \
		./$(ALVO) $$f; st=$$?; \
		if [ $$st -eq 0 ]; then ok=$$((ok+1)); \
		else falhas=$$((falhas+1)); echo "(FALHOU: deveria ser valido, saiu $$st)"; fi; \
	done; \
	echo; echo "==================== CASOS INVALIDOS ===================="; \
	for f in tests/invalidos/*.mac; do \
		echo; echo ">>> $$f"; \
		./$(ALVO) $$f; st=$$?; \
		if [ $$st -eq 1 ]; then ok=$$((ok+1)); \
		else falhas=$$((falhas+1)); echo "(FALHOU: deveria ser invalido, saiu $$st)"; fi; \
	done; \
	echo; echo "RESUMO: $$ok caso(s) corretos, $$falhas falha(s)"; \
	sh tests/roda_diagnostico.sh ./$(ALVO) || falhas=$$((falhas+1)); \
	sh tests/roda_diagnostico.sh ./$(ALVO) tests/dsl/invalidos || falhas=$$((falhas+1)); \
	sh tests/roda_execucao.sh ./$(ALVO) || falhas=$$((falhas+1)); \
	sh tests/roda_placa.sh ./$(ALVO) || falhas=$$((falhas+1)); \
	[ $$falhas -eq 0 ]

bench: $(ALVO)
	@bash scripts/bench/roda_bench.sh ./$(ALVO)

PORTA ?= /dev/ttyUSB0
NOME_PROG = "$$(basename "$(PROG)" .mac)"
CONFERE_PROG = test -n "$(PROG)" || { echo "faltou dizer o programa. Exemplo: make roda PROG=arquivo.mac"; exit 2; }

roda: $(ALVO)
	@$(CONFERE_PROG)
	@sh scripts/compila.sh "$(PROG)" ./$(ALVO) > /dev/null
	@./build/pc/$(NOME_PROG); st=$$?; [ $$st -eq 0 ] || echo "[o programa saiu com codigo $$st]"

placa: $(ALVO)
	@$(CONFERE_PROG)
	@sh scripts/placa.sh "$(PROG)" ./$(ALVO)

simula: placa
	@sh scripts/simula.sh build/placa/$(NOME_PROG).elf

# 115200 e o bootloader novo do Uno; o antigo usa 57600
gravar: placa
	avrdude -c arduino -p m328p -P $(PORTA) -b 115200 -U flash:w:placa/$(NOME_PROG).hex:i

estresse: $(ALVO)
	@sh tests/roda_estresse.sh ./$(ALVO)

asan:
	@$(MAKE) --no-print-directory BINDIR=bin-asan \
		CFLAGS="-std=c99 -Wall -Wextra -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer" \
		test estresse

else

test: $(ALVO)
	@powershell -NoProfile -Command "$$ok=0; $$falhas=0; Write-Host '========== CASOS VALIDOS =========='; Get-ChildItem tests/validos/*.mac | ForEach-Object { Write-Host ''; Write-Host ('>>> ' + $$_.Name); & '$(BINDIR)/macaronica' $$_.FullName; if ($$LASTEXITCODE -eq 0) { $$ok++ } else { $$falhas++; Write-Host ('(FALHOU: deveria ser valido, saiu ' + $$LASTEXITCODE + ')') } }; Write-Host ''; Write-Host '========== CASOS INVALIDOS =========='; Get-ChildItem tests/invalidos/*.mac | ForEach-Object { Write-Host ''; Write-Host ('>>> ' + $$_.Name); & '$(BINDIR)/macaronica' $$_.FullName; if ($$LASTEXITCODE -eq 1) { $$ok++ } else { $$falhas++; Write-Host ('(FALHOU: deveria ser invalido, saiu ' + $$LASTEXITCODE + ')') } }; Write-Host ''; Write-Host ('RESUMO: ' + $$ok + ' caso(s) corretos, ' + $$falhas + ' falha(s)'); Write-Host 'AVISO: aqui so rodaram os casos validos e invalidos do front-end. Os testes de mensagem, de execucao e da placa precisam do Ubuntu (WSL): veja o README.'; if ($$falhas -gt 0) { exit 1 } else { exit 0 }"

estresse asan:
	@echo Esses alvos precisam de shell POSIX: Linux, WSL ou Git Bash.
	@exit /b 1

bench roda placa simula gravar:
	@echo Esse alvo so roda no Linux ou no WSL. Veja o README, secao Ferramentas e versoes.
	@exit /b 1

endif

.PHONY: all test clean estresse asan bench roda placa simula gravar
