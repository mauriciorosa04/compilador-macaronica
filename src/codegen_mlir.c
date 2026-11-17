/* escreve os nucleos em MLIR com os dialetos func, arith, memref e affine.
   O laco e sempre affine.for de 0 a N com N constante, que e a forma
   que os passos de vetorizar e paralelizar reconhecem */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen_mlir.h"

static Nucleo *nucleos;
static int n_nucleos, cap_nucleos;
static int reordenar;

void mlir_permitir_reordenar(int sim) { reordenar = sim; }

void nucleos_limpar(void)
{
    free(nucleos);
    nucleos = NULL;
    n_nucleos = cap_nucleos = 0;
}

const Nucleo *nucleo_registrar(ClasseNucleo classe, Tipo elem, long n)
{
    for (int i = 0; i < n_nucleos; i++)
        if (nucleos[i].classe == classe && nucleos[i].elem == elem &&
            nucleos[i].n == n)
            return &nucleos[i];
    if (n_nucleos == cap_nucleos)
    {
        cap_nucleos = cap_nucleos ? cap_nucleos * 2 : 8;
        Nucleo *maior = (Nucleo *)realloc(nucleos, (size_t)cap_nucleos * sizeof(Nucleo));
        if (maior == NULL)
        {
            fprintf(stderr, "erro fatal: memoria insuficiente\n");
            exit(EXIT_FAILURE);
        }
        nucleos = maior;
    }
    nucleos[n_nucleos].classe = classe;
    nucleos[n_nucleos].elem = elem;
    nucleos[n_nucleos].n = n;
    return &nucleos[n_nucleos++];
}

int nucleos_total(void) { return n_nucleos; }
const Nucleo *nucleo_em(int i) { return &nucleos[i]; }

static const char *nome_classe(ClasseNucleo c)
{
    switch (c)
    {
    case NUC_SOMA: return "soma";
    case NUC_SUB: return "sub";
    case NUC_MUL: return "mul";
    case NUC_ESC_SOMA: return "escsoma";
    case NUC_ESC_SUB: return "escsub";
    case NUC_SUB_ESC: return "subesc";
    case NUC_ESC_MUL: return "escmul";
    case NUC_SOMAVET: return "somavet";
    case NUC_MAXVET: return "maxvet";
    default: return "prodesc";
    }
}

static const char *tipo_mlir(Tipo t)
{
    return t == TP_INTEIRA ? "i32" : t == TP_FLUT ? "f32" : "f64";
}

static const char *tipo_llvm(Tipo t)
{
    return t == TP_INTEIRA ? "i32" : t == TP_FLUT ? "float" : "double";
}

void nucleo_nome(const Nucleo *k, char *buf, size_t tam)
{
    snprintf(buf, tam, "nuc_%s_%s_%ld", nome_classe(k->classe),
             tipo_mlir(k->elem), k->n);
}

void nucleo_declare_llvm(const Nucleo *k, FILE *out)
{
    char nome[64];
    const char *t = tipo_llvm(k->elem);
    nucleo_nome(k, nome, sizeof nome);
    switch (k->classe)
    {
    case NUC_SOMA:
    case NUC_SUB:
    case NUC_MUL:
        fprintf(out, "declare void @%s(ptr, ptr, ptr) nounwind\n", nome);
        break;
    case NUC_SOMAVET:
    case NUC_MAXVET:
        fprintf(out, "declare %s @%s(ptr) nounwind\n", t, nome);
        break;
    case NUC_PRODESC:
        fprintf(out, "declare %s @%s(ptr, ptr) nounwind\n", t, nome);
        break;
    default:
        fprintf(out, "declare void @%s(%s, ptr, ptr) nounwind\n", nome, t);
        break;
    }
}

static const char *arith(const char *op, Tipo t, char *buf)
{
    sprintf(buf, "arith.%s%c", op, t == TP_INTEIRA ? 'i' : 'f');
    return buf;
}

static const char *marca_reducao(Tipo t)
{
    return (reordenar && t != TP_INTEIRA) ? " fastmath<reassoc>" : "";
}

static const char *zero_mlir(Tipo t) { return t == TP_INTEIRA ? "0" : "0.0"; }

/* faz a funcao sair com nounwind no LLVM IR */
#define ATRIBUTOS "attributes { passthrough = [\"nounwind\"] }"

static void gen_nucleo(const Nucleo *k, FILE *out)
{
    char nome[64], op[16], mem[48];
    const char *t = tipo_mlir(k->elem);
    nucleo_nome(k, nome, sizeof nome);
    snprintf(mem, sizeof mem, "memref<%ldx%s>", k->n, t);

    switch (k->classe)
    {
    case NUC_SOMA:
    case NUC_SUB:
    case NUC_MUL:
        arith(k->classe == NUC_SOMA ? "add" : k->classe == NUC_SUB ? "sub" : "mul",
              k->elem, op);
        fprintf(out, "// c[i] = a[i] %c b[i]\n",
                k->classe == NUC_SOMA ? '+' : k->classe == NUC_SUB ? '-' : '*');
        fprintf(out, "func.func @%s(%%a: %s, %%b: %s, %%c: %s) %s {\n",
                nome, mem, mem, mem, ATRIBUTOS);
        fprintf(out, "  affine.for %%i = 0 to %ld {\n", k->n);
        fprintf(out, "    %%x = affine.load %%a[%%i] : %s\n", mem);
        fprintf(out, "    %%y = affine.load %%b[%%i] : %s\n", mem);
        fprintf(out, "    %%z = %s %%x, %%y : %s\n", op, t);
        fprintf(out, "    affine.store %%z, %%c[%%i] : %s\n", mem);
        fprintf(out, "  }\n  return\n}\n\n");
        break;

    case NUC_ESC_SOMA:
    case NUC_ESC_SUB:
    case NUC_SUB_ESC:
    case NUC_ESC_MUL:
        arith(k->classe == NUC_ESC_SOMA ? "add" : k->classe == NUC_ESC_MUL ? "mul" : "sub",
              k->elem, op);
        fprintf(out, "// c[i] = %s\n",
                k->classe == NUC_ESC_SOMA ? "k + a[i]"
                : k->classe == NUC_ESC_MUL ? "k * a[i]"
                : k->classe == NUC_ESC_SUB ? "k - a[i]" : "a[i] - k");
        fprintf(out, "func.func @%s(%%k: %s, %%a: %s, %%c: %s) %s {\n",
                nome, t, mem, mem, ATRIBUTOS);
        fprintf(out, "  affine.for %%i = 0 to %ld {\n", k->n);
        fprintf(out, "    %%x = affine.load %%a[%%i] : %s\n", mem);
        if (k->classe == NUC_SUB_ESC)
            fprintf(out, "    %%z = %s %%x, %%k : %s\n", op, t);
        else
            fprintf(out, "    %%z = %s %%k, %%x : %s\n", op, t);
        fprintf(out, "    affine.store %%z, %%c[%%i] : %s\n", mem);
        fprintf(out, "  }\n  return\n}\n\n");
        break;

    case NUC_SOMAVET:
        fprintf(out, "// soma de todos os a[i]\n");
        fprintf(out, "func.func @%s(%%a: %s) -> %s %s {\n", nome, mem, t, ATRIBUTOS);
        fprintf(out, "  %%zero = arith.constant %s : %s\n", zero_mlir(k->elem), t);
        fprintf(out, "  %%s = affine.for %%i = 0 to %ld iter_args(%%acc = %%zero) -> (%s) {\n",
                k->n, t);
        fprintf(out, "    %%x = affine.load %%a[%%i] : %s\n", mem);
        fprintf(out, "    %%n = %s %%acc, %%x%s : %s\n", arith("add", k->elem, op),
                marca_reducao(k->elem), t);
        fprintf(out, "    affine.yield %%n : %s\n", t);
        fprintf(out, "  }\n  return %%s : %s\n}\n\n", t);
        break;

    case NUC_MAXVET:
        fprintf(out, "// maior dos a[i]\n");
        fprintf(out, "func.func @%s(%%a: %s) -> %s %s {\n", nome, mem, t, ATRIBUTOS);
        /* comeca no primeiro elemento. Nos reais usa cmpf + select porque
           o maximo pronto do arith nao compila pro AVR */
        fprintf(out, "  %%primeiro = affine.load %%a[0] : %s\n", mem);
        fprintf(out, "  %%m = affine.for %%i = 1 to %ld iter_args(%%acc = %%primeiro) -> (%s) {\n",
                k->n, t);
        fprintf(out, "    %%x = affine.load %%a[%%i] : %s\n", mem);
        if (k->elem == TP_INTEIRA)
            fprintf(out, "    %%n = arith.maxsi %%acc, %%x : %s\n", t);
        else
        {
            fprintf(out, "    %%maior = arith.cmpf ogt, %%x, %%acc : %s\n", t);
            fprintf(out, "    %%n = arith.select %%maior, %%x, %%acc : %s\n", t);
        }
        fprintf(out, "    affine.yield %%n : %s\n", t);
        fprintf(out, "  }\n  return %%m : %s\n}\n\n", t);
        break;

    default:
        fprintf(out, "// produto escalar: soma de a[i] * b[i]\n");
        fprintf(out, "func.func @%s(%%a: %s, %%b: %s) -> %s %s {\n",
                nome, mem, mem, t, ATRIBUTOS);
        fprintf(out, "  %%zero = arith.constant %s : %s\n", zero_mlir(k->elem), t);
        fprintf(out, "  %%s = affine.for %%i = 0 to %ld iter_args(%%acc = %%zero) -> (%s) {\n",
                k->n, t);
        fprintf(out, "    %%x = affine.load %%a[%%i] : %s\n", mem);
        fprintf(out, "    %%y = affine.load %%b[%%i] : %s\n", mem);
        fprintf(out, "    %%p = %s %%x, %%y%s : %s\n", arith("mul", k->elem, op),
                marca_reducao(k->elem), t);
        fprintf(out, "    %%n = %s %%acc, %%p%s : %s\n", arith("add", k->elem, op),
                marca_reducao(k->elem), t);
        fprintf(out, "    affine.yield %%n : %s\n", t);
        fprintf(out, "  }\n  return %%s : %s\n}\n\n", t);
        break;
    }
}

void gerar_mlir(const char *nome_fonte, FILE *saida)
{
    fprintf(saida, "// Nucleos vetoriais gerados pelo compilador da Macaronica. Fonte: ");
    for (const char *c = nome_fonte; *c != '\0'; c++)
        fputc((*c >= 0x20 && *c < 0x7F) ? *c : '?', saida);
    fprintf(saida, "\n// Dialetos: func, arith, memref e affine. Desce ate LLVM IR com"
                   " scripts/pipeline.sh.\n\n");
    for (int i = 0; i < n_nucleos; i++)
        gen_nucleo(&nucleos[i], saida);
}
