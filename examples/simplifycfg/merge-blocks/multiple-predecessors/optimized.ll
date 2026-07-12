; ModuleID = '/home/ana/Desktop/llvm-passes/examples/simplifycfg/merge-blocks/multiple-predecessors/original.ll'
source_filename = "/home/ana/Desktop/llvm-passes/examples/simplifycfg/merge-blocks/multiple-predecessors/original.ll"

define i32 @multiple_predecessors(i1 %condition, i32 %x) {
entry:
  br i1 %condition, label %left, label %right

left:                                             ; preds = %entry
  %left_value = add i32 %x, 1
  br label %merge

right:                                            ; preds = %entry
  %right_value = sub i32 %x, 1
  br label %merge

merge:                                            ; preds = %right, %left
  %result = phi i32 [ %left_value, %left ], [ %right_value, %right ]
  ret i32 %result
}
