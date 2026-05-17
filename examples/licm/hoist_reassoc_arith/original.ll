; ModuleID = '/home/andja/Cetvrta godina/Drugi Semestar/KK/projekat/llvm-passes-project/examples/licm/hoist_reassoc_arith//input.c'
source_filename = "/home/andja/Cetvrta godina/Drugi Semestar/KK/projekat/llvm-passes-project/examples/licm/hoist_reassoc_arith//input.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: noinline nounwind uwtable
define dso_local void @process(ptr noundef %arr, i32 noundef %n, float noundef %offset, float noundef %base) #0 {
entry:
  %arr.addr = alloca ptr, align 8
  %n.addr = alloca i32, align 4
  %offset.addr = alloca float, align 4
  %base.addr = alloca float, align 4
  %i = alloca i32, align 4
  store ptr %arr, ptr %arr.addr, align 8
  store i32 %n, ptr %n.addr, align 4
  store float %offset, ptr %offset.addr, align 4
  store float %base, ptr %base.addr, align 4
  store i32 0, ptr %i, align 4
  br label %for.cond

for.cond:                                         ; preds = %for.inc, %entry
  %0 = load i32, ptr %i, align 4
  %1 = load i32, ptr %n.addr, align 4
  %cmp = icmp slt i32 %0, %1
  br i1 %cmp, label %for.body, label %for.end

for.body:                                         ; preds = %for.cond
  %2 = load ptr, ptr %arr.addr, align 8
  %3 = load i32, ptr %i, align 4
  %idxprom = sext i32 %3 to i64
  %arrayidx = getelementptr inbounds float, ptr %2, i64 %idxprom
  %4 = load float, ptr %arrayidx, align 4
  %5 = load float, ptr %offset.addr, align 4
  %add = fadd float %4, %5
  %6 = load float, ptr %base.addr, align 4
  %add1 = fadd float %add, %6
  %7 = load ptr, ptr %arr.addr, align 8
  %8 = load i32, ptr %i, align 4
  %idxprom2 = sext i32 %8 to i64
  %arrayidx3 = getelementptr inbounds float, ptr %7, i64 %idxprom2
  store float %add1, ptr %arrayidx3, align 4
  br label %for.inc

for.inc:                                          ; preds = %for.body
  %9 = load i32, ptr %i, align 4
  %inc = add nsw i32 %9, 1
  store i32 %inc, ptr %i, align 4
  br label %for.cond, !llvm.loop !6

for.end:                                          ; preds = %for.cond
  ret void
}

attributes #0 = { noinline nounwind uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"Ubuntu clang version 18.1.3 (1ubuntu1)"}
!6 = distinct !{!6, !7}
!7 = !{!"llvm.loop.mustprogress"}
