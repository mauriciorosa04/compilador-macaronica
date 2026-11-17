#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symtab.h"
#include "erros.h"

static Escopo *escopo_atual = NULL;

/* djb2 */
static unsigned hash(const char *s)
{
    unsigned h = 5381;
    while (*s)
        h = h * 33 + (unsigned char)*s++;
    return h;
}

void symtab_abrir_escopo(const char *descricao)
{
    Escopo *e = (Escopo *)calloc(1, sizeof(Escopo));
    if (e == NULL)
    {
        fprintf(stderr, "erro fatal: memoria insuficiente\n");
        exit(EXIT_FAILURE);
    }
    e->pai = escopo_atual;
    e->descricao = descricao;
    escopo_atual = e;
}

/* dobra a hash. O simbolo nao muda de endereco, so o vetor de buckets */
static void crescer(Escopo *e)
{
    int novo_n = e->nbuckets ? e->nbuckets * 2 : SYM_BUCKETS_INICIAL;
    Simbolo **novo = (Simbolo **)calloc((size_t)novo_n, sizeof(Simbolo *));
    if (novo == NULL)
    {
        fprintf(stderr, "erro fatal: memoria insuficiente\n");
        exit(EXIT_FAILURE);
    }
    for (Simbolo *s = e->primeiro; s != NULL; s = s->prox_decl)
    {
        unsigned b = s->hash % (unsigned)novo_n;
        s->prox = novo[b];
        novo[b] = s;
    }
    free(e->buckets);
    e->buckets = novo;
    e->nbuckets = novo_n;
}

void symtab_fechar_escopo(void)
{
    if (escopo_atual == NULL)
        return;

    /* anda na ordem de declaracao pros avisos sairem na ordem do fonte */
    Simbolo *s = escopo_atual->primeiro;
    while (s != NULL)
    {
        if (!s->usado &&
            (s->categoria == SIM_VAR || s->categoria == SIM_VET ||
             s->categoria == SIM_PARAM))
        {
            aviso_semantico(s->linha, 1,
                            "'%s' foi declarado mas nunca utilizado", s->nome);
        }
        Simbolo *morto = s;
        s = s->prox_decl;
        free(morto);
    }

    Escopo *fechado = escopo_atual;
    escopo_atual = fechado->pai;
    free(fechado->buckets);
    free(fechado);
}

static Simbolo *buscar_no_escopo(const Escopo *e, const char *nome, unsigned h)
{
    if (e->nbuckets == 0)
        return NULL;
    for (Simbolo *s = e->buckets[h % (unsigned)e->nbuckets]; s; s = s->prox)
        if (s->hash == h && strcmp(s->nome, nome) == 0)
            return s;
    return NULL;
}

Simbolo *symtab_buscar_local(const char *nome)
{
    if (escopo_atual == NULL)
        return NULL;
    return buscar_no_escopo(escopo_atual, nome, hash(nome));
}

Simbolo *symtab_buscar(const char *nome)
{
    unsigned h = hash(nome);
    for (Escopo *e = escopo_atual; e != NULL; e = e->pai)
    {
        Simbolo *s = buscar_no_escopo(e, nome, h);
        if (s != NULL)
            return s;
    }
    return NULL;
}

Simbolo *symtab_inserir(const char *nome, CategoriaSimbolo cat,
                        Tipo tipo, int linha)
{
    Escopo *e = escopo_atual;
    unsigned h = hash(nome);
    if (buscar_no_escopo(e, nome, h) != NULL)
        return NULL;

    Simbolo *s = (Simbolo *)calloc(1, sizeof(Simbolo));
    if (s == NULL)
    {
        fprintf(stderr, "erro fatal: memoria insuficiente\n");
        exit(EXIT_FAILURE);
    }
    strncpy(s->nome, nome, SYM_NOME_MAX - 1);
    s->categoria = cat;
    s->tipo = tipo;
    s->linha = linha;
    s->hash = h;
    s->global = (e->pai == NULL);

    if (e->ultimo != NULL)
        e->ultimo->prox_decl = s;
    else
        e->primeiro = s;
    e->ultimo = s;

    if (e->nsimbolos >= e->nbuckets)
    {
        e->nsimbolos++;
        crescer(e);
        return s;
    }
    e->nsimbolos++;
    unsigned b = h % (unsigned)e->nbuckets;
    s->prox = e->buckets[b];
    e->buckets[b] = s;
    return s;
}
