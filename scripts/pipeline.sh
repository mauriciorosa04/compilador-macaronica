#!/bin/bash
# pipeline.sh - desce os nucleos vetoriais de MLIR ate LLVM IR (MLIR/LLVM 18).
#
#   bash scripts/pipeline.sh ENTRADA.mlir SAIDA.ll [escalar|vet|omp]
#
#   escalar (padrao)  so rebaixa. O .ll sai portavel: sem alvo, indice i32,
#                     nounwind. E o modo usado no make test e na placa.
#   vet               vetoriza no MLIR antes de rebaixar (vetores de 8)
#   omp               paraleliza no MLIR e gera chamadas ao OpenMP
#
# Se a variavel NIVEIS tiver o nome de uma pasta, o script guarda la o mesmo
# nucleo em cada nivel de abstracao por que ele passa:
#   1-affine.mlir   como o compilador gerou (lacos affine)
#   2-scf.mlir      depois do lower-affine (lacos scf, acessos memref)
#   3-cf.mlir       depois do convert-scf-to-cf (blocos e desvios)
#   4-llvm.mlir     tudo no dialeto llvm (ainda MLIR)
#   5.ll            LLVM IR de verdade
#
# Os nomes dos passos foram conferidos no mlir-opt-18 (18.1.3). Cuidado: as
# opcoes de UM passo vao na mesma string separadas por ESPACO; com virgula
# o mlir-opt da erro.
set -e -o pipefail   # sem pipefail um erro do mlir-opt viraria um .ll vazio

ENT="$1"
SAI="$2"
MODO="${3:-escalar}"
if [ -z "$ENT" ] || [ -z "$SAI" ]; then
    echo "uso: bash scripts/pipeline.sh ENTRADA.mlir SAIDA.ll [escalar|vet|omp]" >&2
    exit 2
fi

# no Ubuntu as ferramentas vem com o numero da versao no nome
acha() { if command -v "$1-18" > /dev/null 2>&1; then echo "$1-18"; else echo "$1"; fi; }
MLIR_OPT=$(acha mlir-opt)
MLIR_TRANSLATE=$(acha mlir-translate)

# O tipo "index" do MLIR (contador de laco, indice de vetor) nao tem tamanho
# proprio: cada passo de conversao decide. No modo escalar ele vira i32, que
# e o que deixa o .ll igual ao resto do programa e compilavel pro AVR. Os
# passos de vetor e de OpenMP so sabem trabalhar com i64, entao nesses dois
# modos fica o padrao deles.
if [ "$MODO" = escalar ]; then IDX=" index-bitwidth=32"; else IDX=""; fi

ANTES=()   # transformacoes feitas ainda em cima dos lacos affine
case "$MODO" in
escalar) ;;
vet)
    # Vetoriza os lacos affine: o laco passa a andar de 8 em 8 e o corpo
    # trabalha com vector<8xT>. vectorize-reductions e obrigatorio pras
    # reducoes; sem ele somavet e prodesc ficam escalares, sem aviso nenhum.
    ANTES+=( "--affine-super-vectorize=virtual-vector-size=8 vectorize-reductions=true" )
    ;;
omp)
    # Marca os lacos affine que podem rodar em paralelo (affine.parallel).
    # parallel-reductions e o que deixa a reducao entrar tambem.
    ANTES+=( "--affine-parallelize=max-nested=1 parallel-reductions=1" )
    ;;
*)
    echo "modo desconhecido: $MODO" >&2
    exit 2
    ;;
esac

DESCE=()
# (1) affine.for / affine.load / affine.store -> scf.for + memref.load/store
DESCE+=( "--lower-affine" )
if [ "$MODO" = omp ]; then
    # scf.parallel -> regiao paralela do OpenMP (omp.parallel + omp.wsloop).
    # O canonicalize limpa o que sobra, senao fica um salva e restaura de
    # pilha dentro do laco.
    DESCE+=( "--convert-scf-to-openmp" "--canonicalize" )
fi
if [ "$MODO" = vet ]; then
    # leitura e escrita de vetor em varias dimensoes -> lacos de 1 dimensao
    DESCE+=( "--convert-vector-to-scf" )
fi
# (2) laco estruturado (scf.for) -> blocos basicos com desvio
DESCE+=( "--convert-scf-to-cf" )
if [ "$MODO" = vet ]; then
    # dialeto vector -> dialeto llvm (vira llvm.masked.load, vector.reduce...).
    # Tem que vir antes do convert-func-to-llvm.
    DESCE+=( "--convert-vector-to-llvm" )
fi
# (3) contas do arith -> dialeto llvm
DESCE+=( "--convert-arith-to-llvm=${IDX# }" )
# (4) memref.load/store -> getelementptr + load/store
DESCE+=( "--finalize-memref-to-llvm=${IDX# }" )
# (5) func.func -> funcao do dialeto llvm. use-bare-ptr-memref-call-conv faz
#     cada memref<NxT> virar UM ptr no argumento (sem isso viram 5 argumentos
#     por vetor). So funciona porque o tamanho e fixo.
DESCE+=( "--convert-func-to-llvm=use-bare-ptr-memref-call-conv=1$IDX" )
if [ "$MODO" = omp ]; then
    # dialeto omp -> chamadas a biblioteca do OpenMP (__kmpc_fork_call...)
    DESCE+=( "--convert-openmp-to-llvm" )
fi
# (6) tira as conversoes de tipo provisorias que os passos deixam entre si
DESCE+=( "--reconcile-unrealized-casts" )

# passo sem opcao nao pode ficar com "=" pendurado
limpa() {
    local saida=()
    for p in "$@"; do saida+=( "${p%=}" ); done
    printf '%s\n' "${saida[@]}"
}
mapfile -t DESCE < <(limpa "${DESCE[@]}")

if [ -n "$NIVEIS" ]; then
    mkdir -p "$NIVEIS"
    cp "$ENT" "$NIVEIS/1-affine.mlir"
    "$MLIR_OPT" "$ENT" "${ANTES[@]}" --lower-affine -o "$NIVEIS/2-scf.mlir"
    "$MLIR_OPT" "$ENT" "${ANTES[@]}" --lower-affine --convert-scf-to-cf -o "$NIVEIS/3-cf.mlir"
    "$MLIR_OPT" "$ENT" "${ANTES[@]}" "${DESCE[@]}" -o "$NIVEIS/4-llvm.mlir"
fi

# (7) dialeto llvm (ainda MLIR) -> LLVM IR em texto
"$MLIR_OPT" "$ENT" "${ANTES[@]}" "${DESCE[@]}" | "$MLIR_TRANSLATE" --mlir-to-llvmir -o "$SAI"

if [ -n "$NIVEIS" ]; then cp "$SAI" "$NIVEIS/5.ll"; fi
