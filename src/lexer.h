/* tokens da Macaronica e o estado do lexer */
#ifndef LEXER_H
#define LEXER_H

typedef enum
{
    T_EOF,
    T_ERRO, /* erro lexico ja reportado, o parser so pula */

    T_IDENT,
    T_NUM_INT,
    T_NUM_FLUT,
    T_STRING,

    T_BOOL, T_INTEIRA, T_FLUT, T_PALAVRA, T_DUPLOCARPADO, T_VAZIO,

    T_RECEBA, T_REPETE, T_DURANTE, T_COND, T_CASOCONTRARIO, T_EDAI,
    T_FUNC, T_OUTRAFUNCAO, T_REGISTRO, T_PRINCIPAL, T_RESPOST,
    T_VET, T_RAIZ, T_ESCREVA, T_LEIA,

    T_VER, T_FAL, T_OU, T_E, T_NAO, T_XOR,

    T_MAIS, T_MENOS, T_VEZES, T_DIVIDE, T_DIVINT, T_POT, /* '/' real e '//' inteira */

    T_MENOR, T_MAIOR, T_IGUAL, T_DIFERENTE, /* '!' e o diferente, nao a negacao */

    T_ABRE_PAR, T_FECHA_PAR,
    T_ABRE_COL, T_FECHA_COL,
    T_ABRE_CHAVE, T_FECHA_CHAVE,
    T_ARROBA, T_PONTOVIRG, T_VIRGULA,
    T_PONTO,

    /* extensao vetorial */
    T_SOMAVET, T_MAXVET, T_PRODESC
} TipoToken;

#define LEXEMA_MAX 128

typedef struct
{
    TipoToken tipo;
    char lexema[LEXEMA_MAX];
    int linha;
    int coluna;
    /* so em T_STRING: o texto inteiro, apontando pra dentro do fonte
       (o lexema guarda so o comeco) */
    const char *texto;
    int tam_texto;
} Token;

typedef struct
{
    const char *fonte;
    int pos;
    int linha;
    int coluna;
} Lexer;

void lexer_iniciar(Lexer *lx, const char *fonte);
Token lexer_proximo(Lexer *lx);
const char *token_nome(TipoToken tipo);

#endif
