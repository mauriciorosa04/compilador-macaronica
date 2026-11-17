/* biblioteca de execucao do Arduino Uno (ATmega328P), sem Arduino.h.
   Serial em 115200 8N1. Quando a principal volta o processador para. */
#ifndef F_CPU
#define F_CPU 16000000UL
#endif
#ifndef ZL_BAUD
#define ZL_BAUD 115200UL
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>
#include <math.h>

#define ZL_SEM_DUPLO
#include "zl_runtime.h"

static void zl_serial_inicia(void)
{
    /* modo U2X: em 115200 com 16 MHz o erro de velocidade fica menor */
    uint16_t ubrr = (uint16_t)((F_CPU + 4UL * ZL_BAUD) / (8UL * ZL_BAUD) - 1UL);
    UCSR0A = (1 << U2X0);
    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)(ubrr & 0xFF);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
    UCSR0B = (1 << TXEN0) | (1 << RXEN0);
}

static uint8_t zl_meio_da_linha;

static void zl_poe_char(char c)
{
    while (!(UCSR0A & (1 << UDRE0)))
        ;
    UDR0 = (uint8_t)c;
    zl_meio_da_linha = (c != '\n');
}

static void zl_poe_texto(const char *s)
{
    while (*s != '\0')
        zl_poe_char(*s++);
}
#define ZL_T(texto) texto

static uint8_t zl_serial_recebe(void)
{
    while (!(UCSR0A & (1 << RXC0)))
        ;
    return UDR0;
}

#include "zl_runtime_comum.inc"

/* cli + sleep: ninguem acorda mais. No simavr e isso que encerra */
static void zl_para(void) __attribute__((noreturn));
static void zl_para(void)
{
    if (zl_meio_da_linha)
        zl_poe_char('\n');
    _delay_ms(3);
    cli();
    set_sleep_mode(SLEEP_MODE_IDLE);
    sleep_enable();
    for (;;)
        sleep_cpu();
}

void zl_imprime_int(int32_t v) { zl_poe_i32(v); }
void zl_imprime_flut(float v) { zl_poe_flut(v); }
void zl_imprime_bool(bool v) { zl_poe_texto(v ? "VER" : "FAL"); }
void zl_imprime_nl(void) { zl_poe_char('\n'); }

void zl_imprime_texto(const char *s)
{
    if (s != 0)
        zl_poe_texto(s);
}

/* le sinal, digitos e a parte depois do ponto. Para no primeiro
   caractere que nao e de numero. mag guarda o inteiro sem sinal */
static float zl_le_numero(uint8_t aceita_ponto, int32_t *inteiro, uint8_t *estourou)
{
    uint32_t mag = 0;
    float alto = 0.0f, frac = 0.0f, peso = 0.1f, r;
    uint8_t negativo = 0, achou = 0, no_ponto = 0, passou = 0;
    for (;;)
    {
        uint8_t c = zl_serial_recebe();
        if (c == '-' && !achou)
            negativo = 1;
        else if (c >= '0' && c <= '9')
        {
            uint8_t d = (uint8_t)(c - '0');
            if (no_ponto)
            {
                frac += (float)d * peso;
                peso *= 0.1f;
            }
            else
            {
                alto = alto * 10.0f + (float)d;
                if (mag > (0xFFFFFFFFUL - d) / 10)
                    passou = 1;
                else
                    mag = mag * 10 + d;
            }
            achou = 1;
        }
        else if (c == '.' && aceita_ponto && !no_ponto)
            no_ponto = 1;
        else if (achou)
            break;
    }
    r = (passou ? alto : (float)mag) + frac;
    *estourou = passou || mag > (negativo ? 2147483648UL : 2147483647UL);
    *inteiro = negativo ? (int32_t)(0UL - mag) : (int32_t)mag;
    return negativo ? 0.0f - r : r;
}

int32_t zl_le_int(void)
{
    int32_t v;
    uint8_t estourou;
    zl_le_numero(0, &v, &estourou);
    if (estourou)
        zl_erro_execucao(ZL_ERRO_LEITURA, 0);
    return v;
}

float zl_le_flut(void)
{
    int32_t v;
    uint8_t estourou;
    return zl_le_numero(1, &v, &estourou);
}

/* mensagem na serial e o LED do pino 13 pisca 5 vezes e fica aceso */
void zl_erro_execucao(int32_t codigo, int32_t linha)
{
    uint8_t i;
    if (zl_meio_da_linha)
        zl_poe_char('\n');
    zl_poe_mensagem_erro(codigo, linha);
    DDRB |= (1 << DDB5);
    for (i = 0; i < 5; i++)
    {
        PORTB |= (1 << PORTB5);
        _delay_ms(100);
        PORTB &= (uint8_t)~(1 << PORTB5);
        _delay_ms(100);
    }
    PORTB |= (1 << PORTB5);
    zl_para();
}

int32_t zl_pot_int(int32_t base, int32_t expo) { return zl_pot_int_comum(base, expo); }
float zl_pot_flut(float base, float expo) { return (float)pow(base, expo); }
float zl_raiz_flut(float x) { return (float)sqrt(x); }

bool zl_texto_igual(const char *a, const char *b) { return zl_texto_igual_comum(a, b); }

/* numero impresso na placa: 0 a 13 digitais, 14 a 19 sao A0 a A5 */
static uint8_t zl_pino(int32_t pino, volatile uint8_t **ddr,
                       volatile uint8_t **port, volatile uint8_t **pin)
{
    if (pino >= 0 && pino <= 7)
    {
        *ddr = &DDRD; *port = &PORTD; *pin = &PIND;
        return (uint8_t)(1 << pino);
    }
    if (pino >= 8 && pino <= 13)
    {
        *ddr = &DDRB; *port = &PORTB; *pin = &PINB;
        return (uint8_t)(1 << (pino - 8));
    }
    if (pino >= 14 && pino <= 19)
    {
        *ddr = &DDRC; *port = &PORTC; *pin = &PINC;
        return (uint8_t)(1 << (pino - 14));
    }
    return 0;
}

void zl_pinomodo(int32_t pino, int32_t modo)
{
    volatile uint8_t *ddr, *port, *pin;
    uint8_t bit = zl_pino(pino, &ddr, &port, &pin);
    if (bit == 0)
        return;
    if (modo == 1)
        *ddr |= bit;
    else
    {
        *ddr &= (uint8_t)~bit;
        if (modo == 2)
            *port |= bit;
        else
            *port &= (uint8_t)~bit;
    }
}

void zl_pinoescreve(int32_t pino, int32_t valor)
{
    volatile uint8_t *ddr, *port, *pin;
    uint8_t bit = zl_pino(pino, &ddr, &port, &pin);
    if (bit == 0)
        return;
    if (valor != 0)
        *port |= bit;
    else
        *port &= (uint8_t)~bit;
}

int32_t zl_pinole(int32_t pino)
{
    volatile uint8_t *ddr, *port, *pin;
    uint8_t bit = zl_pino(pino, &ddr, &port, &pin);
    if (bit == 0)
        return 0;
    return (*pin & bit) ? 1 : 0;
}

void zl_espera(int32_t ms)
{
    while (ms > 0)
    {
        _delay_ms(1);
        ms--;
    }
}

int main(void)
{
    zl_serial_inicia();
    zl_principal();
    zl_para();
}
