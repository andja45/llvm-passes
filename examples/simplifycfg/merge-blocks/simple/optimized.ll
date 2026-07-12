; ModuleID = '/home/ana/Desktop/llvm-passes/examples/simplifycfg/merge-blocks/simple/original.ll'
source_filename = "/home/ana/Desktop/llvm-passes/examples/simplifycfg/merge-blocks/simple/original.ll"

define i32 @merge_simple(i32 %x) {
entry:
  %positive = icmp sgt i32 %x, 0
  br i1 %positive, label %prepare, label %return_zero

prepare:                                          ; preds = %entry
  %incremented = add i32 %x, 1
  %doubled = mul i32 %incremented, 2
  ret i32 %doubled

return_zero:                                      ; preds = %entry
  ret i32 0
}
