.data

stack_pointer_increment dd ?

.code

extern DriverEntry:proc

__vmcall proc

	vmcall
	ret

__vmcall endp

__vmcall_with_returned_value proc

  vmcall
  mov rax, rdx
  ret

__vmcall_with_returned_value endp

CSDriverEntry proc

	pushfq
	push r11
	push rax
	push rcx
	push rdx
	push r8
	push r9

	mov rax, rsp
	mov dword ptr [stack_pointer_increment], 0
	and rax, 0Fh
	test rax, rax
	
	jz normal_prologue

	mov dword ptr [stack_pointer_increment], 1
	sub rsp, 8

	normal_prologue:
	call DriverEntry

	mov eax, dword ptr [stack_pointer_increment]
	test eax, eax

	jz normal_epilogue
	add rsp, 8

	normal_epilogue:
	pop r9
	pop r8
	pop rdx
	pop rcx
	pop rax
	pop r11
	popfq

	ret

CSDriverEntry endp

end
