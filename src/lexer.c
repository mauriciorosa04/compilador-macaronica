/* analisador lexico: cada lexer_proximo devolve um token */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "lexer.h"
#include "erros.h"

typedef struct
{
    const char *palavra;
    TipoToken tipo;
} Reservada;

/* os logicos e VER/FAL sao maiusculos, o resto minusculo */
static const Reservada reservadas[] = {
    {"receba", T_RECEBA}, {"repete", T_REPETE}, {"durante", T_DURANTE},
    {"cond", T_COND}, {"casocontrario", T_CASOCONTRARIO}, {"edai", T_EDAI},
    {"func", T_FUNC}, {"outrafuncao", T_OUTRAFUNCAO},
    {"registro", T_REGISTRO}, {"principal", T_PRINCIPAL},
    {"respost", T_RESPOST}, {"vet", T_VET}, {"raiz", T_RAIZ},
    {"escreva", T_ESCREVA}, {"leia", T_LEIA},
    {"somavet", T_SOMAVET}, {"maxvet", T_MAXVET}, {"prodesc", T_PRODESC},
    {"bool", T_BOOL}, {"inteira", T_INTEIRA}, {"flut", T_FLUT},
    {"palavra", T_PALAVRA}, {"duplocarpado", T_DUPLOCARPADO},
    {"vazio", T_VAZIO},
    {"VER", T_VER}, {"FAL", T_FAL},
    {"OU", T_OU}, {"E", T_E}, {"NAO", T_NAO}, {"XOR", T_XOR},
};

static TipoToken procurar_reservada(const char *lexema)
{
    int n = (int)(sizeof(reservadas) / sizeof(reservadas[0]));
    for (int i = 0; i < n; i++)
        if (strcmp(reservadas[i].palavra, lexema) == 0)
            return reservadas[i].tipo;
    return T_IDENT;
}

void lexer_iniciar(Lexer *lx, const char *fonte)
{
    lx->fonte = fonte;
    lx->pos = 0;
    /* pula o BOM que o Bloco de Notas poe na frente */
    if ((unsigned char)fonte[0] == 0xEF && (unsigned char)fonte[1] == 0xBB &&
        (unsigned char)fonte[2] == 0xBF)
        lx->pos = 3;
    lx->linha = 1;
    lx->coluna = 1;
}

static char atual(const Lexer *lx) { return lx->fonte[lx->pos]; }
static char seguinte(const Lexer *lx)
{
    return lx->fonte[lx->pos] ? lx->fonte[lx->pos + 1] : '\0';
}

static int continuacoes_esperadas(unsigned char b)
{
    if (b >= 0xF0)
        return 3;
    if (b >= 0xE0)
        return 2;
    if (b >= 0xC0)
        return 1;
    return 0;
}

/* coluna conta caractere, nao byte: continuacao de UTF-8 nao anda coluna */
static int continua_caractere(const char *fonte, int i)
{
    int dist = 0;
    while (i > 0 && dist < 3 && ((unsigned char)fonte[i] & 0xC0) == 0x80)
    {
        i--;
        dist++;
    }
    return dist > 0 && dist <= continuacoes_esperadas((unsigned char)fonte[i]);
}

static char avancar(Lexer *lx)
{
    char c = lx->fonte[lx->pos];
    if (c == '\n')
    {
        lx->linha++;
        lx->coluna = 1;
    }
    else if (!continua_caractere(lx->fonte, lx->pos))
    {
        lx->coluna++;
    }
    lx->pos++;
    return c;
}

static void pular_brancos(Lexer *lx)
{
    while (atual(lx) == ' ' || atual(lx) == '\t' ||
           atual(lx) == '\r' || atual(lx) == '\n')
        avancar(lx);
}

static Token fazer_token(TipoToken tipo, const char *lexema,
                         int linha, int coluna)
{
    Token t;
    t.tipo = tipo;
    t.linha = linha;
    t.coluna = coluna;
    strncpy(t.lexema, lexema, LEXEMA_MAX - 1);
    t.lexema[LEXEMA_MAX - 1] = '\0';
    t.texto = NULL;
    t.tam_texto = 0;
    return t;
}

/* numero nao leva sinal, o '-' e sempre operador */
static Token ler_numero(Lexer *lx)
{
    int linha = lx->linha, coluna = lx->coluna;
    char buf[LEXEMA_MAX];
    int n = 0;
    int real = 0;

    int estourou = 0;

    while (isdigit((unsigned char)atual(lx)))
    {
        char d = avancar(lx);
        if (n < LEXEMA_MAX - 1) buf[n++] = d; else estourou = 1;
    }

    if (atual(lx) == '.' && isdigit((unsigned char)seguinte(lx)))
    {
        real = 1;
        if (n < LEXEMA_MAX - 1) buf[n++] = avancar(lx); else { avancar(lx); estourou = 1; }
        while (isdigit((unsigned char)atual(lx)))
        {
            char d = avancar(lx);
            if (n < LEXEMA_MAX - 1) buf[n++] = d; else estourou = 1;
        }
    }
    buf[n] = '\0';

    if (estourou)
        erro_lexico(linha, coluna,
                    "numero com mais de %d caracteres (limite desta implementacao)",
                    LEXEMA_MAX - 1);

    /* sobrou ponto sem digito: "12." */
    if (atual(lx) == '.')
    {
        avancar(lx);
        erro_lexico(linha, coluna,
                    "numero real malformado: '%s.' (esperava digitos apos o ponto)", buf);
        return fazer_token(T_ERRO, buf, linha, coluna);
    }

    Token t = fazer_token(real ? T_NUM_FLUT : T_NUM_INT, buf, linha, coluna);
    return t;
}

static Token ler_palavra(Lexer *lx)
{
    int linha = lx->linha, coluna = lx->coluna;
    char buf[LEXEMA_MAX];
    int n = 0;

    int estourou = 0;

    while (isalpha((unsigned char)atual(lx)) || isdigit((unsigned char)atual(lx)))
    {
        char d = avancar(lx);
        if (n < LEXEMA_MAX - 1) buf[n++] = d; else estourou = 1;
    }
    buf[n] = '\0';

    if (estourou)
        erro_lexico(linha, coluna,
                    "identificador com mais de %d caracteres (limite desta implementacao)",
                    LEXEMA_MAX - 1);

    return fazer_token(procurar_reservada(buf), buf, linha, coluna);
}

/* a string nao pode passar do fim da linha */
static Token ler_string(Lexer *lx)
{
    int linha = lx->linha, coluna = lx->coluna;
    char buf[LEXEMA_MAX];
    int n = 0;

    avancar(lx);
    int inicio = lx->pos;
    while (atual(lx) != '"' && atual(lx) != '\n' && atual(lx) != '\0')
    {
        if (n < LEXEMA_MAX - 1)
            buf[n++] = avancar(lx);
        else
            avancar(lx);
    }
    buf[n] = '\0';
    int fim = lx->pos;

    if (atual(lx) != '"')
    {
        erro_lexico(linha, coluna, "literal de palavra sem aspas de fechamento");
        return fazer_token(T_ERRO, buf, linha, coluna);
    }
    avancar(lx);
    Token t = fazer_token(T_STRING, buf, linha, coluna);
    t.texto = lx->fonte + inicio;
    t.tam_texto = fim - inicio;
    return t;
}

/* NAO escrito com til (UTF-8 ou CP1252) vale como o token NAO */
static int ler_nao_com_til(Lexer *lx)
{
    const unsigned char *s = (const unsigned char *)lx->fonte + lx->pos;
    int n = 0;
    if (s[0] != 'N')
        return 0;
    if (s[1] == 0xC3 && s[2] == 0x83 && s[3] == 'O')
        n = 4;
    else if (s[1] == 0xC3 && s[2] == 'O')
        n = 3;
    else
        return 0;
    if (isalnum(s[n]))
        return 0;
    for (int i = 0; i < n; i++)
        avancar(lx);
    return 1;
}

Token lexer_proximo(Lexer *lx)
{
    pular_brancos(lx);

    int linha = lx->linha, coluna = lx->coluna;
    char c = atual(lx);

    if (c == '\0')
        return fazer_token(T_EOF, "<fim>", linha, coluna);

    if (isdigit((unsigned char)c))
        return ler_numero(lx);
    if (c == 'N' && ler_nao_com_til(lx))
        return fazer_token(T_NAO, "NAO", linha, coluna);
    if (isalpha((unsigned char)c))
        return ler_palavra(lx);
    if (c == '"')
        return ler_string(lx);

    avancar(lx);
    switch (c)
    {
    case '+': return fazer_token(T_MAIS, "+", linha, coluna);
    case '-': return fazer_token(T_MENOS, "-", linha, coluna);
    case '*': return fazer_token(T_VEZES, "*", linha, coluna);
    case '^': return fazer_token(T_POT, "^", linha, coluna);
    case '<': return fazer_token(T_MENOR, "<", linha, coluna);
    case '>': return fazer_token(T_MAIOR, ">", linha, coluna);
    case '!': return fazer_token(T_DIFERENTE, "!", linha, coluna);
    case '(': return fazer_token(T_ABRE_PAR, "(", linha, coluna);
    case ')': return fazer_token(T_FECHA_PAR, ")", linha, coluna);
    case '[': return fazer_token(T_ABRE_COL, "[", linha, coluna);
    case ']': return fazer_token(T_FECHA_COL, "]", linha, coluna);
    case '{': return fazer_token(T_ABRE_CHAVE, "{", linha, coluna);
    case '}': return fazer_token(T_FECHA_CHAVE, "}", linha, coluna);
    case '@': return fazer_token(T_ARROBA, "@", linha, coluna);
    case ';': return fazer_token(T_PONTOVIRG, ";", linha, coluna);
    case ',': return fazer_token(T_VIRGULA, ",", linha, coluna);
    case '.': return fazer_token(T_PONTO, ".", linha, coluna);

    case '/': /* espia o proximo, senao "//" vira dois "/" */
        if (atual(lx) == '/')
        {
            avancar(lx);
            return fazer_token(T_DIVINT, "//", linha, coluna);
        }
        return fazer_token(T_DIVIDE, "/", linha, coluna);

    case '=': /* so existe "==" */
        if (atual(lx) == '=')
        {
            avancar(lx);
            return fazer_token(T_IGUAL, "==", linha, coluna);
        }
        erro_lexico(linha, coluna,
                    "'=' isolado nao existe em Macaronica "
                    "(use '==' para comparar ou 'receba' para atribuir)");
        return fazer_token(T_ERRO, "=", linha, coluna);

    default:
    {
        unsigned char uc = (unsigned char)c;
        char lexema[8] = {c, '\0'};
        int n = 1;

        /* junta os bytes do caractere UTF-8 pra dar um erro so */
        if (uc >= 0xC0)
        {
            int esperados = continuacoes_esperadas(uc);
            while (esperados-- > 0 && ((unsigned char)atual(lx) & 0xC0) == 0x80 && n < 7)
                lexema[n++] = avancar(lx);
            lexema[n] = '\0';
            erro_lexico(linha, coluna,
                        "caractere '%s' nao pertence ao alfabeto da linguagem", lexema);
        }
        else if (uc >= 0x80 || !isprint(uc))
        {
            erro_lexico(linha, coluna,
                        "byte 0x%02X nao pertence ao alfabeto da linguagem", uc);
        }
        else
        {
            erro_lexico(linha, coluna,
                        "simbolo '%c' nao pertence ao alfabeto da linguagem", c);
        }
        return fazer_token(T_ERRO, lexema, linha, coluna);
    }
    }
}

const char *token_nome(TipoToken tipo)
{
    switch (tipo)
    {
    case T_EOF: return "EOF";
    case T_ERRO: return "ERRO";
    case T_IDENT: return "IDENT";
    case T_NUM_INT: return "NUM_INT";
    case T_NUM_FLUT: return "NUM_FLUT";
    case T_STRING: return "STRING";
    case T_PONTO: return "PONTO";
    case T_SOMAVET: return "somavet";
    case T_MAXVET: return "maxvet";
    case T_PRODESC: return "prodesc";
    case T_BOOL: return "bool";
    case T_INTEIRA: return "inteira";
    case T_FLUT: return "flut";
    case T_PALAVRA: return "palavra";
    case T_DUPLOCARPADO: return "duplocarpado";
    case T_VAZIO: return "vazio";
    case T_RECEBA: return "receba";
    case T_REPETE: return "repete";
    case T_DURANTE: return "durante";
    case T_COND: return "cond";
    case T_CASOCONTRARIO: return "casocontrario";
    case T_EDAI: return "edai";
    case T_FUNC: return "func";
    case T_OUTRAFUNCAO: return "outrafuncao";
    case T_REGISTRO: return "registro";
    case T_PRINCIPAL: return "principal";
    case T_RESPOST: return "respost";
    case T_VET: return "vet";
    case T_RAIZ: return "raiz";
    case T_ESCREVA: return "escreva";
    case T_LEIA: return "leia";
    case T_VER: return "VER";
    case T_FAL: return "FAL";
    case T_OU: return "OU";
    case T_E: return "E";
    case T_NAO: return "NAO";
    case T_XOR: return "XOR";
    case T_MAIS: return "+";
    case T_MENOS: return "-";
    case T_VEZES: return "*";
    case T_DIVIDE: return "/";
    case T_DIVINT: return "//";
    case T_POT: return "^";
    case T_MENOR: return "<";
    case T_MAIOR: return ">";
    case T_IGUAL: return "==";
    case T_DIFERENTE: return "!";
    case T_ABRE_PAR: return "(";
    case T_FECHA_PAR: return ")";
    case T_ABRE_COL: return "[";
    case T_FECHA_COL: return "]";
    case T_ABRE_CHAVE: return "{";
    case T_FECHA_CHAVE: return "}";
    case T_ARROBA: return "@";
    case T_PONTOVIRG: return ";";
    case T_VIRGULA: return ",";
    default: return "?";
    }
}
