define i32 @multiple_predecessor_phi(
    i1 %condition,
    i32 %x,
    i32 %y) {

entry:
  br i1 %condition, label %left, label %right

left:
  %left_value = add i32 %x, 1
  br label %merge

right:
  %right_value = sub i32 %y, 1
  br label %merge

merge:
  %value = phi i32 [ %left_value, %left ],
                   [ %right_value, %right ]
  ret i32 %value
}