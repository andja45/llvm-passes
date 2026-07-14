define i32 @unreachable_cycle(i1 %condition, i32 %x) {
entry:
  br i1 %condition, label %positive, label %negative

positive:
  %positive_value = add i32 %x, 1
  br label %finish

negative:
  %negative_value = sub i32 %x, 1
  br label %finish

dead1:
  %dead_value = mul i32 %x, 2
  br label %dead2

dead2:
  %dead_next = add i32 %dead_value, 5
  br label %dead1

finish:
  %result = phi i32 [ %positive_value, %positive ],
                    [ %negative_value, %negative ]
  ret i32 %result
}