define i32 @multiple_predecessors(i1 %first,
                                  i1 %second,
                                  i32 %x) {
entry:
  br i1 %first, label %choose, label %other

choose:
  br i1 %second, label %left, label %right

left:
  %left_value = add i32 %x, 1
  br label %skip

right:
  %right_value = sub i32 %x, 1
  br label %skip

skip:
  br label %process

other:
  %other_value = mul i32 %x, 2
  br label %process

process:
  %result = phi i32 [ 10, %skip ],
                    [ %other_value, %other ]
  %final = add i32 %result, 100
  br label %finish

finish:
  ret i32 %final
}