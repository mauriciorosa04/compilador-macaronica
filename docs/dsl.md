# Extensão vetorial da Macaronica

A linguagem já tinha vetor, mas só dava para mexer nele elemento por
elemento, dentro de um laço. A extensão acrescenta operações sobre o vetor
inteiro: somar dois vetores, multiplicar um vetor por um número, somar
todos os elementos, achar o maior e fazer o produto escalar.

Vale para vetor de `inteira`, de `flut` e de `duplocarpado`.

---

## 1. O que existe

| operação            | como se escreve             | o que faz                          |
|---------------------|-----------------------------|------------------------------------|
| elemento a elemento | `c receba a + b;`           | `c{i} = a{i} + b{i}` para todo `i` |
|                     | `c receba a - b;`           | `c{i} = a{i} - b{i}`               |
|                     | `c receba a * b;`           | `c{i} = a{i} * b{i}`               |
| número com vetor    | `c receba 3 * a;`           | `c{i} = 3 * a{i}`                  |
|                     | `c receba a * k;`           | o número pode vir dos dois lados   |
|                     | `c receba a + k;`           | `c{i} = a{i} + k`                  |
|                     | `c receba k - a;`           | `c{i} = k - a{i}`                  |
|                     | `c receba a - k;`           | `c{i} = a{i} - k`                  |
| soma                | `somavet(a)`                | soma de todos os elementos         |
| maior               | `maxvet(a)`                 | o maior elemento                   |
| produto escalar     | `prodesc(a, b)`             | soma de `a{i} * b{i}`              |

Exemplo:

```
inteira principal() @
[
    inteira vet{4} a, b, c;
    inteira i;

    repete ( i receba 0, i < 4, i receba i + 1 ) edai
    [
        a{i} receba i + 1;
        b{i} receba 10;
    ]

    c receba a + b;
    c receba 2 * c;
    escreva(somavet(c));
    escreva(maxvet(c) - prodesc(a, b));
    respost 0;
]
```

---

## 2. Tokens novos

Três palavras reservadas, sensíveis a maiúscula e minúscula como as
outras (D19):

| token       | lexema    |
|-------------|-----------|
| `T_SOMAVET` | `somavet` |
| `T_MAXVET`  | `maxvet`  |
| `T_PRODESC` | `prodesc` |

Por serem reservadas, não podem mais ser nome de variável nem de função.
As operações elemento a elemento não precisaram de token: usam o
`receba`, o `+`, o `-` e o `*` que já existiam.

---

## 3. Gramática

Produções novas e alteradas, na mesma notação de `docs/decisoes.md`:

```ebnf
(* fator ganha a redução *)
fator        = NUM_INT | NUM_FLUT | STRING | "VER" | "FAL"
             | "raiz" "(" expr ")"
             | reducao
             | IDENT | IDENT "{" expr "}" | IDENT "." IDENT [ "{" expr "}" ]
             | chamada
             | "(" expr ")" | "-" fator ;

reducao      = ( "somavet" | "maxvet" ) "(" IDENT ")"
             | "prodesc" "(" IDENT "," IDENT ")" ;

(* a atribuição vetorial é um caso da atribuição que já existia *)
atribuicao   = lvalue "receba" expr
             | atrib_vet ;

atrib_vet    = IDENT "receba" operando_vet op_vet operando_vet ;
operando_vet = IDENT            (* nome de um vetor *)
             | expr ;           (* um número: qualquer expressão escalar *)
op_vet       = "+" | "-" | "*" ;
```

Duas observações sobre como isso é reconhecido.

**A redução recebe um nome, não uma expressão.** Dentro de
`somavet( ... )` só cabe um `IDENT`. Isso é sintático: `somavet(a + b)` é
erro de sintaxe. Para somar a soma de dois vetores, guarda-se primeiro
em um terceiro.

**A atribuição vetorial é decidida pelo tipo, não pela forma.**
`c receba a + b;` tem exatamente a mesma cara quando `a`, `b` e `c` são
números. O parser não sabe a diferença, e nem tem como saber com um token
de antecipação: ele monta a atribuição comum. Quem decide é a semântica,
olhando a tabela de símbolos. Se o destino é o nome de um vetor, a
atribuição é vetorial e passa a valer a produção `atrib_vet`, com as
regras da seção 4. Na árvore, o nó `A_ATRIB` é trocado por `A_ATRIB_VET`.
A gramática continua LL(1) e nenhuma produção antiga mudou de
comportamento.

Como `somavet(a)` é um `fator`, ele entra em qualquer expressão:
`x receba somavet(a) * 2 + maxvet(b);`, `respost somavet(v);`,
`escreva(prodesc(a, b));`. Só não entra direto em condição, pela mesma
regra D6 que barra chamada de função ali: guarda-se em variável e
compara-se a variável.

---

## 4. Regras semânticas

Todas são conferidas na compilação. Nenhuma operação vetorial tem
verificação em tempo de execução.

1. **Só vetor de número.** Operando e destino têm que ser vetor de
   `inteira`, `flut` ou `duplocarpado`. Vetor de `bool` ou de `palavra` é
   erro.
2. **Mesmo tipo.** Todos os vetores da operação, inclusive o destino, têm
   o mesmo tipo de elemento. Não tem conversão entre vetores:
   `inteira vet` mais `flut vet` é erro.
3. **Mesmo tamanho.** Todos os vetores da operação têm o mesmo número de
   elementos.
4. **O número é convertido para o tipo do vetor**, pela mesma regra de
   subir tipo do resto da linguagem. `3 * f` com `f` de `flut` vale, e o
   `3` vira `3.0`. `2.5 * a` com `a` de `inteira` é erro, porque seria
   converter para baixo.
5. **Uma operação por atribuição.** `c receba a + b + d;` é erro. Faz-se
   em dois passos.
6. **Só `+`, `-` e `*`.** `c receba a / b;` e `c receba a ^ 2;` são erro.
7. **Pelo menos um lado é vetor.** `c receba 1 + 2;` com `c` vetor é erro.
8. **O resultado da redução tem o tipo do elemento.** `somavet` de vetor
   de `flut` devolve `flut`.

Mensagens de erro, de `tests/dsl/invalidos/`:

```
a operacao vetorial exige vetores do mesmo tamanho: 'b' tem 8 elemento(s) e 'c' tem 4
a operacao vetorial exige vetores do mesmo tipo: 'f' e' de flut e 'c' e' de inteira
na operacao vetorial o numero e' flut, e o vetor 'c' e' de inteira
prodesc exige vetores do mesmo tamanho: 'a' tem 4 elemento(s) e 'b' tem 8
somavet espera o nome de um vetor, e 'x' nao e' vetor
maxvet so vale pra vetor de inteira, flut ou duplocarpado; 'm' e' vetor de bool
```

---

## 5. Lacunas que tivemos que resolver

### V1 - Vetores com tamanho só conhecido em execução
Não existem. Todo vetor da Macaronica é declarado com um literal
(`vet{1024}`), então o tamanho de todo operando é conhecido na
compilação. Tamanhos diferentes são erro semântico, e nenhum teste de
tamanho é gerado para rodar. É também o que deixa cada núcleo ter tamanho
fixo no MLIR (`memref<1024xi32>`), que é a forma que os passos do MLIR
otimizam melhor.

### V2 - O destino pode ser um dos operandos?
Pode. `a receba a + b;` e `a receba 2 * a;` valem.

É seguro porque cada elemento do resultado só depende dos elementos de
mesmo índice: `a{i}` é lido e escrito na mesma volta, e nenhuma volta lê
o que outra escreveu. Por isso a ordem das voltas não importa, e
vetorizar ou paralelizar não muda o resultado.

O que tornaria isso perigoso é sobreposição parcial: dois vetores que
começam em lugares diferentes do mesmo bloco de memória. Na linguagem
isso não tem como acontecer. Operando é sempre um vetor inteiro, não
existe fatia nem ponteiro, então dois nomes ou são o mesmo vetor ou são
vetores separados. Essa garantia vem da linguagem, não do núcleo: o
núcleo sozinho, chamado de fora com ponteiros sobrepostos, daria
resultado diferente no modo vetorizado.

### V3 - Redução de vetor vazio ou de um elemento
Vetor vazio não existe: `vet{0}` é erro semântico desde o PjBL 1. Com um
elemento, `somavet` e `maxvet` devolvem esse elemento e `prodesc` devolve
o produto dos dois (teste `03_reducoes`).

O `maxvet` começa pelo primeiro elemento e compara com os outros. Assim
ele funciona com vetor só de negativos sem precisar de um "menor valor
possível" para cada tipo.

### V4 - Ordem da soma nas reduções de real
`somavet` e `prodesc` somam na ordem dos índices, do 0 ao último. Com
`flut` e `duplocarpado` isso importa, porque soma de ponto flutuante não
é associativa. Por padrão o compilador não deixa o otimizador trocar a
ordem, e o resultado é o mesmo em qualquer máquina.

A opção `--reordena` libera a troca. O programa fica mais rápido e o
resultado pode mudar nas últimas casas. O efeito está medido em
`docs/relatorio_otimizacao.md`, seção 5.

### V5 - Estouro
Igual ao resto da linguagem: em vetor de `inteira` a conta dá a volta em
complemento de dois, elemento por elemento e também na soma das reduções.

### V6 - Na placa
No Arduino Uno não existe instrução de vetor. Os núcleos descem pelo
mesmo `scripts/pipeline.sh`, no modo `escalar`, e viram laços comuns. O
programa não muda. O teste `tests/placa/05_operacoes_vetoriais` roda os
núcleos no simulador da placa e dá a mesma saída do PC.

---

## 6. Como é gerado

Cada operação vira uma chamada a um núcleo, uma função pequena com o
laço. O núcleo sai em MLIR, em um arquivo ao lado do `.ll`:

```
./compilador prog.mac -o prog.ll      (gera prog.ll e prog.mlir)
bash scripts/pipeline.sh prog.mlir nucleos.ll
llvm-link-18 prog.ll nucleos.ll -S -o tudo.ll
```

ou, tudo de uma vez, `make roda PROG=prog.mac`.

| operação            | núcleo                          | argumentos    |
|---------------------|---------------------------------|---------------|
| `c receba a + b;`   | `nuc_soma_<tipo>_<N>`           | `a, b, c`     |
| `c receba a - b;`   | `nuc_sub_<tipo>_<N>`            | `a, b, c`     |
| `c receba a * b;`   | `nuc_mul_<tipo>_<N>`            | `a, b, c`     |
| `c receba k + a;`   | `nuc_escsoma_<tipo>_<N>`        | `k, a, c`     |
| `c receba k * a;`   | `nuc_escmul_<tipo>_<N>`         | `k, a, c`     |
| `c receba k - a;`   | `nuc_escsub_<tipo>_<N>`         | `k, a, c`     |
| `c receba a - k;`   | `nuc_subesc_<tipo>_<N>`         | `k, a, c`     |
| `somavet(a)`        | `nuc_somavet_<tipo>_<N>`        | `a`           |
| `maxvet(a)`         | `nuc_maxvet_<tipo>_<N>`         | `a`           |
| `prodesc(a, b)`     | `nuc_prodesc_<tipo>_<N>`        | `a, b`        |

`<tipo>` é `i32`, `f32` ou `f64`, e `<N>` é o tamanho do vetor. A soma e
a multiplicação por número são comutativas e usam um núcleo só; a
subtração precisa de dois.

O MLIR de cada núcleo, o pipeline e os níveis por que ele passa estão em
`docs/relatorio_otimizacao.md`.

---

## 7. Testes

`tests/dsl/`, rodados pelo `make test`. Cada programa válido faz a
operação vetorial e a mesma conta com um laço escrito à mão na própria
linguagem, e compara.

| caso                              | o que cobre                                           |
|-----------------------------------|-------------------------------------------------------|
| `01_elemento_a_elemento`          | `+`, `-`, `*` entre vetores; destino igual a operando |
| `02_numero_com_vetor`             | número dos dois lados, as três operações, `flut`      |
| `03_reducoes`                     | `somavet` e `maxvet`; só negativos; um elemento       |
| `04_produto_escalar`              | `prodesc` nos três tipos                              |
| `05_globais_e_funcoes`            | vetor global, redução dentro de expressão e função    |
| `invalidos/tamanhos_diferentes`   | erro semântico de tamanho                             |
| `invalidos/tipos_diferentes`      | erro semântico de tipo                                |
| `invalidos/formas_que_nao_existem`| mais de uma operação, `/`, `^`, sem vetor             |
