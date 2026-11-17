/* analise semantica. Comandos de cima pra baixo, expressoes de baixo pra
   cima. Quem ja deu erro devolve TP_ERRO e o resto fica quieto, pra
   sair uma mensagem so por problema. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include "semantic.h"
#include "symtab.h"
#include "erros.h"

static Tipo retorno_atual = TP_VAZIO;
static const char *nome_func_atual = "";
static int teve_respost_com_valor = 0;
static int teve_respost_errado = 0;

static int achou_principal = 0;

/* conta de RAM do Arduino Uno: globais + textos + principal + maior funcao.
   A reserva fica pra biblioteca e pros enderecos de retorno */
#define UNO_RAM 2048
#define UNO_RESERVA 448
static int alvo_uno = 0;
static int em_funcao = 0;
static int em_registro = 0;
static long long ram_globais, ram_textos, ram_funcao;
static long long ram_principal, ram_maior_funcao;
static long long maior_var_bytes;
static Token maior_var_tok;

int semantica_definir_alvo(const char *alvo)
{
    if (strcmp(alvo, "pc") == 0)
    {
        alvo_uno = 0;
        return 1;
    }
    if (strcmp(alvo, "uno") == 0)
    {
        alvo_uno = 1;
        return 1;
    }
    return 0;
}

/* tamanho de cada tipo no Uno (ponteiro tem 2 bytes) */
static long long bytes_tipo(Tipo t)
{
    switch (t)
    {
    case TP_BOOL: return 1;
    case TP_INTEIRA: return 4;
    case TP_FLUT: return 4;
    case TP_DUPLO: return 8;
    case TP_PALAVRA: return 2;
    default: return 0;
    }
}

static void conferir_tipo_no_alvo(Tipo t, Token onde)
{
    if (alvo_uno && t == TP_DUPLO)
        erro_semantico(onde.linha, onde.coluna,
                       "duplocarpado nao existe no alvo uno: a placa nao tem "
                       "ponto flutuante de 64 bits (use flut)");
}

/* inteira < flut < duplocarpado, so promove pra cima */
static int posto_numerico(Tipo t)
{
    switch (t)
    {
    case TP_INTEIRA: return 1;
    case TP_FLUT: return 2;
    case TP_DUPLO: return 3;
    default: return 0;
    }
}

static int e_numerico(Tipo t) { return posto_numerico(t) > 0; }

static int atribuivel(Tipo dest, Tipo src)
{
    if (dest == TP_ERRO || src == TP_ERRO)
        return 1;
    if (dest == src)
        return 1;
    if (e_numerico(dest) && e_numerico(src))
        return posto_numerico(dest) >= posto_numerico(src);
    return 0;
}

static Tipo maior_tipo(Tipo a, Tipo b)
{
    return posto_numerico(a) >= posto_numerico(b) ? a : b;
}

#define INTEIRA_MAX 2147483647LL

/* satura em vez de estourar: o literal pode ter mais de 100 digitos */
static long long valor_literal(const char *lexema)
{
    long long v = 0;
    for (const char *c = lexema; *c >= '0' && *c <= '9'; c++)
    {
        v = v * 10 + (*c - '0');
        if (v > INTEIRA_MAX + 1)
            return INTEIRA_MAX + 2;
    }
    return v;
}

static Tipo tipo_expr(Ast *no);

/* nao achou: reclama uma vez e insere como TP_ERRO */
static Simbolo *resolver_variavel(Token tok)
{
    Simbolo *s = symtab_buscar(tok.lexema);
    if (s == NULL)
    {
        erro_semantico(tok.linha, tok.coluna,
                       "variavel '%s' usada sem ter sido declarada", tok.lexema);
        s = symtab_inserir(tok.lexema, SIM_VAR, TP_ERRO, tok.linha);
        if (s != NULL)
            s->usado = 1;
        return s;
    }
    s->usado = 1;
    return s;
}

static Tipo tipo_chamada(Ast *no)
{
    Simbolo *f = symtab_buscar(no->tok.lexema);
    if (f == NULL)
    {
        erro_semantico(no->tok.linha, no->tok.coluna,
                       "funcao '%s' chamada sem ter sido declarada",
                       no->tok.lexema);
        return TP_ERRO;
    }
    if (f->categoria != SIM_FUNC)
    {
        erro_semantico(no->tok.linha, no->tok.coluna,
                       "'%s' nao e' uma funcao (declarado como %s na linha %d)",
                       no->tok.lexema,
                       f->categoria == SIM_VET ? "vetor" : "variavel",
                       f->linha);
        return TP_ERRO;
    }
    f->usado = 1;
    no->decl = f->decl;

    int i = 0;
    for (Ast *arg = no->f1; arg != NULL; arg = arg->prox, i++)
    {
        Tipo t_arg = tipo_expr(arg);
        if (i < f->nparams && i < SYM_MAX_PARAMS && !atribuivel(f->params[i], t_arg))
        {
            erro_semantico(arg->tok.linha, arg->tok.coluna,
                           "argumento %d de '%s': esperava %s, recebeu %s",
                           i + 1, no->tok.lexema,
                           nome_tipo(f->params[i]), nome_tipo(t_arg));
        }
    }
    if (i != f->nparams)
    {
        erro_semantico(no->tok.linha, no->tok.coluna,
                       "'%s' espera %d argumento(s), mas recebeu %d",
                       no->tok.lexema, f->nparams, i);
    }
    return f->tipo;
}

static Tipo exigir_numerico(Ast *no, Tipo t, const char *contexto)
{
    if (t == TP_ERRO)
        return TP_ERRO;
    if (!e_numerico(t))
    {
        erro_semantico(no->tok.linha, no->tok.coluna,
                       "%s exige operando numerico "
                       "(inteira, flut ou duplocarpado), recebeu %s",
                       contexto, nome_tipo(t));
        return TP_ERRO;
    }
    return t;
}

/* indice literal da pra conferir na compilacao */
static void conferir_indice_literal(const Ast *indice, int tamanho,
                                    const char *nome)
{
    if (indice == NULL || tamanho <= 0)
        return;
    if (indice->tipo == A_NUM_INT)
    {
        long long idx = valor_literal(indice->tok.lexema);
        if (idx >= tamanho)
            erro_semantico(indice->tok.linha, indice->tok.coluna,
                           "indice %ld fora do vetor '%s' (tamanho %d, "
                           "indices validos de 0 a %d)",
                           (long)idx, nome, tamanho, tamanho - 1);
    }
    else if (indice->tipo == A_NEG && indice->f1 != NULL &&
             indice->f1->tipo == A_NUM_INT &&
             valor_literal(indice->f1->tok.lexema) > 0)
        erro_semantico(indice->tok.linha, indice->tok.coluna,
                       "indice negativo no vetor '%s'", nome);
}

/* nome.campo ou nome.campo{i}: procura na lista de campos do registro */
static Tipo tipo_campo(Ast *no)
{
    Ast *base = no->f1;
    Tipo t_indice = (no->f2 != NULL) ? tipo_expr(no->f2) : TP_INTEIRA;

    Simbolo *s = resolver_variavel(base->tok);
    if (s == NULL)
        return TP_ERRO;
    if (s->categoria == SIM_REGISTRO)
    {
        erro_semantico(base->tok.linha, base->tok.coluna,
                       "'%s' e' o nome de um tipo de registro; declare uma "
                       "variavel dele: registro %s nome;",
                       base->tok.lexema, base->tok.lexema);
        return TP_ERRO;
    }
    if (s->tipo == TP_ERRO)
        return TP_ERRO;
    if (s->tipo != TP_REGISTRO || s->decl == NULL || s->decl->decl == NULL)
    {
        erro_semantico(base->tok.linha, base->tok.coluna,
                       "'%s' nao e' um registro e nao tem campo '%s'",
                       base->tok.lexema, no->tok.lexema);
        return TP_ERRO;
    }
    base->decl = s->decl;
    base->tipo_dado = TP_REGISTRO;

    Ast *reg = s->decl->decl;
    Ast *campo = reg->f1;
    while (campo != NULL && strcmp(campo->tok.lexema, no->tok.lexema) != 0)
        campo = campo->prox;
    if (campo == NULL)
    {
        erro_semantico(no->tok.linha, no->tok.coluna,
                       "registro '%s' nao tem campo '%s'",
                       reg->tok.lexema, no->tok.lexema);
        return TP_ERRO;
    }
    no->decl = campo;

    if (campo->tipo == A_DECL_VET)
    {
        if (no->f2 == NULL)
        {
            erro_semantico(no->tok.linha, no->tok.coluna,
                           "campo '%s' e' um vetor: acesse um elemento com "
                           "%s.%s{indice}",
                           no->tok.lexema, base->tok.lexema, no->tok.lexema);
            return TP_ERRO;
        }
        if (t_indice != TP_ERRO && t_indice != TP_INTEIRA)
            erro_semantico(no->tok.linha, no->tok.coluna,
                           "indice de vetor deve ser inteira, recebeu %s",
                           nome_tipo(t_indice));
        else if (t_indice == TP_INTEIRA)
        {
            long long tam = valor_literal(campo->f1->tok.lexema);
            conferir_indice_literal(no->f2,
                                    (tam > 0 && tam <= INTEIRA_MAX) ? (int)tam : 0,
                                    no->tok.lexema);
        }
    }
    else if (no->f2 != NULL)
    {
        erro_semantico(no->tok.linha, no->tok.coluna,
                       "campo '%s' nao e' um vetor", no->tok.lexema);
        return TP_ERRO;
    }
    return campo->tipo_dado;
}

/* operando de operacao vetorial tem que ser nome de vetor de numero */
static Simbolo *vetor_numerico(Ast *no, const char *onde)
{
    Simbolo *s = resolver_variavel(no->tok);
    if (s == NULL || s->tipo == TP_ERRO)
        return NULL;
    if (s->categoria != SIM_VET)
    {
        erro_semantico(no->tok.linha, no->tok.coluna,
                       "%s espera o nome de um vetor, e '%s' nao e' vetor",
                       onde, no->tok.lexema);
        return NULL;
    }
    if (!e_numerico(s->tipo))
    {
        erro_semantico(no->tok.linha, no->tok.coluna,
                       "%s so vale pra vetor de inteira, flut ou "
                       "duplocarpado; '%s' e' vetor de %s",
                       onde, no->tok.lexema, nome_tipo(s->tipo));
        return NULL;
    }
    if (s->tamanho == 0)
        return NULL;
    no->decl = s->decl;
    no->tipo_dado = s->tipo;
    return s;
}

static int vetores_combinam(const Ast *onde, const char *oper,
                            const Simbolo *a, const Simbolo *b)
{
    if (a->tipo != b->tipo)
    {
        erro_semantico(onde->tok.linha, onde->tok.coluna,
                       "%s exige vetores do mesmo tipo: '%s' e' de %s e "
                       "'%s' e' de %s",
                       oper, a->nome, nome_tipo(a->tipo), b->nome,
                       nome_tipo(b->tipo));
        return 0;
    }
    if (a->tamanho != b->tamanho)
    {
        erro_semantico(onde->tok.linha, onde->tok.coluna,
                       "%s exige vetores do mesmo tamanho: '%s' tem %d "
                       "elemento(s) e '%s' tem %d",
                       oper, a->nome, a->tamanho, b->nome, b->tamanho);
        return 0;
    }
    return 1;
}

static Tipo tipo_reducao(Ast *no)
{
    Simbolo *a = vetor_numerico(no->f1, no->tok.lexema);
    if (no->f2 == NULL)
        return a != NULL ? a->tipo : TP_ERRO;
    Simbolo *b = vetor_numerico(no->f2, no->tok.lexema);
    if (a == NULL || b == NULL || !vetores_combinam(no, "prodesc", a, b))
        return TP_ERRO;
    return a->tipo;
}

/* tipo de um no so. Em operador binario o lado esquerdo ja vem pronto */
static Tipo tipo_no(Ast *no, Tipo tipo_esq)
{
    Tipo resultado = TP_ERRO;

    switch (no->tipo)
    {
    case A_NUM_INT:
        if (valor_literal(no->tok.lexema) > INTEIRA_MAX)
        {
            erro_semantico(no->tok.linha, no->tok.coluna,
                           "literal %s nao cabe em inteira (maximo %ld)",
                           no->tok.lexema, (long)INTEIRA_MAX);
            break;
        }
        resultado = TP_INTEIRA;
        break;
    case A_NUM_FLUT:
        if (strtod(no->tok.lexema, NULL) > FLT_MAX)
        {
            erro_semantico(no->tok.linha, no->tok.coluna,
                           "literal %s nao cabe em flut", no->tok.lexema);
            break;
        }
        resultado = TP_FLUT;
        break;
    case A_STR:
        ram_textos += no->tok.tam_texto + 1;
        resultado = TP_PALAVRA;
        break;
    case A_BOOL_LIT: resultado = TP_BOOL; break;

    case A_VAR:
    {
        Simbolo *s = resolver_variavel(no->tok);
        if (s == NULL)
            break;
        if (s->categoria == SIM_FUNC)
        {
            erro_semantico(no->tok.linha, no->tok.coluna,
                           "'%s' e' uma funcao: use '%s(...)' para chama-la",
                           no->tok.lexema, no->tok.lexema);
            break;
        }
        if (s->categoria == SIM_VET)
        {
            erro_semantico(no->tok.linha, no->tok.coluna,
                           "'%s' e' um vetor: acesse um elemento com %s{indice}",
                           no->tok.lexema, no->tok.lexema);
            break;
        }
        if (s->categoria == SIM_REGISTRO)
        {
            erro_semantico(no->tok.linha, no->tok.coluna,
                           "'%s' e' o nome de um tipo de registro, nao um valor",
                           no->tok.lexema);
            break;
        }
        if (s->tipo == TP_REGISTRO)
        {
            erro_semantico(no->tok.linha, no->tok.coluna,
                           "'%s' e' um registro: use um campo dele (%s.campo)",
                           no->tok.lexema, no->tok.lexema);
            break;
        }
        no->decl = s->decl;
        if (!s->inicializado && s->categoria == SIM_VAR && s->tipo != TP_ERRO &&
            !s->global)
            aviso_semantico(no->tok.linha, no->tok.coluna,
                            "'%s' pode estar sendo usado antes de receber valor",
                            no->tok.lexema);
        resultado = s->tipo;
        break;
    }

    case A_VET_ACESSO:
    {
        Simbolo *s = resolver_variavel(no->tok);
        Tipo t_indice = tipo_expr(no->f1);
        if (t_indice != TP_ERRO && t_indice != TP_INTEIRA)
            erro_semantico(no->tok.linha, no->tok.coluna,
                           "indice de vetor deve ser inteira, recebeu %s",
                           nome_tipo(t_indice));
        if (s == NULL)
            break;
        if (s->categoria != SIM_VET && s->tipo != TP_ERRO)
        {
            erro_semantico(no->tok.linha, no->tok.coluna,
                           "'%s' nao e' um vetor", no->tok.lexema);
            break;
        }
        if (s->categoria == SIM_VET && t_indice == TP_INTEIRA)
            conferir_indice_literal(no->f1, s->tamanho, no->tok.lexema);
        no->decl = s->decl;
        resultado = s->tipo;
        break;
    }

    case A_CAMPO:
        resultado = tipo_campo(no);
        break;

    case A_REDUCAO:
        resultado = tipo_reducao(no);
        break;

    case A_NEG:
        /* -2147483648: o literal sozinho nao cabe, com o menos cabe */
        if (no->f1 != NULL && no->f1->tipo == A_NUM_INT &&
            valor_literal(no->f1->tok.lexema) == INTEIRA_MAX + 1)
        {
            no->f1->tipo_dado = TP_INTEIRA;
            resultado = TP_INTEIRA;
            break;
        }
        resultado = exigir_numerico(no, tipo_expr(no->f1), "menos unario");
        break;

    case A_BIN:
    {
        Tipo te = tipo_esq;
        Tipo td = tipo_expr(no->f2);
        const char *op = no->tok.lexema;

        if (te == TP_ERRO || td == TP_ERRO)
            break;

        if (strcmp(op, "//") == 0)
        {
            if (te != TP_INTEIRA || td != TP_INTEIRA)
            {
                erro_semantico(no->tok.linha, no->tok.coluna,
                               "'//' (divisao inteira) exige dois operandos "
                               "inteira, recebeu %s e %s",
                               nome_tipo(te), nome_tipo(td));
                break;
            }
            resultado = TP_INTEIRA;
            break;
        }

        if (!e_numerico(te) || !e_numerico(td))
        {
            erro_semantico(no->tok.linha, no->tok.coluna,
                           "operador '%s' exige operandos numericos, "
                           "recebeu %s e %s",
                           op, nome_tipo(te), nome_tipo(td));
            break;
        }

        if (strcmp(op, "/") == 0)
            /* '/' e divisao real, da no minimo flut */
            resultado = maior_tipo(TP_FLUT, maior_tipo(te, td));
        else
            resultado = maior_tipo(te, td);
        break;
    }

    case A_RAIZ:
    {
        Tipo t = exigir_numerico(no, tipo_expr(no->f1), "raiz()");
        if (t != TP_ERRO)
            resultado = maior_tipo(TP_FLUT, t);
        break;
    }

    case A_COMP:
    {
        Tipo te = tipo_expr(no->f1);
        Tipo td = tipo_expr(no->f2);
        const char *op = no->tok.lexema;

        if (te == TP_ERRO || td == TP_ERRO)
            break;

        if (strcmp(op, "<") == 0 || strcmp(op, ">") == 0)
        {
            if (!e_numerico(te) || !e_numerico(td))
            {
                erro_semantico(no->tok.linha, no->tok.coluna,
                               "comparador '%s' exige operandos numericos, "
                               "recebeu %s e %s",
                               op, nome_tipo(te), nome_tipo(td));
                break;
            }
        }
        else
        {
            int ok = (e_numerico(te) && e_numerico(td)) || te == td;
            if (!ok)
            {
                erro_semantico(no->tok.linha, no->tok.coluna,
                               "comparador '%s' exige operandos compativeis, "
                               "recebeu %s e %s",
                               op, nome_tipo(te), nome_tipo(td));
                break;
            }
        }
        resultado = TP_BOOL;
        break;
    }

    case A_LOGICO:
    {
        Tipo te = tipo_esq;
        Tipo td = tipo_expr(no->f2);
        if (te == TP_ERRO || td == TP_ERRO)
            break;
        if (te != TP_BOOL || td != TP_BOOL)
        {
            erro_semantico(no->tok.linha, no->tok.coluna,
                           "operador logico '%s' exige operandos bool, "
                           "recebeu %s e %s",
                           no->tok.lexema, nome_tipo(te), nome_tipo(td));
            break;
        }
        resultado = TP_BOOL;
        break;
    }

    case A_NAO:
    {
        Tipo t = tipo_expr(no->f1);
        if (t == TP_ERRO)
            break;
        if (t != TP_BOOL)
        {
            erro_semantico(no->tok.linha, no->tok.coluna,
                           "'NAO' exige operando bool, recebeu %s",
                           nome_tipo(t));
            break;
        }
        resultado = TP_BOOL;
        break;
    }

    case A_CHAMADA:
        resultado = tipo_chamada(no);
        if (resultado == TP_VAZIO)
        {
            erro_semantico(no->tok.linha, no->tok.coluna,
                           "funcao vazio '%s' nao produz valor e nao pode "
                           "aparecer dentro de expressao",
                           no->tok.lexema);
            resultado = TP_ERRO;
        }
        break;

    default:
        break;
    }

    no->tipo_dado = resultado;
    return resultado;
}

/* "a + b + c + ..." desce pela esquerda. Com 200 mil termos a recursao
   estoura, entao a espinha esquerda e andada com laco */
static Tipo tipo_expr(Ast *no)
{
    if (no == NULL)
        return TP_ERRO;
    if (no->tipo != A_BIN && no->tipo != A_LOGICO)
        return tipo_no(no, TP_ERRO);

    Ast **espinha = NULL;
    size_t n = 0, cap = 0;
    Ast *atual = no;
    while (atual != NULL && (atual->tipo == A_BIN || atual->tipo == A_LOGICO))
    {
        if (n == cap)
        {
            cap = cap ? cap * 2 : 32;
            Ast **maior = (Ast **)realloc(espinha, cap * sizeof(Ast *));
            if (maior == NULL)
            {
                fprintf(stderr, "erro fatal: memoria insuficiente\n");
                exit(EXIT_FAILURE);
            }
            espinha = maior;
        }
        espinha[n++] = atual;
        atual = atual->f1;
    }

    Tipo t = tipo_expr(atual);
    while (n > 0)
        t = tipo_no(espinha[--n], t);
    free(espinha);
    return t;
}

static void checar_condicao(Ast *cond)
{
    if (cond == NULL)
        return;
    Tipo t = tipo_expr(cond);
    if (t != TP_BOOL && t != TP_ERRO)
        erro_semantico(cond->tok.linha, cond->tok.coluna,
                       "condicao deve resultar em bool, resultou em %s",
                       nome_tipo(t));
}

static void checar_comando(Ast *cmd);

static void checar_bloco(Ast *blk, const char *descricao)
{
    if (blk == NULL)
        return;
    symtab_abrir_escopo(descricao);
    for (Ast *cmd = blk->f1; cmd != NULL; cmd = cmd->prox)
        checar_comando(cmd);
    symtab_fechar_escopo();
}

/* registro e a soma dos campos (no AVR nao tem alinhamento) */
static long long bytes_decl(const Ast *d)
{
    if (d->tipo == A_DECL_VET)
        return bytes_tipo(d->tipo_dado) * valor_literal(d->f1->tok.lexema);
    if (d->tipo == A_DECL_REG)
    {
        long long total = 0;
        if (d->decl != NULL)
            for (const Ast *c = d->decl->f1; c != NULL; c = c->prox)
                total += bytes_decl(c);
        return total;
    }
    return bytes_tipo(d->tipo_dado);
}

static void contar_ram(const Ast *decl, long long bytes)
{
    if (em_funcao)
        ram_funcao += bytes;
    else
        ram_globais += bytes;
    if (bytes > maior_var_bytes)
    {
        maior_var_bytes = bytes;
        maior_var_tok = decl->tok;
    }
}

static void erro_redeclaracao(const Ast *decl)
{
    Simbolo *anterior = symtab_buscar_local(decl->tok.lexema);
    if (anterior != NULL && anterior->categoria == SIM_FUNC &&
        anterior->linha == 0)
        erro_semantico(decl->tok.linha, decl->tok.coluna,
                       "'%s' e' o nome de uma funcao embutida da linguagem e "
                       "nao pode ser redeclarado no escopo global",
                       decl->tok.lexema);
    else if (anterior != NULL && anterior->categoria == SIM_PARAM)
        erro_semantico(decl->tok.linha, decl->tok.coluna,
                       "'%s' ja e' parametro desta funcao (linha %d), nao "
                       "pode ser redeclarado no corpo",
                       decl->tok.lexema, anterior->linha);
    else
        erro_semantico(decl->tok.linha, decl->tok.coluna,
                       "'%s' redeclarado neste escopo (declaracao anterior "
                       "na linha %d)",
                       decl->tok.lexema,
                       anterior != NULL ? anterior->linha : 0);
}

static void erro_redeclaracao_global(const Ast *decl, const char *nome)
{
    Simbolo *anterior = symtab_buscar_local(nome);
    if (anterior != NULL && anterior->categoria == SIM_FUNC &&
        anterior->linha == 0)
        erro_semantico(decl->tok.linha, decl->tok.coluna,
                       "'%s' e' o nome de uma funcao embutida da linguagem e "
                       "nao pode ser redeclarado no escopo global", nome);
    else
        erro_semantico(decl->tok.linha, decl->tok.coluna,
                       "'%s' ja foi declarado no escopo global", nome);
}

static void declarar_variavel(Ast *decl)
{
    if (decl->tipo_dado == TP_VAZIO)
    {
        erro_semantico(decl->tok.linha, decl->tok.coluna,
                       "variavel '%s' nao pode ser do tipo vazio "
                       "(vazio e' apenas tipo de retorno)",
                       decl->tok.lexema);
        return;
    }

    CategoriaSimbolo cat = (decl->tipo == A_DECL_VET) ? SIM_VET : SIM_VAR;
    Simbolo *s = symtab_inserir(decl->tok.lexema, cat, decl->tipo_dado,
                                decl->tok.linha);
    if (s == NULL)
    {
        erro_redeclaracao(decl);
        return;
    }
    s->decl = decl;
    conferir_tipo_no_alvo(decl->tipo_dado, decl->tok);

    if (decl->tipo == A_DECL_VET)
    {
        long long tam = valor_literal(decl->f1->tok.lexema);
        if (tam <= 0)
            erro_semantico(decl->tok.linha, decl->tok.coluna,
                           "vetor '%s' deve ter tamanho maior que zero",
                           decl->tok.lexema);
        else if (tam > INTEIRA_MAX)
            erro_semantico(decl->tok.linha, decl->tok.coluna,
                           "vetor '%s' tem tamanho %s, acima do maximo (%ld)",
                           decl->tok.lexema, decl->f1->tok.lexema,
                           (long)INTEIRA_MAX);
        s->tamanho = (tam > 0 && tam <= INTEIRA_MAX) ? (int)tam : 0;
        s->inicializado = 1;
    }
    if (!em_registro)
        contar_ram(decl, bytes_decl(decl));
}

/* registro Tipo nome; o tipo tem que ter sido declarado antes */
static void declarar_var_registro(Ast *decl)
{
    Token tipo = decl->f1->tok;
    Simbolo *r = symtab_buscar(tipo.lexema);
    int tipo_ok = (r != NULL && r->categoria == SIM_REGISTRO);
    if (!tipo_ok)
        erro_semantico(tipo.linha, tipo.coluna,
                       "'%s' nao e' um tipo de registro declarado",
                       tipo.lexema);

    Simbolo *s = symtab_inserir(decl->tok.lexema, SIM_VAR,
                                tipo_ok ? TP_REGISTRO : TP_ERRO,
                                decl->tok.linha);
    if (s == NULL)
    {
        erro_redeclaracao(decl);
        return;
    }
    s->inicializado = 1;
    s->decl = decl;
    if (tipo_ok)
    {
        decl->decl = r->decl;
        contar_ram(decl, bytes_decl(decl));
    }
    else
        s->usado = 1;
}

static int e_nome_de_vetor(const Ast *no)
{
    if (no == NULL || no->tipo != A_VAR)
        return 0;
    Simbolo *s = symtab_buscar(no->tok.lexema);
    return s != NULL && s->categoria == SIM_VET;
}

static int usa_vetor_inteiro(const Ast *e)
{
    while (e != NULL && e->tipo == A_BIN)
    {
        if (e_nome_de_vetor(e->f2))
            return 1;
        e = e->f1;
    }
    return e_nome_de_vetor(e);
}

static int lado_vetorial_valido(const Ast *lado)
{
    return e_nome_de_vetor(lado) || !usa_vetor_inteiro(lado);
}

/* c receba a + b;  c receba 3 * a;  uma operacao so por atribuicao */
static void checar_atrib_vetorial(Ast *atrib, Simbolo *dest)
{
    Ast *alvo = atrib->f1, *op = atrib->f2;

    atrib->tipo = A_ATRIB_VET;
    dest->usado = 1;
    alvo->decl = dest->decl;
    alvo->tipo_dado = dest->tipo;
    if (dest->tipo == TP_ERRO || dest->tamanho == 0)
        return;
    if (!e_numerico(dest->tipo))
    {
        erro_semantico(alvo->tok.linha, alvo->tok.coluna,
                       "operacao vetorial so vale pra vetor de inteira, flut "
                       "ou duplocarpado; '%s' e' vetor de %s",
                       alvo->tok.lexema, nome_tipo(dest->tipo));
        return;
    }
    if (op == NULL || op->tipo != A_BIN ||
        (op->tok.tipo != T_MAIS && op->tok.tipo != T_MENOS &&
         op->tok.tipo != T_VEZES) ||
        (!e_nome_de_vetor(op->f1) && !e_nome_de_vetor(op->f2)) ||
        !lado_vetorial_valido(op->f1) || !lado_vetorial_valido(op->f2))
    {
        erro_semantico(atrib->tok.linha, atrib->tok.coluna,
                       "o vetor '%s' inteiro so recebe UMA operacao vetorial "
                       "por atribuicao: dois vetores, ou um numero e um vetor, "
                       "ligados por +, - ou * (pra um elemento so, use "
                       "%s{indice} receba ...)",
                       alvo->tok.lexema, alvo->tok.lexema);
        return;
    }

    Ast *lados[2] = {op->f1, op->f2};
    for (int i = 0; i < 2; i++)
    {
        if (e_nome_de_vetor(lados[i]))
        {
            Simbolo *v = vetor_numerico(lados[i], "a operacao vetorial");
            if (v != NULL && !vetores_combinam(lados[i], "a operacao vetorial", v, dest))
                break;
        }
        else
        {
            Tipo t = tipo_expr(lados[i]);
            if (t != TP_ERRO && !atribuivel(dest->tipo, t))
                erro_semantico(lados[i]->tok.linha, lados[i]->tok.coluna,
                               "na operacao vetorial o numero e' %s, e o "
                               "vetor '%s' e' de %s",
                               nome_tipo(t), alvo->tok.lexema,
                               nome_tipo(dest->tipo));
        }
    }
    op->tipo_dado = dest->tipo;
}

static void checar_atribuicao(Ast *atrib)
{
    if (atrib == NULL)
        return;
    Ast *alvo = atrib->f1;

    /* destino vetor inteiro: e operacao vetorial, decide antes de tipar */
    if (alvo != NULL && alvo->tipo == A_VAR)
    {
        Simbolo *dest = symtab_buscar(alvo->tok.lexema);
        if (dest != NULL && dest->categoria == SIM_VET)
        {
            checar_atrib_vetorial(atrib, dest);
            return;
        }
    }

    Tipo t_expr = tipo_expr(atrib->f2);

    if (alvo == NULL)
        return;

    if (alvo->tipo == A_VAR)
    {
        Simbolo *s = symtab_buscar(alvo->tok.lexema);
        if (s == NULL)
        {
            erro_semantico(alvo->tok.linha, alvo->tok.coluna,
                           "variavel '%s' usada sem ter sido declarada",
                           alvo->tok.lexema);
            s = symtab_inserir(alvo->tok.lexema, SIM_VAR, TP_ERRO,
                               alvo->tok.linha);
            if (s != NULL)
                s->usado = 1;
            return;
        }
        if (s->categoria == SIM_FUNC || s->categoria == SIM_REGISTRO)
        {
            erro_semantico(alvo->tok.linha, alvo->tok.coluna,
                           "'%s' nao e' uma variavel e nao pode receber valor",
                           alvo->tok.lexema);
            return;
        }
        if (s->categoria == SIM_VET)
        {
            erro_semantico(alvo->tok.linha, alvo->tok.coluna,
                           "atribua a um ELEMENTO do vetor: %s{indice} receba ...",
                           alvo->tok.lexema);
            return;
        }
        if (s->tipo == TP_REGISTRO)
        {
            erro_semantico(alvo->tok.linha, alvo->tok.coluna,
                           "'%s' e' um registro: atribua campo a campo "
                           "(%s.campo receba ...)",
                           alvo->tok.lexema, alvo->tok.lexema);
            return;
        }
        alvo->decl = s->decl;
        alvo->tipo_dado = s->tipo;
        s->inicializado = 1;
        if (!atribuivel(s->tipo, t_expr))
            erro_semantico(atrib->tok.linha, atrib->tok.coluna,
                           "tipos incompativeis: '%s' e' %s e nao pode "
                           "receber %s",
                           alvo->tok.lexema, nome_tipo(s->tipo),
                           nome_tipo(t_expr));
    }
    else
    {
        Tipo t_alvo = tipo_expr(alvo);
        if (!atribuivel(t_alvo, t_expr))
            erro_semantico(atrib->tok.linha, atrib->tok.coluna,
                           "tipos incompativeis: %s '%s' e' %s e "
                           "nao pode receber %s",
                           alvo->tipo == A_CAMPO ? "campo" : "elemento de",
                           alvo->tok.lexema, nome_tipo(t_alvo),
                           nome_tipo(t_expr));
    }
}

static void checar_comando(Ast *cmd)
{
    switch (cmd->tipo)
    {
    case A_DECL_VAR:
    case A_DECL_VET:
        declarar_variavel(cmd);
        break;

    case A_DECL_REG:
        declarar_var_registro(cmd);
        break;

    case A_ATRIB:
        checar_atribuicao(cmd);
        break;

    case A_REPETE:
        checar_atribuicao(cmd->f1);
        checar_condicao(cmd->f2);
        checar_atribuicao(cmd->f3);
        checar_bloco(cmd->f4, "bloco do repete");
        break;

    case A_DURANTE:
        checar_condicao(cmd->f1);
        checar_bloco(cmd->f2, "bloco do durante");
        break;

    case A_COND:
        checar_condicao(cmd->f1);
        checar_bloco(cmd->f2, "bloco do cond");
        if (cmd->f3 != NULL)
        {
            if (cmd->f3->tipo == A_COND)
                checar_comando(cmd->f3);
            else
                checar_bloco(cmd->f3, "bloco do casocontrario");
        }
        break;

    case A_RESPOST:
        if (cmd->f1 == NULL)
        {
            if (retorno_atual != TP_VAZIO)
            {
                erro_semantico(cmd->tok.linha, cmd->tok.coluna,
                               "'%s' deve devolver %s, mas este respost "
                               "esta' sem valor",
                               nome_func_atual, nome_tipo(retorno_atual));
                teve_respost_errado = 1;
            }
        }
        else
        {
            Tipo t = tipo_expr(cmd->f1);
            if (retorno_atual == TP_VAZIO)
                erro_semantico(cmd->tok.linha, cmd->tok.coluna,
                               "'%s' e' vazio e nao pode devolver valor",
                               nome_func_atual);
            else if (!atribuivel(retorno_atual, t))
                erro_semantico(cmd->tok.linha, cmd->tok.coluna,
                               "'%s' deve devolver %s, mas o respost "
                               "devolve %s",
                               nome_func_atual, nome_tipo(retorno_atual),
                               nome_tipo(t));
            teve_respost_com_valor = 1;
        }
        break;

    case A_ESCREVA:
    {
        Tipo t = tipo_expr(cmd->f1);
        if (t == TP_VAZIO)
            erro_semantico(cmd->tok.linha, cmd->tok.coluna,
                           "escreva() precisa de um valor para imprimir");
        break;
    }

    case A_LEIA:
    {
        Ast *alvo = cmd->f1;
        if (alvo == NULL)
            break;
        if (alvo->tipo == A_VAR)
        {
            Simbolo *s = symtab_buscar(alvo->tok.lexema);
            if (s != NULL && s->categoria == SIM_VAR)
                s->inicializado = 1;
        }
        Tipo t = tipo_expr(alvo);
        if (t != TP_ERRO && !e_numerico(t))
            erro_semantico(cmd->tok.linha, cmd->tok.coluna,
                           "leia() so le inteira, flut ou duplocarpado; "
                           "'%s' e' %s",
                           alvo->tok.lexema, nome_tipo(t));
        break;
    }

    case A_CMD_CHAMADA:
        tipo_chamada(cmd->f1);
        break;

    default:
        break;
    }
}

static int cond_sempre_retorna(Ast *c);

/* 1 se todo caminho passa por respost com valor. Laco nao conta */
static int sempre_retorna(Ast *blk)
{
    if (blk == NULL)
        return 0;
    for (Ast *cmd = blk->f1; cmd != NULL; cmd = cmd->prox)
    {
        if (cmd->tipo == A_RESPOST && cmd->f1 != NULL)
            return 1;
        if (cmd->tipo == A_COND && cond_sempre_retorna(cmd))
            return 1;
    }
    return 0;
}

static int cond_sempre_retorna(Ast *c)
{
    if (!sempre_retorna(c->f2))
        return 0;
    if (c->f3 == NULL)
        return 0;
    if (c->f3->tipo == A_COND)
        return cond_sempre_retorna(c->f3);
    return sempre_retorna(c->f3);
}

/* o nome entra na tabela antes do corpo, por isso recursao funciona */
static void declarar_funcao(Ast *fn, const char *nome)
{
    Simbolo *s = symtab_inserir(nome, SIM_FUNC, fn->tipo_dado,
                                fn->tok.linha);
    if (s == NULL)
    {
        erro_redeclaracao_global(fn, nome);
        return;
    }
    s->decl = fn;
    s->nparams = 0;
    for (Ast *par = fn->f1; par != NULL; par = par->prox)
    {
        if (s->nparams < SYM_MAX_PARAMS)
            s->params[s->nparams] =
                (par->tipo_dado == TP_VAZIO) ? TP_ERRO : par->tipo_dado;
        s->nparams++;
    }
    if (s->nparams > SYM_MAX_PARAMS)
        erro_semantico(fn->tok.linha, fn->tok.coluna,
                       "'%s' tem %d parametros; esta implementacao confere o "
                       "tipo de no maximo %d",
                       nome, s->nparams, SYM_MAX_PARAMS);

    retorno_atual = fn->tipo_dado;
    nome_func_atual = nome;
    teve_respost_com_valor = 0;
    teve_respost_errado = 0;
    conferir_tipo_no_alvo(fn->tipo_dado, fn->tok);
    em_funcao = 1;
    ram_funcao = 0;

    symtab_abrir_escopo("funcao");
    for (Ast *par = fn->f1; par != NULL; par = par->prox)
    {
        Tipo tp = par->tipo_dado;
        if (tp == TP_VAZIO)
        {
            erro_semantico(par->tok.linha, par->tok.coluna,
                           "parametro '%s' nao pode ser vazio",
                           par->tok.lexema);
            tp = TP_ERRO;
        }
        Simbolo *ps = symtab_inserir(par->tok.lexema, SIM_PARAM, tp,
                                     par->tok.linha);
        if (ps == NULL)
            erro_semantico(par->tok.linha, par->tok.coluna,
                           "parametro '%s' repetido", par->tok.lexema);
        else
        {
            ps->inicializado = 1;
            ps->decl = par;
            conferir_tipo_no_alvo(tp, par->tok);
            contar_ram(par, bytes_tipo(tp));
        }
    }
    /* corpo no mesmo escopo dos parametros, igual em C */
    if (fn->f2 != NULL)
        for (Ast *cmd = fn->f2->f1; cmd != NULL; cmd = cmd->prox)
            checar_comando(cmd);
    symtab_fechar_escopo();

    em_funcao = 0;
    if (fn->tipo == A_PRINCIPAL)
        ram_principal = ram_funcao;
    else if (ram_funcao > ram_maior_funcao)
        ram_maior_funcao = ram_funcao;

    if (fn->tipo_dado != TP_VAZIO && fn->tipo_dado != TP_ERRO &&
        !teve_respost_errado)
    {
        if (!teve_respost_com_valor)
            erro_semantico(fn->tok.linha, fn->tok.coluna,
                           "'%s' declara retorno %s mas nao tem nenhum "
                           "'respost' com valor",
                           nome, nome_tipo(fn->tipo_dado));
        else if (!sempre_retorna(fn->f2))
            erro_semantico(fn->tok.linha, fn->tok.coluna,
                           "'%s' pode chegar ao fim sem respost: algum caminho "
                           "nao devolve valor",
                           nome);
    }
}

static void declarar_registro(Ast *reg)
{
    Simbolo *s = symtab_inserir(reg->tok.lexema, SIM_REGISTRO, TP_ERRO,
                                reg->tok.linha);
    if (s == NULL)
    {
        erro_redeclaracao_global(reg, reg->tok.lexema);
        return;
    }
    s->usado = 1;
    s->decl = reg;

    symtab_abrir_escopo("registro");
    em_registro = 1;
    for (Ast *campo = reg->f1; campo != NULL; campo = campo->prox)
    {
        declarar_variavel(campo);
        Simbolo *cs = symtab_buscar_local(campo->tok.lexema);
        if (cs != NULL)
        {
            cs->usado = 1;
            cs->inicializado = 1;
        }
    }
    em_registro = 0;
    symtab_fechar_escopo();
}

/* E/S fisica da placa. Viram zl_<nome> na biblioteca de execucao */
typedef struct
{
    const char *nome;
    Tipo retorno;
    int nparams; /* todos inteira */
} Embutida;

static const Embutida embutidas[] = {
    {"pinomodo", TP_VAZIO, 2},
    {"pinoescreve", TP_VAZIO, 2},
    {"pinole", TP_INTEIRA, 1},
    {"espera", TP_VAZIO, 1},
};
#define N_EMBUTIDAS ((int)(sizeof embutidas / sizeof embutidas[0]))

static Ast no_embutida[N_EMBUTIDAS];
static Ast param_embutida[N_EMBUTIDAS][2];

static void registrar_embutidas(void)
{
    for (int i = 0; i < N_EMBUTIDAS; i++)
    {
        Ast *fn = &no_embutida[i];
        memset(fn, 0, sizeof *fn);
        fn->tipo = A_FUNCAO;
        fn->tipo_dado = embutidas[i].retorno;
        strncpy(fn->tok.lexema, embutidas[i].nome, LEXEMA_MAX - 1);

        Simbolo *s = symtab_inserir(embutidas[i].nome, SIM_FUNC,
                                    embutidas[i].retorno, 0);
        s->usado = 1;
        s->decl = fn;
        s->nparams = embutidas[i].nparams;
        for (int p = 0; p < embutidas[i].nparams; p++)
        {
            Ast *par = &param_embutida[i][p];
            memset(par, 0, sizeof *par);
            par->tipo = A_PARAM;
            par->tipo_dado = TP_INTEIRA;
            if (p == 0)
                fn->f1 = par;
            else
                param_embutida[i][p - 1].prox = par;
            s->params[p] = TP_INTEIRA;
        }
    }
}

void analisar_semantica(Ast *programa)
{
    if (programa == NULL)
        return;

    symtab_abrir_escopo("global");
    registrar_embutidas();

    for (Ast *decl = programa->f1; decl != NULL; decl = decl->prox)
    {
        switch (decl->tipo)
        {
        case A_REGISTRO:
            declarar_registro(decl);
            break;

        case A_FUNCAO:
            declarar_funcao(decl, decl->tok.lexema);
            break;

        case A_PRINCIPAL:
            achou_principal++;
            if (achou_principal > 1)
            {
                erro_semantico(decl->tok.linha, decl->tok.coluna,
                               "so' pode existir uma funcao principal");
                break;
            }
            if (decl->f1 != NULL)
                erro_semantico(decl->f1->tok.linha, decl->f1->tok.coluna,
                               "principal nao recebe parametros");
            if (decl->tipo_dado != TP_INTEIRA && decl->tipo_dado != TP_VAZIO)
                erro_semantico(decl->tok.linha, decl->tok.coluna,
                               "principal devolve inteira ou vazio, nao %s",
                               nome_tipo(decl->tipo_dado));
            declarar_funcao(decl, "principal");
            break;

        case A_DECL_VAR:
        case A_DECL_VET:
            declarar_variavel(decl);
            break;

        case A_DECL_REG:
            declarar_var_registro(decl);
            break;

        default:
            break;
        }
    }

    if (achou_principal == 0)
        erro_semantico(1, 1, "o programa nao tem funcao principal");

    if (alvo_uno)
    {
        long long precisa = ram_globais + ram_textos + ram_principal +
                            ram_maior_funcao;
        if (precisa > UNO_RAM - UNO_RESERVA)
            erro_semantico(maior_var_tok.linha, maior_var_tok.coluna,
                           "o programa precisa de %ld bytes de RAM pra "
                           "variaveis e textos, e no alvo uno cabem %d (%d da "
                           "placa menos %d reservados pra biblioteca e pra "
                           "pilha); a maior variavel e' '%s', com %ld bytes",
                           (long)precisa, UNO_RAM - UNO_RESERVA, UNO_RAM,
                           UNO_RESERVA, maior_var_tok.lexema,
                           (long)maior_var_bytes);
    }

    symtab_fechar_escopo();
}
