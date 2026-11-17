module {
  func.func @nuc_soma_i32_1024(%arg0: memref<1024xi32>, %arg1: memref<1024xi32>, %arg2: memref<1024xi32>) attributes {passthrough = ["nounwind"]} {
    %c0 = arith.constant 0 : index
    %c1024 = arith.constant 1024 : index
    %c1 = arith.constant 1 : index
    scf.parallel (%arg3) = (%c0) to (%c1024) step (%c1) {
      %0 = memref.load %arg0[%arg3] : memref<1024xi32>
      %1 = memref.load %arg1[%arg3] : memref<1024xi32>
      %2 = arith.addi %0, %1 : i32
      memref.store %2, %arg2[%arg3] : memref<1024xi32>
      scf.reduce 
    }
    return
  }
}

