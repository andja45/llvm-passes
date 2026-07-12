; ModuleID = '/home/ana/Desktop/llvm-passes/examples/simplifycfg/trivial-branch/conditional-predecessor/original.ll'
source_filename = "/home/ana/Desktop/llvm-passes/examples/simplifycfg/trivial-branch/conditional-predecessor/original.ll"

define i32 @conditional_predecessor(i1 %condition, i32 %x) {
entry:
  br i1 %condition, label %prepare, label %alternative

prepare:                                          ; preds = %entry
  %prepared = add i32 %x, 2
  br label %process

alternative:                                      ; preds = %entry
  %alternative_value = sub i32 %x, 1
  br label %process

process:                                          ; preds = %alternative, %prepare
  %value = phi i32 [ %prepared, %prepare ], [ %alternative_value, %alternative ]
  %result = mul i32 %value, 2
  ret i32 %result
}
