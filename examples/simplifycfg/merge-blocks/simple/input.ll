define i32 @merge_simple(i32 %x) {
entry:
  %positive = icmp sgt i32 %x, 0
  br i1 %positive, label %prepare, label %return_zero

prepare:
  %prepared = add i32 %x, 1
  br label %process

process:
  %processed = mul i32 %prepared, 2
  br label %finish

finish:
  %result = sub i32 %processed, 3
  ret i32 %result

return_zero:
  ret i32 0
}