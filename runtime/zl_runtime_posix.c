/* biblioteca de execucao do PC.
   clang saida.ll runtime/zl_runtime_posix.c -o prog -lm */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "zl_runtime.h"

static FILE *zl_saida;

static void zl_poe_char(char c) { fputc(c, zl_saida); }
static void zl_poe_texto(const char *s) { fputs(s, zl_saida); }
#define ZL_T(texto) texto

#include "zl_runtime_comum.inc"

void zl_imprime_int(int32_t v) { zl_poe_i32(v); }
void zl_imprime_flut(float v) { zl_poe_flut(v); }
void zl_imprime_duplo(double v) { zl_poe_duplo(v); }
void zl_imprime_bool(bool v) { zl_poe_texto(v ? "VER" : "FAL"); }
void zl_imprime_nl(void) { zl_poe_char('\n'); }

void zl_imprime_texto(const char *s)
{
    if (s != NULL)
        zl_poe_texto(s);
}

void zl_erro_execucao(int32_t codigo, int32_t linha)
{
    fflush(stdout);
    /* solta antes o que o programa ja escreveu */
    zl_saida = stderr;
    zl_poe_mensagem_erro(codigo, linha);
    exit(100 + (int)codigo);
}

int32_t zl_le_int(void)
{
    long long v;
    /* numero que nao cabe em inteira tambem e erro de leitura */
    if (scanf("%lld", &v) != 1 || v < INT32_MIN || v > INT32_MAX)
        zl_erro_execucao(ZL_ERRO_LEITURA, 0);
    return (int32_t)v;
}

float zl_le_flut(void)
{
    float v;
    if (scanf("%f", &v) != 1)
        zl_erro_execucao(ZL_ERRO_LEITURA, 0);
    return v;
}

double zl_le_duplo(void)
{
    double v;
    if (scanf("%lf", &v) != 1)
        zl_erro_execucao(ZL_ERRO_LEITURA, 0);
    return v;
}

int32_t zl_pot_int(int32_t base, int32_t expo) { return zl_pot_int_comum(base, expo); }
float zl_pot_flut(float base, float expo) { return powf(base, expo); }
double zl_pot_duplo(double base, double expo) { return pow(base, expo); }
float zl_raiz_flut(float x) { return sqrtf(x); }
double zl_raiz_duplo(double x) { return sqrt(x); }

bool zl_texto_igual(const char *a, const char *b) { return zl_texto_igual_comum(a, b); }

/* no PC nao tem pino: guarda o ultimo valor escrito */
static int32_t zl_pinos[20];

void zl_pinomodo(int32_t pino, int32_t modo)
{
    (void)pino;
    (void)modo;
}

void zl_pinoescreve(int32_t pino, int32_t valor)
{
    if (pino >= 0 && pino < 20)
        zl_pinos[pino] = (valor != 0);
}

int32_t zl_pinole(int32_t pino)
{
    return (pino >= 0 && pino < 20) ? zl_pinos[pino] : 0;
}

void zl_espera(int32_t ms) { (void)ms; }

int main(void)
{
    zl_saida = stdout;
    int32_t r = zl_principal();
    fflush(stdout);
    return (int)r;
}
