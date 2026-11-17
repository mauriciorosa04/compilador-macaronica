/* parte igual nas duas plataformas: formatacao de numero e mensagem de
   erro, pra saida do PC e da placa baterem caractere a caractere.
   Quem inclui define zl_poe_char, zl_poe_texto e ZL_T antes. */
static void zl_poe_u32(uint32_t u, uint8_t minimo)
{
    char buf[10];
    uint8_t n = 0;
    do
    {
        buf[n++] = (char)('0' + (u % 10));
        u /= 10;
    } while (u != 0);
    while (n < minimo)
        buf[n++] = '0';
    while (n > 0)
        zl_poe_char(buf[--n]);
}

static void zl_poe_i32(int32_t v)
{
    uint32_t u = (uint32_t)v;
    if (v < 0)
    {
        zl_poe_char('-');
        u = 0u - u; /* funciona ate com -2147483648 */
    }
    zl_poe_u32(u, 1);
}

/* 6 casas, so com conta de float de 32 bits (e o que o Uno tem) */
static void zl_poe_flut(float v)
{
    uint32_t inteiro, frac;

    if (v != v)
    {
        zl_poe_texto(ZL_T("nan"));
        return;
    }
    if (v < 0.0f)
    {
        zl_poe_char('-');
        v = -v;
    }
    if (v > 3.4028234e38f)
    {
        zl_poe_texto(ZL_T("inf"));
        return;
    }
    if (v >= 4294967040.0f)
    {
        uint8_t expo = 0;
        while (v >= 10.0f)
        {
            v /= 10.0f;
            expo++;
        }
        inteiro = (uint32_t)v;
        frac = (uint32_t)((v - (float)inteiro) * 1000000.0f + 0.5f);
        if (frac >= 1000000UL)
        {
            frac -= 1000000UL;
            inteiro++;
        }
        if (inteiro >= 10)
        {
            inteiro = 1;
            frac = 0;
            expo++;
        }
        zl_poe_u32(inteiro, 1);
        zl_poe_char('.');
        zl_poe_u32(frac, 6);
        zl_poe_texto(ZL_T("e+"));
        zl_poe_u32(expo, 2);
        return;
    }
    inteiro = (uint32_t)v;
    frac = (uint32_t)((v - (float)inteiro) * 1000000.0f + 0.5f);
    if (frac >= 1000000UL)
    {
        frac -= 1000000UL;
        inteiro++;
    }
    zl_poe_u32(inteiro, 1);
    zl_poe_char('.');
    zl_poe_u32(frac, 6);
}

#ifndef ZL_SEM_DUPLO
static void zl_poe_u64(uint64_t u)
{
    char buf[20];
    uint8_t n = 0;
    do
    {
        buf[n++] = (char)('0' + (u % 10));
        u /= 10;
    } while (u != 0);
    while (n > 0)
        zl_poe_char(buf[--n]);
}

static void zl_poe_duplo(double v)
{
    uint64_t inteiro;
    uint32_t frac;

    if (v != v)
    {
        zl_poe_texto(ZL_T("nan"));
        return;
    }
    if (v < 0.0)
    {
        zl_poe_char('-');
        v = -v;
    }
    if (v > 1.7976931348623157e308)
    {
        zl_poe_texto(ZL_T("inf"));
        return;
    }
    if (v >= 18446744073709549568.0)
    {
        uint16_t expo = 0;
        while (v >= 10.0)
        {
            v /= 10.0;
            expo++;
        }
        inteiro = (uint64_t)v;
        frac = (uint32_t)((v - (double)inteiro) * 1000000.0 + 0.5);
        if (frac >= 1000000UL)
        {
            frac -= 1000000UL;
            inteiro++;
        }
        if (inteiro >= 10)
        {
            inteiro = 1;
            frac = 0;
            expo++;
        }
        zl_poe_u64(inteiro);
        zl_poe_char('.');
        zl_poe_u32(frac, 6);
        zl_poe_texto(ZL_T("e+"));
        zl_poe_u32(expo, 2);
        return;
    }
    inteiro = (uint64_t)v;
    frac = (uint32_t)((v - (double)inteiro) * 1000000.0 + 0.5);
    if (frac >= 1000000UL)
    {
        frac -= 1000000UL;
        inteiro++;
    }
    zl_poe_u64(inteiro);
    zl_poe_char('.');
    zl_poe_u32(frac, 6);
}
#endif

static void zl_poe_mensagem_erro(int32_t codigo, int32_t linha)
{
    zl_poe_texto(ZL_T("erro de execucao"));
    if (linha > 0)
    {
        zl_poe_texto(ZL_T(" na linha "));
        zl_poe_i32(linha);
    }
    zl_poe_texto(ZL_T(": "));
    switch (codigo)
    {
    case ZL_ERRO_INDICE:
        zl_poe_texto(ZL_T("indice fora dos limites do vetor"));
        break;
    case ZL_ERRO_DIV_ZERO:
        zl_poe_texto(ZL_T("divisao inteira por zero"));
        break;
    case ZL_ERRO_LEITURA:
        zl_poe_texto(ZL_T("leia() nao encontrou um numero na entrada"));
        break;
    default:
        zl_poe_texto(ZL_T("erro desconhecido"));
        break;
    }
    zl_poe_char('\n');
}

/* quadrados sucessivos, sem sinal pro estouro dar a volta */
static int32_t zl_pot_int_comum(int32_t base, int32_t expo)
{
    uint32_t b = (uint32_t)base, r = 1;
    if (expo < 0)
    {
        /* expoente negativo: 1 / base^n truncado */
        if (base == 1)
            return 1;
        if (base == -1)
            return (expo % 2 == 0) ? 1 : -1;
        return 0;
    }
    while (expo > 0)
    {
        if (expo & 1)
            r *= b;
        b *= b;
        expo >>= 1;
    }
    return (int32_t)r;
}

/* nulo vale texto vazio */
static bool zl_texto_igual_comum(const char *a, const char *b)
{
    if (a == 0)
        a = "";
    if (b == 0)
        b = "";
    while (*a != '\0' && *a == *b)
    {
        a++;
        b++;
    }
    return *a == *b;
}
