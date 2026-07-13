; ModuleID = '/home/ana/Desktop/llvm-passes/examples/simplifycfg/unreachable/simple/original.ll'
source_filename = "/home/ana/Desktop/llvm-passes/examples/simplifycfg/unreachable/simple/original.ll"

define i32 @unreachable_simple(i1 %condition, i32 %x) {
entry:
  br i1 %condition, label %positive, label %negative

positive:                                         ; preds = %entry
  %positive_value = add i32 %x, 1
  br label %finish

negative:                                         ; preds = %entry
  %negative_value = sub i32 %x, 1
  br label %finish

finish:                                           ; preds = %negative, %positive
  %result = phi i32 [ %positive_value, %positive ], [ %negative_value, %negative ]
  ret i32 %result
}
