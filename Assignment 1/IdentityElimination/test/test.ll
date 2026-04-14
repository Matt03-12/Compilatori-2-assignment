define i32 @add_x_0(i32 %x) {
  %tmp = add i32 %x, 0
  ret i32 %tmp
}

define i32 @add_0_x(i32 %x) {
  %tmp = add i32 0, %x
  ret i32 %tmp
}

define i32 @mul_x_1(i32 %x) {
  %tmp = mul i32 %x, 1
  ret i32 %tmp
}

define i32 @mul_1_x(i32 %x) {
  %tmp = mul i32 1, %x
  ret i32 %tmp
}

define i64 @add_i64(i64 %x) {
  %tmp = add i64 %x, 0
  ret i64 %tmp
}


define i32 @combo(i32 %x) {
  %a = add i32 %x, 0
  %b = mul i32 %a, 1
  ret i32 %b
}


define float @fadd_x_0(float %x) {
  %tmp = fadd float %x, 0.0
  ret float %tmp
}


define float @nan_case(float %x) {
  %tmp = fadd float %x, 0.0
  ret float %tmp
}

; side effects
declare i32 @foo()

define i32 @side_effect() {
  %call = call i32 @foo()
  %tmp = add i32 %call, 0
  ret i32 %tmp
}
