section .text

extern malloc, mmap, munmap, memcpy

global gen_init
global gen_yield
global gen_return
global gen_send
global gen_is_done

; structure layout:
; 00 mmap'd address
; 04 other's esp
; both are NULL when the generator's done

; stack save format:
; ebp
; ebx
; edi
; esi
; the thing to return or NULL

stack_size equ 1024 * 4 ; hopefully one page

gen_init:
    push ebp
    mov ebp, esp
    push ebx ; back up ebx
    push edi ; back up edi
    push esi ; back up esi
    ; malloc the struct
    sub esp, 8 ; align
    push 8
    call malloc
    add esp, 4 + 8
    mov ebx, eax ; struct pointer in ebx
    ; allocate the new stack
    sub esp, 8 ; align
    push 0 ; offset
    push -1 ; fd
    push 0x122 ; MAP_PRIVATE | MAP_ANONYMOUS | MAP_GROWSDOWN
    push 0x7 ; read, write, execute
    push stack_size
    push 0 ; addr = NULL
    call mmap
    add esp, 6*4 + 8
    mov dword[ebx], eax
    mov dword[ebx + 4], esp
    lea edi, [eax + stack_size] ; stack starts at edi-1, grows down
    ; populate the new stack
    ; copy the caller's stack frame
    ; already aligned
    mov eax, dword[ebp]
    mov eax, dword[eax]
    push eax ; make sure to copy caller's caller's stack frame to capture args
    sub dword[esp], ebp
    push ebp ; src
    sub edi, dword[esp + 4] ; edi at the new esp, same place where we save ebp
    push edi ; dst = new_stack - size
    call memcpy
    add esp, 3*4 ; could skip this
    ; find the newly-copied saved-ebp of the caller
    mov esi, dword[ebp]
    sub esi, ebp
    add esi, edi
    ; temporarily swtch to the new stack
    mov esp, edi
    mov dword[esp], esi ; fix up the ebp
    push dword[ebp - 4] ; ebx
    push dword[ebp - 8] ; edi
    push dword[ebp - 12] ; esi
    push ebx ; going to return the struct pointer next time
    ; switch back to the old stack
    xchg esp, dword[ebx + 4]
    ; cleanup
    mov eax, ebx ; return the pointer
    mov esi, dword[ebp - 8] ; restore esi
    mov edi, dword[ebp - 8] ; restore edi
    mov ebx, dword[ebp - 4] ; restore ebx
    leave
    ; simulate returning from the function
    ; call leave once again
    leave
    ret

gen_yield:
gen_send:
    ; wow, they actually are completely the same func!
    ; dump the registers
    push ebp
    mov ebp, esp
    push ebx
    push edi
    push esi
    push 0 ; going to return the actual value
    mov ecx, dword[ebp + 8] ; struct pointer
    mov eax, dword[ebp + 12] ; value to return
    ; switch to the other stack
    xchg esp, dword[ecx + 4]
    ; if there's a value to return, load it
    pop edx
    test edx, edx
    cmovnz eax, edx
    ; restore the registers
    pop esi
    pop edi
    pop ebx
    pop ebp ; don't "mov esp, ebp" or leave
    ret

gen_return:
    ; no need to dump stuff!
    mov ebx, dword[esp + 4] ; struct pointer
    mov edi, dword[esp + 8] ; return value
    ; switch to the old stack
    mov esp, dword[ebx + 4]
    ; unmap the "new" stack
    sub esp, 4 ; align - hopefully
    push stack_size
    push dword[ebx]
    call munmap
    add esp, 8 + 4
    ; zero out the struct
    mov dword[ebx], 0
    mov dword[ebx + 4], 0
    ; return value
    mov eax, edi
    ; restore the registers
    add esp, 4 ; skip the optional return value
    pop esi
    pop edi
    pop ebx
    pop ebp ; don't "mov esp, ebp" or leave
    ret

gen_is_done:
    ; the only non-magical function here
    mov ecx, dword[esp + 4] ; struct pointer
    mov ecx, dword[ecx]
    xor eax, eax
    test ecx, ecx
    setz al
    ret
