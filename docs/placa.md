# Execução em placa: Arduino Uno

O mesmo `.ll` que roda no PC vira um `.hex` para o Arduino Uno. Só muda o
que acontece depois do compilador: quem escolhe o alvo é o `llc`, e a
biblioteca de execução é a da placa.

---

## 1. A placa

| item             | valor                                                    |
|------------------|----------------------------------------------------------|
| placa            | Arduino Uno R3                                           |
| microcontrolador | ATmega328P, AVR de 8 bits, 16 MHz                        |
| flash            | 32 KB (32256 bytes livres, o resto é do bootloader)      |
| RAM              | 2048 bytes                                               |
| sistema          | nenhum: o programa é a única coisa rodando               |
| tripla           | `avr`                                                    |
| `-mcpu`          | `atmega328p`                                             |
| executável       | Intel HEX (`placa/*.hex`)                                |
| saída            | serial pelo cabo USB, 115200 bps, 8N1                    |

## 2. Ferramentas e versões

| ferramenta        | versão                      | para quê                                |
|-------------------|-----------------------------|-----------------------------------------|
| `llc-18`          | LLVM 18.1.3                 | LLVM IR para código de máquina do AVR   |
| `avr-gcc`         | 7.3.0                       | compila a biblioteca e faz a ligação    |
| avr-libc          | 2.0.0                       | código de partida e contas de 32 bits   |
| `avr-objcopy`     | binutils 2.26               | ELF para HEX                            |
| `avrdude`         | 7.1                         | grava na placa                          |
| `simavr`          | 1.6                         | simulador, usado no `make test`         |

Todas vêm dos pacotes do Ubuntu 24.04 (`llvm-18`, `gcc-avr`, `avr-libc`,
`binutils-avr`, `avrdude`, `simavr`).

---

## 3. Como construir e gravar

```
make placa PROG=tests/placa/01_laco_e_vetor.mac
```

Gera `placa/01_laco_e_vetor.hex` e mostra quanto ocupou. O que o alvo faz
está em `scripts/placa.sh`, um comando por passo:

```
# 1. Macaronica -> LLVM IR (o mesmo .ll do PC; --alvo=uno só liga as conferências)
./bin/macaronica --alvo=uno prog.mac -o build/placa/prog.ll

# 2. LLVM IR -> código do AVR. É só aqui que o alvo aparece.
llc-18 -mtriple=avr -mcpu=atmega328p -filetype=obj \
       -function-sections -data-sections build/placa/prog.ll -o build/placa/prog.o

# 3. biblioteca de execução da placa
avr-gcc -mmcu=atmega328p -Os -Wall -std=c99 -ffunction-sections -fdata-sections \
        -c runtime/zl_runtime_uno.c -o build/placa/zl_runtime_uno.o

# 4. ligação
avr-gcc -mmcu=atmega328p -Wl,--gc-sections build/placa/prog.o \
        build/placa/zl_runtime_uno.o -o build/placa/prog.elf -lm

# 5. ELF -> HEX
avr-objcopy -O ihex -R .eeprom build/placa/prog.elf placa/prog.hex

# 6. tamanho
avr-size build/placa/prog.elf
```

Se o programa usa operação vetorial, entre o passo 2 e o 3 os núcleos em
MLIR descem pelo `scripts/pipeline.sh` e viram mais um objeto.

Para rodar sem a placa, no simulador:

```
make simula PROG=tests/placa/01_laco_e_vetor.mac
```

Para gravar, com a placa no USB:

```
make gravar PROG=tests/placa/01_laco_e_vetor.mac PORTA=/dev/ttyACM0
```

que é o mesmo que

```
avrdude -c arduino -p m328p -P /dev/ttyACM0 -b 115200 -U flash:w:placa/01_laco_e_vetor.hex:i
```

A porta é `/dev/ttyACM0` no Uno original e `/dev/ttyUSB0` nas placas com
o conversor CH340. Placa antiga, com o bootloader velho, grava a 57600.

**No Windows com WSL** o Linux não enxerga o USB sozinho. Tem dois
caminhos. Nenhum dos dois foi testado por nós com a placa na mão; no
simulador está tudo conferido.

### Caminho 1: gravar pelo Windows, com o avrdude da IDE do Arduino

É o mais simples. O `.hex` é gerado no WSL e gravado pelo Windows.

1. Instale a IDE do Arduino 2 e abra uma vez, com internet. Na primeira
   abertura ela baixa o pacote "Arduino AVR Boards", e o `avrdude` vem
   junto. Ele fica em
   `%LOCALAPPDATA%\Arduino15\packages\arduino\tools\avrdude\<versão>` e não
   entra no PATH.
2. Ligue a placa e veja no Gerenciador de Dispositivos, em Portas (COM e
   LPT), qual porta ela ganhou. Feche o monitor serial da IDE, se estiver
   aberto: a porta só aceita um programa por vez.
3. No PowerShell, na pasta do projeto, trocando `COM3` pela porta:

```
$av = (Get-ChildItem "$env:LOCALAPPDATA\Arduino15\packages\arduino\tools\avrdude" -Directory | Select-Object -Last 1).FullName
& "$av\bin\avrdude.exe" -C "$av\etc\avrdude.conf" -c arduino -p m328p -P COM3 -b 115200 -D -U flash:w:placa\prog.hex:i
```

A primeira linha acha a pasta do `avrdude`; a segunda grava. Os
parâmetros são os que a própria IDE usa para o Uno. Clone com o chip CH340
usa o mesmo comando, só muda a porta.

### Caminho 2: passar o USB para o WSL e usar o `make gravar`

1. PowerShell como administrador:
   `winget install --interactive --exact dorssel.usbipd-win`. Depois feche
   e abra o PowerShell de novo.
2. Deixe uma janela do Ubuntu aberta.
3. PowerShell como administrador: `usbipd list` mostra os dispositivos.
   Anote o BUSID da linha da placa (por exemplo `1-4`) e rode
   `usbipd bind --busid 1-4`.
4. PowerShell normal: `usbipd attach --wsl --busid 1-4`. Tem que repetir
   toda vez que religar a placa.
5. No Ubuntu, uma vez só, para poder usar a porta serial:
   `sudo usermod -aG dialout $USER`. Depois feche o Ubuntu, rode
   `wsl --shutdown` no PowerShell e abra o Ubuntu de novo. Sem isso o
   `avrdude` responde `cannot open port ...: Permission denied`.
6. No Ubuntu, `ls /dev/ttyACM* /dev/ttyUSB*` mostra a porta: `ttyACM0` no
   Uno original, `ttyUSB0` no clone com CH340. Aí é o `make gravar` de
   cima.
7. Enquanto a placa está no WSL o Windows não enxerga a porta. Para
   devolver (e usar o monitor serial da IDE): `usbipd detach --busid 1-4`.

Para ver a saída: qualquer monitor serial a 115200 (o da IDE do Arduino
serve). A placa reinicia quando o monitor abre, então o programa roda de
novo desde o começo.

---

## 4. Biblioteca de execução da placa

`runtime/zl_runtime_uno.c`. Não usa `Arduino.h`: mexe direto nos
registradores do ATmega328P. Tem as mesmas funções de
`runtime/zl_runtime.h` que a do PC, menos as de `duplocarpado`.

- **Saída e entrada** pela USART0, que é a serial do cabo USB.
- **Formatação de número** é a mesma do PC, porque as duas bibliotecas
  incluem `zl_runtime_comum.inc`. Não usa `printf`: é variádica e gasta
  flash demais.
- **Erro de execução** aparece de dois jeitos. A mensagem sai na serial,
  igual à do PC (`erro de execucao na linha 8: indice fora dos limites do
  vetor`), e o LED da placa (pino 13) pisca 5 vezes e fica aceso.
- **Fim do programa**: não existe sistema para onde voltar. A biblioteca
  desliga as interrupções e põe o processador para dormir até o reset.
  O valor do `respost` da `principal` é ignorado.
- **`^` e `raiz`** usam `pow` e `sqrt` da avr-libc, que lá são de 32 bits.

---

## 5. E/S física

Quatro funções já vêm declaradas na linguagem. Elas entram no escopo
global da tabela de símbolos antes de o programa ser analisado, então
passam pela mesma conferência de qualquer função: chamar com argumento a
menos ou com tipo errado dá o erro semântico de sempre.

| função                     | o que faz                                             |
|----------------------------|-------------------------------------------------------|
| `pinomodo(pino, modo)`     | modo 0 entrada, 1 saída, 2 entrada com pull-up        |
| `pinoescreve(pino, valor)` | 0 desliga, qualquer outro valor liga                  |
| `pinole(pino)`             | devolve 0 ou 1                                        |
| `espera(ms)`               | para por esse tanto de milissegundos                  |

No IR elas viram chamada a `zl_pinomodo`, `zl_pinoescreve`, `zl_pinole` e
`zl_espera`, da biblioteca.

Exemplo, `tests/placa/04_pisca_led.mac`: pisca três vezes o LED que já
vem soldado na placa (pino 13), lendo o pino de volta a cada troca.

```
inteira principal() @
[
    inteira i, lido;

    pinomodo(13, 1);
    repete ( i receba 0, i < 3, i receba i + 1 ) edai
    [
        pinoescreve(13, 1);
        lido receba pinole(13);
        escreva(lido);
        espera(200);

        pinoescreve(13, 0);
        lido receba pinole(13);
        escreva(lido);
        espera(200);
    ]
    escreva("fim");
    respost 0;
]
```

No PC não existe pino. A biblioteca do PC guarda o último valor escrito
em cada um, `pinole` devolve esse valor e `espera` não espera. Assim o
mesmo programa roda e dá a mesma saída no `make test`.

---

## 6. Verificação de memória: `--alvo=uno`

Com `--alvo=uno` o compilador faz duas conferências a mais, e as duas são
erro semântico com linha e coluna.

**`duplocarpado` não existe no alvo.** O avr-gcc trata `double` como 32
bits e não tem as rotinas de 64. Ver B8 em `docs/decisoes_backend.md`.

```
[ERRO SEMANTICO] linha 8, coluna 18: duplocarpado nao existe no alvo uno:
a placa nao tem ponto flutuante de 64 bits (use flut)
```

**As variáveis têm que caber na RAM.** A placa tem 2048 bytes para tudo:
variáveis globais, textos, variáveis da biblioteca e pilha. O compilador
soma

```
globais + textos + variáveis da principal + variáveis da maior das outras funções
```

e recusa o programa se passar de 1600 bytes. Os 448 restantes ficam para
a biblioteca e para os endereços de retorno das chamadas. Tamanho de cada
tipo na placa: `inteira` e `flut` 4 bytes, `bool` 1, `palavra` 2 (é um
ponteiro), vetor é o tamanho vezes o elemento, registro é a soma dos
campos.

Programa recusado, `tests/diagnostico/placa_vetor_nao_cabe_na_ram.mac`:

```
inteira principal() @
[
    inteira vet{500} v;
    v{0} receba 1;
    respost v{0};
]
```

```
[ERRO SEMANTICO] linha 3, coluna 22: o programa precisa de 2000 bytes de RAM
pra variaveis e textos, e no alvo uno cabem 1600 (2048 da placa menos 448
reservados pra biblioteca e pra pilha); a maior variavel e' 'v', com 2000 bytes
```

Sem `--alvo=uno` o mesmo programa é válido e roda no PC.

O que a conta não pega: recursão funda. Quantas vezes uma função chama a
si mesma só se sabe rodando. A reserva cobre o caso comum (o teste de
recursão vai até 15 níveis); um programa que desça centenas de níveis
estoura a pilha na placa sem aviso.

---

## 7. Decisões que dependem do alvo

### P1 - Qual é o maior vetor?
O que couber nos 1600 bytes junto com o resto do programa. Sozinho na
`principal`, um vetor de `inteira` ou de `flut` vai até 400 elementos e um
de `bool` até 1600. Na prática é menos, porque os textos do programa
também contam.

### P2 - Número dos pinos
É o número impresso na placa: 0 a 13 para os digitais e 14 a 19 para A0 a
A5. A linguagem não inventa numeração própria; quem olha a placa sabe o
número. Pino fora dessa faixa é ignorado pela biblioteca.

### P3 - Nome das funções embutidas
`pinomodo`, `pinoescreve`, `pinole`, sem `_`. O alfabeto da Macaronica
não tem `_` (D3 do PjBL 1), então um nome com ele nem passaria no léxico.

### P4 - `duplocarpado`
Rejeitado no alvo (seção 6 e B8).

### P5 - Largura dos tipos
`inteira` continua com 32 bits, mesmo num processador de 8. As contas de
32 bits viram chamadas a rotinas da avr-libc, que a ligação traz sozinha.
O programa dá o mesmo resultado que no PC; só é mais lento.

### P6 - Textos ficam na RAM
Cada literal de `palavra` vai para a seção `.data`, que no AVR é copiada
da flash para a RAM na partida. Dava para deixar na flash e ler de lá,
mas aí o IR teria que ser diferente para a placa. Preferimos manter um
`.ll` só e contar os textos na verificação de memória.

### P7 - Núcleos vetoriais sem SIMD
O ATmega328P não tem instrução de vetor. Os núcleos das operações
vetoriais descem pelo mesmo pipeline do PC, no modo `escalar`, com o tipo
`index` do MLIR virando `i32`. Viram laços comuns. A vetorização e o
OpenMP do estudo de otimização não se aplicam aqui.

### P8 - `leia`
Lê da mesma serial. Como não existe "fim da entrada" numa serial, a placa
fica esperando alguém digitar; o erro de "entrada acabou" do PC não
acontece. Número que não cabe em `inteira` dá erro de leitura, igual no PC.

---

## 8. Programas da placa e tamanho

Os programas ficam em `tests/placa/`, com a saída esperada, e os `.hex`
prontos em `placa/`.

| programa                  | o que mostra                              | flash (bytes) | RAM fixa (bytes) |
|---------------------------|-------------------------------------------|---------------|------------------|
| `01_laco_e_vetor`         | dois laços e um vetor                     | 3494          | 173              |
| `02_recursao`             | fatorial e Fibonacci, `^`, `raiz`, `/`    | 3102          | 13               |
| `03_erro_de_execucao`     | índice fora do vetor: mensagem e LED      | 1230          | 179              |
| `04_pisca_led`            | E/S física: `pinomodo`, `pinoescreve`...  | 1518          | 5                |
| `05_operacoes_vetoriais`  | núcleos em MLIR rodando na placa          | 4672          | 161              |

Flash é `text + data` do `avr-size`, de 32256 disponíveis. RAM fixa é
`data + bss`, de 2048; o que sobra é pilha. O maior programa usa 14% da
flash.

### Mesma saída do PC

Cada programa de `tests/placa/` é rodado de dois jeitos pelo `make test`,
contra o mesmo arquivo `.esperado`:

- no PC, pelo `tests/roda_execucao.sh` (com `lli` e como executável);
- no simulador da placa, pelo `tests/roda_placa.sh`: monta o `.hex` de
  verdade, roda o `.elf` no `simavr` e compara o que saiu pela serial.

No caso do erro de execução a comparação inclui a mensagem, que na placa
sai pela mesma serial.

Com isso o mesmo `.ll` está sendo executado em duas arquiteturas bem
diferentes: x86-64 de 64 bits e AVR de 8 bits.

### Vídeo

<https://youtube.com/shorts/rEROxNEwKCw>

O programa `04_pisca_led` rodando no Arduino Uno: o LED da placa piscando
e a saída no monitor serial.
