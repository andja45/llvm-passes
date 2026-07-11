define i32 @unreachable_cycle(i32 %x) {
entry:
  br label %live

live:
  ret i32 %x

dead1:
  br label %dead2

dead2:
  br label %dead1
}