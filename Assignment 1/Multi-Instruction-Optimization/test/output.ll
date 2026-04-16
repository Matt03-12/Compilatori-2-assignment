; ModuleID = 'test/test.ll'
source_filename = "test/test.ll"

define i32 @add_sub(i32 %b) {
entry:
  ret i32 %b
}

define i32 @add_sub_comm(i32 %b) {
entry:
  ret i32 %b
}

define i32 @sub_add(i32 %b) {
entry:
  ret i32 %b
}

define i32 @sub_add_comm(i32 %b) {
entry:
  ret i32 %b
}

define i32 @mul_div(i32 %b) {
entry:
  ret i32 %b
}

define i32 @mul_div_comm(i32 %b) {
entry:
  ret i32 %b
}

define i32 @mul_div_zero(i32 %b) {
entry:
  %a = mul i32 %b, 0
  %c = sdiv i32 %a, 0
  ret i32 %c
}

define i32 @div_mul(i32 %b) {
entry:
  %a = sdiv i32 %b, 2
  %c = mul i32 %a, 2
  ret i32 %c
}
