; ModuleID = 'test/test.ll'
source_filename = "test/test.ll"

define i32 @mul_0(i32 %x) {
entry:
  ret i32 0
}

define i32 @mul_1(i32 %x) {
entry:
  ret i32 %x
}

define i32 @mul_pow(i32 %x) {
entry:
  %shl.pow2 = shl i32 %x, 3
  ret i32 %shl.pow2
}

define i32 @mul_x_15(i32 %x) {
entry:
  %shl.1 = shl i32 %x, 1
  %add.1 = add i32 %x, %shl.1
  %shl.2 = shl i32 %x, 2
  %add.2 = add i32 %add.1, %shl.2
  %shl.3 = shl i32 %x, 3
  %add.3 = add i32 %add.2, %shl.3
  ret i32 %add.3
}

define i32 @mul_-3(i32 %x) {
entry:
  %shl.1 = shl i32 %x, 1
  %add.1 = add i32 %x, %shl.1
  %neg = sub i32 0, %add.1
  ret i32 %neg
}

define i32 @div_1(i32 %x) {
entry:
  ret i32 %x
}

define i32 @div_0(i32 %x) {
entry:
  %tmp = sdiv i32 %x, 0
  ret i32 %tmp
}

define i32 @div_pow2(i32 %x) {
entry:
  %ashr.div = ashr i32 %x, 3
  ret i32 %ashr.div
}

define i32 @div_neg4(i32 %x) {
entry:
  %ashr.div = ashr i32 %x, 2
  %neg = sub i32 0, %ashr.div
  ret i32 %neg
}

define i32 @combo(i32 %x) {
entry:
  %shl.1 = shl i32 %x, 1
  %add.1 = add i32 %x, %shl.1
  %shl.2 = shl i32 %x, 2
  %add.2 = add i32 %add.1, %shl.2
  %shl.3 = shl i32 %x, 3
  %add.3 = add i32 %add.2, %shl.3
  %ashr.div = ashr i32 %add.3, 3
  ret i32 %ashr.div
}
