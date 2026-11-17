# Decisões do back-end - geração de LLVM IR

Este arquivo cobre a segunda parte do compilador: como a árvore do PjBL 1
vira LLVM IR, que decisões a gente teve que tomar onde a linguagem não
dizia nada, e o que o `mem2reg` faz com o código que a gente gera.

As decisões estão numeradas B1 a B21. As oito primeiras são as lacunas que
o enunciado manda resolver; as outras a gente achou escrevendo o gerador.

---

## 1. Como o gerador está montado

```
fonte -> lexer -> parser -> AST -> semântica -> AST anotada -> codegen_llvm -> .ll
```

O gerador é um módulo só, `src/codegen_llvm.c`, com uma função pública:

```c
void gerar_llvm(Ast *programa, const char *nome_fonte, FILE *saida);
```

Ele só é chamado depois que a semântica passou sem erro, e não confere mais
nada: confia na árvore. Duas anotações que a semântica deixa nos nós são o
que permite isso.

- `tipo_dado`: o tipo de cada expressão. É dele que sai qual instrução
  usar (`add` ou `fadd`) e onde entra conversão.
- `decl`: cada uso aponta para a declaração. Uma variável aponta para o nó
  onde foi declarada, uma chamada aponta para a função, um acesso a campo
  aponta para o campo dentro do registro.

Por causa do `decl` o gerador não tem tabela de símbolos nem regra de
escopo. Quem resolveu "qual `x` é esse" foi a semântica, uma vez só. O
gerador só segue o ponteiro. Se a regra de escopo mudar um dia, muda em um
lugar.

Uso pela linha de comando:

```
./compilador programa.mac -o programa.ll
```

Sem `-o` o programa só é validado, igual ao PjBL 1. Com `-o` e programa
válido, o `.ll` é gravado e nada sai na tela. Com `-o` e programa inválido,
os erros saem como antes, o código de saída é 1 e o arquivo não é criado.
Se já existia um `.ll` antigo com aquele nome, ele é apagado, para ninguém
rodar código velho achando que é o novo.

---

## 2. Tipos

| Macaronica     | LLVM                  | Observação                                  |
|----------------|-----------------------|---------------------------------------------|
| `inteira`      | `i32`                 | 32 bits com sinal em qualquer alvo          |
| `flut`         | `float`               | IEEE 754 de 32 bits                         |
| `duplocarpado` | `double`              | IEEE 754 de 64 bits                         |
| `bool`         | `i1`                  | `VER` é `true`, `FAL` é `false`             |
| `palavra`      | `ptr`                 | aponta para um `[N x i8]` terminado em zero |
| `vazio`        | `void`                | só como retorno de função                   |
| vetor          | `[N x T]`             | tamanho fixo, conhecido na compilação       |
| registro       | `%struct.Nome`        | um campo depois do outro, na ordem do fonte |

---

## 3. O que cada construção vira

| Construção                  | LLVM IR                                                              |
|-----------------------------|----------------------------------------------------------------------|
| variável local, parâmetro   | `alloca` no bloco de entrada, depois `load` e `store`                |
| variável global             | `@mac_nome = global T 0` (ou `zeroinitializer`)                      |
| `+ - *`                     | `add sub mul` ou `fadd fsub fmul`                                    |
| `/`                         | conversão dos dois lados e `fdiv`                                    |
| `//`                        | `sdiv`, protegido contra divisor 0 e divisor -1                      |
| `^` e `raiz()`              | chamada a `zl_pot_*` e `zl_raiz_*`                                   |
| `< > == !`                  | `icmp slt/sgt/eq/ne` ou `fcmp olt/ogt/oeq/une`                       |
| `E`, `OU`                   | desvio com curto-circuito e `phi`                                    |
| `XOR`, `NAO`                | `xor i1`                                                             |
| `cond / casocontrario`      | blocos `cond.entao`, `cond.senao`, `cond.fim` com `br i1`            |
| `durante`                   | blocos `durante.teste`, `durante.corpo`, `durante.fim`               |
| `repete`                    | blocos `repete.teste`, `repete.corpo`, `repete.passo`, `repete.fim`  |
| função, chamada, `respost`  | `define ... nounwind`, `call`, `ret`                                 |
| `principal`                 | `define i32 @zl_principal() nounwind`                                |
| `v{i}`                      | teste de limite e `getelementptr [N x T], ptr, i32 0, i32 %i`        |
| `r.campo`                   | `getelementptr %struct.Nome, ptr, i32 0, i32 posição`                |
| literal de `palavra`        | `@.str.N = private unnamed_addr constant [N x i8] c"...\00"`         |
| `escreva`, `leia`           | chamada a `zl_imprime_*` e `zl_le_*`                                 |

Todo comando do fonte começa com um comentário `; linha N: ...` no IR, e os
nomes vêm do programa: `%total.addr` é o endereço de `total`, `%total.15` é
um valor lido de `total`, `%soma.18` é o resultado de uma soma.

---

## 4. Lacunas que o enunciado manda resolver

### B1 - Promoção e conversão entre os tipos numéricos
A conversão implícita só sobe: `inteira -> flut -> duplocarpado`. É a regra
D9 do PjBL 1. Para baixo não existe: `inteira x; x receba 2.5;` continua
sendo erro semântico, então o gerador nunca precisa de `fptosi` nem de
`fptrunc`.

Em toda operação com tipos diferentes, o lado mais estreito é convertido
para o tipo do outro antes da conta, com uma instrução explícita:

| de        | para           | instrução                    |
|-----------|----------------|------------------------------|
| `inteira` | `flut`         | `sitofp i32 %x to float`     |
| `inteira` | `duplocarpado` | `sitofp i32 %x to double`    |
| `flut`    | `duplocarpado` | `fpext float %x to double`   |

O mesmo vale para atribuição, argumento de função e `respost`: o valor é
convertido para o tipo do destino.

Literal com ponto é sempre `flut`. Não tem sufixo para escrever um literal
`duplocarpado`; `d receba 0.1` guarda em `d` o `0.1` de 32 bits estendido.
Preferimos uma regra só (o tipo do literal não depende de onde ele aparece)
a adivinhar pelo contexto.

### B2 - `/` e `//`
`/` é sempre divisão real. Entre duas `inteira` os dois lados viram `flut`
e a conta é `fdiv`: `7 / 2` dá `3.500000`. Com `flut` ou `duplocarpado` no
meio, a conta é feita no maior dos tipos.

`//` é divisão inteira e só aceita `inteira` dos dois lados; com `flut` é
erro semântico (D10). O resultado trunca em direção a zero, como em C:
`-7 // 2` dá `-3`. É o que o `sdiv` faz, e é o que a maioria das pessoas
espera de quem veio do C.

| expressão        | tipo           | instruções                      | resultado  |
|------------------|----------------|---------------------------------|------------|
| `7 / 2`          | `flut`         | `sitofp`, `sitofp`, `fdiv`      | `3.500000` |
| `7 // 2`         | `inteira`      | `sdiv`                          | `3`        |
| `7.0 / 2`        | `flut`         | `sitofp`, `fdiv`                | `3.500000` |
| `d / 2` (duplo)  | `duplocarpado` | `sitofp ... to double`, `fdiv`  |            |
| `7.0 // 2`       | erro semântico |                                 |            |

### B3 - `^` e `raiz()`
As duas viram chamada à biblioteca de execução, e não intrínseco do LLVM.
A gente testou os intrínsecos antes de decidir, e eles não sobrevivem na
placa: `llvm.powi` com expoente `i32` nem compila para o AVR
(`POWI exponent does not match sizeof(int)`), e `llvm.pow.f32` vira chamada
a `powf`, que a avr-libc não tem. Com a conta na biblioteca o `.ll` não
cita nome nenhum de `libm` e cada plataforma resolve do seu jeito.

Tipo do resultado:

- `inteira ^ inteira` dá `inteira`, calculada com inteiros por quadrados
  sucessivos (`zl_pot_int`). É exata: não passa por ponto flutuante, então
  `3 ^ 2` é `9` e não `8.999999`. Expoente negativo dá o inverso truncado,
  que é `0` (só sobra `1` ou `-1` quando a base é `1` ou `-1`).
- se um dos lados é `flut` ou `duplocarpado`, a conta é feita nesse tipo.
- `raiz()` de `inteira` ou `flut` dá `flut`; de `duplocarpado` dá
  `duplocarpado`. Raiz de número negativo dá `nan`, sem parar o programa.

`^` continua associativa à direita: `2 ^ 3 ^ 2` é `2 ^ 9 = 512`.

### B4 - `E`, `OU`, `XOR`
`E` e `OU` têm curto-circuito: o lado direito só é avaliado se o esquerdo
não decidiu a resposta. Isso não é só otimização, muda o que o programa
faz. Em

```
cond ( i < 3 E v{i} > 0 ) edai [ ... ]
```

o `v{i}` nem é lido quando `i` é 3 ou mais, então a condição pode proteger
o acesso ao vetor. Sem curto-circuito esse programa pararia com erro de
índice. O teste `11_logicos_e_curto_circuito` mostra isso rodando.

O IR de `a E b` é um desvio e um `phi`:

```llvm
  br i1 %a, label %e.dir.9, label %e.fim.9
e.dir.9:
  ...                          ; calcula %b
  br label %e.fim.9
e.fim.9:
  %e.9 = phi i1 [ false, %bloco_de_onde_veio ], [ %b, %bloco_do_lado_direito ]
```

Se a execução chegou em `e.fim.9` direto, o resultado é `false`; se passou
pelo lado direito, é o valor dele. No `OU` é igual, trocando o desvio e a
constante por `true`.

`XOR` precisa sempre dos dois lados, então é só `xor i1 %a, %b`. `NAO` é
`xor i1 %a, true`.

### B5 - `respost` dentro de `principal`
O valor do `respost` da `principal` é o código de saída do programa. A
`principal` vira `define i32 @zl_principal()`, e o `main` de verdade, que
fica na biblioteca, devolve esse valor para o sistema. `respost 7;` faz o
programa sair com 7 (teste `15_codigo_de_saida`). O sistema só guarda os 8
bits de baixo, então `respost 300;` sai com 44.

Para isso fazer sentido, a `principal` só pode ser `inteira` ou `vazio` e
não recebe parâmetro (ninguém teria como passar). Qualquer outra forma é
erro semântico. `vazio principal()` sai sempre com 0.

Na placa não existe para quem devolver código de saída: o valor é ignorado
e o programa para.

Em função `vazio` o `respost;` vira `ret void`, e chegar no fim do corpo
sem `respost` também (D5 do PjBL 1).

### B6 - Divisão por zero, estouro e índice fora do vetor
Nada disso é comportamento indefinido aqui. A política é:

| situação                         | o que acontece                                         |
|----------------------------------|--------------------------------------------------------|
| `//` com divisor zero            | o programa para: erro de execução 2, com a linha       |
| `/` com divisor zero             | segue o IEEE 754: `inf` ou `nan`, sem parar            |
| estouro em `+ - *` de `inteira`  | dá a volta em complemento de dois                      |
| `-2147483648 // -1`              | dá a volta também: o resultado é `-2147483648`         |
| índice de vetor fora de `0..N-1` | o programa para: erro de execução 1, com a linha       |

A mensagem sai na saída de erro com a linha do fonte:

```
erro de execucao na linha 7: indice fora dos limites do vetor
```

e o processo sai com `100 + código` (101 para índice, 102 para divisão,
103 para leitura).

Três detalhes de como isso é gerado:

- O `sdiv` do LLVM é indefinido para divisor 0 e para
  `-2147483648 / -1`. A gente mediu: sem proteção o programa morre com
  SIGFPE no `lli`, e com `-O2` e zero constante ele nem para, imprime lixo
  e sai com 0. Por isso nenhum dos dois casos chega no `sdiv`. O divisor
  zero desvia para o erro. O divisor -1 é trocado por 1 com um `select` e o
  resultado vira `0 - x`, que dá a volta sozinho.
- O gerador nunca escreve `nsw` nem `nuw` nas contas. Com essas marcas o
  otimizador pode assumir que o estouro não acontece, e aí
  `x + 1 > x` com `x = 2147483647` dá um resultado sem `-O2` e outro com.
  Sem elas, `add i32` dá a volta do mesmo jeito em todo modo.
- O teste de limite do vetor é uma comparação só, sem sinal:
  `icmp ult i32 %i, N`. Índice negativo visto como número sem sinal é
  enorme, então cai fora junto com o `>= N`.

### B7 - Formato de impressão
`escreva(x)` imprime o valor e pula a linha, sempre.

| tipo                   | formato                                       | exemplo       |
|------------------------|-----------------------------------------------|---------------|
| `inteira`              | decimal                                       | `-42`         |
| `flut`, `duplocarpado` | parte inteira, ponto e 6 casas, arredondado   | `3.500000`    |
| `bool`                 | `VER` ou `FAL`                                | `VER`         |
| `palavra`              | o texto                                       | `ola`         |

Casos de borda dos reais: `nan`, `inf` e `-inf` saem assim; número grande
demais para ter a parte inteira escrita (acima de 2^32 em `flut`, de 2^64
em `duplocarpado`) sai com expoente, `1.234568e+15`.

A formatação é código nosso, em `runtime/zl_runtime_comum.inc`, sem
`printf`. O motivo é a placa: o mesmo arquivo é compilado para o PC e para
o Arduino, então a saída é igual caractere a caractere e o mesmo
`.esperado` serve para os dois.

### B8 - Largura dos tipos em qualquer plataforma
`inteira` é `i32` sempre, inclusive no Arduino Uno, que tem processador de
8 bits e `int` de 16 em C. O programa não pode dar resultado diferente
porque mudou de placa. Por isso a biblioteca usa `int32_t` em tudo e nunca
`int`.

`duplocarpado` é o caso difícil. O avr-gcc 7.3 do Uno trata `double` como
32 bits e a avr-libc não traz as rotinas de 64 (`__adddf3`, `__muldf3`,
`__extendsfdf2`): o programa compila e falha na ligação com
`undefined reference`. As saídas eram rebaixar para `float` em silêncio,
escrever as rotinas de 64 bits em software, ou rejeitar.

Rejeitamos: com `--alvo=uno`, usar `duplocarpado` é erro semântico, com a
linha. Rebaixar faria o mesmo programa dar resultados diferentes no PC e na
placa sem avisar ninguém, que é exatamente o que a largura fixa quer
evitar. E escrever as rotinas de 64 bits gastaria uma boa parte dos 32 KB
de flash com um tipo que quem programa para o Uno pode trocar por `flut`.

---

## 5. Lacunas que a gente achou

### B9 - Toda variável nasce zerada
A linguagem não diz o que vale uma variável antes da primeira atribuição.
Aqui ela vale zero: `0`, `0.0`, `FAL`, e texto vazio para `palavra`. Sem
isso cada modo de execução mostrava um lixo diferente para o mesmo
programa (a gente viu três valores diferentes em `lli`, `-O0` e `-O2`).

O `store` do zero fica no ponto da declaração, não no começo da função. A
diferença aparece em laço: uma variável declarada dentro do corpo é zerada
de novo a cada volta (teste `16_escopo_e_sombra`). Vetor local é zerado por
um laço pequeno no próprio IR, que serve para qualquer tamanho. Variável
global nasce zerada pelo inicializador, sem código.

O aviso do PjBL 1 de "pode estar sendo usado antes de receber valor"
continua existindo: o valor é definido, mas quase sempre é descuido.

### B10 - Registro ganhou variável e acesso a campo
No PjBL 1 o registro parava na declaração do tipo (D12), porque a
especificação não mostra como usar. Para gerar `%struct` e `getelementptr`
de campo precisa existir variável de registro, então a gente estendeu a
linguagem com o mínimo:

```
registro Aluno @ [ inteira matricula; flut media; inteira vet{3} notas; ]

registro Aluno a;            (variável, local ou global)
a.matricula receba 1234;     (campo)
a.notas{0} receba 7;         (campo que é vetor)
```

A declaração de variável reaproveita a palavra `registro`. O que vem depois
do nome do tipo decide: `@` abre a declaração do tipo, um nome declara
variável. Continua LL(1). O acesso usa `.`, que já existia no alfabeto por
causa dos números reais; solto ele virou um token novo.

O registro é usado campo a campo. Não dá para atribuir um registro inteiro,
passar como argumento nem imprimir; as três coisas são erro semântico com
mensagem própria. Era o suficiente para o que o trabalho pede e cada uma
dessas abriria mais decisões (cópia, passagem por valor ou referência).

### B11 - Prefixo `mac_` em tudo que é do programa
Função e variável global do programa ganham o prefixo `mac_` no IR:
`quadrado` vira `@mac_quadrado`. Sem isso uma função chamada `main` não
liga (símbolo duplicado com o `main` da biblioteca), e uma chamada `abs`
dá resultado errado com `-O2`, porque o otimizador reconhece o nome e
troca a chamada pelo `llvm.abs` dele. A gente viu os dois acontecerem.
Como identificador da Macaronica não tem `_`, nada que o programa declare
bate com um nome prefixado.

### B12 - Constante `flut` em hexadecimal
`store float 0.1` não monta: o LLVM recusa decimal que não é exato em
`float`. Toda constante `flut` sai no formato hexadecimal de 16 dígitos,
que são os bits do `double` com exatamente o valor do `float`
(`0.1` vira `0x3FB99999A0000000`).

### B13 - Todo `alloca` no bloco de entrada
Mesmo variável declarada dentro de laço tem o `alloca` no começo da
função. `alloca` dentro de laço reserva pilha de novo a cada volta (com 3
milhões de voltas o programa morreu com falha de segmentação), e o
`mem2reg` só promove o que está no bloco de entrada.

Como bloco interno pode repetir o nome de uma variável de fora, a de bloco
interno ganha um número: `%x.addr` é o `x` do corpo da função e
`%x.addr.1` é o `x` declarado dentro de um `cond`.

### B14 - Código depois de `respost`
Todo bloco básico do LLVM termina em exatamente uma instrução de desvio ou
retorno. Comando escrito depois de um `respost` vai para um bloco novo,
`morto.N`, que ninguém alcança. O IR continua válido e o otimizador joga o
bloco fora. Pelo mesmo motivo, em função com tipo a junção de um `cond` em
que todos os ramos já retornaram termina em `unreachable`.

### B15 - Sem `align`
O gerador não escreve `align` em `alloca`, `load` nem `store`. O `llc`
para AVR calcula endereço errado em `alloca` com `align` de 4 ou mais e
índice variável (a gente reproduziu o defeito). Sem `align` cada alvo usa o
alinhamento natural dele. A única exceção é `align 1` nas constantes de
texto.

### B16 - `leia` só lê número
`leia(x)` aceita `inteira`, `flut` e `duplocarpado`. Para `bool` e
`palavra` é erro semântico: a linguagem não define o formato de entrada
deles, e ler texto precisaria de memória dinâmica. Se a entrada acabou ou
não tem número, o programa para com o erro de execução 3.
Número que não cabe em `inteira` (fora de -2147483648 a 2147483647) também
é erro de leitura, no PC e na placa. Antes o valor dava a volta em silêncio
e o programa seguia com um número que ninguém digitou.

### B17 - Comparar `palavra` compara o texto
`a == b` entre duas `palavra` compara o conteúdo (`zl_texto_igual`), não o
ponteiro. Comparar ponteiro faria dois textos iguais darem diferente
dependendo de onde vieram.

### B18 - Ordem de avaliação
Operandos e argumentos são avaliados da esquerda para a direita. Na
atribuição o lado direito é calculado antes do endereço do destino. Em C
isso é indefinido; aqui é fixo, para o programa dar a mesma saída em todo
alvo.

### B19 - Cadeia longa de operadores não usa recursão
`a + b + c + ...` é uma árvore que desce pela esquerda. O gerador anda por
essa espinha com um laço, igual a semântica faz, então uma soma de 200 mil
termos não estoura a pilha do compilador.

### B20 - `-2147483648`
O menor valor de `inteira` não cabe como literal positivo. A forma
`-2147483648` é aceita como um caso só dela, e menos na frente de qualquer
literal vira constante negativa direto no IR.

### B21 - Menos unário liga mais forte que `^`
Na gramática o menos unário faz parte do fator, então ele é aplicado antes
da potência: `-2 ^ 2` é `(-2) ^ 2 = 4`, e não `-(2 ^ 2) = -4` como na
matemática. Quem quiser o outro sentido escreve `-(2 ^ 2)` ou `0 - 2 ^ 2`.
A especificação não define a precedência do menos unário. O expoente
também pode ser negativo: `2 ^ -1`.

---

## 6. IR portável

O mesmo `.ll` roda no PC e na placa. As regras e onde cada uma é
garantida:

| regra                                   | como o gerador cumpre                                   |
|-----------------------------------------|---------------------------------------------------------|
| sem `target triple` nem `datalayout`    | o gerador nunca escreve; o alvo vem do `clang` ou `llc` |
| tipos de largura fixa                   | tabela da seção 2                                       |
| sem função variádica                    | só chama `zl_*`, todas de assinatura fixa               |
| endereço só por `getelementptr`, `i32`  | `gen_endereco()` é o único lugar que calcula endereço   |
| `nounwind` em tudo                      | em todo `define` e todo `declare`                       |
| entrada em `@zl_principal`              | `main` fica na biblioteca                               |

`tests/confere_ir.sh` confere essas regras em cada `.ll` que o `make test`
gera. Se alguém mudar o gerador e quebrar uma, o teste acusa.

### Biblioteca de execução

`runtime/zl_runtime.h` é a única ponte com a plataforma.

| função                                   | para quê                                   |
|------------------------------------------|--------------------------------------------|
| `zl_imprime_int/flut/duplo/bool/texto`   | `escreva`, uma por tipo                    |
| `zl_imprime_nl`                          | pula a linha no fim do `escreva`           |
| `zl_le_int/flut/duplo`                   | `leia`                                     |
| `zl_erro_execucao(codigo, linha)`        | para o programa com mensagem               |
| `zl_pot_int/flut/duplo`                  | `^`                                        |
| `zl_raiz_flut/duplo`                     | `raiz()`                                   |
| `zl_texto_igual`                         | `==` e `!` entre palavras                  |

`zl_runtime_posix.c` é a versão do PC. `zl_runtime_comum.inc` é a parte
igual em toda plataforma (formatação de número, mensagem de erro, potência
inteira).

Como rodar um programa à mão:

```
./compilador prog.mac -o prog.ll
clang-18 -S -emit-llvm runtime/zl_runtime_posix.c -o rt.ll
lli-18 --extra-module=rt.ll prog.ll

clang-18 -Wno-override-module prog.ll runtime/zl_runtime_posix.c -o prog -lm
./prog
```

O `-Wno-override-module` só cala o aviso de que o `.ll` não tem alvo, que
é de propósito.

---

## 7. Verificação em tempo de execução só quando precisa

O teste de limite e o teste de divisor zero custam instruções. O gerador
só coloca quando não dá para saber a resposta na compilação.

Índice escrito como literal já foi conferido pela semântica (`v{9}` num
`vet{5}` é erro de compilação), então não ganha teste:

```llvm
  ; v{2} receba 31;
  %v.elem.4 = getelementptr inbounds [6 x i32], ptr %v.addr, i32 0, i32 2
  store i32 31, ptr %v.elem.4
```

Índice calculado ganha:

```llvm
  ; soma receba soma + v{i};
  %i.10 = load i32, ptr %i.addr
  %dentro.11 = icmp ult i32 %i.10, 6
  br i1 %dentro.11, label %idx.ok.11, label %idx.erro.11
idx.erro.11:
  call void @zl_erro_execucao(i32 1, i32 16)
  unreachable
idx.ok.11:
  %v.elem.12 = getelementptr inbounds [6 x i32], ptr %v.addr, i32 0, i32 %i.10
```

Com a divisão inteira é igual. `n // 2` tem divisor literal diferente de
zero e vira um `sdiv` só. `a // b` ganha o teste de zero e o tratamento do
-1. Os dois aparecem no IR de `tests/execucao/10_divisao_real_e_inteira.mac`.

---

## 8. `mem2reg` e as instruções `phi`

O código que a gente gera guarda toda variável na memória. Isso é fácil de
gerar, porque não precisa saber de onde vem cada valor, mas é lento. O
passo `mem2reg` do LLVM troca as variáveis de memória por valores em
registradores, e para isso precisa do `phi`.

O programa analisado é `tests/execucao/20_laco_com_condicional.mac`, que
tem um laço com um condicional dentro:

```
outrafuncao inteira somapares(inteira n) @
[
    inteira i, soma, resto;
    soma receba 0;
    repete ( i receba 0, i < n, i receba i + 1 ) edai
    [
        resto receba i - i // 2 * 2;
        cond ( resto == 0 ) edai
        [
            soma receba soma + i;
        ]
    ]
    respost soma;
]
```

Comandos:

```
./compilador tests/execucao/20_laco_com_condicional.mac -o antes.ll
opt-18 -S -passes=mem2reg antes.ll -o mem2reg.ll
opt-18 -S -O2 antes.ll -o o2.ll
```

### Antes: tudo na memória

```llvm
define i32 @mac_somapares(i32 %n) nounwind {
funcao.entrada:
  %n.addr = alloca i32
  %i.addr = alloca i32
  %soma.addr = alloca i32
  %resto.addr = alloca i32
  store i32 %n, ptr %n.addr
  ; linha 3: declaracao de 'i'
  store i32 0, ptr %i.addr
  ; linha 3: declaracao de 'soma'
  store i32 0, ptr %soma.addr
  ; linha 3: declaracao de 'resto'
  store i32 0, ptr %resto.addr
  ; linha 4: atribuicao a 'soma'
  store i32 0, ptr %soma.addr
  ; linha 5: repete
  store i32 0, ptr %i.addr
  br label %repete.teste.1
repete.teste.1:
  %i.2 = load i32, ptr %i.addr
  %n.4 = load i32, ptr %n.addr
  %cmp.3 = icmp slt i32 %i.2, %n.4
  br i1 %cmp.3, label %repete.corpo.1, label %repete.fim.1
repete.corpo.1:
  ; linha 7: atribuicao a 'resto'
  %i.5 = load i32, ptr %i.addr
  %i.6 = load i32, ptr %i.addr
  %div.7 = sdiv i32 %i.6, 2
  %mul.8 = mul i32 %div.7, 2
  %sub.9 = sub i32 %i.5, %mul.8
  store i32 %sub.9, ptr %resto.addr
  ; linha 8: cond
  %resto.11 = load i32, ptr %resto.addr
  %cmp.12 = icmp eq i32 %resto.11, 0
  br i1 %cmp.12, label %cond.entao.10, label %cond.fim.10
cond.entao.10:
  ; linha 10: atribuicao a 'soma'
  %soma.13 = load i32, ptr %soma.addr
  %i.14 = load i32, ptr %i.addr
  %soma.15 = add i32 %soma.13, %i.14
  store i32 %soma.15, ptr %soma.addr
  br label %cond.fim.10
cond.fim.10:
  br label %repete.passo.1
repete.passo.1:
  %i.16 = load i32, ptr %i.addr
  %soma.17 = add i32 %i.16, 1
  store i32 %soma.17, ptr %i.addr
  br label %repete.teste.1
repete.fim.1:
  ; linha 13: respost
  %soma.18 = load i32, ptr %soma.addr
  ret i32 %soma.18
}
```

### Depois do `mem2reg`

```llvm
define i32 @mac_somapares(i32 %n) #0 {
funcao.entrada:
  br label %repete.teste.1

repete.teste.1:                    ; preds = %repete.passo.1, %funcao.entrada
  %soma.addr.0 = phi i32 [ 0, %funcao.entrada ], [ %soma.addr.1, %repete.passo.1 ]
  %i.addr.0 = phi i32 [ 0, %funcao.entrada ], [ %soma.17, %repete.passo.1 ]
  %cmp.3 = icmp slt i32 %i.addr.0, %n
  br i1 %cmp.3, label %repete.corpo.1, label %repete.fim.1

repete.corpo.1:                    ; preds = %repete.teste.1
  %div.7 = sdiv i32 %i.addr.0, 2
  %mul.8 = mul i32 %div.7, 2
  %sub.9 = sub i32 %i.addr.0, %mul.8
  %cmp.12 = icmp eq i32 %sub.9, 0
  br i1 %cmp.12, label %cond.entao.10, label %cond.fim.10

cond.entao.10:                     ; preds = %repete.corpo.1
  %soma.15 = add i32 %soma.addr.0, %i.addr.0
  br label %cond.fim.10

cond.fim.10:                       ; preds = %cond.entao.10, %repete.corpo.1
  %soma.addr.1 = phi i32 [ %soma.15, %cond.entao.10 ], [ %soma.addr.0, %repete.corpo.1 ]
  br label %repete.passo.1

repete.passo.1:                    ; preds = %cond.fim.10
  %soma.17 = add i32 %i.addr.0, 1
  br label %repete.teste.1

repete.fim.1:                      ; preds = %repete.teste.1
  ret i32 %soma.addr.0
}
```

Os 4 `alloca`, os 9 `load` e os 9 `store` sumiram. A função caiu de
43 para 24 instruções, e nenhuma mexe mais com memória.

### Por que apareceram três `phi`

Em SSA cada valor é definido uma vez só. Enquanto a variável está na
memória isso não incomoda, porque o `store` pode repetir à vontade. Quando
ela vira valor, sobra um problema: num bloco onde chegam dois caminhos, e
cada caminho deixou um valor diferente para a mesma variável, qual vale? O
`phi` responde isso. Ele escolhe o valor conforme o bloco de onde a
execução veio.

1. `%soma.addr.0`, no cabeçalho do laço. Em `repete.teste.1` chega gente
   de dois lugares: de antes do laço, onde `soma` vale `0`, e do fim de uma
   volta (`repete.passo.1`), onde `soma` vale o que a volta deixou
   (`%soma.addr.1`).
2. `%i.addr.0`, no mesmo lugar e pelo mesmo motivo. Vindo de fora `i` vale
   `0`; vindo do passo vale `i + 1`. O valor de dentro se chama `%soma.17`
   só porque foi o nome que o nosso gerador deu para aquela soma.
3. `%soma.addr.1`, na junção do `cond`. Em `cond.fim.10` chega quem entrou
   no `cond`, com a `soma` nova (`%soma.15`), e quem pulou, com a `soma`
   que já tinha (`%soma.addr.0`).

Duas variáveis não ganharam `phi`, e isso também explica a regra:

- `n` nunca é atribuída depois de entrar. Todo `load` dela virou o próprio
  parâmetro `%n`.
- `resto` é atribuída e lida dentro do mesmo bloco, a cada volta. O valor
  dela nunca atravessa uma junção, então `%sub.9` é usado direto.

Ou seja, `phi` aparece onde um valor que muda cruza um ponto de encontro
de caminhos: o cabeçalho de todo laço e o fim de todo condicional.

Um detalhe do nosso gerador que o `mem2reg` exige: ele só promove `alloca`
que está no bloco de entrada (B13). Com o `alloca` no meio do corpo a
variável ficaria na memória.

### Depois do `-O2`

```llvm
define i32 @mac_somapares(i32 %n) local_unnamed_addr #0 {
funcao.entrada:
  %cmp.36 = icmp sgt i32 %n, 0
  br i1 %cmp.36, label %repete.corpo.1, label %repete.fim.1

repete.corpo.1:                    ; preds = %funcao.entrada, %repete.corpo.1
  %soma.addr.08 = phi i32 [ %spec.select, %repete.corpo.1 ], [ 0, %funcao.entrada ]
  %i.addr.07 = phi i32 [ %soma.17, %repete.corpo.1 ], [ 0, %funcao.entrada ]
  %0 = and i32 %i.addr.07, 1
  %cmp.12 = icmp eq i32 %0, 0
  %soma.15 = select i1 %cmp.12, i32 %i.addr.07, i32 0
  %spec.select = add i32 %soma.15, %soma.addr.08
  %soma.17 = add nuw nsw i32 %i.addr.07, 1
  %cmp.3 = icmp slt i32 %soma.17, %n
  br i1 %cmp.3, label %repete.corpo.1, label %repete.fim.1

repete.fim.1:                      ; preds = %repete.corpo.1, %funcao.entrada
  %soma.addr.0.lcssa = phi i32 [ 0, %funcao.entrada ], [ %spec.select, %repete.corpo.1 ]
  ret i32 %soma.addr.0.lcssa
}
```

Sobraram 13 instruções. O que mudou em relação ao `mem2reg`:

- **O laço foi girado.** O teste saiu do cabeçalho e foi para o fim da
  volta, com uma guarda antes (`n > 0`). Fica um desvio por volta em vez
  de dois.
- **O resto virou um `and`.** `i - i // 2 * 2 == 0` é perguntar se `i` é
  par. O otimizador percebeu e trocou divisão, multiplicação e subtração
  por `and i32 %i, 1`.
- **O `cond` sumiu.** Em vez de desviar, ele soma `i` ou soma `0`,
  escolhendo com `select`. O laço ficou com um bloco só, e com isso o
  `phi` da junção do `cond` deixou de existir.
- **Apareceu `nuw nsw` no incremento.** A gente não escreve essas marcas
  (B6), mas aqui o otimizador provou que `i + 1` não estoura, porque `i` é
  menor que `n`. É o jeito certo de elas aparecerem: por prova, não por
  promessa do gerador.
- **O `phi` novo no fim** (`.lcssa`) guarda o valor de `soma` na saída do
  laço: `0` se o laço nem rodou, ou a última soma.

E na `principal` a chamada desapareceu. `somapares(10)` foi calculada
durante a compilação:

```llvm
define noundef i32 @zl_principal() local_unnamed_addr #1 {
funcao.entrada:
  tail call void @zl_imprime_int(i32 20)
  tail call void @zl_imprime_nl()
  tail call void @zl_imprime_int(i32 0)
  tail call void @zl_imprime_nl()
  ret i32 0
}
```

| versão         | instruções em `somapares` | acessos à memória | `phi` |
|----------------|---------------------------|-------------------|-------|
| gerada por nós | 43                        | 22                | 0     |
| `mem2reg`      | 24                        | 0                 | 3     |
| `-O2`          | 13                        | 0                 | 3     |

Dois cuidados que a gente toma com o `opt`. Ele apaga os comentários
`; linha N`, e sem alvo definido ele troca os índices de `getelementptr`
para `i64`. Por isso o `.ll` entregue e testado é sempre o que sai do
compilador; `mem2reg` e `-O2` rodam em cópia.

### Os `phi` que o próprio gerador escreve

Em dois lugares a gente escreve `phi` na mão, porque ali não existe
variável do programa para guardar o valor: no resultado do `E` e do `OU`
(B4) e no contador do laço que zera um vetor local (B9). Nos dois casos a
alternativa seria criar um `alloca` escondido só para o `mem2reg` desfazer
depois.

---

## 9. Testes

`make test` roda tudo: os casos do PjBL 1 (válidos, inválidos e mensagens)
e os de execução em `tests/execucao/`. Para cada programa de execução o
script `tests/roda_execucao.sh`:

1. compila com `-o`;
2. passa o `.ll` no `llvm-as` e no `opt -passes=verify`;
3. confere as regras do IR portável (`tests/confere_ir.sh`);
4. roda com `lli`, ligando a biblioteca;
5. monta um executável com `clang` e roda de novo;
6. compara saída, código de saída e mensagem de erro com o esperado, nos
   dois modos.

Além disso confere que nenhum programa de `tests/invalidos/` gera arquivo
com `-o`, e que o IR dos programas de `tests/validos/` passa no
verificador.

| caso                             | o que cobre                                      |
|----------------------------------|--------------------------------------------------|
| `01_aritmetica_precedencia`      | precedência e associatividade                    |
| `02_condicionais_aninhadas`      | `cond` dentro de `cond`, `casocontrario cond`    |
| `03_repete_e_durante`            | os dois laços, laço dentro de laço               |
| `04_recursao`                    | fatorial e Fibonacci                             |
| `05_vetor_soma_e_maior`          | soma e maior elemento, vetor de `flut`           |
| `06_registro`                    | campos, campo vetor, registro global             |
| `07_entrada_e_saida`             | `leia` dos três tipos numéricos                  |
| `08_tipos_mistos`                | `inteira`, `flut` e `duplocarpado` misturados    |
| `09_potencia_e_raiz`             | `^` e `raiz()` em todos os tipos                 |
| `10_divisao_real_e_inteira`      | diferença entre `/` e `//`                       |
| `11_logicos_e_curto_circuito`    | `E OU XOR NAO`, curto-circuito protegendo vetor  |
| `12_globais_e_palavras`          | globais, texto, comparação de `palavra`          |
| `13_erro_indice_fora_do_vetor`   | erro de execução 1, com linha                    |
| `14_erro_divisao_por_zero`       | erro de execução 2, com linha                    |
| `15_codigo_de_saida`             | `respost` da `principal`                         |
| `16_escopo_e_sombra`             | variável de bloco interno, zerada a cada volta   |
| `17_estouro_da_a_volta`          | estouro de `inteira`, `x // -1`                  |
| `18_principal_vazio`             | `vazio principal()`                              |
| `19_erro_de_leitura`             | entrada que acaba no meio                        |
| `20_laco_com_condicional`        | o programa da análise do `mem2reg`               |

As saídas esperadas foram calculadas à mão antes de rodar o compilador.

---

## 10. Limites do back-end

- Vetor local mora na pilha. Um vetor local de milhões de elementos
  estoura a pilha em execução; vetor grande tem que ser global.
- Registro só é usado campo a campo (B10).
- Não tem conversão de real para inteira: a linguagem não tem operador
  para isso.
- Literal `duplocarpado` não existe (B1).
