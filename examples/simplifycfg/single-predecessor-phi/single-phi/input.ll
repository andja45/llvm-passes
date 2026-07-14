define i32 @single_predecessor_phi(i1 %condition, i32 %x) {
entry:
  br i1 %condition, label %prepare, label %fallback

prepare:
  %prepared = add i32 %x, 2
  br i1 %condition, label %process, label %fallback

process:
  %value = phi i32 [ %prepared, %prepare ]
  %result = mul i32 %value, 2
  br label %finish

fallback:
  %fallback_value = sub i32 %x, 1
  br label %finish

finish:
  %final = phi i32 [ %result, %process ],
                   [ %fallback_value, %fallback ]
  ret i32 %final
}