module {
  func.func @nuc_soma_i32_1024(%arg0: memref<1024xi32>, %arg1: memref<1024xi32>, %arg2: memref<1024xi32>) attributes {passthrough = ["nounwind"]} {
    %c0 = arith.constant 0 : index
    %c1024 = arith.constant 1024 : index
    %c8 = arith.constant 8 : index
    cf.br ^bb1(%c0 : index)
  ^bb1(%0: index):  // 2 preds: ^bb0, ^bb2
    %1 = arith.cmpi slt, %0, %c1024 : index
    cf.cond_br %1, ^bb2, ^bb3
  ^bb2:  // pred: ^bb1
    %c0_i32 = arith.constant 0 : i32
    %2 = vector.transfer_read %arg0[%0], %c0_i32 : memref<1024xi32>, vector<8xi32>
    %c0_i32_0 = arith.constant 0 : i32
    %3 = vector.transfer_read %arg1[%0], %c0_i32_0 : memref<1024xi32>, vector<8xi32>
    %4 = arith.addi %2, %3 : vector<8xi32>
    vector.transfer_write %4, %arg2[%0] : vector<8xi32>, memref<1024xi32>
    %5 = arith.addi %0, %c8 : index
    cf.br ^bb1(%5 : index)
  ^bb3:  // pred: ^bb1
    return
  }
}

