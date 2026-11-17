/* analisador sintatico, descida recursiva com um token de lookahead.
   Em erro entra em modo panico e sincroniza no ';' ou no ']' */
#include <stdio.h>
#include <string.h>
#include "parser.h"
#include "erros.h"

/* teto de aninhamento, pra recursao nao estourar a pilha */
#define PROFUNDIDADE_MAX 1024

typedef struct
{
    Lexer *lx;
    Token lookahead;
    int panico;
    int profundidade;
    int fim_avisado;
} Parser;

/* T_ERRO o lexer ja reportou, aqui so pula */
static void avancar(Parser *p)
{
    do
    {
        p->lookahead = lexer_proximo(p->lx);
    } while (p->lookahead.tipo == T_ERRO);
}

static void erro_sint(Parser *p, const char *esperado)
{
    if (p->panico)
        return;
    p->panico = 1;

    if (p->lookahead.tipo == T_EOF)
    {
        if (p->fim_avisado)
            return;
        p->fim_avisado = 1;
        erro_sintatico(p->lookahead.linha, p->lookahead.coluna,
                       "esperava %s, mas a entrada terminou", esperado);
    }
    else
        erro_sintatico(p->lookahead.linha, p->lookahead.coluna,
                       "esperava %s, encontrado '%s'",
                       esperado, p->lookahead.lexema);
}

static int entrar(Parser *p)
{
    if (p->profundidade >= PROFUNDIDADE_MAX)
    {
        if (!p->panico)
        {
            p->panico = 1;
            erro_sintatico(p->lookahead.linha, p->lookahead.coluna,
                           "aninhamento muito profundo (limite de %d niveis)",
                           PROFUNDIDADE_MAX);
        }
        return 0;
    }
    p->profundidade++;
    return 1;
}

static void sair(Parser *p) { p->profundidade--; }

static void sincronizar(Parser *p)
{
    while (p->lookahead.tipo != T_EOF &&
           p->lookahead.tipo != T_PONTOVIRG &&
           p->lookahead.tipo != T_FECHA_COL)
        avancar(p);
    if (p->lookahead.tipo == T_PONTOVIRG)
        avancar(p);
    p->panico = 0;
}

/* no erro nao consome o token */
static Token casar(Parser *p, TipoToken esperado, const char *descricao)
{
    if (p->lookahead.tipo == esperado)
    {
        Token t = p->lookahead;
        avancar(p);
        return t;
    }
    erro_sint(p, descricao);
    return p->lookahead;
}

static int e_token_de_tipo(TipoToken t)
{
    return t == T_BOOL || t == T_INTEIRA || t == T_FLUT ||
           t == T_PALAVRA || t == T_DUPLOCARPADO || t == T_VAZIO;
}

static Tipo token_para_tipo(TipoToken t)
{
    switch (t)
    {
    case T_BOOL: return TP_BOOL;
    case T_INTEIRA: return TP_INTEIRA;
    case T_FLUT: return TP_FLUT;
    case T_PALAVRA: return TP_PALAVRA;
    case T_DUPLOCARPADO: return TP_DUPLO;
    case T_VAZIO: return TP_VAZIO;
    default: return TP_ERRO;
    }
}

static Ast *expr(Parser *p);
static Ast *condicao(Parser *p);
static Ast *bloco(Parser *p);
static Ast *comando(Parser *p);
static Ast *condicional(Parser *p);

/* chamada -> IDENT '(' [ expr { ',' expr } ] ')' */
static Ast *chamada(Parser *p, Token nome)
{
    Ast *no = ast_novo(A_CHAMADA, nome);
    casar(p, T_ABRE_PAR, "'('");

    if (p->lookahead.tipo != T_FECHA_PAR)
    {
        Ast *arg = expr(p);
        no->f1 = arg;
        while (p->lookahead.tipo == T_VIRGULA)
        {
            avancar(p);
            arg->prox = expr(p);
            if (arg->prox == NULL)
                break;
            arg = arg->prox;
        }
    }
    casar(p, T_FECHA_PAR, "')'");
    return no;
}

/* nome, nome{i}, nome.campo ou nome.campo{i} */
static Ast *acesso(Parser *p, Token nome)
{
    if (p->lookahead.tipo == T_ABRE_CHAVE)
    {
        avancar(p);
        Ast *no = ast_novo(A_VET_ACESSO, nome);
        no->f1 = expr(p);
        casar(p, T_FECHA_CHAVE, "'}' fechando o indice do vetor");
        return no;
    }
    if (p->lookahead.tipo == T_PONTO)
    {
        avancar(p);
        Token campo = casar(p, T_IDENT, "o nome do campo depois do '.'");
        Ast *no = ast_novo(A_CAMPO, campo);
        no->f1 = ast_novo(A_VAR, nome);
        if (p->lookahead.tipo == T_ABRE_CHAVE)
        {
            avancar(p);
            no->f2 = expr(p);
            casar(p, T_FECHA_CHAVE, "'}' fechando o indice do vetor");
        }
        return no;
    }
    return ast_novo(A_VAR, nome);
}

static Ast *fator_interno(Parser *p);

/* fator -> NUM | STRING | VER | FAL | raiz(expr) | reducao | acesso
          | chamada | '(' expr ')' | '-' fator */
static Ast *fator(Parser *p)
{
    if (!entrar(p))
        return NULL;
    Ast *r = fator_interno(p);
    sair(p);
    return r;
}

static Ast *fator_interno(Parser *p)
{
    Token t = p->lookahead;

    switch (t.tipo)
    {
    case T_NUM_INT:
        avancar(p);
        return ast_novo(A_NUM_INT, t);

    case T_NUM_FLUT:
        avancar(p);
        return ast_novo(A_NUM_FLUT, t);

    case T_STRING:
        avancar(p);
        return ast_novo(A_STR, t);

    case T_VER:
    case T_FAL:
        avancar(p);
        return ast_novo(A_BOOL_LIT, t);

    case T_RAIZ:
    {
        avancar(p);
        Ast *no = ast_novo(A_RAIZ, t);
        casar(p, T_ABRE_PAR, "'(' apos raiz");
        no->f1 = expr(p);
        casar(p, T_FECHA_PAR, "')'");
        return no;
    }

    case T_SOMAVET:
    case T_MAXVET:
    case T_PRODESC:
    {
        avancar(p);
        Ast *no = ast_novo(A_REDUCAO, t);
        casar(p, T_ABRE_PAR, "'(' depois do nome da reducao");
        Token a = casar(p, T_IDENT, "o nome de um vetor");
        no->f1 = ast_novo(A_VAR, a);
        if (t.tipo == T_PRODESC)
        {
            casar(p, T_VIRGULA, "',' entre os dois vetores do prodesc");
            Token b = casar(p, T_IDENT, "o nome do segundo vetor");
            no->f2 = ast_novo(A_VAR, b);
        }
        casar(p, T_FECHA_PAR, "')'");
        return no;
    }

    case T_IDENT:
    {
        avancar(p);
        if (p->lookahead.tipo == T_ABRE_PAR)
            return chamada(p, t);
        return acesso(p, t);
    }

    case T_ABRE_PAR:
    {
        avancar(p);
        Ast *no = expr(p);
        casar(p, T_FECHA_PAR, "')'");
        return no;
    }

    case T_MENOS:
    {
        avancar(p);
        Ast *no = ast_novo(A_NEG, t);
        no->f1 = fator(p);
        return no;
    }

    default:
        erro_sint(p, "um numero, variavel, chamada, raiz(), VER/FAL ou '('");
        return NULL;
    }
}

/* pot -> fator [ '^' pot ]   associa a direita: 2^3^2 = 2^9 */
static Ast *pot(Parser *p)
{
    Ast *esq = fator(p);
    if (p->lookahead.tipo == T_POT)
    {
        Token op = p->lookahead;
        avancar(p);
        Ast *no = ast_novo(A_BIN, op);
        no->f1 = esq;
        if (entrar(p))
        {
            no->f2 = pot(p);
            sair(p);
        }
        return no;
    }
    return esq;
}

/* mult -> pot { ('*' | '/' | '//') pot } */
static Ast *mult(Parser *p)
{
    Ast *esq = pot(p);
    while (p->lookahead.tipo == T_VEZES || p->lookahead.tipo == T_DIVIDE ||
           p->lookahead.tipo == T_DIVINT)
    {
        Token op = p->lookahead;
        avancar(p);
        Ast *no = ast_novo(A_BIN, op);
        no->f1 = esq;
        no->f2 = pot(p);
        esq = no;
    }
    return esq;
}

/* expr -> mult { ('+' | '-') mult } */
static Ast *expr(Parser *p)
{
    Ast *esq = mult(p);
    while (p->lookahead.tipo == T_MAIS || p->lookahead.tipo == T_MENOS)
    {
        Token op = p->lookahead;
        avancar(p);
        Ast *no = ast_novo(A_BIN, op);
        no->f1 = esq;
        no->f2 = mult(p);
        esq = no;
    }
    return esq;
}

/* na condicao o operando e so variavel, numero ou VER/FAL (sem conta) */
static Ast *operando_cond(Parser *p)
{
    Token t = p->lookahead;

    if (t.tipo == T_IDENT)
    {
        avancar(p);
        return acesso(p, t);
    }
    if (t.tipo == T_NUM_INT)
    {
        avancar(p);
        return ast_novo(A_NUM_INT, t);
    }
    if (t.tipo == T_NUM_FLUT)
    {
        avancar(p);
        return ast_novo(A_NUM_FLUT, t);
    }
    if (t.tipo == T_VER || t.tipo == T_FAL)
    {
        avancar(p);
        return ast_novo(A_BOOL_LIT, t);
    }

    erro_sint(p, "uma variavel, um numero ou VER/FAL na condicao");
    return NULL;
}

/* cond_atom -> '(' condicao ')' | operando [ comparador operando ] */
static Ast *cond_atom(Parser *p)
{
    if (p->lookahead.tipo == T_ABRE_PAR)
    {
        avancar(p);
        Ast *no = condicao(p);
        casar(p, T_FECHA_PAR, "')'");
        return no;
    }

    Ast *esq = operando_cond(p);
    Token op = p->lookahead;
    if (op.tipo == T_MENOR || op.tipo == T_MAIOR ||
        op.tipo == T_IGUAL || op.tipo == T_DIFERENTE)
    {
        avancar(p);
        Ast *no = ast_novo(A_COMP, op);
        no->f1 = esq;
        no->f2 = operando_cond(p);
        return no;
    }

    /* mensagem melhor que "esperava ')'" pra quem pos conta na condicao */
    if (op.tipo == T_MAIS || op.tipo == T_MENOS || op.tipo == T_VEZES ||
        op.tipo == T_DIVIDE || op.tipo == T_DIVINT || op.tipo == T_POT)
        erro_sint(p, "um comparador ou ')' (aritmetica nao e permitida dentro "
                     "da condicao: calcule antes, guarde numa variavel e "
                     "compare a variavel)");
    return esq;
}

static Ast *cond_nao(Parser *p)
{
    if (!entrar(p))
        return NULL;
    Ast *r;
    if (p->lookahead.tipo == T_NAO)
    {
        Token t = p->lookahead;
        avancar(p);
        r = ast_novo(A_NAO, t);
        r->f1 = cond_nao(p);
    }
    else
        r = cond_atom(p);
    sair(p);
    return r;
}

/* E liga mais forte que OU e XOR */
static Ast *cond_e(Parser *p)
{
    Ast *esq = cond_nao(p);
    while (p->lookahead.tipo == T_E)
    {
        Token op = p->lookahead;
        avancar(p);
        Ast *no = ast_novo(A_LOGICO, op);
        no->f1 = esq;
        no->f2 = cond_nao(p);
        esq = no;
    }
    return esq;
}

static Ast *condicao(Parser *p)
{
    Ast *esq = cond_e(p);
    while (p->lookahead.tipo == T_OU || p->lookahead.tipo == T_XOR)
    {
        Token op = p->lookahead;
        avancar(p);
        Ast *no = ast_novo(A_LOGICO, op);
        no->f1 = esq;
        no->f2 = cond_e(p);
        esq = no;
    }
    return esq;
}

static Ast *lvalue(Parser *p)
{
    Token nome = casar(p, T_IDENT, "um nome de variavel");
    return acesso(p, nome);
}

/* atribuicao sem o ';', o repete usa com virgula */
static Ast *atrib_nucleo(Parser *p)
{
    Ast *alvo = lvalue(p);
    Token r = casar(p, T_RECEBA, "'receba'");
    Ast *no = ast_novo(A_ATRIB, r);
    no->f1 = alvo;
    no->f2 = expr(p);
    return no;
}

/* decl -> tipo [ 'vet' '{' NUM '}' ] IDENT { ',' IDENT } ';' */
static Ast *decl_variaveis(Parser *p, Token tipo_tok)
{
    Tipo tp = token_para_tipo(tipo_tok.tipo);
    int e_vetor = 0;
    Token tamanho;
    memset(&tamanho, 0, sizeof(tamanho));

    if (p->lookahead.tipo == T_VET)
    {
        e_vetor = 1;
        avancar(p);
        casar(p, T_ABRE_CHAVE, "'{' com o tamanho do vetor");
        tamanho = casar(p, T_NUM_INT, "um numero inteiro (tamanho do vetor)");
        casar(p, T_FECHA_CHAVE, "'}'");
    }

    Ast *primeiro = NULL, *ultimo = NULL;
    for (;;)
    {
        Token nome = casar(p, T_IDENT, "um nome de variavel");
        Ast *no = ast_novo(e_vetor ? A_DECL_VET : A_DECL_VAR, nome);
        no->tipo_dado = tp;
        if (e_vetor)
            no->f1 = ast_novo(A_NUM_INT, tamanho);

        if (primeiro == NULL)
            primeiro = no;
        else
            ultimo->prox = no;
        ultimo = no;

        if (p->lookahead.tipo != T_VIRGULA)
            break;
        avancar(p);
    }
    casar(p, T_PONTOVIRG, "';' encerrando a declaracao");
    return primeiro;
}

static Ast *decl_registros(Parser *p, Token tipo_reg)
{
    Ast *primeiro = NULL, *ultimo = NULL;
    for (;;)
    {
        Token nome = casar(p, T_IDENT, "o nome da variavel de registro");
        Ast *no = ast_novo(A_DECL_REG, nome);
        no->tipo_dado = TP_REGISTRO;
        no->f1 = ast_novo(A_VAR, tipo_reg);

        if (primeiro == NULL)
            primeiro = no;
        else
            ultimo->prox = no;
        ultimo = no;

        if (p->lookahead.tipo != T_VIRGULA)
            break;
        avancar(p);
    }
    casar(p, T_PONTOVIRG, "';' encerrando a declaracao");
    return primeiro;
}

/* cond (c) edai bloco [ casocontrario (cond... | edai bloco) ] */
static Ast *condicional(Parser *p)
{
    Token t = casar(p, T_COND, "'cond'");
    Ast *no = ast_novo(A_COND, t);

    casar(p, T_ABRE_PAR, "'('");
    no->f1 = condicao(p);
    casar(p, T_FECHA_PAR, "')'");
    casar(p, T_EDAI, "'edai'");
    no->f2 = bloco(p);

    if (p->lookahead.tipo == T_CASOCONTRARIO)
    {
        avancar(p);
        if (p->lookahead.tipo == T_COND)
        {
            if (entrar(p))
            {
                no->f3 = condicional(p);
                sair(p);
            }
        }
        else
        {
            casar(p, T_EDAI, "'edai' ou 'cond' apos casocontrario");
            no->f3 = bloco(p);
        }
    }
    return no;
}

static Ast *comando(Parser *p)
{
    Token t = p->lookahead;

    if (e_token_de_tipo(t.tipo))
    {
        avancar(p);
        return decl_variaveis(p, t);
    }

    switch (t.tipo)
    {
    case T_REGISTRO:
    {
        avancar(p);
        Token tipo_reg = casar(p, T_IDENT, "o nome do tipo de registro");
        return decl_registros(p, tipo_reg);
    }

    case T_REPETE:
    {
        avancar(p);
        Ast *no = ast_novo(A_REPETE, t);
        casar(p, T_ABRE_PAR, "'('");
        no->f1 = atrib_nucleo(p);
        casar(p, T_VIRGULA, "','");
        no->f2 = condicao(p);
        casar(p, T_VIRGULA, "','");
        no->f3 = atrib_nucleo(p);
        casar(p, T_FECHA_PAR, "')'");
        casar(p, T_EDAI, "'edai'");
        no->f4 = bloco(p);
        return no;
    }

    case T_DURANTE:
    {
        avancar(p);
        Ast *no = ast_novo(A_DURANTE, t);
        casar(p, T_ABRE_PAR, "'('");
        no->f1 = condicao(p);
        casar(p, T_FECHA_PAR, "')'");
        casar(p, T_EDAI, "'edai'");
        no->f2 = bloco(p);
        return no;
    }

    case T_COND:
        return condicional(p);

    case T_RESPOST:
    {
        avancar(p);
        Ast *no = ast_novo(A_RESPOST, t);
        if (p->lookahead.tipo != T_PONTOVIRG)
            no->f1 = expr(p);
        casar(p, T_PONTOVIRG, "';'");
        return no;
    }

    case T_ESCREVA:
    {
        avancar(p);
        Ast *no = ast_novo(A_ESCREVA, t);
        casar(p, T_ABRE_PAR, "'('");
        no->f1 = expr(p);
        casar(p, T_FECHA_PAR, "')'");
        casar(p, T_PONTOVIRG, "';'");
        return no;
    }

    case T_LEIA:
    {
        avancar(p);
        Ast *no = ast_novo(A_LEIA, t);
        casar(p, T_ABRE_PAR, "'('");
        no->f1 = lvalue(p);
        casar(p, T_FECHA_PAR, "')'");
        casar(p, T_PONTOVIRG, "';'");
        return no;
    }

    case T_IDENT:
    {
        avancar(p);
        if (p->lookahead.tipo == T_ABRE_PAR)
        {
            Ast *no = ast_novo(A_CMD_CHAMADA, t);
            no->f1 = chamada(p, t);
            casar(p, T_PONTOVIRG, "';'");
            return no;
        }
        Ast *alvo = acesso(p, t);
        Token r = casar(p, T_RECEBA, "'receba'");
        Ast *no = ast_novo(A_ATRIB, r);
        no->f1 = alvo;
        no->f2 = expr(p);
        casar(p, T_PONTOVIRG, "';'");
        return no;
    }

    default:
        erro_sint(p, "um comando (declaracao, atribuicao, repete, durante, "
                     "cond, respost, escreva, leia ou chamada)");
        return NULL;
    }
}

static Ast *bloco_interno(Parser *p);

static Ast *bloco(Parser *p)
{
    if (!entrar(p))
        return NULL;
    Ast *r = bloco_interno(p);
    sair(p);
    return r;
}

static Ast *bloco_interno(Parser *p)
{
    Token t = casar(p, T_ABRE_COL, "'[' abrindo o bloco");
    Ast *no = ast_novo(A_BLOCO, t);

    Ast *ultimo = NULL;
    while (p->lookahead.tipo != T_FECHA_COL && p->lookahead.tipo != T_EOF)
    {
        Ast *cmd = comando(p);
        if (p->panico)
            sincronizar(p);
        if (cmd == NULL)
            continue;
        if (no->f1 == NULL)
            no->f1 = cmd;
        else
            ultimo->prox = cmd;
        ultimo = cmd;
        while (ultimo->prox != NULL)
            ultimo = ultimo->prox;
    }
    casar(p, T_FECHA_COL, "']' fechando o bloco");
    return no;
}

static Ast *parametros(Parser *p)
{
    if (p->lookahead.tipo == T_FECHA_PAR)
        return NULL;

    Ast *primeiro = NULL, *ultimo = NULL;
    for (;;)
    {
        Token tipo_tok = p->lookahead;
        if (!e_token_de_tipo(tipo_tok.tipo))
        {
            erro_sint(p, "um tipo de dado no parametro");
            break;
        }
        avancar(p);
        Token nome = casar(p, T_IDENT, "o nome do parametro");

        Ast *no = ast_novo(A_PARAM, nome);
        no->tipo_dado = token_para_tipo(tipo_tok.tipo);
        if (primeiro == NULL)
            primeiro = no;
        else
            ultimo->prox = no;
        ultimo = no;

        if (p->lookahead.tipo != T_VIRGULA)
            break;
        avancar(p);
    }
    return primeiro;
}

/* funcao -> ('outrafuncao' | 'func') tipo IDENT '(' params ')' '@' bloco */
static Ast *funcao(Parser *p)
{
    avancar(p);

    Token tipo_tok = p->lookahead;
    Tipo retorno = TP_ERRO;
    if (e_token_de_tipo(tipo_tok.tipo))
    {
        retorno = token_para_tipo(tipo_tok.tipo);
        avancar(p);
    }
    else
        erro_sint(p, "o tipo de retorno da funcao");

    Token nome = casar(p, T_IDENT, "o nome da funcao");
    Ast *no = ast_novo(A_FUNCAO, nome);
    no->tipo_dado = retorno;

    casar(p, T_ABRE_PAR, "'('");
    no->f1 = parametros(p);
    casar(p, T_FECHA_PAR, "')'");
    casar(p, T_ARROBA, "'@' antes do corpo da funcao");
    no->f2 = bloco(p);
    return no;
}

static Ast *registro(Parser *p)
{
    avancar(p);
    Token nome = casar(p, T_IDENT, "o nome do registro");
    if (p->lookahead.tipo == T_IDENT)
        return decl_registros(p, nome);
    Ast *no = ast_novo(A_REGISTRO, nome);

    casar(p, T_ARROBA, "'@'");
    casar(p, T_ABRE_COL, "'[' abrindo os campos do registro");

    Ast *ultimo = NULL;
    while (p->lookahead.tipo != T_FECHA_COL && p->lookahead.tipo != T_EOF)
    {
        Token tipo_tok = p->lookahead;
        if (!e_token_de_tipo(tipo_tok.tipo))
        {
            erro_sint(p, "um tipo de dado no campo do registro");
            sincronizar(p);
            continue;
        }
        avancar(p);
        Ast *campo = decl_variaveis(p, tipo_tok);
        if (p->panico)
            sincronizar(p);
        if (campo == NULL)
            continue;
        if (no->f1 == NULL)
            no->f1 = campo;
        else
            ultimo->prox = campo;
        ultimo = campo;
        while (ultimo->prox != NULL)
            ultimo = ultimo->prox;
    }
    casar(p, T_FECHA_COL, "']' fechando o registro");
    return no;
}

static Ast *principal(Parser *p, Token tipo_tok)
{
    Token nome = casar(p, T_PRINCIPAL, "'principal'");
    Ast *no = ast_novo(A_PRINCIPAL, nome);
    no->tipo_dado = token_para_tipo(tipo_tok.tipo);

    casar(p, T_ABRE_PAR, "'('");
    no->f1 = parametros(p);
    casar(p, T_FECHA_PAR, "')'");
    casar(p, T_ARROBA, "'@' antes do corpo de principal");
    no->f2 = bloco(p);
    return no;
}

/* programa -> { registro | funcao | principal | declaracao global } */
Ast *parse_programa(Lexer *lx)
{
    Parser p;
    p.lx = lx;
    p.panico = 0;
    p.profundidade = 0;
    p.fim_avisado = 0;
    avancar(&p);

    Token raiz_tok;
    memset(&raiz_tok, 0, sizeof(raiz_tok));
    Ast *raiz = ast_novo(A_PROGRAMA, raiz_tok);
    Ast *ultimo = NULL;

    while (p.lookahead.tipo != T_EOF)
    {
        Ast *decl = NULL;
        Token t = p.lookahead;

        if (t.tipo == T_REGISTRO)
            decl = registro(&p);
        else if (t.tipo == T_OUTRAFUNCAO || t.tipo == T_FUNC)
            decl = funcao(&p);
        else if (e_token_de_tipo(t.tipo))
        {
            avancar(&p);
            if (p.lookahead.tipo == T_PRINCIPAL)
                decl = principal(&p, t);
            else
                decl = decl_variaveis(&p, t);
        }
        else
        {
            erro_sint(&p, "'registro', 'outrafuncao', 'principal' ou uma "
                          "declaracao de variavel global");
            do
                avancar(&p);
            while (p.lookahead.tipo != T_EOF && p.lookahead.tipo != T_REGISTRO &&
                   p.lookahead.tipo != T_OUTRAFUNCAO && p.lookahead.tipo != T_FUNC &&
                   !e_token_de_tipo(p.lookahead.tipo));
            p.panico = 0;
        }

        if (p.panico)
            sincronizar(&p);
        if (decl == NULL)
            continue;

        if (raiz->f1 == NULL)
            raiz->f1 = decl;
        else
            ultimo->prox = decl;
        ultimo = decl;
        while (ultimo->prox != NULL)
            ultimo = ultimo->prox;
    }
    return raiz;
}
