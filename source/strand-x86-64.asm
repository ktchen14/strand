SECTION .text

global _strand_initialize
global _strand_switch
extern _strand_return

; void strand_initialize(strand_t *strand <rdi>, void *(*main)(void *) <rsi>, void *ctxt <rdx>)
_strand_initialize:
  ; rsp <rax> = strand->data + strand->size
  lea rax, [rdi + 0x20]
  add rax, [rdi + 0x08]

  ; rsp[-0x08] = strand_return_hook
  lea r11, [rel strand_return_hook]
  mov [rax - 0x08], r11
  mov [rax - 0x10], rsi ; rsp[-0x10] = main
  ; mov [rax - 0x18], rdx ; rsp[-0x18] = ctxt

  sub rax, 0x10         ; rsp -= 0x10
  mov [rdi + 0x10], rax ; strand->rsp = rsp

  ret

; void *strand_switch(
;   strand_t **selfp <rdi>, strand_t *next <rsi>, void *result <rdx>)
_strand_switch:
  ; self <rax> = *selfp
  mov rax, [rdi]

  ; save self
  mov [rax + 0x10], rsp ; self->rsp = rsp
  mov [rsp - 0x08], rbx
  mov [rsp - 0x10], rbp
  mov [rsp - 0x18], r12
  mov [rsp - 0x20], r13
  mov [rsp - 0x28], r14
  mov [rsp - 0x30], r15
  fnstcw [rsp - 0x38]
  stmxcsr [rsp - 0x3A]

  ; *selfp = next
  mov [rdi], rsi
  ; next->status = STRAND_ACTIVE
  mov BYTE [rsi + 0x18], 0

  ; load next
  mov rsp, [rsi + 0x10] ; rsp = next->rsp
  mov rbx, [rsp - 0x08]
  mov rbp, [rsp - 0x10]
  mov r12, [rsp - 0x18]
  mov r13, [rsp - 0x20]
  mov r14, [rsp - 0x28]
  mov r15, [rsp - 0x30]
  fldcw [rsp - 0x38]
  ldmxcsr [rsp - 0x3A]

  ; return = result
  mov rax, rdx
  ; call with result as argument 1
  mov rdi, rdx

  ; jump to next->rip
  ret

strand_entrance_hook:
  pop rsi
  ret

; A hook that will call `strand_return(x)` when `return x` is used in a strand
strand_return_hook:
  mov rdi, rax
  call _strand_return
