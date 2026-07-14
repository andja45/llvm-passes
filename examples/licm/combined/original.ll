; ModuleID = '/home/andja/Cetvrta godina/Drugi Semestar/KK/projekat/llvm-passes/examples/licm/combined//input.c'
source_filename = "/home/andja/Cetvrta godina/Drugi Semestar/KK/projekat/llvm-passes/examples/licm/combined//input.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: noinline nounwind uwtable
define dso_local void @pipeline(ptr noalias noundef %dst, ptr noalias noundef %src, ptr noalias noundef %acc, i64 noundef %offset, float noundef %scale, float noundef %base, i32 noundef %a, i32 noundef %b, double noundef %cx, ptr noalias noundef %factor) #0 {
entry:
  %dst.addr = alloca ptr, align 8
  %src.addr = alloca ptr, align 8
  %acc.addr = alloca ptr, align 8
  %offset.addr = alloca i64, align 8
  %scale.addr = alloca float, align 4
  %base.addr = alloca float, align 4
  %a.addr = alloca i32, align 4
  %b.addr = alloca i32, align 4
  %cx.addr = alloca double, align 8
  %factor.addr = alloca ptr, align 8
  %i = alloca i64, align 8
  %coeff = alloca i32, align 4
  %raw = alloca float, align 4
  %normed = alloca float, align 4
  %adj = alloca float, align 4
  %q = alloca float, align 4
  %boost = alloca float, align 4
  %f = alloca float, align 4
  store ptr %dst, ptr %dst.addr, align 8
  store ptr %src, ptr %src.addr, align 8
  store ptr %acc, ptr %acc.addr, align 8
  store i64 %offset, ptr %offset.addr, align 8
  store float %scale, ptr %scale.addr, align 4
  store float %base, ptr %base.addr, align 4
  store i32 %a, ptr %a.addr, align 4
  store i32 %b, ptr %b.addr, align 4
  store double %cx, ptr %cx.addr, align 8
  store ptr %factor, ptr %factor.addr, align 8
  store i64 0, ptr %i, align 8
  br label %for.cond

for.cond:                                         ; preds = %for.inc, %entry
  %0 = load i64, ptr %i, align 8
  %cmp = icmp slt i64 %0, 4
  br i1 %cmp, label %for.body, label %for.end

for.body:                                         ; preds = %for.cond
  %1 = load i32, ptr %a.addr, align 4
  %2 = load i32, ptr %b.addr, align 4
  %mul = mul nsw i32 %1, %2
  %add = add nsw i32 %mul, 1
  store i32 %add, ptr %coeff, align 4
  %3 = load ptr, ptr %src.addr, align 8
  %4 = load i64, ptr %i, align 8
  %5 = load i64, ptr %offset.addr, align 8
  %add1 = add nsw i64 %4, %5
  %arrayidx = getelementptr inbounds float, ptr %3, i64 %add1
  %6 = load float, ptr %arrayidx, align 4
  store float %6, ptr %raw, align 4
  %7 = load float, ptr %raw, align 4
  %8 = load float, ptr %scale.addr, align 4
  %div = fdiv float %7, %8
  store float %div, ptr %normed, align 4
  %9 = load float, ptr %normed, align 4
  %10 = load float, ptr %base.addr, align 4
  %add2 = fadd float %9, %10
  %11 = load float, ptr %scale.addr, align 4
  %add3 = fadd float %add2, %11
  store float %add3, ptr %adj, align 4
  %12 = load i32, ptr %a.addr, align 4
  %13 = load i32, ptr %b.addr, align 4
  %div4 = sdiv i32 %12, %13
  %conv = sitofp i32 %div4 to float
  store float %conv, ptr %q, align 4
  %14 = load double, ptr %cx.addr, align 8
  %call = call double @compute(double noundef %14) #3
  %conv5 = fptrunc double %call to float
  store float %conv5, ptr %boost, align 4
  %15 = load ptr, ptr %factor.addr, align 8
  %16 = load float, ptr %15, align 4
  store float %16, ptr %f, align 4
  %17 = load float, ptr %normed, align 4
  %18 = load float, ptr %q, align 4
  %mul6 = fmul float %17, %18
  %19 = load i32, ptr %coeff, align 4
  %conv7 = sitofp i32 %19 to float
  %20 = load ptr, ptr %acc.addr, align 8
  %21 = load float, ptr %20, align 4
  %22 = call float @llvm.fmuladd.f32(float %mul6, float %conv7, float %21)
  store float %22, ptr %20, align 4
  %23 = load float, ptr %adj, align 4
  %24 = load float, ptr %boost, align 4
  %mul9 = fmul float %23, %24
  %25 = load float, ptr %f, align 4
  %mul10 = fmul float %mul9, %25
  %26 = load ptr, ptr %dst.addr, align 8
  %27 = load i64, ptr %i, align 8
  %arrayidx11 = getelementptr inbounds float, ptr %26, i64 %27
  store float %mul10, ptr %arrayidx11, align 4
  br label %for.inc

for.inc:                                          ; preds = %for.body
  %28 = load i64, ptr %i, align 8
  %inc = add nsw i64 %28, 1
  store i64 %inc, ptr %i, align 8
  br label %for.cond, !llvm.loop !6

for.end:                                          ; preds = %for.cond
  ret void
}

; Function Attrs: nounwind willreturn memory(none)
declare double @compute(double noundef) #1

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare float @llvm.fmuladd.f32(float, float, float) #2

attributes #0 = { noinline nounwind uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nounwind willreturn memory(none) "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #2 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
attributes #3 = { nounwind willreturn memory(none) }

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
