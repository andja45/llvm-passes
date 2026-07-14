; ModuleID = '/home/ana/Desktop/llvm-passes/examples/simplifycfg/single-predecessor-phi/multiple-phis/original.ll'
source_filename = "/home/ana/Desktop/llvm-passes/examples/simplifycfg/single-predecessor-phi/multiple-phis/original.ll"

define i32 @multiple_single_predecessor_phis(i1 %condition, i32 %x, i32 %y) {
entry:
  br i1 %condition, label %prepare, label %fallback

prepare:                                          ; preds = %entry
  %prepared_x = add i32 %x, 1
  %prepared_y = sub i32 %y, 1
  br i1 %condition, label %process, label %fallback

process:                                          ; preds = %prepare
  %sum = add i32 %prepared_x, %prepared_y
  br label %finish

fallback:                                         ; preds = %prepare, %entry
  %fallback_result = add i32 %x, %y
  br label %finish

finish:                                           ; preds = %fallback, %process
  %final = phi i32 [ %sum, %process ], [ %fallback_result, %fallback ]
  ret i32 %final
}
