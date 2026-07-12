define i32 @merge_simple(i32 %x) {
entry:
  %positive = icmp sgt i32 %x, 0
  br i1 %positive, label %prepare, label %return_zero

prepare:
  br label %process

process:
  %incremented = add i32 %x, 1
  %doubled = mul i32 %incremented, 2
  br label %finish

finish:
  ret i32 %doubled

return_zero:
  ret i32 0
}