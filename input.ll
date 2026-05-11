define i32 @test(i32 %n, i32 %a, i32 %b, ptr %p) {
entry:
  br label %outer.header

outer.header:
  %i = phi i32 [ 0, %entry ], [ %i.next, %outer.latch ]
  %acc = phi i32 [ 0, %entry ], [ %acc.next, %outer.latch ]
  %outer.cond = icmp slt i32 %i, %n
  br i1 %outer.cond, label %outer.body, label %outer.exit

outer.body:
  %x = mul i32 %a, %b               ; INVARIANT outer -> entry
  %y = add i32 %x, %a               ; INVARIANT (chain) -> entry
  br label %inner.header

inner.header:
  %j = phi i32 [ 0, %outer.body ], [ %j.next, %inner.body ]
  %inner.cond = icmp slt i32 %j, %n
  br i1 %inner.cond, label %inner.body, label %inner.exit

inner.body:
  %z = mul i32 %y, %a               ; INVARIANT inner (e poi outer)
  %w = add i32 %z, %j               ; NON invariant (usa %j)
  %s = add i32 %w, 1                ; NON invariant (usa %w)
  store i32 %s, ptr %p              ; SIDE EFFECT - mai hoistare
  %j.next = add i32 %j, 1           ; NON invariant (usa %j)
  br label %inner.header

inner.exit:
  br label %outer.latch

outer.latch:
  %acc.next = add i32 %acc, %y
  %i.next = add i32 %i, 1
  br label %outer.header

outer.exit:
  ret i32 %acc
}