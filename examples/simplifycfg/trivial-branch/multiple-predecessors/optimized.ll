; ModuleID = '/home/ana/Desktop/llvm-passes/examples/simplifycfg/trivial-branch/multiple-predecessors/original.ll'
source_filename = "/home/ana/Desktop/llvm-passes/examples/simplifycfg/trivial-branch/multiple-predecessors/original.ll"

define i32 @multiple_predecessors(i1 %first, i1 %second, i32 %x) {
entry:
  br i1 %first, label %choose, label %other

choose:                                           ; preds = %entry
  br i1 %second, label %left, label %right

left:                                             ; preds = %choose
  %left_value = add i32 %x, 1
  br label %process

right:                                            ; preds = %choose
  %right_value = sub i32 %x, 1
  br label %process

other:                                            ; preds = %entry
  %other_value = mul i32 %x, 2
  br label %process

process:                                          ; preds = %left, %right, %other
  %result = phi i32 [ %other_value, %other ], [ 10, %right ], [ 10, %left ]
  %final = add i32 %result, 100
  ret i32 %final
}
