define i32 @add_sub(i32 %b) {
entry:
  %a = add i32 %b, 5
  %c = sub i32 %a, 5
  ret i32 %c
}


define i32 @sub_add(i32 %b) {
entry:
  %a = sub i32 %b, 3
  %c = add i32 %a, 3
  ret i32 %c
}


define i32 @mul_div(i32 %b) {
entry:
  %a = mul nsw i32 %b, 4
  %c = sdiv i32 %a, 4
  ret i32 %c
}


define i32 @div_mul(i32 %b) {
entry:
  %a = sdiv i32 %b, 2
  %c = mul i32 %a, 2
  ret i32 %c
}
