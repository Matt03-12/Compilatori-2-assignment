; ModuleID = 'test/test.ll'
source_filename = "test/test.ll"

define i32 @mul_15_x(i32 %x) {
  %shl.1 = shl i32 %x, 1
  %add.1 = add i32 %x, %shl.1
  %shl.2 = shl i32 %x, 2
  %add.2 = add i32 %add.1, %shl.2
  %shl.3 = shl i32 %x, 3
  %add.3 = add i32 %add.2, %shl.3
  ret i32 %add.3
}

define i32 @div_x_8(i32 %x) {
  %tmp = sdiv i32 %x, 8
  ret i32 %tmp
}

define i32 @combo(i32 %x) {
  %shl.1 = shl i32 %x, 1
  %add.1 = add i32 %x, %shl.1
  %shl.2 = shl i32 %x, 2
  %add.2 = add i32 %add.1, %shl.2
  %shl.3 = shl i32 %x, 3
  %add.3 = add i32 %add.2, %shl.3
  %b = sdiv i32 %add.3, 8
  ret i32 %b
}

define i32 @div_x_7(i32 %x) {
  %tmp = sdiv i32 %x, 7
  ret i32 %tmp
}

define i32 @mul_14_x(i32 %x) {
  %shl.1 = shl i32 %x, 1
  %shl.2 = shl i32 %x, 2
  %add.2 = add i32 %shl.1, %shl.2
  %shl.3 = shl i32 %x, 3
  %add.3 = add i32 %add.2, %shl.3
  ret i32 %add.3
}

define i32 @div_negative(i32 %x) {
  %tmp = sdiv i32 %x, 8
  ret i32 %tmp
}
