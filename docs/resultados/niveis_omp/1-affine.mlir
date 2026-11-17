// Nucleos vetoriais gerados pelo compilador da Macaronica. Fonte: build/bench/niveis.mac
// Dialetos: func, arith, memref e affine. Desce ate LLVM IR com scripts/pipeline.sh.

// c[i] = a[i] + b[i]
func.func @nuc_soma_i32_1024(%a: memref<1024xi32>, %b: memref<1024xi32>, %c: memref<1024xi32>) attributes { passthrough = ["nounwind"] } {
  affine.for %i = 0 to 1024 {
    %x = affine.load %a[%i] : memref<1024xi32>
    %y = affine.load %b[%i] : memref<1024xi32>
    %z = arith.addi %x, %y : i32
    affine.store %z, %c[%i] : memref<1024xi32>
  }
  return
}

