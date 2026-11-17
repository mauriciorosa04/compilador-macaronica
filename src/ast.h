/* arvore sintatica. Cada no tem ate 4 filhos (f1..f4) e o prox,
   que liga os irmaos de uma lista (comandos, parametros, argumentos) */
#ifndef AST_H
#define AST_H

#include "lexer.h"

typedef enum
{
    TP_BOOL,
    TP_INTEIRA,
    TP_FLUT,
    TP_DUPLO,
    TP_PALAVRA,
    TP_VAZIO,
    TP_REGISTRO,
    TP_INDEFINIDO,
    TP_ERRO /* sub-arvore que ja deu erro, pra nao repetir mensagem */
} Tipo;

const char *nome_tipo(Tipo t);

typedef enum
{
    A_PROGRAMA,
    A_REGISTRO, /* f1 = campos */
    A_FUNCAO, /* f1 = params, f2 = bloco */
    A_PRINCIPAL,
    A_PARAM,
    A_BLOCO,

    A_DECL_VAR,
    A_DECL_VET, /* f1 = tamanho */
    A_DECL_REG,
    A_ATRIB, /* f1 = destino, f2 = expressao */
    A_ATRIB_VET, /* a semantica troca A_ATRIB por este quando o destino e vetor */
    A_REPETE, /* f1 = inicio, f2 = condicao, f3 = passo, f4 = bloco */
    A_DURANTE,
    A_COND, /* f1 = condicao, f2 = entao, f3 = outro A_COND ou o bloco do senao */
    A_RESPOST, /* f1 pode ser NULL */
    A_ESCREVA,
    A_LEIA,
    A_CMD_CHAMADA,

    A_BIN, /* tok = operador */
    A_NEG,
    A_COMP,
    A_LOGICO,
    A_NAO,
    A_RAIZ,
    A_REDUCAO,
    A_CHAMADA, /* f1 = primeiro argumento, os outros pelo prox */
    A_VAR,
    A_VET_ACESSO,
    A_CAMPO,
    A_NUM_INT,
    A_NUM_FLUT,
    A_STR,
    A_BOOL_LIT
} TipoAst;

typedef struct Ast
{
    TipoAst tipo;
    Token tok;
    Tipo tipo_dado;
    struct Ast *f1, *f2, *f3, *f4;
    struct Ast *prox;

    /* uso -> declaracao, preenchido pela semantica. O gerador de codigo
       usa isso e nao precisa da tabela de simbolos */
    struct Ast *decl;
    int num; /* -1 global, 0 corpo da funcao, N bloco interno */
} Ast;

Ast *ast_novo(TipoAst tipo, Token tok);
void ast_liberar(Ast *no);
void ast_imprimir(const Ast *no, int nivel);

#endif
