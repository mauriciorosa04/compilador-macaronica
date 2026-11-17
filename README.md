# Compilador da Linguagem Macaronica

**Integrantes:** Mauricio, Eduardo e Joao Augusto
**Disciplina:** Linguagens Formais e Compiladores
**Linguagem implementada:** Macaronica

Compilador da Macaronica escrito em C (C99). Lê um programa `.mac`,
valida (léxico, sintático e semântico) e gera LLVM IR. O programa gerado
roda no PC e num Arduino Uno a partir do mesmo `.ll`.

| parte   | o que tem                                                              |
|---------|------------------------------------------------------------------------|
| PjBL 1  | front-end: lexer, parser LL(1), tabela de símbolos, semântica          |
| PjBL 3  | back-end: geração de LLVM IR portável e biblioteca de execução         |
| PjBL 4  | operações vetoriais com núcleos em MLIR, estudo de otimização e placa  |

## Ferramentas e versões

O compilador em si só precisa de `gcc` e `make`, e compila em Linux e em
Windows (MinGW). Para rodar os programas gerados precisa do LLVM, e para a
placa precisa do ferramental AVR. Testamos tudo com estas versões, no
Ubuntu 24.04 (dentro do WSL2):

| ferramenta                                   | versão  | pacote do Ubuntu            |
|----------------------------------------------|---------|-----------------------------|
| `gcc`, `make`                                | 13.2, 4.3 | `build-essential`         |
| `clang-18`                                   | 18.1.3  | `clang-18`                  |
| `lli-18`, `llc-18`, `opt-18`, `llvm-as-18`, `llvm-link-18` | 18.1.3 | `llvm-18`      |
| `mlir-opt-18`, `mlir-translate-18`           | 18.1.3  | `mlir-18-tools`             |
| OpenMP (só para o `make bench`)              | 18.1.3  | `libomp-18-dev`             |
| `avr-gcc`, avr-libc, `avr-objcopy`           | 7.3.0, 2.0.0, 2.26 | `gcc-avr`, `avr-libc`, `binutils-avr` |
| `avrdude`                                    | 7.1     | `avrdude`                   |
| `simavr`                                     | 1.6     | `simavr`                    |
| `python3`, matplotlib (só para o `make bench`) | 3.12, 3.6 | `python3-matplotlib`    |

```
sudo apt install build-essential clang-18 llvm-18 mlir-18-tools libomp-18-dev \
                 gcc-avr avr-libc binutils-avr avrdude simavr python3-matplotlib
```

O `Dockerfile` instala esses mesmos pacotes numa imagem do Ubuntu 24.04:

```
docker build -t macaronica .
docker run --rm macaronica            # roda o make test
```

## Compilação

```
make
```

Gera `bin/macaronica` e uma cópia com o nome `./compilador`. Compila sem
nenhum warning com `-std=c99 -Wall -Wextra`.

## Uso

```
./compilador programa.mac                 # só valida e mostra o relatório
./compilador programa.mac -o programa.ll  # gera LLVM IR
./compilador --alvo=uno programa.mac -o programa.ll   # confere se cabe no Arduino Uno
./compilador --tokens programa.mac        # mostra os tokens
./compilador --ast programa.mac           # mostra a árvore sintática anotada
./compilador -h                           # ajuda
```

Com `-o`, programa válido gera o arquivo e não escreve nada na tela.
Programa inválido mostra os erros e não gera arquivo. Se o programa tem
operação vetorial, sai também um `programa.mlir` com os núcleos.

Códigos de saída:

- `0` a entrada é válida;
- `1` a entrada tem erro léxico, sintático ou semântico;
- `2` problema de uso (opção que não existe, arquivo que não abre).

`AVISO SEMANTICO` (variável nunca usada, variável lida antes de receber
valor) não é erro: sai na saída de erro, mas o programa continua válido.

### Do programa à execução

No PC, num comando:

```
make roda PROG=tests/execucao/04_recursao.mac
```

Na mão, é isto que ele faz:

```
./compilador prog.mac -o prog.ll
clang-18 -Wno-override-module prog.ll runtime/zl_runtime_posix.c -o prog -lm
./prog
```

ou, sem gerar executável:

```
clang-18 -S -emit-llvm runtime/zl_runtime_posix.c -o rt.ll
lli-18 --extra-module=rt.ll prog.ll
```

Na placa:

```
make placa  PROG=tests/placa/04_pisca_led.mac     # gera placa/04_pisca_led.hex
make simula PROG=tests/placa/04_pisca_led.mac     # roda no simulador simavr
make gravar PROG=tests/placa/04_pisca_led.mac PORTA=/dev/ttyACM0
```

## Testes

```
make test
```

Roda tudo e sai com erro se qualquer caso falhar:

| bateria                    | o que confere                                                   | casos |
|----------------------------|-----------------------------------------------------------------|-------|
| `tests/validos`, `invalidos` | PjBL 1: válido sai 0, inválido sai 1                          | 16    |
| `tests/diagnostico`        | a mensagem de erro exata, linha e coluna                        | 39    |
| `tests/dsl/invalidos`      | erros semânticos da extensão vetorial                           | 3     |
| `tests/execucao`           | gera o `.ll`, passa no `llvm-as` e no verificador, roda por `lli` e como executável, compara a saída | 20 |
| `tests/dsl`                | operações vetoriais, com os núcleos descendo pelo MLIR          | 5     |
| `tests/placa` no PC        | os programas da placa, com a mesma saída esperada               | 5     |
| `tests/placa` no `simavr`  | os mesmos programas montados para o Arduino Uno                 | 5     |

A parte de execução também confere que nenhum programa inválido gera
arquivo com `-o` e que o IR dos programas válidos do PjBL 1 passa no
verificador.

Sem o LLVM instalado (por exemplo no Windows, fora do WSL) o `make test`
roda a parte do front-end e avisa que a de execução não rodou.

Outros alvos:

```
make estresse    # entradas gigantes geradas na hora (cadeias de 200 mil termos...)
make asan        # recompila com AddressSanitizer e UBSan e roda tudo de novo
make bench       # estudo de otimização: mede, gera tabela e gráficos
```

## Estrutura

```
├── src/
│   ├── main.c            linha de comando; junta as fases
│   ├── lexer.c/.h        análise léxica (AFD, linha e coluna)
│   ├── parser.c/.h       análise sintática (LL(1), AST, modo pânico)
│   ├── ast.c/.h          árvore sintática abstrata
│   ├── symtab.c/.h       tabela de símbolos (hash por escopo)
│   ├── semantic.c/.h     tipos, escopo, assinaturas, alvo
│   ├── codegen_llvm.c/.h geração de LLVM IR a partir da AST anotada
│   ├── codegen_mlir.c/.h núcleos das operações vetoriais em MLIR
│   └── erros.c/.h        mensagens e contagem de erros
├── runtime/
│   ├── zl_runtime.h          interface da biblioteca de execução
│   ├── zl_runtime_comum.inc  parte igual em toda plataforma
│   ├── zl_runtime_posix.c    PC
│   └── zl_runtime_uno.c      Arduino Uno
├── scripts/
│   ├── pipeline.sh       MLIR até LLVM IR, um comentário por passo
│   ├── compila.sh        do .mac ao executável do PC
│   ├── placa.sh          do .mac ao .hex do Arduino Uno
│   ├── simula.sh         roda um .elf no simavr
│   └── bench/            medição, tabela e gráficos
├── tests/
│   ├── validos/  invalidos/  diagnostico/  estresse/    (front-end)
│   ├── execucao/         programas com saída esperada
│   ├── dsl/              extensão vetorial
│   └── placa/            programas da placa
├── docs/
│   ├── decisoes.md             gramática e decisões do front-end (D1-D20)
│   ├── decisoes_backend.md     tipos, decisões B1-B21, mem2reg e phi
│   ├── dsl.md                  especificação da extensão vetorial
│   ├── relatorio_otimizacao.md MLIR e estudo de otimização
│   ├── placa.md                Arduino Uno
│   └── resultados/             medidas, tabelas e gráficos do make bench
├── placa/                .hex prontos para gravar
├── Dockerfile
├── Makefile
└── README.md
```

## Documentação

- `docs/decisoes.md`: gramática EBNF e as decisões D1 a D20 do front-end.
- `docs/decisoes_backend.md`: como cada construção vira LLVM IR, as
  decisões B1 a B21 (conversões, `/` e `//`, curto-circuito, erros de
  execução, formato de impressão, largura dos tipos) e a análise do
  `mem2reg` com as instruções `phi`.
- `docs/dsl.md`: tokens, gramática e regras das operações vetoriais.
- `docs/relatorio_otimizacao.md`: o núcleo em cada nível do MLIR, o
  pipeline, as medidas e a pergunta da redução de ponto flutuante.
- `docs/placa.md`: construção, gravação, verificação de memória e tamanho
  dos programas no Arduino Uno.

## Declaração de uso de IA

Usamos ferramenta de IA generativa (Claude) como apoio no
desenvolvimento deste trabalho, nas duas partes. Onde exatamente:

- No código C de `src/` e de `runtime/`: a divisão em módulos e a escrita
  do código foram feitas com apoio da ferramenta, a partir da
  especificação da linguagem e dos requisitos do enunciado. Isso inclui o
  gerador de LLVM IR, o gerador dos núcleos em MLIR e as bibliotecas de
  execução do PC e do Arduino Uno. A gente revisou e ajustou o que veio.
- Nos scripts de `scripts/` e de `tests/` (pipeline do MLIR, construção
  para a placa, medição e execução dos testes): mesma coisa.
- Nos casos de teste de `tests/`, incluindo as saídas esperadas.
- Na redação dos documentos de `docs/` e deste README, partindo da
  especificação, do código e dos resultados medidos. As medidas de
  `docs/resultados/` saíram da execução do `make bench` na nossa máquina.

Todo mundo do grupo revisou o que está sendo entregue e consegue
explicar e defender qualquer trecho.
