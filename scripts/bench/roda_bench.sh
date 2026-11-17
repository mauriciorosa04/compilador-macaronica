#!/bin/bash
# estudo de otimizacao (make bench). Configuracoes:
#   C0  escalar, -O0          C1  escalar, -O2 -march=native
#   C1r C1 com --reordena     C2  vetorizado no MLIR
#   C3  OpenMP, 1 2 4 8 threads
# Saida em docs/resultados/
set -e -o pipefail
BIN="${1:-./bin/macaronica}"
REPS="${2:-7}"
RAIZ="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$RAIZ"
RES=docs/resultados
T=build/bench
rm -rf "$T"
mkdir -p "$T" "$RES"

acha() { if command -v "$1-18" > /dev/null 2>&1; then echo "$1-18"; else echo "$1"; fi; }
CLANG=$(acha clang)
OPT=$(acha opt)
ALVO="-O2 -march=native"

echo "config,nucleo,N,threads,rep,ns,resultado" > "$RES/medidas.csv"
echo "etapa,config,N,microssegundos" > "$RES/compilacao.csv"

cron() {
    local etapa=$1 config=$2 n=$3
    shift 3
    local t0 t1
    t0=$(date +%s%N)
    "$@"
    t1=$(date +%s%N)
    echo "$etapa,$config,$n,$(( (t1 - t0) / 1000 ))" >> "$RES/compilacao.csv"
}

mede() {
    local config=$1 n=$2 th=$3
    for k in k1_i32 k1_f32 k2_i32 k2_f32; do
        OMP_NUM_THREADS=$th "$T/bench_${config}_$n" $k "$REPS" |
            awk -F, -v c="$config" -v t="$th" '{ print c "," $1 "," $2 "," t "," $3 "," $4 "," $5 }' \
            >> "$RES/medidas.csv"
    done
}

{
    echo "maquina: $(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2 | sed 's/^ //')"
    echo "nucleos logicos: $(nproc)"
    echo "extensoes: $(grep -m1 flags /proc/cpuinfo | tr ' ' '\n' | grep -E '^(sse2|avx|avx2|avx512f)$' | tr '\n' ' ')"
    echo "$($CLANG --version | head -1)"
    echo "$($(acha mlir-opt) --version | grep -m1 version | sed 's/^ *//')"
    echo "repeticoes por ponto: $REPS (o valor usado e a mediana)"
} > "$RES/maquina.txt"
cat "$RES/maquina.txt"

for N in 1000000 10000000; do
    echo "== N = $N"
    cat > "$T/bench_$N.mac" <<FIM
inteira vet{$N} ai, bi, ci;
flut vet{$N} af, bf, cf;

inteira principal() @
[
    ci receba ai + bi;
    cf receba af + bf;
    escreva(somavet(ai));
    escreva(somavet(af));
    respost 0;
]
FIM
    cron macaronica todas $N "$BIN" "$T/bench_$N.mac" -o "$T/normal_$N.ll"
    "$BIN" --reordena "$T/bench_$N.mac" -o "$T/reordena_$N.ll"

    cron pipeline escalar $N bash scripts/pipeline.sh "$T/normal_$N.mlir" "$T/esc_$N.ll" escalar
    cron pipeline escalar-reordena $N bash scripts/pipeline.sh "$T/reordena_$N.mlir" "$T/escr_$N.ll" escalar
    cron pipeline vet $N bash scripts/pipeline.sh "$T/normal_$N.mlir" "$T/vet_$N.ll" vet
    cron pipeline omp $N bash scripts/pipeline.sh "$T/normal_$N.mlir" "$T/omp_$N.ll" omp

    NOMES="-DN_ELEM=$N -DK1_I32=nuc_soma_i32_$N -DK1_F32=nuc_soma_f32_$N -DK2_I32=nuc_somavet_i32_$N -DK2_F32=nuc_somavet_f32_$N"
    $CLANG -O2 $NOMES -c scripts/bench/driver.c -o "$T/driver_$N.o"

    constroi() {
        local config=$1 ll=$2 flags=$3
        shift 3
        cron clang "$config" $N $CLANG -Wno-override-module $flags -c "$ll" -o "$T/nuc_${config}_$N.o"
        $CLANG "$T/driver_$N.o" "$T/nuc_${config}_$N.o" "$@" -o "$T/bench_${config}_$N"
    }
    constroi C0 "$T/esc_$N.ll" "-O0"
    constroi C1 "$T/esc_$N.ll" "$ALVO"
    constroi C1r "$T/escr_$N.ll" "$ALVO"
    constroi C2 "$T/vet_$N.ll" "$ALVO"
    constroi C3 "$T/omp_$N.ll" "$ALVO" -fopenmp=libomp

    for c in C0 C1 C1r C2; do mede $c $N 1; done
    for th in 1 2 4 8; do mede C3 $N $th; done
done

N=1000000
$CLANG -Wno-override-module $ALVO -Rpass=loop-vectorize -Rpass-missed=loop-vectorize \
    -Rpass-analysis=loop-vectorize -c "$T/esc_$N.ll" -o /dev/null 2> "$RES/rpass_C1.txt" || true
$CLANG -Wno-override-module $ALVO -Rpass=loop-vectorize -Rpass-missed=loop-vectorize \
    -Rpass-analysis=loop-vectorize -c "$T/escr_$N.ll" -o /dev/null 2> "$RES/rpass_C1r.txt" || true

$CLANG -Wno-override-module $ALVO -ffast-math -Rpass=loop-vectorize -Rpass-missed=loop-vectorize     -Rpass-analysis=loop-vectorize -c "$T/esc_$N.ll" -o /dev/null 2> "$RES/rpass_C1_ffast_math.txt" || true
echo "chamadas llvm.masked.* no IR da C2 (N = $N): $(grep -c 'call.*llvm.masked' "$T/vet_$N.ll" || true)" > "$RES/mascaras_C2.txt"

$OPT -O2 -S "$T/esc_$N.ll" -o "$T/o2_sem_alvo.ll"
$OPT -O2 -mtriple=x86_64-pc-linux-gnu -mcpu=native -S "$T/esc_$N.ll" -o "$T/o2_com_alvo.ll"
$OPT -O2 -mtriple=x86_64-pc-linux-gnu -mcpu=native -S "$T/escr_$N.ll" -o "$T/o2_reordena.ll"
conta() { grep -c -E "<[0-9]+ x $2>" "$1" || true; }
{
    echo "linhas com tipo de vetor no IR dos nucleos (N = $N), depois do opt -O2"
    echo "sem alvo (sem -mtriple):        i32 $(conta "$T/o2_sem_alvo.ll" i32)   float $(conta "$T/o2_sem_alvo.ll" float)"
    echo "com alvo (-mcpu=native):        i32 $(conta "$T/o2_com_alvo.ll" i32)   float $(conta "$T/o2_com_alvo.ll" float)"
    echo "com alvo e --reordena:          i32 $(conta "$T/o2_reordena.ll" i32)   float $(conta "$T/o2_reordena.ll" float)"
    echo
    echo "largura usada: $(grep -o -E '<[0-9]+ x (i32|float)>' "$T/o2_reordena.ll" | sort | uniq -c | tr '\n' ';')"
} > "$RES/vetores_no_ir.txt"
sed -n "/^define float @nuc_somavet_f32_$N/,/^}/p" "$T/o2_com_alvo.ll" > "$RES/ir_somavet_f32_C1.ll"
sed -n "/^define float @nuc_somavet_f32_$N/,/^}/p" "$T/o2_reordena.ll" > "$RES/ir_somavet_f32_C1r.ll"
sed -n "/^define i32 @nuc_somavet_i32_$N/,/^}/p" "$T/o2_com_alvo.ll" > "$RES/ir_somavet_i32_C1.ll"

cat > "$T/niveis.mac" <<FIM
inteira principal() @
[
    inteira vet{1024} a, b, c;
    c receba a + b;
    respost 0;
]
FIM
"$BIN" "$T/niveis.mac" -o "$T/niveis.ll"
NIVEIS="$RES/niveis" bash scripts/pipeline.sh "$T/niveis.mlir" "$T/niveis.nuc.ll" escalar
NIVEIS="$RES/niveis_vet" bash scripts/pipeline.sh "$T/niveis.mlir" "$T/niveis.vet.ll" vet
NIVEIS="$RES/niveis_omp" bash scripts/pipeline.sh "$T/niveis.mlir" "$T/niveis.omp.ll" omp

python3 scripts/bench/tabela.py "$RES"
echo "resultados em $RES/"
