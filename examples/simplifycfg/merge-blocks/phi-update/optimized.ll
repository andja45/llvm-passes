; ModuleID = '/home/ana/Desktop/llvm-passes/examples/simplifycfg/merge-blocks/phi-update/original.ll'
source_filename = "/home/ana/Desktop/llvm-passes/examples/simplifycfg/merge-blocks/phi-update/original.ll"

define i32 @merge_with_phi(i1 %condition, i32 %x) {
entry:
  br i1 %condition, label %prepare, label %fallback

prepare:                                          ; preds = %entry
  %incremented = add i32 %x, 1
  %doubled = mul i32 %incremented, 2
  br label %exit

fallback:                                         ; preds = %entry
  br label %exit

exit:                                             ; preds = %fallback, %prepare
  %result = phi i32 [ %doubled, %prepare ], [ 0, %fallback ]
  ret i32 %result
}
