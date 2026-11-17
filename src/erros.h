/* reporte de erro das tres fases. Nunca aborta no primeiro: conta e segue */
#ifndef ERROS_H
#define ERROS_H

void erro_lexico(int linha, int coluna, const char *fmt, ...);
void erro_sintatico(int linha, int coluna, const char *fmt, ...);
void erro_semantico(int linha, int coluna, const char *fmt, ...);
void aviso_semantico(int linha, int coluna, const char *fmt, ...);

/* usado no --tokens, que varre o fonte duas vezes */
void erros_silenciar(int silencioso);

int total_erros_lexicos(void);
int total_erros_sintaticos(void);
int total_erros_semanticos(void);
int total_erros(void);

#endif
