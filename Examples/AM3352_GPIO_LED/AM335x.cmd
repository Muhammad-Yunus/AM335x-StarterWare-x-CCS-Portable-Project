-stack  0x0008                             /* SOFTWARE STACK SIZE           */
-heap   0x2000                             /* HEAP AREA SIZE                */
-e Entry
--diag_suppress=10063

MEMORY
{
        DDR_MEM        : org = 0x80000000  len = 0x7FFFFFF           /* RAM */
}

SECTIONS
{
    .text:Entry : load > 0x80000000

    .text    : load > DDR_MEM              /* CODE                          */
    .data    : load > DDR_MEM              /* INITIALIZED GLOBAL AND STATIC VARIABLES */
    .bss     : load > DDR_MEM              /* UNINITIALIZED OR ZERO INITIALIZED */
                                           /* GLOBAL & STATIC VARIABLES */
                    RUN_START(bss_start)
                    RUN_END(bss_end)
    .const   : load > DDR_MEM              /* GLOBAL CONSTANTS              */
    .stack   : load > 0x87FFFFF0           /* SOFTWARE SYSTEM STACK         */
}