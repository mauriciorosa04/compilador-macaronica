/* tabela de simbolos: uma hash por escopo, a busca sobe ate o global */
#ifndef SYMTAB_H
#define SYMTAB_H

#include "ast.h"

#define SYM_NOME_MAX LEXEMA_MAX
#define SYM_MAX_PARAMS 64

#define SYM_BUCKETS_INICIAL 8

typedef enum
{
    SIM_VAR,
    SIM_VET,
    SIM_PARAM,
    SIM_FUNC,
    SIM_REGISTRO
} CategoriaSimbolo;

typedef struct Simbolo
{
    char nome[SYM_NOME_MAX];
    CategoriaSimbolo categoria;
    Tipo tipo;
    int linha;
    int usado;
    int inicializado;
    int global;
    int tamanho;
    int nparams; /* nparams e params so valem pra funcao */
    Tipo params[SYM_MAX_PARAMS];
    struct Ast *decl;
    unsigned hash;
    struct Simbolo *prox;
    struct Simbolo *prox_decl; /* ordem de declaracao */
} Simbolo;

typedef struct Escopo
{
    Simbolo **buckets;
    int nbuckets;
    int nsimbolos;
    Simbolo *primeiro;
    Simbolo *ultimo;
    struct Escopo *pai;
    const char *descricao;
} Escopo;

void symtab_abrir_escopo(const char *descricao);
void symtab_fechar_escopo(void); /* avisa o que nao foi usado */

/* devolve NULL se o nome ja existe no escopo atual */
Simbolo *symtab_inserir(const char *nome, CategoriaSimbolo cat,
                        Tipo tipo, int linha);

Simbolo *symtab_buscar(const char *nome);

Simbolo *symtab_buscar_local(const char *nome);

#endif
