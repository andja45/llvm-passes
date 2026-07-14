define i32 @multiple_successors(i1 %condition, i32 %x) {
entry:
  br i1 %condition, label %positive, label %negative

positive:
  %incremented = add i32 %x, 1
  ret i32 %incremented

negative:
  %decremented = sub i32 %x, 1
  ret i32 %decremented
}