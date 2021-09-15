
    SECTION asm_functions,code

    ; shared symbols (C and assembly)
    ; function name (add underscore)
    
    xref _fibonacci
    xref _average
    xref _multiply

; integer argument passed via register d0
; return value in d0
_fibonacci:
   cmp.l  #0,d0
   beq    .exit
   
   move.l #0,d1
   move.l #1,d2
   sub.l  #1,d0
.loop:
   add.l  d2,d1
   move.l d1,d4
   move.l d2,d1
   move.l d4,d2
   dbra   d0,.loop
   
   move.l d1,d0
.exit:
   rts

; double arguments passed via registers fp0, fp1
; return value in fp0
_average:
   fadd fp1,fp0
   fdiv #2,fp0    
   rts
      
; double arguments passed via stack
; stack contains: 
; - return address (4 bytes) = offset arg1 (4)
; - double1 (8 bytes) = offset arg2 (4+8)
; - double2 (8 bytes)   
_multiply:
   fmove.d 4(sp),fp0
   fmove.d 12(sp),fp1
   fmul  fp1,fp0
   rts
   
   END
