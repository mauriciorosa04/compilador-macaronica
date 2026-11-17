#include <stdio.h>
#include <stdlib.h>
#include "ast.h"

const char *nome_tipo(Tipo t)
{
    switch (t)
    {
    case TP_BOOL: return "bool";
    case TP_INTEIRA: return "inteira";
    case TP_FLUT: return "flut";
    case TP_DUPLO: return "duplocarpado";
    case TP_PALAVRA: return "palavra";
    case TP_VAZIO: return "vazio";
    case TP_REGISTRO: return "registro";
    case TP_ERRO: return "<erro>";
    default: return "<indefinido>";
    }
}

Ast *ast_novo(TipoAst tipo, Token tok)
{
    Ast *no = (Ast *)calloc(1, sizeof(Ast));
    if (no == NULL)
    {
        fprintf(stderr, "erro fatal: memoria insuficiente\n");
        exit(EXIT_FAILURE);
    }
    no->tipo = tipo;
    no->tok = tok;
    no->tipo_dado = TP_INDEFINIDO;
    return no;
}

/* pilha no heap: liberar e imprimir sem recursao, porque a arvore
   pode ter centenas de milhares de niveis */
typedef struct
{
    const Ast *no;
    int nivel;
} Item;

typedef struct
{
    Item *itens;
    size_t usados, cap;
} Pilha;

static void empilhar(Pilha *p, const Ast *no, int nivel)
{
    if (no == NULL)
        return;
    if (p->usados == p->cap)
    {
        size_t nova = p->cap ? p->cap * 2 : 64;
        Item *maior = (Item *)realloc(p->itens, nova * sizeof(Item));
        if (maior == NULL)
        {
            fprintf(stderr, "erro fatal: memoria insuficiente\n");
            exit(EXIT_FAILURE);
        }
        p->itens = maior;
        p->cap = nova;
    }
    p->itens[p->usados].no = no;
    p->itens[p->usados].nivel = nivel;
    p->usados++;
}

void ast_liberar(Ast *raiz)
{
    Pilha p = {NULL, 0, 0};
    empilhar(&p, raiz, 0);
    while (p.usados > 0)
    {
        const Ast *no = p.itens[--p.usados].no;
        empilhar(&p, no->f1, 0);
        empilhar(&p, no->f2, 0);
        empilhar(&p, no->f3, 0);
        empilhar(&p, no->f4, 0);
        empilhar(&p, no->prox, 0);
        free((void *)no);
    }
    free(p.itens);
}

static const char *rotulo(const Ast *no)
{
    switch (no->tipo)
    {
    case A_PROGRAMA: return "PROGRAMA";
    case A_REGISTRO: return "REGISTRO";
    case A_FUNCAO: return "FUNCAO";
    case A_PRINCIPAL: return "PRINCIPAL";
    case A_PARAM: return "PARAM";
    case A_BLOCO: return "BLOCO";
    case A_DECL_VAR: return "DECL_VAR";
    case A_DECL_VET: return "DECL_VET";
    case A_DECL_REG: return "DECL_REGISTRO";
    case A_ATRIB: return "ATRIBUICAO (receba)";
    case A_ATRIB_VET: return "ATRIBUICAO VETORIAL";
    case A_REPETE: return "REPETE";
    case A_DURANTE: return "DURANTE";
    case A_COND: return "COND";
    case A_RESPOST: return "RESPOST";
    case A_ESCREVA: return "ESCREVA";
    case A_LEIA: return "LEIA";
    case A_CMD_CHAMADA: return "CHAMADA (comando)";
    case A_BIN: return "OP_BIN";
    case A_NEG: return "NEG_UNARIO";
    case A_COMP: return "COMPARACAO";
    case A_LOGICO: return "OP_LOGICO";
    case A_NAO: return "NAO";
    case A_RAIZ: return "RAIZ";
    case A_REDUCAO: return "REDUCAO";
    case A_CHAMADA: return "CHAMADA";
    case A_VAR: return "VAR";
    case A_VET_ACESSO: return "VET_ACESSO";
    case A_CAMPO: return "CAMPO";
    case A_NUM_INT: return "NUM_INT";
    case A_NUM_FLUT: return "NUM_FLUT";
    case A_STR: return "STRING";
    case A_BOOL_LIT: return "BOOL_LIT";
    default: return "?";
    }
}

void ast_imprimir(const Ast *raiz, int nivel)
{
    Pilha p = {NULL, 0, 0};
    empilhar(&p, raiz, nivel);
    while (p.usados > 0)
    {
        Item it = p.itens[--p.usados];
        const Ast *no = it.no;

        for (int i = 0; i < it.nivel; i++)
            printf("  ");
        printf("%s", rotulo(no));
        if (no->tok.lexema[0] != '\0')
            printf(" '%s'", no->tok.lexema);
        if (no->tipo_dado != TP_INDEFINIDO)
            printf("  : %s", nome_tipo(no->tipo_dado));
        printf("\n");

        /* empilha ao contrario pra sair na ordem no, filhos, irmaos */
        empilhar(&p, no->prox, it.nivel);
        empilhar(&p, no->f4, it.nivel + 1);
        empilhar(&p, no->f3, it.nivel + 1);
        empilhar(&p, no->f2, it.nivel + 1);
        empilhar(&p, no->f1, it.nivel + 1);
    }
    free(p.itens);
}
