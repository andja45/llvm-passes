define i32 @multiple_single_predecessor_phis(
    i1 %condition,
    i32 %x,
    i32 %y) {

entry:
  br i1 %condition, label %prepare, label %fallback

prepare:
  %prepared_x = add i32 %x, 1
  %prepared_y = sub i32 %y, 1
  br i1 %condition, label %process, label %fallback

process:
  %first = phi i32 [ %prepared_x, %prepare ]
  %second = phi i32 [ %prepared_y, %prepare ]
  %sum = add i32 %first, %second
  br label %finish

fallback:
  %fallback_result = add i32 %x, %y
  br label %finish

finish:
  %final = phi i32 [ %sum, %process ],
                   [ %fallback_result, %fallback ]
  ret i32 %final
}