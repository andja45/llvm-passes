define i32 @unreachable_simple(i32 %x) {
entry:
  br label %live

live:
  ret i32 %x

dead:
  ret i32 0
}