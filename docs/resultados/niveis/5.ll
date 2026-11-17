; ModuleID = 'LLVMDialectModule'
source_filename = "LLVMDialectModule"

; Function Attrs: nounwind
define void @nuc_soma_i32_1024(ptr %0, ptr %1, ptr %2) #0 {
  %4 = insertvalue { ptr, ptr, i32, [1 x i32], [1 x i32] } undef, ptr %0, 0
  %5 = insertvalue { ptr, ptr, i32, [1 x i32], [1 x i32] } %4, ptr %0, 1
  %6 = insertvalue { ptr, ptr, i32, [1 x i32], [1 x i32] } %5, i32 0, 2
  %7 = insertvalue { ptr, ptr, i32, [1 x i32], [1 x i32] } %6, i32 1024, 3, 0
  %8 = insertvalue { ptr, ptr, i32, [1 x i32], [1 x i32] } %7, i32 1, 4, 0
  %9 = insertvalue { ptr, ptr, i32, [1 x i32], [1 x i32] } undef, ptr %1, 0
  %10 = insertvalue { ptr, ptr, i32, [1 x i32], [1 x i32] } %9, ptr %1, 1
  %11 = insertvalue { ptr, ptr, i32, [1 x i32], [1 x i32] } %10, i32 0, 2
  %12 = insertvalue { ptr, ptr, i32, [1 x i32], [1 x i32] } %11, i32 1024, 3, 0
  %13 = insertvalue { ptr, ptr, i32, [1 x i32], [1 x i32] } %12, i32 1, 4, 0
  %14 = insertvalue { ptr, ptr, i32, [1 x i32], [1 x i32] } undef, ptr %2, 0
  %15 = insertvalue { ptr, ptr, i32, [1 x i32], [1 x i32] } %14, ptr %2, 1
  %16 = insertvalue { ptr, ptr, i32, [1 x i32], [1 x i32] } %15, i32 0, 2
  %17 = insertvalue { ptr, ptr, i32, [1 x i32], [1 x i32] } %16, i32 1024, 3, 0
  %18 = insertvalue { ptr, ptr, i32, [1 x i32], [1 x i32] } %17, i32 1, 4, 0
  br label %19

19:                                               ; preds = %22, %3
  %20 = phi i32 [ %32, %22 ], [ 0, %3 ]
  %21 = icmp slt i32 %20, 1024
  br i1 %21, label %22, label %33

22:                                               ; preds = %19
  %23 = extractvalue { ptr, ptr, i32, [1 x i32], [1 x i32] } %8, 1
  %24 = getelementptr i32, ptr %23, i32 %20
  %25 = load i32, ptr %24, align 4
  %26 = extractvalue { ptr, ptr, i32, [1 x i32], [1 x i32] } %13, 1
  %27 = getelementptr i32, ptr %26, i32 %20
  %28 = load i32, ptr %27, align 4
  %29 = add i32 %25, %28
  %30 = extractvalue { ptr, ptr, i32, [1 x i32], [1 x i32] } %18, 1
  %31 = getelementptr i32, ptr %30, i32 %20
  store i32 %29, ptr %31, align 4
  %32 = add i32 %20, 1
  br label %19

33:                                               ; preds = %19
  ret void
}

attributes #0 = { nounwind }

!llvm.module.flags = !{!0}

!0 = !{i32 2, !"Debug Info Version", i32 3}
