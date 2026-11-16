# Replacement for 0x12BED8: chapter slot (1-based, a1) -> screen position (*a2 = x, *a3 = y).
# Original had a fixed 3x4 table, so slots 13 and 14 got no position.
# This lays chapters out 3 columns x 5 rows: x from the original column table, y = 95 + 100*row.
        .set noreorder
        .set noat
        .text
        .globl hubgrid
hubgrid:
        addiu   $a1, $a1, -1
        sltiu   $t0, $a1, 15
        beqz    $t0, 9f
        ori     $t0, $zero, 3
        divu    $zero, $a1, $t0
        mflo    $t1                 # row
        mfhi    $t2                 # column
        sll     $t2, $t2, 2
        lui     $t3, %hi(cols)
        addu    $t3, $t3, $t2
        lwc1    $f14, %lo(cols)($t3)
        lwc1    $f12, 0x30($a0)
        mul.s   $f12, $f12, $f14
        swc1    $f12, 0($a2)
        sll     $t4, $t1, 6         # row*64
        sll     $t5, $t1, 5         # row*32
        addu    $t4, $t4, $t5
        sll     $t5, $t1, 2         # row*4
        addu    $t4, $t4, $t5       # row*100
        addiu   $t4, $t4, 95
        mtc1    $t4, $f14
        nop
        cvt.s.w $f14, $f14
        lwc1    $f12, 0x34($a0)
        mul.s   $f12, $f12, $f14
        swc1    $f12, 0($a3)
9:      jr      $ra
        nop
        .align 2
cols:   .float 50.0, 440.0, 730.0
