define i32 @unreachable_phi() {
entry:
  br label %live

dead:
  br label %live

live:
  %result = phi i32 [ 10, %entry ], [ 20, %dead ]
  ret i32 %result
}