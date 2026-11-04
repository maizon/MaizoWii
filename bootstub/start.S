/* MAIZO Wii clean-room bootstrap, GPL-3.0-or-later.
 * Runs in MEM2, away from the child DOL MEM1 destinations. */
.section .text.start,"ax"
.globl _start
_start:
    lis 1,0x9302
    ori 1,1,0xfff0
    bl maizo_boot_main
1:  b 1b
