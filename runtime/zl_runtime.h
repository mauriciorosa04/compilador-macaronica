/* biblioteca de execucao da Macaronica: a unica ponte entre o codigo
   gerado e a plataforma. Assinatura fixa e tipos de largura fixa.
     zl_runtime_posix.c   PC
     zl_runtime_uno.c     Arduino Uno */
#ifndef ZL_RUNTIME_H
#define ZL_RUNTIME_H

#include <stdint.h>
#include <stdbool.h>

#define ZL_ERRO_INDICE   1
#define ZL_ERRO_DIV_ZERO 2
#define ZL_ERRO_LEITURA  3

/* quem gera e o compilador. O main fica na biblioteca */
int32_t zl_principal(void);

void zl_imprime_int(int32_t v);
void zl_imprime_flut(float v);
void zl_imprime_bool(bool v);
void zl_imprime_texto(const char *s);
void zl_imprime_nl(void);

int32_t zl_le_int(void);
float zl_le_flut(void);

/* mostra a mensagem com a linha e nao volta. No PC sai com 100 + codigo */
void zl_erro_execucao(int32_t codigo, int32_t linha);

int32_t zl_pot_int(int32_t base, int32_t expo);
float zl_pot_flut(float base, float expo);
float zl_raiz_flut(float x);

bool zl_texto_igual(const char *a, const char *b);

void zl_pinomodo(int32_t pino, int32_t modo); /* 0 entrada, 1 saida, 2 pull-up */
void zl_pinoescreve(int32_t pino, int32_t valor);
int32_t zl_pinole(int32_t pino);
void zl_espera(int32_t ms);

/* o avr-gcc do Uno nao tem double de 64 bits */
#ifndef ZL_SEM_DUPLO
void zl_imprime_duplo(double v);
double zl_le_duplo(void);
double zl_pot_duplo(double base, double expo);
double zl_raiz_duplo(double x);
#endif

#endif
