.intel_syntax noprefix
.text
.globl invoke
.def invoke; .scl 2; .type 32; .endef
.p2align 4
invoke:
 push rdi
 push rsi
 push rbx
 push r12
 push r14
 sub rsp, 0x30
 mov r10, rcx
 mov rdi, rdx
 mov rsi, r8
 mov r12, r9
 mov rbx, [rsp+0x80]
 mov r14d, [rsp+0x88]
 mov edx, [rsp+0x90]
 mov eax, [rsp+0x98]
 call r10
 mov eax, [rsi+8]
 add rsp, 0x30
 pop r14
 pop r12
 pop rbx
 pop rsi
 pop rdi
 ret
