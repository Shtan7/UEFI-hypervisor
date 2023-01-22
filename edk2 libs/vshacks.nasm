%define ASM_PFX(a) a

%define PcdGet32(a) _gPcd_FixedAtBuild_ %+ a

%macro retfq 0

    DB 0x48
    retf 

%endmacro