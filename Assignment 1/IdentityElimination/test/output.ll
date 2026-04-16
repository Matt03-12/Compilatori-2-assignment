; ModuleID = 'test/test.ll'
source_filename = "test/test.ll"

define i32 @add_x_0(i32 %x) {
  ret i32 %x
}

define i32 @mul_x_1(i32 %x) {
  ret i32 %x
}

define i32 @sub_x_0(i32 %x) {
entry:
  ret i32 %x
}

define i32 @shl_x_0(i32 %x) {
entry:
  ret i32 %x
}

define i32 @or_0_x(i32 %x) {
entry:
  ret i32 %x
}

define i32 @xor_0_x(i32 %x) {
entry:
  ret i32 %x
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

declare i32 @foo()

define i32 @side_effect() {
  %call = call i32 @foo()
  ret i32 %call
}
