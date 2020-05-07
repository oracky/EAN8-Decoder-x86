;Author: Michal Oracki
;Title: "EAN-8 decoder"
;Date: 04.05.2020

section	.text
global  getRow, getCode
;extern error  // old version of code

getRow:
	push ebp
	mov	ebp, esp
	push ebx
	push esi

	mov	eax, DWORD [ebp+12]	;address of *buffer to eax
	mov ebx, DWORD [ebp+8]  ;address of *pImg
	mov esi, DWORD [ebp+16] ;last address of *pImg
getRow_prepareNextByte:
    mov dl, BYTE [ebx]
    mov ch, 0  ; bits counter in byte
getRow_getBit:
    mov dh, dl   ; copy of byte
    and dh, 128  ; mask 10000000
    shr dh, 7    ; right shift to leave only most significant bit
    shl dl, 1    ; left shift of original byte
    mov [eax], dh
    inc eax
    inc ch
    cmp ch, 8
    jne getRow_getBit
    inc ebx
    cmp ebx, esi
    jne getRow_prepareNextByte
getRow_exit:
    mov eax, DWORD [ebp+12]
    pop esi
    pop ebx
	pop	ebp
	ret

getCode:
    ;this function compare 4(number of digits in one side) values
    ;with hardcoded codes in array and insert corresponding ascii values
    ;into current buffer
    push ebp
    mov	ebp, esp
    push ebx
    push esi

    mov eax, DWORD [ebp+8]   ;address of buffer
    mov ebx, DWORD  [ebp+12] ;address of codes
    mov ch, 0   ;inserted chars
getCode_config:
    mov esi, DWORD [ebp+16] ;address of codesARR
    mov cl, 0   ;count loop
    mov dl, '0' ;ascii code of digit 0
    sub esi, 4
    dec cl
getCode_loop:
    inc cl
    add esi, 4   ;add 4 because it is int size
    mov dh, [ebx]
    cmp cl, 9
    ja  getCode_not_found
    cmp dh, [esi]   ;compare code value with code in hardcoded array
    jne getCode_loop
getCode_insert_char:
    add dl, cl
    mov [eax], dl   ;insert ascii value to buffer
    inc eax
    inc ch
    add ebx, 4
    cmp ch, 4   ;check if all 4 digits were inserted
    jne getCode_config
    mov eax, DWORD [ebp+8]
    jmp getCode_exit
getCode_not_found:
    ;call error // old version of code
    mov eax, 0     ;setting first sign to null to show error
getCode_exit:
    pop esi
    pop ebx
    pop	ebp
    ret
