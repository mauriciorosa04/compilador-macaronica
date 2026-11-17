/* parser de descida recursiva: devolve a arvore do programa */
#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"

Ast *parse_programa(Lexer *lx);

#endif
