module {
  func.func @nuc_soma_i32_1024(%arg0: memref<1024xi32>, %arg1: memref<1024xi32>, %arg2: memref<1024xi32>) attributes {passthrough = ["nounwind"]} {
    %c0 = arith.constant 0 : index
    %c1024 = arith.constant 1024 : index
    %c8 = arith.constant 8 : index
    scf.for %arg3 = %c0 to %c1024 step %c8 {
      %c0_i32 = arith.constant 0 : i32
      %0 = vector.transfer_read %arg0[%arg3], %c0_i32 : memref<1024xi32>, vector<8xi32>
      %c0_i32_0 = arith.constant 0 : i32
      %1 = vector.transfer_read %arg1[%arg3], %c0_i32_0 : memref<1024xi32>, vector<8xi32>
      %2 = arith.addi %0, %1 : vector<8xi32>
      vector.transfer_write %2, %arg2[%arg3] : vector<8xi32>, memref<1024xi32>
    }
    return
  }
}

