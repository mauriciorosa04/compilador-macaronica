import csv
import statistics
import sys
from collections import defaultdict

pasta = sys.argv[1] if len(sys.argv) > 1 else 'docs/resultados'

tempos = defaultdict(list)
valores = {}
with open(pasta + '/medidas.csv', newline='') as f:
    for linha in csv.DictReader(f):
        chave = (linha['config'], linha['nucleo'], int(linha['N']), int(linha['threads']))
        tempos[chave].append(int(linha['ns']) / 1e6)
        valores[chave] = linha['resultado']

resumo = {}
for chave, ms in tempos.items():
    resumo[chave] = (statistics.median(ms), min(ms), max(ms), len(ms))

with open(pasta + '/resumo.csv', 'w', newline='') as f:
    w = csv.writer(f)
    w.writerow(['config', 'nucleo', 'N', 'threads', 'mediana_ms', 'min_ms', 'max_ms', 'reps', 'resultado'])
    for chave in sorted(resumo):
        med, mn, mx, n = resumo[chave]
        w.writerow(list(chave) + ['%.4f' % med, '%.4f' % mn, '%.4f' % mx, n, valores[chave]])

NUCLEOS = ['k1_i32', 'k1_f32', 'k2_i32', 'k2_f32']
NOME = {'k1_i32': 'c = a + b (inteira)', 'k1_f32': 'c = a + b (flut)',
        'k2_i32': 'somavet (inteira)', 'k2_f32': 'somavet (flut)'}
SEQ = ['C0', 'C1', 'C1r', 'C2']
TAMANHOS = sorted({c[2] for c in resumo})
THREADS = sorted({c[3] for c in resumo if c[0] == 'C3'})

def ms(config, nucleo, n, th=1):
    p = resumo.get((config, nucleo, n, th))
    return p[0] if p else None

def celula(v):
    return '-' if v is None else ('%.3f' % v if v < 10 else '%.1f' % v)

linhas = []
for n in TAMANHOS:
    linhas.append('### N = %d\n' % n)
    linhas.append('Mediana do tempo de uma chamada do nucleo, em milissegundos.\n')
    cab = ['nucleo'] + SEQ + ['C3 %dt' % t for t in THREADS]
    linhas.append('| ' + ' | '.join(cab) + ' |')
    linhas.append('|' + '---|' * len(cab))
    for k in NUCLEOS:
        cel = [NOME[k]] + [celula(ms(c, k, n)) for c in SEQ] + \
              [celula(ms('C3', k, n, t)) for t in THREADS]
        linhas.append('| ' + ' | '.join(cel) + ' |')
    linhas.append('')
    linhas.append('Ganho sobre a C0 (quantas vezes mais rapido).\n')
    linhas.append('| ' + ' | '.join(cab) + ' |')
    linhas.append('|' + '---|' * len(cab))
    for k in NUCLEOS:
        base = ms('C0', k, n)
        pontos = [ms(c, k, n) for c in SEQ] + [ms('C3', k, n, t) for t in THREADS]
        cel = [NOME[k]] + ['-' if (p is None or base is None) else '%.1fx' % (base / p) for p in pontos]
        linhas.append('| ' + ' | '.join(cel) + ' |')
    linhas.append('')

linhas.append('### Valor devolvido pelo somavet de flut\n')
linhas.append('Cada elemento vale 0.1, entao a soma certa e N / 10.\n')
linhas.append('| N | certo | C0 | C1 | C1r | C2 | ' + ' | '.join('C3 %dt' % t for t in THREADS) + ' |')
linhas.append('|' + '---|' * (6 + len(THREADS)))
for n in TAMANHOS:
    cel = [str(n), str(n // 10)] + [valores.get((c, 'k2_f32', n, 1), '-') for c in SEQ] + \
          [valores.get(('C3', 'k2_f32', n, t), '-') for t in THREADS]
    linhas.append('| ' + ' | '.join(cel) + ' |')
linhas.append('')

linhas.append('### Tempo de construcao\n')
linhas.append('Mediana por etapa, em milissegundos (compilacao.csv).\n')
etapas = defaultdict(list)
with open(pasta + '/compilacao.csv', newline='') as f:
    for linha in csv.DictReader(f):
        etapas[(linha['etapa'], linha['config'])].append(int(linha['microssegundos']) / 1000.0)
linhas.append('| etapa | configuracao | ms |')
linhas.append('|---|---|---|')
for (etapa, config), v in sorted(etapas.items()):
    linhas.append('| %s | %s | %.1f |' % (etapa, config, statistics.median(v)))
linhas.append('')

with open(pasta + '/tabela.md', 'w') as f:
    f.write('\n'.join(linhas))
print('\n'.join(linhas))

try:
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
except ImportError:
    print('matplotlib nao encontrado: os graficos nao foram gerados')
    sys.exit(0)

n = TAMANHOS[-1]
configs = ['C1', 'C1r', 'C2'] + [('C3', t) for t in THREADS]
rotulos = ['C1', 'C1r', 'C2'] + ['C3 %dt' % t for t in THREADS]
fig, ax = plt.subplots(figsize=(10, 4.5))
largura = 0.8 / len(configs)
for j, c in enumerate(configs):
    ganhos = []
    for k in NUCLEOS:
        base = ms('C0', k, n)
        t = ms(c[0], k, n, c[1]) if isinstance(c, tuple) else ms(c, k, n)
        ganhos.append(base / t if (base and t) else 0)
    ax.bar([i + j * largura for i in range(len(NUCLEOS))], ganhos, largura, label=rotulos[j])
ax.set_xticks([i + largura * (len(configs) - 1) / 2 for i in range(len(NUCLEOS))])
ax.set_xticklabels([NOME[k] for k in NUCLEOS])
ax.set_ylabel('vezes mais rapido que a C0')
ax.set_yscale('log')
ax.set_title('Ganho de cada configuracao sobre a C0 (N = %d)' % n)
ax.legend(ncol=4, fontsize=8)
ax.grid(axis='y', alpha=0.3)
fig.tight_layout()
fig.savefig(pasta + '/ganho_sobre_c0.png', dpi=130)

fig, ax = plt.subplots(figsize=(6.5, 4.5))
for k in NUCLEOS:
    um = ms('C3', k, n, THREADS[0])
    ax.plot(THREADS, [um / ms('C3', k, n, t) for t in THREADS], marker='o', label=NOME[k])
ax.plot(THREADS, THREADS, linestyle='--', color='gray', label='ideal')
ax.set_xscale('log', base=2)
ax.set_yscale('log', base=2)
ax.set_xticks(THREADS)
ax.set_xticklabels([str(t) for t in THREADS])
ax.set_yticks(THREADS)
ax.set_yticklabels([str(t) for t in THREADS])
ax.set_xlabel('threads')
ax.set_ylabel('ganho sobre 1 thread')
ax.set_title('C3 (OpenMP): ganho por numero de threads (N = %d)' % n)
ax.legend(fontsize=8)
ax.grid(alpha=0.3)
fig.tight_layout()
fig.savefig(pasta + '/threads.png', dpi=130)
print('graficos gravados em', pasta)
