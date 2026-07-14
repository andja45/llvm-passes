; ModuleID = '/home/ana/Desktop/llvm-passes/examples/simplifycfg/single-predecessor-phi/single-phi/original.ll'
source_filename = "/home/ana/Desktop/llvm-passes/examples/simplifycfg/single-predecessor-phi/single-phi/original.ll"

define i32 @single_predecessor_phi(i1 %condition, i32 %x) {
entry:
  br i1 %condition, label %prepare, label %fallback

prepare:                                          ; preds = %entry
  %prepared = add i32 %x, 2
  br i1 %condition, label %process, label %fallback

process:                                          ; preds = %prepare
  %result = mul i32 %prepared, 2
  br label %finish

fallback:                                         ; preds = %prepare, %entry
  %fallback_value = sub i32 %x, 1
  br label %finish

finish:                                           ; preds = %fallback, %process
  %final = phi i32 [ %result, %process ], [ %fallback_value, %fallback ]
  ret i32 %final
}
