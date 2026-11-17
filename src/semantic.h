/* analise semantica: confere tipos e escopos e anota a arvore */
#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "ast.h"

void analisar_semantica(Ast *programa);

int semantica_definir_alvo(const char *alvo);

#endif
