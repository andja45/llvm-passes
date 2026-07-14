define i32 @unreachable_phi(i1 %condition, i32 %x) {
entry:
  br i1 %condition, label %positive, label %negative

positive:
  %positive_value = add i32 %x, 1
  br label %finish

negative:
  %negative_value = sub i32 %x, 1
  br label %finish

dead:
  %dead_value = mul i32 %x, 10
  br label %finish

finish:
  %result = phi i32
      [ %positive_value, %positive ],
      [ %negative_value, %negative ],
      [ %dead_value, %dead ]

  %final = add i32 %result, 100
  ret i32 %final
}