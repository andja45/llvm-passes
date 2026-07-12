; ModuleID = '/home/ana/Desktop/llvm-passes/examples/simplifycfg/merge-blocks/simple/original.ll'
source_filename = "/home/ana/Desktop/llvm-passes/examples/simplifycfg/merge-blocks/simple/original.ll"

define i32 @merge_simple(i32 %x) {
entry:
  %positive = icmp sgt i32 %x, 0
  br i1 %positive, label %prepare, label %return_zero

prepare:                                          ; preds = %entry
  %prepared = add i32 %x, 1
  %processed = mul i32 %prepared, 2
  %result = sub i32 %processed, 3
  ret i32 %result

return_zero:                                      ; preds = %entry
  ret i32 0
}
