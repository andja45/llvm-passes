define void @self_loop() {
entry:
  br label %loop

loop:
  br label %loop
}