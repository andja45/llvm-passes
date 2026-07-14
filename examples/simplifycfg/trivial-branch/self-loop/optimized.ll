; ModuleID = '/home/ana/Desktop/llvm-passes/examples/simplifycfg/trivial-branch/self-loop/original.ll'
source_filename = "/home/ana/Desktop/llvm-passes/examples/simplifycfg/trivial-branch/self-loop/original.ll"

define void @self_loop() {
entry:
  br label %loop

loop:                                             ; preds = %loop, %entry
  br label %loop
}
