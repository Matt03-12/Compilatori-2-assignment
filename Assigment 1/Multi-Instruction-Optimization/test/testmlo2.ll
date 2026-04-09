define i32 @test(i32 %b,i32 %e) {
entry:
  %a = add i32 1, %b
  %d = add i32 1, %e
  %c = sub i32 %a, 1
  %f = sub i32 %d, 1
  %h = sub i32 %f, %c
  
  ret i32 %h
}
