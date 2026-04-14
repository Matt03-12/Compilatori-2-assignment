; ModuleID = 'test/test.ll'
source_filename = "test/test.ll"

define i32 @add_x_0(i32 %x) {
  ret i32 %x
}

define i32 @add_0_x(i32 %x) {
  ret i32 %x
}

define i32 @mul_x_1(i32 %x) {
  ret i32 %x
}

define i32 @mul_1_x(i32 %x) {
  ret i32 %x
}

define i64 @add_i64(i64 %x) {
  ret i64 %x
}

define i32 @combo(i32 %x) {
  ret i32 %x
}

define float @fadd_x_0(float %x) {
  %tmp = fadd float %x, 0.000000e+00
  ret float %tmp
}

define float @nan_case(float %x) {
  %tmp = fadd float %x, 0.000000e+00
  ret float %tmp
}

declare i32 @foo()

define i32 @side_effect() {
  %call = call i32 @foo()
  ret i32 %call
}
