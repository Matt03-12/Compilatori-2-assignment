define i32 @add_x_0(i32 %x) {
  %tmp = add i32 %x, 0
  ret i32 %tmp
}

define i32 @mul_x_1(i32 %x) {
  %tmp = mul i32 %x, 1
  ret i32 %tmp
}

define i32 @sub_x_0(i32 %x) {
entry:
  %a = sub i32 %x, 0
  ret i32 %a
}

define i32 @shl_x_0(i32 %x) {
entry:
  %a = shl i32 %x, 0
  ret i32 %a
}

define i32 @or_0_x(i32 %x) {
entry:
  %a = or i32 0, %x
  ret i32 %a
}

define i32 @xor_0_x(i32 %x) {
entry:
  %a = xor i32 0, %x
  ret i32 %a
}

define i32 @sub_0_x(i32 %x) {
entry:
  %a = sub i32 0, %x
  ret i32 %a
}

define i32 @shl_0_x(i32 %x) {
entry:
  %a = shl i32 0, %x
  ret i32 %a
}

; side effects
declare i32 @foo()

define i32 @side_effect() {
  %call = call i32 @foo()
  %tmp = add i32 %call, 0
  ret i32 %tmp
}
