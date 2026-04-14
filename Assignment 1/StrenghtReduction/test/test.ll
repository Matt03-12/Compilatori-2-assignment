define i32 @mul_15_x(i32 %x) {
  %tmp = mul i32 %x, 15
  ret i32 %tmp
}


define i32 @div_x_8(i32 %x) {
  %tmp = sdiv i32 %x, 8
  ret i32 %tmp
}


define i32 @combo(i32 %x) {
  %a = mul i32 %x, 15
  %b = sdiv i32 %a, 8
  ret i32 %b
}

define i32 @div_x_7(i32 %x) {
  %tmp = sdiv i32 %x, 7
  ret i32 %tmp
}

define i32 @mul_14_x(i32 %x) {
  %tmp = mul i32 %x, 14
  ret i32 %tmp
}


define i32 @div_negative(i32 %x) {
  %tmp = sdiv i32 %x, 8
  ret i32 %tmp
}
