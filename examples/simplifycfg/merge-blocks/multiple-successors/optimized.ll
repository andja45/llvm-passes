; ModuleID = '/home/ana/Desktop/llvm-passes/examples/simplifycfg/merge-blocks/multiple-successors/original.ll'
source_filename = "/home/ana/Desktop/llvm-passes/examples/simplifycfg/merge-blocks/multiple-successors/original.ll"

define i32 @multiple_successors(i1 %condition, i32 %x) {
entry:
  br i1 %condition, label %positive, label %negative

positive:                                         ; preds = %entry
  %incremented = add i32 %x, 1
  ret i32 %incremented

negative:                                         ; preds = %entry
  %decremented = sub i32 %x, 1
  ret i32 %decremented
}
