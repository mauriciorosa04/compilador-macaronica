#include <stdio.h>
#include <stdarg.h>
#include "erros.h"

static int n_lexicos = 0;
static int n_sintaticos = 0;
static int n_semanticos = 0;
static int silencioso = 0;

void erros_silenciar(int s) { silencioso = s; }

/* monta a linha toda e escreve de uma vez (stderr nao tem buffer) */
static void reportar(const char *fase, int linha, int coluna,
                     const char *fmt, va_list args)
{
    char buf[1024];
    int n = snprintf(buf, sizeof buf, "[%s] linha %d, coluna %d: ",
                     fase, linha, coluna);
    if (n < 0 || n >= (int)sizeof buf)
        n = 0;
    int m = vsnprintf(buf + n, sizeof buf - (size_t)n - 1, fmt, args);
    if (m > 0)
        n += m;
    if (n > (int)sizeof buf - 2)
        n = (int)sizeof buf - 2;
    buf[n] = '\n';
    buf[n + 1] = '\0';
    fputs(buf, stderr);
}

void erro_lexico(int linha, int coluna, const char *fmt, ...)
{
    if (silencioso)
        return;

    va_list args;
    va_start(args, fmt);
    reportar("ERRO LEXICO", linha, coluna, fmt, args);
    va_end(args);
    n_lexicos++;
}

void erro_sintatico(int linha, int coluna, const char *fmt, ...)
{
    if (silencioso)
        return;

    va_list args;
    va_start(args, fmt);
    reportar("ERRO SINTATICO", linha, coluna, fmt, args);
    va_end(args);
    n_sintaticos++;
}

void erro_semantico(int linha, int coluna, const char *fmt, ...)
{
    if (silencioso)
        return;

    va_list args;
    va_start(args, fmt);
    reportar("ERRO SEMANTICO", linha, coluna, fmt, args);
    va_end(args);
    n_semanticos++;
}

/* aviso nao conta como erro */
void aviso_semantico(int linha, int coluna, const char *fmt, ...)
{
    if (silencioso)
        return;

    va_list args;
    va_start(args, fmt);
    reportar("AVISO SEMANTICO", linha, coluna, fmt, args);
    va_end(args);
}

int total_erros_lexicos(void) { return n_lexicos; }
int total_erros_sintaticos(void) { return n_sintaticos; }
int total_erros_semanticos(void) { return n_semanticos; }
int total_erros(void) { return n_lexicos + n_sintaticos + n_semanticos; }
