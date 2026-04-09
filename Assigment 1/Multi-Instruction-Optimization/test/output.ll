; ModuleID = 'test/testmlo2.ll'
source_filename = "test/testmlo2.ll"

define i32 @test(i32 %b, i32 %e) {
entry:
  %a = add i32 1, %b
  %d = add i32 1, %e
  %h = sub i32 %e, %b
  ret i32 %h
}
