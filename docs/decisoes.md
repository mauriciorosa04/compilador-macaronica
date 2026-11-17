# Decisões de projeto - front-end da Macaronica

Duas coisas neste arquivo: a gramática que a gente de fato implementou,
e a lista das decisões que tivemos que tomar onde a definição da
linguagem era omissa ou se contradizia. A Seção 4.3 do enunciado pede
justamente isso ("identificar, decidir e documentar").

---

## 1. Gramática implementada (EBNF)

Convenções: `{ X }` = zero ou mais repetições; `[ X ]` = opcional;
`|` = alternativa; terminais entre aspas.

```ebnf
programa      = { registro | funcao | principal | decl_global | decl_registro } ;

registro      = "registro" IDENT "@" "[" { declaracao } "]" ;
decl_registro = "registro" IDENT IDENT { "," IDENT } ";" ;   (* ver D12 *)

funcao        = ( "outrafuncao" | "func" ) tipo IDENT
                "(" [ parametros ] ")" "@" bloco ;

principal     = tipo "principal" "(" [ parametros ] ")" "@" bloco ;

parametros    = tipo IDENT { "," tipo IDENT } ;

tipo          = "bool" | "inteira" | "flut" | "palavra"
              | "duplocarpado" | "vazio" ;

bloco         = "[" { comando } "]" ;

comando       = declaracao
              | decl_registro
              | atribuicao ";"
              | repete | durante | condicional
              | "respost" [ expr ] ";"
              | "escreva" "(" expr ")" ";"
              | "leia" "(" lvalue ")" ";"
              | chamada ";" ;

declaracao    = tipo [ "vet" "{" NUM_INT "}" ] IDENT { "," IDENT } ";" ;
decl_global   = declaracao ;                    (* ver decisão D8 *)

atribuicao    = lvalue "receba" expr ;
lvalue        = IDENT [ "{" expr "}" ]
              | IDENT "." IDENT [ "{" expr "}" ] ;          (* campo de registro *)

repete        = "repete" "(" atribuicao "," condicao "," atribuicao ")"
                "edai" bloco ;
durante       = "durante" "(" condicao ")" "edai" bloco ;

condicional   = "cond" "(" condicao ")" "edai" bloco
                [ "casocontrario" ( condicional | "edai" bloco ) ] ;

(* condições: segue a regra CONDICAO da especificação (operando é
   apenas VARIÁVEL ou NÚMERO), estendida com os operadores lógicos
   definidos na tabela de tokens (ver decisão D6) *)
condicao      = cond_e { ( "OU" | "XOR" ) cond_e } ;
cond_e        = cond_nao { "E" cond_nao } ;
cond_nao      = "NAO" cond_nao | cond_atom ;
cond_atom     = "(" condicao ")"
              | operando [ comparador operando ] ;  (* sozinho: tem que ser bool *)
operando      = IDENT [ "{" expr "}" ] | IDENT "." IDENT [ "{" expr "}" ]
              | NUM_INT | NUM_FLUT | "VER" | "FAL" ;
comparador    = "<" | ">" | "==" | "!" ;

(* expressões aritméticas, por ordem crescente de precedência *)
expr          = mult { ( "+" | "-" ) mult } ;
mult          = pot  { ( "*" | "/" | "//" ) pot } ;
pot           = fator [ "^" pot ] ;             (* assoc. à direita *)
fator         = NUM_INT | NUM_FLUT | STRING | "VER" | "FAL"
              | "raiz" "(" expr ")"
              | reducao                                     (* docs/dsl.md *)
              | IDENT | IDENT "{" expr "}" | IDENT "." IDENT [ "{" expr "}" ]
              | chamada
              | "(" expr ")" | "-" fator ;

reducao       = ( "somavet" | "maxvet" ) "(" IDENT ")"
              | "prodesc" "(" IDENT "," IDENT ")" ;

chamada       = IDENT "(" [ expr { "," expr } ] ")" ;
```

Uma observação sobre `principal`: como ela está dentro do `{ }` da regra
`programa`, a gramática aceita as declarações de topo em qualquer ordem e
em qualquer quantidade, inclusive um programa sem nenhuma `principal` ou
com duas. A exigência de existir exatamente uma não é sintática; os dois
casos são rejeitados na fase semântica. A partir do back-end a semântica
também exige que a `principal` não tenha parâmetros e devolva `inteira` ou
`vazio` (B5 em `decisoes_backend.md`).

A gramática acima já inclui o que entrou na segunda parte do trabalho: a
variável de registro e o acesso a campo (D12) e as reduções da extensão
vetorial (`docs/dsl.md`).

Léxico:

```
IDENT     = letra { letra | digito }          (sem '_': fora do alfabeto)
NUM_INT   = digito { digito }
NUM_FLUT  = digito { digito } "." digito { digito }
STRING    = '"' { caractere-exceto-aspas-e-quebra-de-linha } '"'
```

---

## 2. Ambiguidades que o próprio enunciado lista (Seção 4.3)

### D13 - Acesso a elemento de vetor
O acesso ficou com a mesma chave da declaração: `nome{indice}`, tanto
para ler quanto para atribuir (`notas{i} receba 10;`). A declaração usa
chave para o tamanho (`tipo vet{tamanho} nome`) e a especificação nunca
mostra como acessar um elemento, então além da consistência pesou o fato
de não sobrar outro delimitador: `[ ]` é bloco e `( )` é condição e
lista de argumentos.

O índice começa em 0 e vai até tamanho menos 1. Índice literal fora
dessa faixa (`v{9}` num `vet{5}`, ou `v{-1}`) é erro semântico, tanto
lendo quanto atribuindo. Índice calculado só se resolveria em execução,
que está fora do escopo do trabalho.

### D14 - Sintaxe de chamada de função
A especificação define só a declaração. Chamada ficou
`nome(arg1, arg2, ...)`, que é a forma que o próprio enunciado usa nos
exemplos. Chamada também vale como comando sozinho: `saudacao();`.

### D5 - `respost` em função `vazio`
Em função `vazio` aceitamos `respost ;` sem valor, e aceitamos também
que não exista `respost` nenhum no corpo. `respost` com valor dentro de
`vazio` é erro semântico.

Na direção contrária, função que não é `vazio` precisa devolver valor em
todo caminho possível, não basta existir um `respost` em algum lugar.
`respost` dentro de laço não conta, porque o laço pode nem executar; um
`cond` só conta se tiver `casocontrario` final e todos os ramos
devolverem. É a mesma regra do aviso "control reaches end of non-void
function" do gcc, só que aqui é erro. Função sem nenhum `respost` com
valor e função que pode escapar por um caminho sem `respost` recebem
mensagens diferentes.

---

## 3. Ambiguidades que a gente achou por conta própria

Essas são lacunas que o enunciado não lista. Apareceram enquanto a gente
escrevia o código e os casos de teste.

### D1 - `func` x `outrafuncao`
As duas palavras valem como sinônimo na declaração de função
não-principal. A tabela de palavras reservadas traz `func`, a estrutura
de declaração no texto usa `outrafuncao`, e não havia critério para
eleger uma das duas. Ambas continuam reservadas, então nenhuma delas
pode ser usada como identificador.

### D2 - `NÃO` tem caractere fora do alfabeto
A negação aparece como "NÃO" na tabela de tokens, só que o alfabeto
oficial só tem `a..z A..Z 0..9` e símbolos ASCII. O token canônico é
`NAO`, sem acento. A grafia `NÃO` com til, exatamente como está na
tabela, é aceita como sinônimo, tanto em UTF-8 quanto em CP1252 (os dois
jeitos que um editor no Linux ou no Windows salvaria o arquivo). Fora
esse caso, qualquer caractere fora do ASCII é erro léxico, reportado por
caractere e não por byte, com o caractere legível na mensagem.

### D3 - Símbolos que a especificação usa e não colocou no alfabeto
Não dá para escrever nem o exemplo oficial usando só o alfabeto da Seção
4.2. Quatro símbolos aparecem nas estruturas e nos exemplos mas ficaram
de fora dele: `@` (separador do corpo de funções e registros), `,`
(separador de parâmetros), `"` (sem ele não dá para escrever literal
`palavra`) e `.` (sem ele não dá para escrever `flut` nem
`duplocarpado`). O alfabeto do lexer tem esses quatro a mais, e nada
além deles.

### D4 - Literais de `palavra`
Literal de `palavra` ficou entre aspas duplas e em uma linha só; aspas
que não fecham até o fim da linha viram erro léxico. A especificação não
diz nada sobre eles, e deixar a string atravessar quebra de linha só
atrapalharia a recuperação de erro do léxico.

### D6 - O que pode aparecer dentro de uma condição
A regra `CONDICAO -> VAR COMPARADOR VAR | VAR COMPARADOR NUM |
NUM COMPARADOR NUM` é bem explícita: operando é variável ou número, e
só. Nada de expressão aritmética ou chamada de função ali dentro. O
exemplo oficial reforça isso, porque faz
`resultado receba quadrado(numero);` antes de chegar no
`cond ( resultado > 20 )`. Mantivemos a regra estrita para os operandos.
O que estendemos foi só os operadores lógicos `OU/E/NAO/XOR`, que estão
na tabela de tokens mas não teriam nenhum lugar para aparecer, mais
parênteses para agrupar.

Três consequências que vale deixar explícitas:

- Uma variável `bool` sozinha vale como condição: `cond ( pronto )`,
  `durante ( NAO acabou )`, `cond ( pronto E n > 3 )`. Sem isso uma
  variável `bool` não teria onde ser lida. O operando sozinho tem que ser
  `bool`; `cond ( n )` com `n` inteira é erro semântico.
- Aritmética nos operandos continua proibida, e quem escreve
  `cond ( a + 1 > 2 )` recebe uma mensagem dizendo isso e como contornar
  (calcular antes, guardar na variável, comparar a variável).
- O índice de um vetor dentro da condição é uma expressão completa:
  `cond ( v{i+1} > 2 )` vale. A restrição é sobre os operandos da
  comparação, não sobre o índice.

### D7 - Primeiro campo do `repete`
A estrutura é `repete ( var , condição , iteração )`, mas "var" sozinho
não inicializa nada. Tratamos o primeiro e o terceiro campos como
atribuição (`i receba 0`, `i receba i + 1`); sem isso o laço não teria
como inicializar nem avançar o contador.

### D8 - Variáveis globais
Declaração fora de função é aceita, coisa que a especificação não trata.
A tabela de símbolos já é uma pilha de escopos, então o escopo global
saiu de graça. A visibilidade é que não é total: `analisar_semantica()`
percorre as declarações de topo em ordem textual e analisa o corpo de
cada função no momento em que ela aparece, de modo que uma global
declarada depois de uma função não é enxergada lá dentro. Vale a mesma
regra de declarar antes de usar que a D18 descreve para funções.

### D9 - Compatibilidade entre tipos numéricos
A promoção implícita é só para cima (`inteira -> flut ->
duplocarpado`), nunca para baixo, já que nada é dito sobre misturar os
três. É a mesma regra de widening de C e Java. `bool` e `palavra` só são
compatíveis com eles mesmos.

### D10 - Divisão `/` x `//`
Como a linguagem já tem `//` explícito para divisão inteira, `/` virou a
divisão real: resultado no mínimo `flut`, mesmo com duas `inteira` dos
dois lados (é o que o Python 3 faz). E `//` exige os dois operandos
`inteira`; usar com `flut` ou `duplocarpado` é erro semântico. Essa
última restrição é específica da linguagem e quem verifica é a fase
semântica.

### D11 - Valor de retorno descartado
`quadrado(3);` como comando é válido e o valor simplesmente se perde.
Obrigar a aproveitar o retorno criaria uma regra que a especificação não
pede.

### D12 - `registro`: declaração, variável e campo
A especificação define como declarar um registro e para por aí: não tem
sintaxe de acesso a campo nem de declaração de variável com tipo
registro. No PjBL 1 o registro ficou só com a declaração do tipo (nome
único no escopo global, campos sem repetição e com tipo válido), e
deixamos o resto registrado como limitação.

Na segunda parte isso deixou de bastar: gerar código para registro
precisa de variável e de campo. A extensão ficou com o mínimo:

```
registro Aluno a, b;        (variável de registro, local ou global)
a.matricula receba 1234;    (campo)
a.notas{0} receba 7;        (campo que é vetor)
```

A declaração reaproveita a palavra `registro`: depois do nome do tipo, um
`@` abre a declaração do tipo e um nome declara variável, então continua
LL(1). O acesso usa o `.`, que já estava no alfabeto por causa dos números
reais e solto virou um token novo. O registro é usado campo a campo:
atribuir, imprimir ou passar um registro inteiro é erro semântico. Os
detalhes de geração estão em B10, em `decisoes_backend.md`.

### D15 - Entrada e saída
O caso de teste que o enunciado cobra precisa ler e imprimir, e a
linguagem não define nenhum comando de E/S. `escreva(expr);` e
`leia(var);` entraram como palavras reservadas. A parte semântica checa
os dois: não se imprime `vazio`, e `leia` marca a variável como
inicializada.

### D16 - Comentários
A linguagem simplesmente não tem comentário. Não há nada especificado a
respeito, e `//`, que seria o candidato óbvio, já está ocupado pela
divisão inteira.

### D17 - O token `!`
`!` aparece na tabela como comparador de diferença. Ficou sendo sempre
isso: comparador binário, escrito `a ! b`. Negação lógica é só `NAO`, e
`!=` não existe na linguagem.

### D18 - Declaração antes do uso (funções)
Toda função precisa aparecer no texto antes da primeira chamada, porque
a linguagem não tem protótipo e o semântico faz uma passagem só.
Recursão direta funciona, porque o nome entra na tabela de símbolos
antes do corpo ser analisado. Recursão mútua não funciona e ficou fora
do escopo.

### D19 - Palavra reservada é sensível a maiúscula e minúscula
A tabela de tokens mistura os dois casos e não diz se a comparação
ignora a caixa. Em `lexer.c` a busca é por lexema exato, então a caixa
faz parte da palavra: `VER`, `FAL`, `OU`, `E`, `NAO` e `XOR` só valem em
maiúsculo, e todas as outras (`cond`, `repete`, `inteira`, `respost` e
companhia) só em minúsculo. Escrever `Cond` ou `ver` não é erro léxico;
vira identificador comum, e o problema aparece depois, no sintático ou
no semântico.

### D20 - Parâmetro e corpo da função no mesmo escopo
Parâmetro e variável declarada direto no corpo da função vivem no mesmo
escopo, como em C. Então `f(inteira n) @ [ inteira n; ... ]` é
redeclaração e dá erro, com mensagem dizendo que `n` já é parâmetro, em
vez de sombrear o parâmetro em silêncio e deixar dois avisos confusos
para trás. Blocos internos (`cond`, `repete`, `durante`) continuam
abrindo escopo próprio, e lá dentro sombrear uma variável de fora é
permitido.

---

## 4. Decisões de arquitetura

Divisão dos módulos:

- `erros.c` - formato único de mensagem e contador de erros por fase.
- `lexer.c` - AFD escrito na mão, maior prefixo (maximal munch), guarda
  linha e coluna de cada token.
- `parser.c` - LL(1) por descida recursiva, monta a AST e faz modo
  pânico sincronizando em `;` ou `]`.
- `ast.c` - nó com até 4 filhos mais lista de irmãos, e a impressão
  anotada da árvore.
- `symtab.c` - pilha de escopos, cada escopo sendo uma tabela hash
  (djb2) que cresce sozinha conforme entram símbolos.
- `semantic.c` - tipagem bottom-up por atributo sintetizado, conferência
  de assinatura e de escopo.
- `codegen_llvm.c` e `codegen_mlir.c` - geração de código, na segunda
  parte do trabalho (`decisoes_backend.md` e `dsl.md`).

Duas escolhas que valem explicação:

A análise semântica só roda se o sintático não acusou nenhum erro. Uma
AST remendada pela recuperação de erro geraria um monte de mensagem
semântica falsa em cima de código que nem existe direito.

Erro não aborta a análise. O léxico devolve `T_ERRO` e continua; o
sintático sincroniza no próximo `;` ou `]`; o semântico marca a
sub-árvore como `TP_ERRO` para não sair cascateando mensagem. A ideia é
um problema real gerar uma mensagem só.

Fora os três erros (léxico, sintático e semântico), existe uma quarta
categoria de saída: `AVISO SEMANTICO`, usado para variável declarada e
nunca utilizada e para variável possivelmente usada antes de receber
valor. Aviso não invalida o programa nem entra na contagem de erros.

---

## 5. Limites da implementação

Buffer fixo e recursão têm teto. Em todos os casos abaixo, passar do teto
gera erro com linha e coluna. Nenhum deles trunca em silêncio.

- Identificador e número: até 127 caracteres (`LEXEMA_MAX`). Acima disso
  o lexer consome o token inteiro e emite um erro léxico, em vez de parar
  no meio e partir o nome em dois tokens (o que geraria erro sintático
  falso). A tabela de símbolos usa o mesmo limite, então declaração e uso
  casam sempre.
- Literal de `palavra`: sem limite de tamanho. O lexema do token guarda
  só os primeiros 127 caracteres, para as mensagens, e o texto inteiro vai
  por ponteiro para dentro do fonte, que é o que o gerador de código usa.
- Literal `inteira`: até 2147483647. Acima disso é erro semântico. A
  forma `-2147483648` é aceita como caso único, por ser o menor valor do
  tipo. Literal `flut` que não cabe em 32 bits também é erro.
- Tamanho de vetor: de 1 até 2147483647, conferido na declaração. Índice
  literal é conferido contra o tamanho em qualquer vetor.
- Parâmetros por função: a contagem é exata para qualquer quantidade; o
  tipo é conferido até o 64º (`SYM_MAX_PARAMS`). Função com mais de 64
  parâmetros gera um erro semântico na declaração, e a chamada correta
  dela não é rejeitada por engano.
- Aninhamento: 1024 níveis somados (`PROFUNDIDADE_MAX`), contando bloco
  dentro de bloco, parêntese dentro de parêntese, cadeia de `NAO`,
  cadeia de `^` e cadeia de `casocontrario cond`. Acima disso vira erro sintático comum e
  o modo pânico desenrola. Medimos o motivo do teto: sem ele, a pilha
  padrão do Windows (2 MB) estoura perto de 2.000 parênteses. O Makefile
  pede 16 MB de pilha no Windows (aí estoura perto de 16.000) e o Linux
  já dá 8 MB, então a folga sobre o teto é de 8x ou mais.
- Byte nulo (0x00) no arquivo: erro léxico, e a análise continua no que
  vem depois (o byte é trocado por espaço). Sem isso, o lexer, que
  trabalha sobre string C, pararia ali e o resto do arquivo passaria sem
  ser visto.
- BOM UTF-8 no início do arquivo (o Bloco de Notas põe): ignorado.
- Coluna conta caracteres, não bytes: um acento dentro de uma string ocupa
  uma coluna só. Byte de continuação UTF-8 solto, sem o byte inicial, não
  é parte de caractere nenhum e ocupa a coluna dele.
- Cadeia de operadores (`a + b + c + ...`, `x E y E z ...`): sem limite
  de comprimento. A árvore dessas cadeias desce pela esquerda, e tanto a
  semântica quanto o gerador de código andam por ela com laço, não com
  recursão. Uma soma de 200 mil termos é analisada normalmente.
- Quantidade de declarações e de comandos: sem limite fixo. A tabela de
  símbolos de cada escopo cresce sozinha e a árvore é percorrida e
  liberada sem recursão sobre a lista de comandos.
- Opção de linha de comando que não existe, ou dois arquivos de entrada:
  erro de uso, código de saída 2.
