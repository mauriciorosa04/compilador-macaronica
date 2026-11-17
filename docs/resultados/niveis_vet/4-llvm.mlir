module {
  llvm.func @nuc_soma_i32_1024(%arg0: !llvm.ptr, %arg1: !llvm.ptr, %arg2: !llvm.ptr) attributes {passthrough = ["nounwind"]} {
    %0 = llvm.mlir.undef : !llvm.struct<(ptr, ptr, i64, array<1 x i64>, array<1 x i64>)>
    %1 = llvm.insertvalue %arg0, %0[0] : !llvm.struct<(ptr, ptr, i64, array<1 x i64>, array<1 x i64>)> 
    %2 = llvm.insertvalue %arg0, %1[1] : !llvm.struct<(ptr, ptr, i64, array<1 x i64>, array<1 x i64>)> 
    %3 = llvm.mlir.constant(0 : index) : i64
    %4 = llvm.insertvalue %3, %2[2] : !llvm.struct<(ptr, ptr, i64, array<1 x i64>, array<1 x i64>)> 
    %5 = llvm.mlir.constant(1024 : index) : i64
    %6 = llvm.insertvalue %5, %4[3, 0] : !llvm.struct<(ptr, ptr, i64, array<1 x i64>, array<1 x i64>)> 
    %7 = llvm.mlir.constant(1 : index) : i64
    %8 = llvm.insertvalue %7, %6[4, 0] : !llvm.struct<(ptr, ptr, i64, array<1 x i64>, array<1 x i64>)> 
    %9 = llvm.mlir.undef : !llvm.struct<(ptr, ptr, i64, array<1 x i64>, array<1 x i64>)>
    %10 = llvm.insertvalue %arg1, %9[0] : !llvm.struct<(ptr, ptr, i64, array<1 x i64>, array<1 x i64>)> 
    %11 = llvm.insertvalue %arg1, %10[1] : !llvm.struct<(ptr, ptr, i64, array<1 x i64>, array<1 x i64>)> 
    %12 = llvm.mlir.constant(0 : index) : i64
    %13 = llvm.insertvalue %12, %11[2] : !llvm.struct<(ptr, ptr, i64, array<1 x i64>, array<1 x i64>)> 
    %14 = llvm.mlir.constant(1024 : index) : i64
    %15 = llvm.insertvalue %14, %13[3, 0] : !llvm.struct<(ptr, ptr, i64, array<1 x i64>, array<1 x i64>)> 
    %16 = llvm.mlir.constant(1 : index) : i64
    %17 = llvm.insertvalue %16, %15[4, 0] : !llvm.struct<(ptr, ptr, i64, array<1 x i64>, array<1 x i64>)> 
    %18 = llvm.mlir.undef : !llvm.struct<(ptr, ptr, i64, array<1 x i64>, array<1 x i64>)>
    %19 = llvm.insertvalue %arg2, %18[0] : !llvm.struct<(ptr, ptr, i64, array<1 x i64>, array<1 x i64>)> 
    %20 = llvm.insertvalue %arg2, %19[1] : !llvm.struct<(ptr, ptr, i64, array<1 x i64>, array<1 x i64>)> 
    %21 = llvm.mlir.constant(0 : index) : i64
    %22 = llvm.insertvalue %21, %20[2] : !llvm.struct<(ptr, ptr, i64, array<1 x i64>, array<1 x i64>)> 
    %23 = llvm.mlir.constant(1024 : index) : i64
    %24 = llvm.insertvalue %23, %22[3, 0] : !llvm.struct<(ptr, ptr, i64, array<1 x i64>, array<1 x i64>)> 
    %25 = llvm.mlir.constant(1 : index) : i64
    %26 = llvm.insertvalue %25, %24[4, 0] : !llvm.struct<(ptr, ptr, i64, array<1 x i64>, array<1 x i64>)> 
    %27 = llvm.mlir.constant(0 : i32) : i32
    %28 = llvm.mlir.constant(0 : index) : i64
    %29 = llvm.mlir.constant(1024 : index) : i64
    %30 = llvm.mlir.constant(8 : index) : i64
    llvm.br ^bb1(%28 : i64)
  ^bb1(%31: i64):  // 2 preds: ^bb0, ^bb2
    %32 = llvm.icmp "slt" %31, %29 : i64
    llvm.cond_br %32, ^bb2, ^bb3
  ^bb2:  // pred: ^bb1
    %33 = llvm.mlir.constant(0 : index) : i64
    %34 = llvm.mlir.constant(1024 : index) : i64
    %35 = llvm.sub %34, %31  : i64
    %36 = llvm.mlir.constant(dense<[0, 1, 2, 3, 4, 5, 6, 7]> : vector<8xi32>) : vector<8xi32>
    %37 = llvm.trunc %35 : i64 to i32
    %38 = llvm.mlir.undef : vector<8xi32>
    %39 = llvm.mlir.constant(0 : i32) : i32
    %40 = llvm.insertelement %37, %38[%39 : i32] : vector<8xi32>
    %41 = llvm.shufflevector %40, %38 [0, 0, 0, 0, 0, 0, 0, 0] : vector<8xi32> 
    %42 = llvm.icmp "sgt" %41, %36 : vector<8xi32>
    %43 = llvm.mlir.constant(dense<0> : vector<8xi32>) : vector<8xi32>
    %44 = llvm.extractvalue %8[1] : !llvm.struct<(ptr, ptr, i64, array<1 x i64>, array<1 x i64>)> 
    %45 = llvm.getelementptr %44[%31] : (!llvm.ptr, i64) -> !llvm.ptr, i32
    %46 = llvm.intr.masked.load %45, %42, %43 {alignment = 4 : i32} : (!llvm.ptr, vector<8xi1>, vector<8xi32>) -> vector<8xi32>
    %47 = llvm.mlir.constant(0 : index) : i64
    %48 = llvm.mlir.constant(1024 : index) : i64
    %49 = llvm.sub %48, %31  : i64
    %50 = llvm.mlir.constant(dense<[0, 1, 2, 3, 4, 5, 6, 7]> : vector<8xi32>) : vector<8xi32>
    %51 = llvm.trunc %49 : i64 to i32
    %52 = llvm.mlir.undef : vector<8xi32>
    %53 = llvm.mlir.constant(0 : i32) : i32
    %54 = llvm.insertelement %51, %52[%53 : i32] : vector<8xi32>
    %55 = llvm.shufflevector %54, %52 [0, 0, 0, 0, 0, 0, 0, 0] : vector<8xi32> 
    %56 = llvm.icmp "sgt" %55, %50 : vector<8xi32>
    %57 = llvm.mlir.constant(dense<0> : vector<8xi32>) : vector<8xi32>
    %58 = llvm.extractvalue %17[1] : !llvm.struct<(ptr, ptr, i64, array<1 x i64>, array<1 x i64>)> 
    %59 = llvm.getelementptr %58[%31] : (!llvm.ptr, i64) -> !llvm.ptr, i32
    %60 = llvm.intr.masked.load %59, %56, %57 {alignment = 4 : i32} : (!llvm.ptr, vector<8xi1>, vector<8xi32>) -> vector<8xi32>
    %61 = llvm.add %46, %60  : vector<8xi32>
    %62 = llvm.mlir.constant(0 : index) : i64
    %63 = llvm.mlir.constant(1024 : index) : i64
    %64 = llvm.sub %63, %31  : i64
    %65 = llvm.mlir.constant(dense<[0, 1, 2, 3, 4, 5, 6, 7]> : vector<8xi32>) : vector<8xi32>
    %66 = llvm.trunc %64 : i64 to i32
    %67 = llvm.mlir.undef : vector<8xi32>
    %68 = llvm.mlir.constant(0 : i32) : i32
    %69 = llvm.insertelement %66, %67[%68 : i32] : vector<8xi32>
    %70 = llvm.shufflevector %69, %67 [0, 0, 0, 0, 0, 0, 0, 0] : vector<8xi32> 
    %71 = llvm.icmp "sgt" %70, %65 : vector<8xi32>
    %72 = llvm.extractvalue %26[1] : !llvm.struct<(ptr, ptr, i64, array<1 x i64>, array<1 x i64>)> 
    %73 = llvm.getelementptr %72[%31] : (!llvm.ptr, i64) -> !llvm.ptr, i32
    llvm.intr.masked.store %61, %73, %71 {alignment = 4 : i32} : vector<8xi32>, vector<8xi1> into !llvm.ptr
    %74 = llvm.add %31, %30  : i64
    llvm.br ^bb1(%74 : i64)
  ^bb3:  // pred: ^bb1
    llvm.return
  }
}

