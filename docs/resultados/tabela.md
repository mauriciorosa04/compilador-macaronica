### N = 1000000

Mediana do tempo de uma chamada do nucleo, em milissegundos.

| nucleo | C0 | C1 | C1r | C2 | C3 1t | C3 2t | C3 4t | C3 8t |
|---|---|---|---|---|---|---|---|---|
| c = a + b (inteira) | 0.822 | 0.194 | 0.175 | 0.178 | 0.162 | 0.103 | 0.059 | 0.050 |
| c = a + b (flut) | 0.783 | 0.158 | 0.178 | 0.167 | 0.188 | 0.101 | 0.087 | 0.048 |
| somavet (inteira) | 0.839 | 0.029 | 0.030 | 0.042 | 0.035 | 0.018 | 0.009 | 0.011 |
| somavet (flut) | 4.370 | 0.606 | 0.033 | 0.079 | 0.644 | 0.308 | 0.163 | 0.084 |

Ganho sobre a C0 (quantas vezes mais rapido).

| nucleo | C0 | C1 | C1r | C2 | C3 1t | C3 2t | C3 4t | C3 8t |
|---|---|---|---|---|---|---|---|---|
| c = a + b (inteira) | 1.0x | 4.2x | 4.7x | 4.6x | 5.1x | 8.0x | 13.9x | 16.4x |
| c = a + b (flut) | 1.0x | 4.9x | 4.4x | 4.7x | 4.2x | 7.8x | 9.0x | 16.1x |
| somavet (inteira) | 1.0x | 28.6x | 28.4x | 20.0x | 23.7x | 47.0x | 92.6x | 75.2x |
| somavet (flut) | 1.0x | 7.2x | 132.7x | 55.0x | 6.8x | 14.2x | 26.8x | 52.1x |

### N = 10000000

Mediana do tempo de uma chamada do nucleo, em milissegundos.

| nucleo | C0 | C1 | C1r | C2 | C3 1t | C3 2t | C3 4t | C3 8t |
|---|---|---|---|---|---|---|---|---|
| c = a + b (inteira) | 8.984 | 4.554 | 4.469 | 3.829 | 4.769 | 3.242 | 2.913 | 3.069 |
| c = a + b (flut) | 8.570 | 3.985 | 4.845 | 3.753 | 4.738 | 3.241 | 2.818 | 3.002 |
| somavet (inteira) | 9.304 | 1.442 | 1.194 | 0.855 | 1.205 | 0.835 | 0.670 | 0.632 |
| somavet (flut) | 47.2 | 6.064 | 0.865 | 0.931 | 6.784 | 3.342 | 1.709 | 0.947 |

Ganho sobre a C0 (quantas vezes mais rapido).

| nucleo | C0 | C1 | C1r | C2 | C3 1t | C3 2t | C3 4t | C3 8t |
|---|---|---|---|---|---|---|---|---|
| c = a + b (inteira) | 1.0x | 2.0x | 2.0x | 2.3x | 1.9x | 2.8x | 3.1x | 2.9x |
| c = a + b (flut) | 1.0x | 2.2x | 1.8x | 2.3x | 1.8x | 2.6x | 3.0x | 2.9x |
| somavet (inteira) | 1.0x | 6.5x | 7.8x | 10.9x | 7.7x | 11.1x | 13.9x | 14.7x |
| somavet (flut) | 1.0x | 7.8x | 54.6x | 50.7x | 7.0x | 14.1x | 27.6x | 49.9x |

### Valor devolvido pelo somavet de flut

Cada elemento vale 0.1, entao a soma certa e N / 10.

| N | certo | C0 | C1 | C1r | C2 | C3 1t | C3 2t | C3 4t | C3 8t |
|---|---|---|---|---|---|---|---|---|---|
| 1000000 | 100000 | 100958.344 | 100958.344 | 99985 | 99910.3203 | 100958.344 | 100354.195 | 99759.8516 | 99910.3281 |
| 10000000 | 1000000 | 1087937 | 1087937 | 998501.375 | 1010791.88 | 1087937 | 956549.562 | 975599.125 | 1010791.75 |

### Tempo de construcao

Mediana por etapa, em milissegundos (compilacao.csv).

| etapa | configuracao | ms |
|---|---|---|
| clang | C0 | 30.0 |
| clang | C1 | 101.4 |
| clang | C1r | 92.3 |
| clang | C2 | 68.8 |
| clang | C3 | 59.8 |
| macaronica | todas | 13.0 |
| pipeline | escalar | 41.9 |
| pipeline | escalar-reordena | 39.8 |
| pipeline | omp | 38.4 |
| pipeline | vet | 40.5 |
