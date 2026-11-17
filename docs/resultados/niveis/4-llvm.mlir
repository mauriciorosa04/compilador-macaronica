module {
  llvm.func @nuc_soma_i32_1024(%arg0: !llvm.ptr, %arg1: !llvm.ptr, %arg2: !llvm.ptr) attributes {passthrough = ["nounwind"]} {
    %0 = llvm.mlir.undef : !llvm.struct<(ptr, ptr, i32, array<1 x i32>, array<1 x i32>)>
    %1 = llvm.insertvalue %arg0, %0[0] : !llvm.struct<(ptr, ptr, i32, array<1 x i32>, array<1 x i32>)> 
    %2 = llvm.insertvalue %arg0, %1[1] : !llvm.struct<(ptr, ptr, i32, array<1 x i32>, array<1 x i32>)> 
    %3 = llvm.mlir.constant(0 : index) : i32
    %4 = llvm.insertvalue %3, %2[2] : !llvm.struct<(ptr, ptr, i32, array<1 x i32>, array<1 x i32>)> 
    %5 = llvm.mlir.constant(1024 : index) : i32
    %6 = llvm.insertvalue %5, %4[3, 0] : !llvm.struct<(ptr, ptr, i32, array<1 x i32>, array<1 x i32>)> 
    %7 = llvm.mlir.constant(1 : index) : i32
    %8 = llvm.insertvalue %7, %6[4, 0] : !llvm.struct<(ptr, ptr, i32, array<1 x i32>, array<1 x i32>)> 
    %9 = llvm.mlir.undef : !llvm.struct<(ptr, ptr, i32, array<1 x i32>, array<1 x i32>)>
    %10 = llvm.insertvalue %arg1, %9[0] : !llvm.struct<(ptr, ptr, i32, array<1 x i32>, array<1 x i32>)> 
    %11 = llvm.insertvalue %arg1, %10[1] : !llvm.struct<(ptr, ptr, i32, array<1 x i32>, array<1 x i32>)> 
    %12 = llvm.mlir.constant(0 : index) : i32
    %13 = llvm.insertvalue %12, %11[2] : !llvm.struct<(ptr, ptr, i32, array<1 x i32>, array<1 x i32>)> 
    %14 = llvm.mlir.constant(1024 : index) : i32
    %15 = llvm.insertvalue %14, %13[3, 0] : !llvm.struct<(ptr, ptr, i32, array<1 x i32>, array<1 x i32>)> 
    %16 = llvm.mlir.constant(1 : index) : i32
    %17 = llvm.insertvalue %16, %15[4, 0] : !llvm.struct<(ptr, ptr, i32, array<1 x i32>, array<1 x i32>)> 
    %18 = llvm.mlir.undef : !llvm.struct<(ptr, ptr, i32, array<1 x i32>, array<1 x i32>)>
    %19 = llvm.insertvalue %arg2, %18[0] : !llvm.struct<(ptr, ptr, i32, array<1 x i32>, array<1 x i32>)> 
    %20 = llvm.insertvalue %arg2, %19[1] : !llvm.struct<(ptr, ptr, i32, array<1 x i32>, array<1 x i32>)> 
    %21 = llvm.mlir.constant(0 : index) : i32
    %22 = llvm.insertvalue %21, %20[2] : !llvm.struct<(ptr, ptr, i32, array<1 x i32>, array<1 x i32>)> 
    %23 = llvm.mlir.constant(1024 : index) : i32
    %24 = llvm.insertvalue %23, %22[3, 0] : !llvm.struct<(ptr, ptr, i32, array<1 x i32>, array<1 x i32>)> 
    %25 = llvm.mlir.constant(1 : index) : i32
    %26 = llvm.insertvalue %25, %24[4, 0] : !llvm.struct<(ptr, ptr, i32, array<1 x i32>, array<1 x i32>)> 
    %27 = llvm.mlir.constant(0 : index) : i32
    %28 = llvm.mlir.constant(1024 : index) : i32
    %29 = llvm.mlir.constant(1 : index) : i32
    llvm.br ^bb1(%27 : i32)
  ^bb1(%30: i32):  // 2 preds: ^bb0, ^bb2
    %31 = llvm.icmp "slt" %30, %28 : i32
    llvm.cond_br %31, ^bb2, ^bb3
  ^bb2:  // pred: ^bb1
    %32 = llvm.extractvalue %8[1] : !llvm.struct<(ptr, ptr, i32, array<1 x i32>, array<1 x i32>)> 
    %33 = llvm.getelementptr %32[%30] : (!llvm.ptr, i32) -> !llvm.ptr, i32
    %34 = llvm.load %33 : !llvm.ptr -> i32
    %35 = llvm.extractvalue %17[1] : !llvm.struct<(ptr, ptr, i32, array<1 x i32>, array<1 x i32>)> 
    %36 = llvm.getelementptr %35[%30] : (!llvm.ptr, i32) -> !llvm.ptr, i32
    %37 = llvm.load %36 : !llvm.ptr -> i32
    %38 = llvm.add %34, %37  : i32
    %39 = llvm.extractvalue %26[1] : !llvm.struct<(ptr, ptr, i32, array<1 x i32>, array<1 x i32>)> 
    %40 = llvm.getelementptr %39[%30] : (!llvm.ptr, i32) -> !llvm.ptr, i32
    llvm.store %38, %40 : i32, !llvm.ptr
    %41 = llvm.add %30, %29  : i32
    llvm.br ^bb1(%41 : i32)
  ^bb3:  // pred: ^bb1
    llvm.return
  }
}

