; Copyright © 2015 Hansoft AB 
; Distributed under the MIT license, see license text in LICENSE.Malterlib

;-----------------------------------------------------
PUBLIC fg_MalterlibGetFramePtr_X86_64
PUBLIC fg_MalterlibGetRDTSC_X86_64
PUBLIC fg_MalterlibGetCurrentThreadID_X86_64

;-----------------------------------------------------
;_DATA SEGMENT DWORD PUBLIC USE32 'DATA'

;_DATA ENDS 

;-----------------------------------------------------
AMD64_CODE SEGMENT READ EXECUTE ALIGN(16)

;-----------------------------------------------------
fg_MalterlibGetFramePtr_X86_64 PROC
; C

	mov rax, rdi
	ret

fg_MalterlibGetFramePtr_X86_64 ENDP

fg_MalterlibGetCurrentThreadID_X86_64 PROC
; C

    mov qword ptr rax, gs:[30h] 
    mov dword ptr eax, [rax+48h] 
	ret

fg_MalterlibGetCurrentThreadID_X86_64 ENDP

fg_MalterlibGetRDTSC_X86_64 PROC
; C

	rdtsc
;	mov dword ptr [rax], eax
;	mov dword ptr [rax+4], edx
	ret

fg_MalterlibGetRDTSC_X86_64 ENDP

AMD64_CODE ENDS 

END
