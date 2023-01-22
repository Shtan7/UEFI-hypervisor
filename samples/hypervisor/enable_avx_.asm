.code

enable_avx proc

  mov rax, cr4
  or eax, 040000h
  mov cr4, rax

  mov rcx, 0
  xgetbv

  or eax, 6
  mov rcx, 0
  xsetbv

  ret

enable_avx endp

end
