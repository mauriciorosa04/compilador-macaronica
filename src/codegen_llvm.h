/* gerador de LLVM IR a partir da arvore anotada */
#ifndef CODEGEN_LLVM_H
#define CODEGEN_LLVM_H

#include <stdio.h>
#include "ast.h"

void gerar_llvm(Ast *programa, const char *nome_fonte, FILE *saida);

#endif
