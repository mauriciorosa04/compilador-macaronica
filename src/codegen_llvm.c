/* gerador de LLVM IR em texto.
 *
 * Toda variavel fica na memoria (alloca na entrada, load e store). Quem
 * passa pra registrador e o mem2reg. O IR nao tem alvo nenhum, entao o
 * mesmo .ll serve pro PC e pra placa.
 *
 * Nomes: @mac_nome funcao/global, %nome.addr endereco, %nome.N valor lido,
 * rotulos sempre com ponto pra nao bater com nome do programa. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "codegen_llvm.h"
#include "codegen_mlir.h"

#define NOME_MAX (LEXEMA_MAX + 40)

/* as funcoes de expressao ficam fora do gen_expr pra recursao gastar menos pilha */
#if defined(__GNUC__)
#define SEM_INLINE __attribute__((noinline))
#else
#define SEM_INLINE
#endif

typedef struct
{
    char txt[NOME_MAX];
} Valor;

static FILE *out;
static int contador; /* numera valores e rotulos da funcao */
static int n_blocos;
static int terminado; /* o bloco basico atual ja fechou (ret, br) */
static char bloco_atual[NOME_MAX];
static Tipo retorno_atual;
static int na_principal;

typedef enum
{
    RT_IMPRIME_INT, RT_IMPRIME_FLUT, RT_IMPRIME_DUPLO, RT_IMPRIME_BOOL,
    RT_IMPRIME_TEXTO, RT_IMPRIME_NL,
    RT_LE_INT, RT_LE_FLUT, RT_LE_DUPLO,
    RT_ERRO,
    RT_POT_INT, RT_POT_FLUT, RT_POT_DUPLO,
    RT_RAIZ_FLUT, RT_RAIZ_DUPLO,
    RT_TEXTO_IGUAL,
    RT_TOTAL
} FuncRuntime;

/* so sai no .ll o declare do que o programa usou */
static const char *rt_declare[RT_TOTAL] = {
    "declare void @zl_imprime_int(i32) nounwind",
    "declare void @zl_imprime_flut(float) nounwind",
    "declare void @zl_imprime_duplo(double) nounwind",
    "declare void @zl_imprime_bool(i1 zeroext) nounwind",
    "declare void @zl_imprime_texto(ptr) nounwind",
    "declare void @zl_imprime_nl() nounwind",
    "declare i32 @zl_le_int() nounwind",
    "declare float @zl_le_flut() nounwind",
    "declare double @zl_le_duplo() nounwind",
    "declare void @zl_erro_execucao(i32, i32) noreturn nounwind",
    "declare i32 @zl_pot_int(i32, i32) nounwind",
    "declare float @zl_pot_flut(float, float) nounwind",
    "declare double @zl_pot_duplo(double, double) nounwind",
    "declare float @zl_raiz_flut(float) nounwind",
    "declare double @zl_raiz_duplo(double) nounwind",
    "declare zeroext i1 @zl_texto_igual(ptr, ptr) nounwind",
};
static int rt_usada[RT_TOTAL];

#define EMBUTIDAS_MAX 16
static const Ast *embutida_usada[EMBUTIDAS_MAX];
static int n_embutidas;

static void usa_embutida(const Ast *fn)
{
    for (int i = 0; i < n_embutidas; i++)
        if (embutida_usada[i] == fn)
            return;
    if (n_embutidas < EMBUTIDAS_MAX)
        embutida_usada[n_embutidas++] = fn;
}

#define ERRO_INDICE 1
#define ERRO_DIV_ZERO 2

typedef struct
{
    const char *texto;
    int tam;
} Texto;
static Texto *textos;
static int n_textos, cap_textos;

static void sem_memoria(void)
{
    fprintf(stderr, "erro fatal: memoria insuficiente\n");
    exit(EXIT_FAILURE);
}

static Valor valor(const char *fmt, ...)
{
    Valor v;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(v.txt, sizeof v.txt, fmt, ap);
    va_end(ap);
    return v;
}

static Valor temp(const char *base)
{
    return valor("%%%s.%d", base, ++contador);
}

/* codigo depois de um respost vai pra um bloco que ninguem alcanca */
static void abre_bloco_morto(void)
{
    snprintf(bloco_atual, sizeof bloco_atual, "morto.%d", ++contador);
    fprintf(out, "%s:\n", bloco_atual);
    terminado = 0;
}

static void instr(const char *fmt, ...)
{
    if (terminado)
        abre_bloco_morto();
    va_list ap;
    va_start(ap, fmt);
    fputs("  ", out);
    vfprintf(out, fmt, ap);
    fputc('\n', out);
    va_end(ap);
}

static void terminador(const char *fmt, ...)
{
    if (terminado)
        abre_bloco_morto();
    va_list ap;
    va_start(ap, fmt);
    fputs("  ", out);
    vfprintf(out, fmt, ap);
    fputc('\n', out);
    va_end(ap);
    terminado = 1;
}

static void desvio(const char *destino)
{
    if (terminado)
        return;
    fprintf(out, "  br label %%%s\n", destino);
    terminado = 1;
}

static void desvio_cond(Valor c, const char *se_ver, const char *se_fal)
{
    if (terminado)
        abre_bloco_morto();
    fprintf(out, "  br i1 %s, label %%%s, label %%%s\n", c.txt, se_ver, se_fal);
    terminado = 1;
}

/* se o bloco anterior ficou aberto, cai neste com br */
static void rotulo(const char *nome)
{
    if (!terminado)
        fprintf(out, "  br label %%%s\n", nome);
    fprintf(out, "%s:\n", nome);
    snprintf(bloco_atual, sizeof bloco_atual, "%s", nome);
    terminado = 0;
}

static void usa(FuncRuntime f) { rt_usada[f] = 1; }

static void comentario(const Ast *cmd, const char *texto, const char *nome)
{
    if (nome != NULL)
        instr("; linha %d: %s '%s'", cmd->tok.linha, texto, nome);
    else
        instr("; linha %d: %s", cmd->tok.linha, texto);
}

static const char *ll_tipo(Tipo t)
{
    switch (t)
    {
    case TP_BOOL: return "i1";
    case TP_INTEIRA: return "i32";
    case TP_FLUT: return "float";
    case TP_DUPLO: return "double";
    case TP_PALAVRA: return "ptr";
    default: return "void";
    }
}

static const char *ll_zero(Tipo t)
{
    switch (t)
    {
    case TP_BOOL: return "false";
    case TP_INTEIRA: return "0";
    case TP_PALAVRA: return "null";
    default: return "0.0";
    }
}

static const char *sufixo_real(Tipo t) { return t == TP_DUPLO ? "duplo" : "flut"; }

static long tamanho_vetor(const Ast *decl)
{
    return strtol(decl->f1->tok.lexema, NULL, 10);
}

static Valor ll_tipo_decl(const Ast *decl)
{
    if (decl->tipo == A_DECL_VET)
        return valor("[%ld x %s]", tamanho_vetor(decl), ll_tipo(decl->tipo_dado));
    if (decl->tipo == A_DECL_REG)
        return valor("%%struct.%s", decl->decl->tok.lexema);
    return valor("%s", ll_tipo(decl->tipo_dado));
}

static Valor endereco_decl(const Ast *decl)
{
    if (decl->num < 0)
        return valor("@mac_%s", decl->tok.lexema);
    if (decl->num == 0)
        return valor("%%%s.addr", decl->tok.lexema);
    return valor("%%%s.addr.%d", decl->tok.lexema, decl->num);
}

/* float vai em hexa: o LLVM so aceita decimal se o valor for exato */
static Valor const_flut(const char *lexema, int negativo)
{
    float f = strtof(lexema, NULL);
    double d = negativo ? -(double)f : (double)f;
    unsigned long long bits;
    memcpy(&bits, &d, sizeof bits);
    return valor("0x%08lX%08lX", (unsigned long)(bits >> 32),
                 (unsigned long)(bits & 0xFFFFFFFFUL));
}

/* texto repetido reaproveita a mesma constante */
static int indice_texto(const char *texto, int tam)
{
    int limite = n_textos < 256 ? n_textos : 256;
    for (int i = 0; i < limite; i++)
        if (textos[i].tam == tam && memcmp(textos[i].texto, texto, (size_t)tam) == 0)
            return i;
    if (n_textos == cap_textos)
    {
        cap_textos = cap_textos ? cap_textos * 2 : 16;
        Texto *maior = (Texto *)realloc(textos, (size_t)cap_textos * sizeof(Texto));
        if (maior == NULL)
            sem_memoria();
        textos = maior;
    }
    textos[n_textos].texto = texto;
    textos[n_textos].tam = tam;
    return n_textos++;
}

static Valor gen_expr(const Ast *no);

/* so pra cima: inteira -> flut -> duplocarpado */
static Valor converte(Valor v, Tipo de, Tipo para)
{
    if (de == para)
        return v;
    Valor r = temp("conv");
    if (de == TP_INTEIRA)
        instr("%s = sitofp i32 %s to %s", r.txt, v.txt, ll_tipo(para));
    else
        instr("%s = fpext float %s to double", r.txt, v.txt);
    return r;
}

static int indice_constante(const Ast *idx, long *v)
{
    if (idx->tipo == A_NUM_INT)
    {
        *v = strtol(idx->tok.lexema, NULL, 10);
        return 1;
    }
    if (idx->tipo == A_NEG && idx->f1->tipo == A_NUM_INT)
    {
        *v = 0;
        return 1;
    }
    return 0;
}

/* indice constante ja foi conferido na compilacao. O calculado ganha um
   teste sem sinal: negativo vira numero enorme e cai fora junto */
static Valor gen_indice(const Ast *idx, long tamanho, int linha)
{
    long constante;
    if (indice_constante(idx, &constante))
        return valor("%ld", constante);

    Valor i = gen_expr(idx);
    int k = ++contador;
    char ok[32], erro[32];
    snprintf(ok, sizeof ok, "idx.ok.%d", k);
    snprintf(erro, sizeof erro, "idx.erro.%d", k);

    instr("%%dentro.%d = icmp ult i32 %s, %ld", k, i.txt, tamanho);
    desvio_cond(valor("%%dentro.%d", k), ok, erro);
    rotulo(erro);
    usa(RT_ERRO);
    instr("call void @zl_erro_execucao(i32 %d, i32 %d)", ERRO_INDICE, linha);
    terminador("unreachable");
    rotulo(ok);
    return i;
}

/* unico lugar que calcula endereco, sempre com getelementptr */
static SEM_INLINE Valor gen_endereco(const Ast *no)
{
    if (no->tipo == A_VAR)
        return endereco_decl(no->decl);

    if (no->tipo == A_VET_ACESSO)
    {
        const Ast *vet = no->decl;
        long n = tamanho_vetor(vet);
        Valor base = endereco_decl(vet);
        Valor idx = gen_indice(no->f1, n, no->tok.linha);
        Valor r = valor("%%%s.elem.%d", no->tok.lexema, ++contador);
        instr("%s = getelementptr inbounds [%ld x %s], ptr %s, i32 0, i32 %s",
              r.txt, n, ll_tipo(vet->tipo_dado), base.txt, idx.txt);
        return r;
    }

    const Ast *var = no->f1->decl;
    const Ast *reg = var->decl;
    const Ast *campo = no->decl;
    int pos = 0;
    for (const Ast *c = reg->f1; c != campo; c = c->prox)
        pos++;
    Valor base = endereco_decl(var);

    if (campo->tipo == A_DECL_VET)
    {
        Valor idx = gen_indice(no->f2, tamanho_vetor(campo), no->tok.linha);
        Valor r = valor("%%%s.elem.%d", no->tok.lexema, ++contador);
        instr("%s = getelementptr inbounds %%struct.%s, ptr %s, i32 0, i32 %d, i32 %s",
              r.txt, reg->tok.lexema, base.txt, pos, idx.txt);
        return r;
    }
    Valor r = valor("%%%s.campo.%d", no->tok.lexema, ++contador);
    instr("%s = getelementptr inbounds %%struct.%s, ptr %s, i32 0, i32 %d",
          r.txt, reg->tok.lexema, base.txt, pos);
    return r;
}

static Valor gen_base_vetor(const Ast *var)
{
    const Ast *vet = var->decl;
    Valor r = valor("%%%s.base.%d", var->tok.lexema, ++contador);
    instr("%s = getelementptr inbounds [%ld x %s], ptr %s, i32 0, i32 0",
          r.txt, tamanho_vetor(vet), ll_tipo(vet->tipo_dado),
          endereco_decl(vet).txt);
    return r;
}

static SEM_INLINE Valor gen_chamada(const Ast *no)
{
    const Ast *fn = no->decl;
    int n = 0;
    for (const Ast *a = no->f1; a != NULL; a = a->prox)
        n++;

    Valor *args = NULL;
    if (n > 0)
    {
        args = (Valor *)malloc((size_t)n * sizeof(Valor));
        if (args == NULL)
            sem_memoria();
    }
    const Ast *par = fn->f1;
    int i = 0;
    for (const Ast *a = no->f1; a != NULL; a = a->prox, par = par->prox, i++)
        args[i] = converte(gen_expr(a), a->tipo_dado, par->tipo_dado);

    Valor r = valor("%s", "");
    if (terminado)
        abre_bloco_morto();
    fputs("  ", out);
    if (fn->tipo_dado != TP_VAZIO)
    {
        r = temp("cham");
        fprintf(out, "%s = ", r.txt);
    }
    if (fn->f2 == NULL)
        usa_embutida(fn);
    fprintf(out, "call %s @%s%s(", ll_tipo(fn->tipo_dado),
            fn->f2 == NULL ? "zl_" : "mac_", fn->tok.lexema);
    par = fn->f1;
    for (i = 0; i < n; i++, par = par->prox)
        fprintf(out, "%s%s %s", i ? ", " : "", ll_tipo(par->tipo_dado), args[i].txt);
    fputs(")\n", out);
    free(args);
    return r;
}

/* sdiv e indefinido com divisor 0 e com -2147483648 // -1.
   O zero vira erro de execucao e o -1 vira 0 - x */
static SEM_INLINE Valor gen_div_inteira(const Ast *no, Valor a, Valor b)
{
    const Ast *dir = no->f2;
    if (dir->tipo == A_NUM_INT && strtol(dir->tok.lexema, NULL, 10) != 0)
    {
        Valor r = temp("div");
        instr("%s = sdiv i32 %s, %s", r.txt, a.txt, b.txt);
        return r;
    }

    int k = ++contador;
    char ok[32], erro[32];
    snprintf(ok, sizeof ok, "div.ok.%d", k);
    snprintf(erro, sizeof erro, "div.erro.%d", k);

    instr("%%zero.%d = icmp eq i32 %s, 0", k, b.txt);
    desvio_cond(valor("%%zero.%d", k), erro, ok);
    rotulo(erro);
    usa(RT_ERRO);
    instr("call void @zl_erro_execucao(i32 %d, i32 %d)", ERRO_DIV_ZERO,
          no->tok.linha);
    terminador("unreachable");
    rotulo(ok);
    instr("%%menos1.%d = icmp eq i32 %s, -1", k, b.txt);
    instr("%%divisor.%d = select i1 %%menos1.%d, i32 1, i32 %s", k, k, b.txt);
    instr("%%quoc.%d = sdiv i32 %s, %%divisor.%d", k, a.txt, k);
    instr("%%oposto.%d = sub i32 0, %s", k, a.txt);
    instr("%%div.%d = select i1 %%menos1.%d, i32 %%oposto.%d, i32 %%quoc.%d",
          k, k, k, k);
    return valor("%%div.%d", k);
}

static SEM_INLINE Valor gen_aritmetica(const Ast *no, Valor esq)
{
    Tipo t = no->tipo_dado;
    const char *tl = ll_tipo(t);
    Valor a = converte(esq, no->f1->tipo_dado, t);
    Valor b = converte(gen_expr(no->f2), no->f2->tipo_dado, t);
    int real = (t != TP_INTEIRA);
    Valor r;

    switch (no->tok.tipo)
    {
    case T_MAIS:
        r = temp("soma");
        instr("%s = %s %s %s, %s", r.txt, real ? "fadd" : "add", tl, a.txt, b.txt);
        return r;
    case T_MENOS:
        r = temp("sub");
        instr("%s = %s %s %s, %s", r.txt, real ? "fsub" : "sub", tl, a.txt, b.txt);
        return r;
    case T_VEZES:
        r = temp("mul");
        instr("%s = %s %s %s, %s", r.txt, real ? "fmul" : "mul", tl, a.txt, b.txt);
        return r;
    case T_DIVIDE:
        r = temp("quoc");
        instr("%s = fdiv %s %s, %s", r.txt, tl, a.txt, b.txt);
        return r;
    case T_DIVINT:
        return gen_div_inteira(no, a, b);
    default:
        r = temp("pot");
        if (t == TP_INTEIRA)
        {
            usa(RT_POT_INT);
            instr("%s = call i32 @zl_pot_int(i32 %s, i32 %s)", r.txt, a.txt, b.txt);
        }
        else
        {
            usa(t == TP_DUPLO ? RT_POT_DUPLO : RT_POT_FLUT);
            instr("%s = call %s @zl_pot_%s(%s %s, %s %s)", r.txt, tl,
                  sufixo_real(t), tl, a.txt, tl, b.txt);
        }
        return r;
    }
}

/* E e OU com curto-circuito, o resultado sai num phi. XOR avalia os dois */
static SEM_INLINE Valor gen_logico(const Ast *no, Valor esq)
{
    if (no->tok.tipo == T_XOR)
    {
        Valor d = gen_expr(no->f2);
        Valor r = temp("xor");
        instr("%s = xor i1 %s, %s", r.txt, esq.txt, d.txt);
        return r;
    }

    int e_logico = (no->tok.tipo == T_E);
    const char *nome = e_logico ? "e" : "ou";
    int k = ++contador;
    char dir[32], fim[32], veio_esq[NOME_MAX], veio_dir[NOME_MAX];
    snprintf(dir, sizeof dir, "%s.dir.%d", nome, k);
    snprintf(fim, sizeof fim, "%s.fim.%d", nome, k);

    if (terminado)
        abre_bloco_morto();
    snprintf(veio_esq, sizeof veio_esq, "%s", bloco_atual);
    if (e_logico)
        desvio_cond(esq, dir, fim);
    else
        desvio_cond(esq, fim, dir);

    rotulo(dir);
    Valor d = gen_expr(no->f2);
    if (terminado)
        abre_bloco_morto();
    snprintf(veio_dir, sizeof veio_dir, "%s", bloco_atual);
    desvio(fim);

    rotulo(fim);
    instr("%%%s.%d = phi i1 [ %s, %%%s ], [ %s, %%%s ]", nome, k,
          e_logico ? "false" : "true", veio_esq, d.txt, veio_dir);
    return valor("%%%s.%d", nome, k);
}

/* mesma ideia do tipo_expr da semantica: anda a espinha esquerda com laco */
static SEM_INLINE Valor gen_cadeia(const Ast *no)
{
    const Ast **espinha = NULL;
    size_t n = 0, cap = 0;
    const Ast *atual = no;
    while (atual->tipo == A_BIN || atual->tipo == A_LOGICO)
    {
        if (n == cap)
        {
            cap = cap ? cap * 2 : 32;
            const Ast **maior =
                (const Ast **)realloc((void *)espinha, cap * sizeof(Ast *));
            if (maior == NULL)
                sem_memoria();
            espinha = maior;
        }
        espinha[n++] = atual;
        atual = atual->f1;
    }

    Valor v = gen_expr(atual);
    while (n > 0)
    {
        const Ast *pai = espinha[--n];
        v = (pai->tipo == A_BIN) ? gen_aritmetica(pai, v) : gen_logico(pai, v);
    }
    free((void *)espinha);
    return v;
}

static SEM_INLINE Valor gen_comparacao(const Ast *no)
{
    Tipo te = no->f1->tipo_dado, td = no->f2->tipo_dado;
    Valor a = gen_expr(no->f1);
    Valor b;
    Valor r = temp("cmp");

    /* palavra compara o conteudo, nao o ponteiro */
    if (te == TP_PALAVRA)
    {
        b = gen_expr(no->f2);
        usa(RT_TEXTO_IGUAL);
        if (no->tok.tipo == T_IGUAL)
        {
            instr("%s = call zeroext i1 @zl_texto_igual(ptr %s, ptr %s)",
                  r.txt, a.txt, b.txt);
            return r;
        }
        Valor igual = temp("igual");
        instr("%s = call zeroext i1 @zl_texto_igual(ptr %s, ptr %s)",
              igual.txt, a.txt, b.txt);
        instr("%s = xor i1 %s, true", r.txt, igual.txt);
        return r;
    }
    if (te == TP_BOOL)
    {
        b = gen_expr(no->f2);
        instr("%s = icmp %s i1 %s, %s", r.txt,
              no->tok.tipo == T_IGUAL ? "eq" : "ne", a.txt, b.txt);
        return r;
    }

    Tipo t = (te == TP_DUPLO || td == TP_DUPLO) ? TP_DUPLO
           : (te == TP_FLUT || td == TP_FLUT) ? TP_FLUT : TP_INTEIRA;
    a = converte(a, te, t);
    b = converte(gen_expr(no->f2), td, t);
    const char *pred;
    if (t == TP_INTEIRA)
        pred = no->tok.tipo == T_MENOR ? "icmp slt"
             : no->tok.tipo == T_MAIOR ? "icmp sgt"
             : no->tok.tipo == T_IGUAL ? "icmp eq" : "icmp ne";
    else
        pred = no->tok.tipo == T_MENOR ? "fcmp olt"
             : no->tok.tipo == T_MAIOR ? "fcmp ogt"
             : no->tok.tipo == T_IGUAL ? "fcmp oeq" : "fcmp une";
    instr("%s = %s %s %s, %s", r.txt, pred, ll_tipo(t), a.txt, b.txt);
    return r;
}

static SEM_INLINE Valor gen_carrega(const Ast *no)
{
    Valor e = (no->tipo == A_VAR) ? endereco_decl(no->decl) : gen_endereco(no);
    Valor r = valor("%%%s.%d", no->tok.lexema, ++contador);
    instr("%s = load %s, ptr %s", r.txt, ll_tipo(no->tipo_dado), e.txt);
    return r;
}

/* menos na frente de literal vira constante negativa */
static SEM_INLINE Valor gen_neg(const Ast *no)
{
    if (no->f1->tipo == A_NUM_INT)
    {
        unsigned long u = strtoul(no->f1->tok.lexema, NULL, 10);
        return u == 0 ? valor("0") : valor("-%lu", u);
    }
    if (no->f1->tipo == A_NUM_FLUT)
        return const_flut(no->f1->tok.lexema, 1);

    Valor v = gen_expr(no->f1);
    Valor r = temp("neg");
    if (no->tipo_dado == TP_INTEIRA)
        instr("%s = sub i32 0, %s", r.txt, v.txt);
    else
        instr("%s = fneg %s %s", r.txt, ll_tipo(no->tipo_dado), v.txt);
    return r;
}

static SEM_INLINE Valor gen_raiz(const Ast *no)
{
    Tipo t = no->tipo_dado;
    Valor v = converte(gen_expr(no->f1), no->f1->tipo_dado, t);
    Valor r = temp("raiz");
    usa(t == TP_DUPLO ? RT_RAIZ_DUPLO : RT_RAIZ_FLUT);
    instr("%s = call %s @zl_raiz_%s(%s %s)", r.txt, ll_tipo(t),
          sufixo_real(t), ll_tipo(t), v.txt);
    return r;
}

static SEM_INLINE Valor gen_reducao(const Ast *no)
{
    Tipo t = no->tipo_dado;
    long n = tamanho_vetor(no->f1->decl);
    char nome[64];
    Valor r;
    Valor a = gen_base_vetor(no->f1);
    if (no->tok.tipo == T_PRODESC)
    {
        Valor b = gen_base_vetor(no->f2);
        nucleo_nome(nucleo_registrar(NUC_PRODESC, t, n), nome, sizeof nome);
        r = temp("prodesc");
        instr("%s = call %s @%s(ptr %s, ptr %s)", r.txt, ll_tipo(t), nome,
              a.txt, b.txt);
        return r;
    }
    int soma = (no->tok.tipo == T_SOMAVET);
    nucleo_nome(nucleo_registrar(soma ? NUC_SOMAVET : NUC_MAXVET, t, n),
                nome, sizeof nome);
    r = temp(soma ? "somavet" : "maxvet");
    instr("%s = call %s @%s(ptr %s)", r.txt, ll_tipo(t), nome, a.txt);
    return r;
}

static SEM_INLINE Valor gen_nao(const Ast *no)
{
    Valor v = gen_expr(no->f1);
    Valor r = temp("nao");
    instr("%s = xor i1 %s, true", r.txt, v.txt);
    return r;
}

static Valor gen_expr(const Ast *no)
{
    switch (no->tipo)
    {
    case A_NUM_INT:
        return valor("%ld", strtol(no->tok.lexema, NULL, 10));

    case A_NUM_FLUT:
        return const_flut(no->tok.lexema, 0);

    case A_BOOL_LIT:
        return valor("%s", no->tok.tipo == T_VER ? "true" : "false");

    case A_STR:
        return valor("@.str.%d", indice_texto(no->tok.texto, no->tok.tam_texto));

    case A_VAR:
    case A_VET_ACESSO:
    case A_CAMPO:
        return gen_carrega(no);

    case A_NEG:
        return gen_neg(no);

    case A_RAIZ:
        return gen_raiz(no);

    case A_REDUCAO:
        return gen_reducao(no);

    case A_NAO:
        return gen_nao(no);

    case A_BIN:
    case A_LOGICO:
        return gen_cadeia(no);

    case A_COMP:
        return gen_comparacao(no);

    case A_CHAMADA:
        return gen_chamada(no);

    default:
        return valor("0");
    }
}

static void gen_bloco(const Ast *blk);
static void gen_cond(const Ast *c);

/* laco no proprio IR, o contador e um phi */
static void gen_zera_vetor(Valor vet, long n, Tipo elem)
{
    int k = ++contador;
    char teste[32], corpo[32], fim[32], veio[NOME_MAX];
    snprintf(teste, sizeof teste, "zera.teste.%d", k);
    snprintf(corpo, sizeof corpo, "zera.corpo.%d", k);
    snprintf(fim, sizeof fim, "zera.fim.%d", k);

    if (terminado)
        abre_bloco_morto();
    snprintf(veio, sizeof veio, "%s", bloco_atual);
    rotulo(teste);
    instr("%%zera.i.%d = phi i32 [ 0, %%%s ], [ %%zera.prox.%d, %%%s ]",
          k, veio, k, corpo);
    instr("%%zera.segue.%d = icmp slt i32 %%zera.i.%d, %ld", k, k, n);
    desvio_cond(valor("%%zera.segue.%d", k), corpo, fim);
    rotulo(corpo);
    instr("%%zera.elem.%d = getelementptr inbounds [%ld x %s], ptr %s, i32 0, i32 %%zera.i.%d",
          k, n, ll_tipo(elem), vet.txt, k);
    instr("store %s %s, ptr %%zera.elem.%d", ll_tipo(elem), ll_zero(elem), k);
    instr("%%zera.prox.%d = add i32 %%zera.i.%d, 1", k, k);
    desvio(teste);
    rotulo(fim);
}

/* toda variavel nasce zerada onde e declarada */
static void gen_declaracao(const Ast *d)
{
    Valor e = endereco_decl(d);

    if (d->tipo == A_DECL_VAR)
    {
        comentario(d, "declaracao de", d->tok.lexema);
        instr("store %s %s, ptr %s", ll_tipo(d->tipo_dado),
              ll_zero(d->tipo_dado), e.txt);
        return;
    }
    if (d->tipo == A_DECL_VET)
    {
        comentario(d, "declaracao do vetor", d->tok.lexema);
        gen_zera_vetor(e, tamanho_vetor(d), d->tipo_dado);
        return;
    }

    comentario(d, "declaracao do registro", d->tok.lexema);
    const Ast *reg = d->decl;
    int pos = 0;
    for (const Ast *c = reg->f1; c != NULL; c = c->prox, pos++)
    {
        Valor p = valor("%%%s.campo.%d", c->tok.lexema, ++contador);
        instr("%s = getelementptr inbounds %%struct.%s, ptr %s, i32 0, i32 %d",
              p.txt, reg->tok.lexema, e.txt, pos);
        if (c->tipo == A_DECL_VET)
            gen_zera_vetor(p, tamanho_vetor(c), c->tipo_dado);
        else
            instr("store %s %s, ptr %s", ll_tipo(c->tipo_dado),
                  ll_zero(c->tipo_dado), p.txt);
    }
}

static void gen_atrib(const Ast *atrib)
{
    const Ast *alvo = atrib->f1;
    Valor v = converte(gen_expr(atrib->f2), atrib->f2->tipo_dado, alvo->tipo_dado);
    Valor e = gen_endereco(alvo);
    instr("store %s %s, ptr %s", ll_tipo(alvo->tipo_dado), v.txt, e.txt);
}

/* nao gera laco aqui: chama o nucleo, que sai em MLIR */
static void gen_atrib_vetorial(const Ast *cmd)
{
    const Ast *dest = cmd->f1, *op = cmd->f2;
    Tipo t = dest->tipo_dado;
    long n = tamanho_vetor(dest->decl);
    const Ast *esq = op->f1, *dir = op->f2;
    int esq_vet = (esq->tipo == A_VAR && esq->decl->tipo == A_DECL_VET);
    int dir_vet = (dir->tipo == A_VAR && dir->decl->tipo == A_DECL_VET);
    char nome[64];

    if (esq_vet && dir_vet)
    {
        ClasseNucleo k = op->tok.tipo == T_MAIS ? NUC_SOMA
                       : op->tok.tipo == T_MENOS ? NUC_SUB : NUC_MUL;
        Valor a = gen_base_vetor(esq);
        Valor b = gen_base_vetor(dir);
        Valor c = gen_base_vetor(dest);
        nucleo_nome(nucleo_registrar(k, t, n), nome, sizeof nome);
        instr("call void @%s(ptr %s, ptr %s, ptr %s)", nome, a.txt, b.txt, c.txt);
        return;
    }

    const Ast *num = esq_vet ? dir : esq;
    const Ast *vet = esq_vet ? esq : dir;
    ClasseNucleo k = op->tok.tipo == T_MAIS ? NUC_ESC_SOMA
                   : op->tok.tipo == T_VEZES ? NUC_ESC_MUL
                   : esq_vet ? NUC_SUB_ESC : NUC_ESC_SUB;
    Valor v = converte(gen_expr(num), num->tipo_dado, t);
    Valor a = gen_base_vetor(vet);
    Valor c = gen_base_vetor(dest);
    nucleo_nome(nucleo_registrar(k, t, n), nome, sizeof nome);
    instr("call void @%s(%s %s, ptr %s, ptr %s)", nome, ll_tipo(t), v.txt,
          a.txt, c.txt);
}

static void gen_escreva(const Ast *cmd)
{
    Tipo t = cmd->f1->tipo_dado;
    Valor v = gen_expr(cmd->f1);
    switch (t)
    {
    case TP_INTEIRA:
        usa(RT_IMPRIME_INT);
        instr("call void @zl_imprime_int(i32 %s)", v.txt);
        break;
    case TP_FLUT:
        usa(RT_IMPRIME_FLUT);
        instr("call void @zl_imprime_flut(float %s)", v.txt);
        break;
    case TP_DUPLO:
        usa(RT_IMPRIME_DUPLO);
        instr("call void @zl_imprime_duplo(double %s)", v.txt);
        break;
    case TP_BOOL:
        usa(RT_IMPRIME_BOOL);
        instr("call void @zl_imprime_bool(i1 zeroext %s)", v.txt);
        break;
    default:
        usa(RT_IMPRIME_TEXTO);
        instr("call void @zl_imprime_texto(ptr %s)", v.txt);
        break;
    }
    usa(RT_IMPRIME_NL);
    instr("call void @zl_imprime_nl()");
}

static void gen_leia(const Ast *cmd)
{
    const Ast *alvo = cmd->f1;
    Tipo t = alvo->tipo_dado;
    Valor e = gen_endereco(alvo);
    Valor r = temp("lido");
    if (t == TP_INTEIRA)
    {
        usa(RT_LE_INT);
        instr("%s = call i32 @zl_le_int()", r.txt);
    }
    else
    {
        usa(t == TP_DUPLO ? RT_LE_DUPLO : RT_LE_FLUT);
        instr("%s = call %s @zl_le_%s()", r.txt, ll_tipo(t), sufixo_real(t));
    }
    instr("store %s %s, ptr %s", ll_tipo(t), r.txt, e.txt);
}

static void gen_respost(const Ast *cmd)
{
    if (cmd->f1 == NULL)
    {
        terminador(na_principal ? "ret i32 0" : "ret void");
        return;
    }
    Valor v = converte(gen_expr(cmd->f1), cmd->f1->tipo_dado, retorno_atual);
    terminador("ret %s %s", ll_tipo(retorno_atual), v.txt);
}

static void gen_durante(const Ast *cmd)
{
    int k = ++contador;
    char teste[32], corpo[32], fim[32];
    snprintf(teste, sizeof teste, "durante.teste.%d", k);
    snprintf(corpo, sizeof corpo, "durante.corpo.%d", k);
    snprintf(fim, sizeof fim, "durante.fim.%d", k);

    rotulo(teste);
    Valor c = gen_expr(cmd->f1);
    desvio_cond(c, corpo, fim);
    rotulo(corpo);
    gen_bloco(cmd->f2);
    desvio(teste);
    rotulo(fim);
}

/* inicio uma vez, depois teste -> corpo -> passo -> teste */
static void gen_repete(const Ast *cmd)
{
    int k = ++contador;
    char teste[32], corpo[32], passo[32], fim[32];
    snprintf(teste, sizeof teste, "repete.teste.%d", k);
    snprintf(corpo, sizeof corpo, "repete.corpo.%d", k);
    snprintf(passo, sizeof passo, "repete.passo.%d", k);
    snprintf(fim, sizeof fim, "repete.fim.%d", k);

    gen_atrib(cmd->f1);
    rotulo(teste);
    Valor c = gen_expr(cmd->f2);
    desvio_cond(c, corpo, fim);
    rotulo(corpo);
    gen_bloco(cmd->f4);
    desvio(passo);
    rotulo(passo);
    gen_atrib(cmd->f3);
    desvio(teste);
    rotulo(fim);
}

static void gen_cond(const Ast *c)
{
    int k = ++contador;
    char entao[32], senao[32], fim[32];
    snprintf(entao, sizeof entao, "cond.entao.%d", k);
    snprintf(senao, sizeof senao, "cond.senao.%d", k);
    snprintf(fim, sizeof fim, "cond.fim.%d", k);

    Valor v = gen_expr(c->f1);
    desvio_cond(v, entao, c->f3 != NULL ? senao : fim);
    rotulo(entao);
    gen_bloco(c->f2);
    desvio(fim);
    if (c->f3 != NULL)
    {
        rotulo(senao);
        if (c->f3->tipo == A_COND)
        {
            comentario(c->f3, "casocontrario cond", NULL);
            gen_cond(c->f3);
        }
        else
            gen_bloco(c->f3);
        desvio(fim);
    }
    rotulo(fim);
}

static void gen_comando(const Ast *cmd)
{
    switch (cmd->tipo)
    {
    case A_DECL_VAR:
    case A_DECL_VET:
    case A_DECL_REG:
        gen_declaracao(cmd);
        break;

    case A_ATRIB:
        comentario(cmd, "atribuicao a", cmd->f1->tipo == A_CAMPO
                                            ? cmd->f1->f1->tok.lexema
                                            : cmd->f1->tok.lexema);
        gen_atrib(cmd);
        break;

    case A_ATRIB_VET:
        comentario(cmd, "operacao vetorial em", cmd->f1->tok.lexema);
        gen_atrib_vetorial(cmd);
        break;

    case A_COND:
        comentario(cmd, "cond", NULL);
        gen_cond(cmd);
        break;

    case A_DURANTE:
        comentario(cmd, "durante", NULL);
        gen_durante(cmd);
        break;

    case A_REPETE:
        comentario(cmd, "repete", NULL);
        gen_repete(cmd);
        break;

    case A_RESPOST:
        comentario(cmd, "respost", NULL);
        gen_respost(cmd);
        break;

    case A_ESCREVA:
        comentario(cmd, "escreva", NULL);
        gen_escreva(cmd);
        break;

    case A_LEIA:
        comentario(cmd, "leia", NULL);
        gen_leia(cmd);
        break;

    case A_CMD_CHAMADA:
        comentario(cmd, "chamada de", cmd->f1->tok.lexema);
        gen_chamada(cmd->f1);
        break;

    default:
        break;
    }
}

static void gen_bloco(const Ast *blk)
{
    for (const Ast *cmd = blk->f1; cmd != NULL; cmd = cmd->prox)
        gen_comando(cmd);
}

/* todo alloca vai no bloco de entrada, ate os de bloco interno:
   dentro de laco a pilha cresceria a cada volta */
static void aloca_bloco(Ast *blk, int nivel)
{
    for (Ast *cmd = blk->f1; cmd != NULL; cmd = cmd->prox)
    {
        switch (cmd->tipo)
        {
        case A_DECL_VAR:
        case A_DECL_VET:
        case A_DECL_REG:
            cmd->num = (nivel == 0) ? 0 : ++n_blocos;
            instr("%s = alloca %s", endereco_decl(cmd).txt, ll_tipo_decl(cmd).txt);
            break;
        case A_REPETE:
            aloca_bloco(cmd->f4, nivel + 1);
            break;
        case A_DURANTE:
            aloca_bloco(cmd->f2, nivel + 1);
            break;
        case A_COND:
            for (Ast *c = cmd; c != NULL; c = c->f3)
            {
                if (c->tipo != A_COND)
                {
                    aloca_bloco(c, nivel + 1);
                    break;
                }
                aloca_bloco(c->f2, nivel + 1);
            }
            break;
        default:
            break;
        }
    }
}

static void gen_funcao(Ast *fn, int principal)
{
    contador = 0;
    n_blocos = 0;
    terminado = 0;
    na_principal = principal;
    retorno_atual = principal ? TP_INTEIRA : fn->tipo_dado;

    fprintf(out, "; linha %d\n", fn->tok.linha);
    if (principal)
        fprintf(out, "define i32 @zl_principal() nounwind {\n");
    else
    {
        fprintf(out, "define %s @mac_%s(", ll_tipo(fn->tipo_dado), fn->tok.lexema);
        for (const Ast *p = fn->f1; p != NULL; p = p->prox)
            fprintf(out, "%s%s %%%s", p == fn->f1 ? "" : ", ",
                    ll_tipo(p->tipo_dado), p->tok.lexema);
        fprintf(out, ") nounwind {\n");
    }
    fprintf(out, "funcao.entrada:\n");
    snprintf(bloco_atual, sizeof bloco_atual, "funcao.entrada");

    for (Ast *p = fn->f1; p != NULL; p = p->prox)
    {
        p->num = 0;
        instr("%%%s.addr = alloca %s", p->tok.lexema, ll_tipo(p->tipo_dado));
    }
    aloca_bloco(fn->f2, 0);
    for (const Ast *p = fn->f1; p != NULL; p = p->prox)
        instr("store %s %%%s, ptr %%%s.addr", ll_tipo(p->tipo_dado),
              p->tok.lexema, p->tok.lexema);

    gen_bloco(fn->f2);

    if (!terminado)
    {
        if (principal)
            terminador("ret i32 0");
        else if (fn->tipo_dado == TP_VAZIO)
            terminador("ret void");
        else
            terminador("unreachable");
    }
    fprintf(out, "}\n\n");
}

static void gen_registros(const Ast *programa)
{
    int algum = 0;
    for (const Ast *d = programa->f1; d != NULL; d = d->prox)
    {
        if (d->tipo != A_REGISTRO)
            continue;
        fprintf(out, "%%struct.%s = type {", d->tok.lexema);
        for (const Ast *c = d->f1; c != NULL; c = c->prox)
            fprintf(out, "%s %s", c == d->f1 ? "" : ",", ll_tipo_decl(c).txt);
        fprintf(out, " }   ; linha %d\n", d->tok.linha);
        algum = 1;
    }
    if (algum)
        fputc('\n', out);
}

static void gen_globais(Ast *programa)
{
    int algum = 0;
    for (Ast *d = programa->f1; d != NULL; d = d->prox)
    {
        if (d->tipo != A_DECL_VAR && d->tipo != A_DECL_VET && d->tipo != A_DECL_REG)
            continue;
        d->num = -1;
        if (d->tipo == A_DECL_VAR)
            fprintf(out, "@mac_%s = global %s %s", d->tok.lexema,
                    ll_tipo(d->tipo_dado), ll_zero(d->tipo_dado));
        else
            fprintf(out, "@mac_%s = global %s zeroinitializer", d->tok.lexema,
                    ll_tipo_decl(d).txt);
        fprintf(out, "   ; linha %d\n", d->tok.linha);
        algum = 1;
    }
    if (algum)
        fputc('\n', out);
}

/* o que nao e ASCII visivel sai escapado em hexa */
static void gen_textos(void)
{
    for (int i = 0; i < n_textos; i++)
    {
        fprintf(out, "@.str.%d = private unnamed_addr constant [%d x i8] c\"",
                i, textos[i].tam + 1);
        for (int j = 0; j < textos[i].tam; j++)
        {
            unsigned char c = (unsigned char)textos[i].texto[j];
            if (c >= 0x20 && c < 0x7F && c != '"' && c != '\\')
                fputc(c, out);
            else
                fprintf(out, "\\%02X", c);
        }
        fprintf(out, "\\00\", align 1\n");
    }
    if (n_textos > 0)
        fputc('\n', out);
}

void gerar_llvm(Ast *programa, const char *nome_fonte, FILE *saida)
{
    out = saida;
    n_textos = 0;
    n_embutidas = 0;
    nucleos_limpar();
    memset(rt_usada, 0, sizeof rt_usada);

    fprintf(out, "; Gerado pelo compilador da Macaronica. Fonte: ");
    for (const char *c = nome_fonte; *c != '\0'; c++)
        fputc((*c >= 0x20 && *c < 0x7F) ? *c : '?', out);
    fprintf(out, "\n; IR portavel: sem target triple e sem datalayout. O alvo e"
                 " escolhido na hora\n; de montar (lli, clang, llc -mtriple).\n\n");

    gen_registros(programa);
    gen_globais(programa);

    for (Ast *d = programa->f1; d != NULL; d = d->prox)
    {
        if (d->tipo == A_FUNCAO)
            gen_funcao(d, 0);
        else if (d->tipo == A_PRINCIPAL)
            gen_funcao(d, 1);
    }

    gen_textos();

    fprintf(out, "; biblioteca de execucao (runtime/zl_runtime.h)\n");
    for (int i = 0; i < RT_TOTAL; i++)
        if (rt_usada[i])
            fprintf(out, "%s\n", rt_declare[i]);
    for (int i = 0; i < n_embutidas; i++)
    {
        const Ast *fn = embutida_usada[i];
        fprintf(out, "declare %s @zl_%s(", ll_tipo(fn->tipo_dado), fn->tok.lexema);
        for (const Ast *p = fn->f1; p != NULL; p = p->prox)
            fprintf(out, "%s%s", p == fn->f1 ? "" : ", ", ll_tipo(p->tipo_dado));
        fprintf(out, ") nounwind\n");
    }

    if (nucleos_total() > 0)
    {
        fprintf(out, "\n; nucleos das operacoes vetoriais: o corpo sai em MLIR,"
                     " num arquivo a parte\n");
        for (int i = 0; i < nucleos_total(); i++)
            nucleo_declare_llvm(nucleo_em(i), out);
    }

    free(textos);
    textos = NULL;
    n_textos = cap_textos = 0;
}
