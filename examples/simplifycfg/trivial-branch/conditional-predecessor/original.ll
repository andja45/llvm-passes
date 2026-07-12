define i32 @conditional_predecessor(i1 %condition, i32 %x) {
entry:
  br i1 %condition, label %prepare, label %alternative

prepare:
  %prepared = add i32 %x, 2
  br label %skip

skip:
  br label %process

alternative:
  %alternative_value = sub i32 %x, 1
  br label %process

process:
  %value = phi i32 [ %prepared, %skip ],
                   [ %alternative_value, %alternative ]
  %result = mul i32 %value, 2
  br label %finish

finish:
  ret i32 %result
}