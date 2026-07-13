; ModuleID = '/home/andja/Cetvrta godina/Drugi Semestar/KK/projekat/llvm-passes/examples/licm/combined//original.ll'
source_filename = "/home/andja/Cetvrta godina/Drugi Semestar/KK/projekat/llvm-passes/examples/licm/combined//input.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: noinline nounwind uwtable
define dso_local void @pipeline(ptr noalias noundef %dst, ptr noalias noundef %src, ptr noalias noundef %acc, i64 noundef %offset, float noundef %scale, float noundef %base, i32 noundef %a, i32 noundef %b, double noundef %cx, ptr noalias noundef %factor) #0 {
entry:
  %mul = mul nsw i32 %a, %b
  %call = call double @compute(double noundef %cx) #3
  %0 = load float, ptr %factor, align 4
  %promoted.init = load float, ptr %acc, align 4
  %reassoc = fadd float %base, %scale
  %gep.base = getelementptr float, ptr %src, i64 %offset
  %recip = fdiv float 1.000000e+00, %scale
  %div4 = sdiv i32 %a, %b
  %add = add nsw i32 %mul, 1
  %conv = sitofp i32 %div4 to float
  %conv5 = fptrunc double %call to float
  %reassoc1 = fmul float %recip, %conv
  %reassoc2 = fmul float %conv5, %0
  %conv7 = sitofp i32 %add to float
  br label %for.cond

for.cond:                                         ; preds = %for.inc, %entry
  %promoted = phi float [ %promoted.init, %entry ], [ %2, %for.inc ]
  %i.0 = phi i64 [ 0, %entry ], [ %inc, %for.inc ]
  %cmp = icmp slt i64 %i.0, 4
  br i1 %cmp, label %for.body, label %for.end

for.body:                                         ; preds = %for.cond
  %gep.var = getelementptr float, ptr %gep.base, i64 %i.0
  %1 = load float, ptr %gep.var, align 4
  %recip.mul = fmul float %1, %reassoc1
  %add2 = fadd float %recip.mul, %reassoc
  %2 = call float @llvm.fmuladd.f32(float %recip.mul, float %conv7, float %promoted)
  %mul9 = fmul float %add2, %reassoc2
  %arrayidx11 = getelementptr inbounds float, ptr %dst, i64 %i.0
  store float %mul9, ptr %arrayidx11, align 4
  br label %for.inc

for.inc:                                          ; preds = %for.body
  %inc = add nsw i64 %i.0, 1
  br label %for.cond, !llvm.loop !6

for.end:                                          ; preds = %for.cond
  %promoted.lcssa = phi float [ %promoted, %for.cond ]
  store float %promoted.lcssa, ptr %acc, align 4
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
