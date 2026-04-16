; RUN: opt -S -strength-reduction %s | FileCheck %s

define i32 @mul_0(i32 %x) {
entry:
  %tmp = mul i32 %x, 0
  ret i32 %tmp
}

define i32 @mul_1(i32 %x) {
entry:
  %tmp = mul i32 %x, 1
  ret i32 %tmp
}

define i32 @mul_pow(i32 %x) {
entry:
  %tmp = mul i32 %x, 8
  ret i32 %tmp
}

define i32 @mul_x_15(i32 %x) {
entry:
  %tmp = mul i32 %x, 15
  ret i32 %tmp
}

define i32 @mul_-3(i32 %x) {
entry:
  %tmp = mul i32 %x, -3
  ret i32 %tmp
}

define i32 @div_1(i32 %x) {
entry:
  %tmp = sdiv i32 %x, 1
  ret i32 %tmp
}


define i32 @div_0(i32 %x) {
entry:
  %tmp = sdiv i32 %x, 0
  ret i32 %tmp
}

define i32 @div_pow2(i32 %x) {
entry:
  %tmp = sdiv i32 %x, 8
  ret i32 %tmp
}

define i32 @div_neg4(i32 %x) {
entry:
  %tmp = sdiv i32 %x, -4
  ret i32 %tmp
}

;nota per le divisioni non potenza di 2

define i32 @combo(i32 %x) {
entry:
  %a = mul i32 %x, 15
  %b = sdiv i32 %a, 8
  ret i32 %b
}
