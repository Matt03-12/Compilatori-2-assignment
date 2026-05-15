; ============================================================
; test_licm.ll
; Test cases per la pass LICM custom (code-motion-pass).
; Già in SSA, niente alloca/store di locals: niente bisogno di mem2reg.
;
; Esecuzione:
;     opt -load-pass-plugin=./libTestPass.so \
;         -passes=code-motion-pass test_licm.ll -S -o test_out.ll
;
; Ogni funzione esercita un aspetto preciso delle fix.
; [ATTESO]  cosa fa la pass corretta.
; [PRE-FIX] come si comportava la versione buggata (per contrasto).
; ============================================================


; ============================================================
; 1) Baseline: add invariant, single exit.
;    [ATTESO]  %c = add %a, %b  ->  preheader del loop.
; ============================================================
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


; ============================================================
; 2) FIX (1) — lato negativo.
;    sdiv protetta da `if (b != 0)`, loop con due exit
;    (early return + uscita naturale).
;    Exit blocks = { %early.exit, %for.end }.
;    %div.then non domina nessuno dei due, e sdiv con divisore
;    non-costante non è speculative -> isSafeToHoist = false.
;    [ATTESO]  %q = sdiv  RESTA in %div.then.
;    [PRE-FIX] %q veniva spostata nel preheader: SIGFPE se b==0.
; ============================================================
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
  %q       = sdiv i32 %a, %b                 ; <-- candidato pericoloso
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


; ============================================================
; 3) FIX (1) — lato positivo via dominanza.
;    do-while single-BB: %do.body è sia header che latch e domina
;    l'unico exit %do.end. La sdiv non è speculative ma il body
;    eseguiva comunque in ogni run -> hoist legittimo.
;    Il check `n <= 0` esterno garantisce che entriamo nel loop
;    solo se serve.
;    [ATTESO]  %q = sdiv  ->  preheader del do-while.
; ============================================================
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


; ============================================================
; 4) FIX (1) — lato positivo via speculation.
;    Loop con due exit (early break + naturale), ma l'invariant è
;    un semplice add: isSafeToSpeculativelyExecute = true, quindi
;    il check di dominanza degli exit non viene nemmeno richiesto.
;    [ATTESO]  %c = add  ->  preheader.
; ============================================================
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


; ============================================================
; 5) FIX (2) — per-loop invariant sets.
;    %x = add %i, %k:
;      - invariant per il loop INNER (i e k vengono da fuori inner)
;      - NON invariant per OUTER (i è l'i.v. dell'outer, è una PHI
;        nell'header outer e non è marcata invariant)
;    [ATTESO]  %x  ->  preheader DELL'INNER, e basta.
;    [PRE-FIX] %x finiva nel preheader OUTER -> use-before-def di
;              %i (che è definita più giù in %outer.cond) -> IR
;              malformata, verifier fail.
; ============================================================
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


; ============================================================
; 6) FIX (4) — load conservativo.
;    Il puntatore %p è invariant (argomento). Sotto la vecchia
;    logica il load %x veniva marcato invariant e hoistato. Ma
;    nel loop c'è uno store su %q[i]: senza alias analysis non
;    possiamo escludere q == p, quindi:
;        loopMayWriteMemory(L) = true  -> niente load nel set.
;    [ATTESO]  %x = load  RESTA in %for.body.
; ============================================================
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
