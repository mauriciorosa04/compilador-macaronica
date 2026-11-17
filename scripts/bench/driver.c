#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

void K1_I32(int32_t *a, int32_t *b, int32_t *c);
void K1_F32(float *a, float *b, float *c);
int32_t K2_I32(int32_t *a);
float K2_F32(float *a);

static int64_t agora_ns(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (int64_t)t.tv_sec * 1000000000LL + t.tv_nsec;
}

static void *aloca(size_t bytes)
{
    size_t tam = (bytes + 63) & ~(size_t)63;
    void *p = aligned_alloc(64, tam);
    if (p == NULL)
    {
        fprintf(stderr, "sem memoria\n");
        exit(2);
    }
    memset(p, 0, tam);
    return p;
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        fprintf(stderr, "uso: %s <k1_i32|k1_f32|k2_i32|k2_f32> <repeticoes>\n", argv[0]);
        return 2;
    }
    const char *nuc = argv[1];
    int reps = atoi(argv[2]);
    long n = N_ELEM;
    int real = strstr(nuc, "f32") != NULL;
    int k1 = strncmp(nuc, "k1", 2) == 0;

    int32_t *ai = aloca(n * sizeof(int32_t)), *bi = aloca(n * sizeof(int32_t)),
            *ci = aloca(n * sizeof(int32_t));
    float *af = aloca(n * sizeof(float)), *bf = aloca(n * sizeof(float)),
          *cf = aloca(n * sizeof(float));
    for (long i = 0; i < n; i++)
    {
        ai[i] = (int32_t)(i % 100);
        bi[i] = (int32_t)(i % 7);
        af[i] = 0.1f;
        bf[i] = (float)(i % 7) * 0.5f;
    }

    int aquecimento = 0, r = 0;
    int64_t inicio = agora_ns();
    while (r <= reps)
    {
        double res = 0.0;
        int64_t t0 = agora_ns();
        if (k1 && !real)
            K1_I32(ai, bi, ci);
        else if (k1)
            K1_F32(af, bf, cf);
        else if (!real)
            res = (double)K2_I32(ai);
        else
            res = (double)K2_F32(af);
        int64_t t1 = agora_ns();
        if (r == 0)
        {
            aquecimento++;
            if ((aquecimento >= 2 && agora_ns() - inicio >= 200000000LL) || aquecimento >= 500)
                r = 1;
            continue;
        }
        if (k1)
        {
            double s = 0.0;
            for (long i = 0; i < n; i++)
                s += real ? (double)cf[i] : (double)ci[i];
            res = s;
        }
        printf("%s,%ld,%d,%lld,%.9g\n", nuc, n, r, (long long)(t1 - t0), res);
        r++;
    }
    return 0;
}
