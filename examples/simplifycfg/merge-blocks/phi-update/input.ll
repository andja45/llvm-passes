define i32 @merge_with_phi(i1 %condition, i32 %x) {
entry:
  br i1 %condition, label %prepare, label %fallback

prepare:
  %incremented = add i32 %x, 1
  br label %process

process:
  %selected = phi i32 [ %incremented, %prepare ]
  %doubled = mul i32 %selected, 2
  br label %exit

fallback:
  %fallback_value = sub i32 %x, 1
  br label %exit

exit:
  %result = phi i32 [ %doubled, %process ],
                    [ %fallback_value, %fallback ]
  ret i32 %result
}