define i32 @f_basic_invariant(i32 %n, i32 %a, i32 %b) {
entry:
  br label %for.cond

for.cond:
  %i   = phi i32 [ 0, %entry ], [ %i.next,   %for.inc ]
  %sum = phi i32 [ 0, %entry ], [ %sum.next, %for.inc ]
  %cmp = icmp slt i32 %i, %n
  br i1 %cmp, label %for.body, label %for.end

for.body:
  %c        = add i32 %a, %b
  %sum.next = add i32 %sum, %c
  br label %for.inc

for.inc:
  %i.next = add i32 %i, 1
  br label %for.cond

for.end:
  ret i32 %sum
}


define i32 @f_div_guarded_multiexit(i32 %n, i32 %a, i32 %b, i32 %max) {
entry:
  br label %for.cond

for.cond:
  %i   = phi i32 [ 0, %entry ], [ %i.next,    %for.inc ]
  %sum = phi i32 [ 0, %entry ], [ %sum.merge, %for.inc ]
  %cmp = icmp slt i32 %i, %n
  br i1 %cmp, label %for.body, label %for.end

for.body:
  %early.cmp = icmp sgt i32 %sum, %max
  br i1 %early.cmp, label %early.exit, label %div.check

div.check:
  %bnz = icmp ne i32 %b, 0
  br i1 %bnz, label %div.then, label %div.skip

div.then:
  %q       = sdiv i32 %a, %b               
  %sum.add = add i32 %sum, %q
  br label %div.skip

div.skip:
  %sum.merge = phi i32 [ %sum.add, %div.then ], [ %sum, %div.check ]
  br label %for.inc

for.inc:
  %i.next = add i32 %i, 1
  br label %for.cond

early.exit:
  ret i32 %sum

for.end:
  ret i32 %sum
}



define i32 @f_div_dowhile_safe(i32 %n, i32 %a, i32 %b) {
entry:
  %n.le.zero = icmp sle i32 %n, 0
  br i1 %n.le.zero, label %early.zero, label %do.body

do.body:
  %i   = phi i32 [ 0, %entry ], [ %i.next,   %do.body ]
  %sum = phi i32 [ 0, %entry ], [ %sum.next, %do.body ]
  %q        = sdiv i32 %a, %b
  %sum.next = add i32 %sum, %q
  %i.next   = add i32 %i, 1
  %cont     = icmp slt i32 %i.next, %n
  br i1 %cont, label %do.body, label %do.end

do.end:
  ret i32 %sum.next

early.zero:
  ret i32 0
}


define i32 @f_multi_exit_speculative(i32 %n, i32 %a, i32 %b, i32 %t) {
entry:
  br label %for.cond

for.cond:
  %i   = phi i32 [ 0, %entry ], [ %i.next,   %for.inc ]
  %sum = phi i32 [ 0, %entry ], [ %sum.next, %for.inc ]
  %cmp = icmp slt i32 %i, %n
  br i1 %cmp, label %for.body, label %for.end

for.body:
  %c        = add i32 %a, %b
  %sum.next = add i32 %sum, %c
  %too.big  = icmp sgt i32 %sum.next, %t
  br i1 %too.big, label %break.exit, label %for.inc

for.inc:
  %i.next = add i32 %i, 1
  br label %for.cond

break.exit:
  ret i32 %sum.next

for.end:
  ret i32 %sum
}


define i32 @f_nested_outer_dep(i32 %N, i32 %M, i32 %k) {
entry:
  br label %outer.cond

outer.cond:
  %i     = phi i32 [ 0, %entry ], [ %i.next,      %outer.inc ]
  %total = phi i32 [ 0, %entry ], [ %total.inner, %outer.inc ]
  %outer.cmp = icmp slt i32 %i, %N
  br i1 %outer.cmp, label %inner.cond, label %outer.end

inner.cond:
  %j           = phi i32 [ 0,      %outer.cond ], [ %j.next,     %inner.inc ]
  %total.inner = phi i32 [ %total, %outer.cond ], [ %total.next, %inner.inc ]
  %inner.cmp = icmp slt i32 %j, %M
  br i1 %inner.cmp, label %inner.body, label %outer.inc

inner.body:
  %x          = add i32 %i, %k             ; invariant per inner, non per outer
  %xj         = mul i32 %x, %j
  %total.next = add i32 %total.inner, %xj
  br label %inner.inc

inner.inc:
  %j.next = add i32 %j, 1
  br label %inner.cond

outer.inc:
  %i.next = add i32 %i, 1
  br label %outer.cond

outer.end:
  ret i32 %total
}


define void @f_load_with_store(ptr %p, ptr %q, i32 %n) {
entry:
  br label %for.cond

for.cond:
  %i   = phi i32 [ 0, %entry ], [ %i.next, %for.inc ]
  %cmp = icmp slt i32 %i, %n
  br i1 %cmp, label %for.body, label %for.end

for.body:
  %x   = load i32, ptr %p
  %sum = add i32 %x, %i
  %q.i = getelementptr i32, ptr %q, i32 %i
  store i32 %sum, ptr %q.i
  br label %for.inc

for.inc:
  %i.next = add i32 %i, 1
  br label %for.cond

for.end:
  ret void
}
