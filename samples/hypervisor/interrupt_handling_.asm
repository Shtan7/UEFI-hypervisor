.code

extern handle_host_exception : proc

;
; The index to track an interrupt number for generating AsmDefaultExceptionHandlers.
;
index = 0

PUSHAQ macro
        push    rax
        push    rcx
        push    rdx
        push    rbx
        push    rbp
        push    rsi
        push    rdi
        push    r8
        push    r9
        push    r10
        push    r11
        push    r12
        push    r13
        push    r14
        push    r15
endm

;
;   @brief Loads all general purpose registers, except for RSP, from the stack.
;
;   @details This macro does not alter the flag register.
;
POPAQ macro
        pop     r15
        pop     r14
        pop     r13
        pop     r12
        pop     r11
        pop     r10
        pop     r9
        pop     r8
        pop     rdi
        pop     rsi
        pop     rbp
        pop     rbx
        pop     rdx
        pop     rcx
        pop     rax
endm

;
; Generates the default exception handler code for the given interrupt/exception
; number. The generated code assumes that the interrupt/exception does not push
; error code.
;
; index is incremented whenever this macro is used.
;
INTERRUPT_HANDLER macro interrupt_number
  ;
  ; Push dummy error code for consistent stack layout.
  ;
  push    0
  push    interrupt_number
  jmp     common_exception_handler
  index = index + 1
endm

;
; Generates the default exception handler code for the given interrupt/exception
; number. The generated code assumes that the interrupt/exception pushes error code.
;
; index is incremented whenever this macro is used.
;
INTERRUPT_HANDLER_WITH_CODE macro interrupt_number
  ;
  ; Error code is expected to be pushed by the processor.
  ;
  nop
  nop
  push    interrupt_number
  jmp     common_exception_handler
  index = index + 1
endm

;
; @brief The default host exception handlers.
;
; @details This is the function containing actually 256 stub functions generated
;   with the INTERRUPT_HANDLER and INTERRUPT_HANDLER_WITH_CODE macros. Each function
;   works as a hendler of the corresponding interrupt/exception in the host.
;
default_exception_handlers proc
  ;
  ; INT0-7
  ;
  repeat 8
  INTERRUPT_HANDLER       index
  endm

  ;
  ; INT8, INT9
  ;
  INTERRUPT_HANDLER_WITH_CODE   index
  INTERRUPT_HANDLER       index

  ;
  ; INT10-14
  ;
  repeat 5
  INTERRUPT_HANDLER_WITH_CODE   index
  endm

  ;
  ; INT15-16
  ;
  repeat 2
  INTERRUPT_HANDLER       index
  endm

  ;
  ; INT17
  ;
  INTERRUPT_HANDLER_WITH_CODE   index

  ;
  ; INT18-255
  ;
  repeat 238
  INTERRUPT_HANDLER       index
  endm
default_exception_handlers endp

;
; @brief The common logic for the exception handlers.
;
; @details This function pushes register values into the stack and calls the
;   high-level handler written in C.
;
common_exception_handler proc
  PUSHAQ
  mov     rcx, rsp
  sub     rsp, 20h
  call    handle_host_exception
  add     rsp, 20h
  POPAQ

  ;
  ; Remove the error code and interrupt number.
  ;
  add     rsp, 10h
  iretq
common_exception_handler endp

  end
