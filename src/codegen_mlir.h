/* nucleos das operacoes vetoriais. O programa sai em LLVM IR e so chama
   os nucleos. O corpo deles sai em MLIR, desce pelo scripts/pipeline.sh
   e os dois .ll sao juntados com llvm-link.
   Um nucleo por operacao, tipo e tamanho: nuc_soma_i32_1024 */
#ifndef CODEGEN_MLIR_H
#define CODEGEN_MLIR_H

#include <stdio.h>
#include "ast.h"

typedef enum
{
    NUC_SOMA, /* c[i] = a[i] + b[i] */
    NUC_SUB,
    NUC_MUL,
    NUC_ESC_SOMA, /* c[i] = k + a[i] */
    NUC_ESC_SUB, /* c[i] = k - a[i] */
    NUC_SUB_ESC, /* c[i] = a[i] - k */
    NUC_ESC_MUL,
    NUC_SOMAVET,
    NUC_MAXVET,
    NUC_PRODESC
} ClasseNucleo;

typedef struct
{
    ClasseNucleo classe;
    Tipo elem;
    long n;
} Nucleo;

void nucleos_limpar(void);
const Nucleo *nucleo_registrar(ClasseNucleo classe, Tipo elem, long n);
int nucleos_total(void);
const Nucleo *nucleo_em(int i);

void nucleo_nome(const Nucleo *k, char *buf, size_t tam);

void nucleo_declare_llvm(const Nucleo *k, FILE *out);

/* --reordena: reducao de real sai com fastmath<reassoc> */
void mlir_permitir_reordenar(int sim);

void gerar_mlir(const char *nome_fonte, FILE *saida);

#endif
