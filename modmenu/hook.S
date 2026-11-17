#define SQ(rt,off) .word (0x1F<<26)|(29<<21)|((rt)<<16)|(off)
#define LQ(rt,off) .word (0x1E<<26)|(29<<21)|((rt)<<16)|(off)
# Hook stub. Entry of the text-system draw (0x2E33F0) jumps here.
# Saves every register the game might care about at full 128-bit width (EE),
# calls menu_tick(textSys), restores, runs the two displaced instructions, returns.
        .set noreorder
        .set noat
        .section .text.hook,"ax"
        .macro SAVEALL
        addiu $sp, $sp, -0x180
        SQ(1,0x000)
        SQ(2,0x010)
        SQ(3,0x020)
        SQ(4,0x030)
        SQ(5,0x040)
        SQ(6,0x050)
        SQ(7,0x060)
        SQ(8,0x070)
        SQ(9,0x080)
        SQ(10,0x090)
        SQ(11,0x0A0)
        SQ(16,0x0B0)
        SQ(17,0x0C0)
        SQ(18,0x0D0)
        SQ(19,0x0E0)
        SQ(20,0x0F0)
        SQ(21,0x100)
        SQ(22,0x110)
        SQ(23,0x120)
        SQ(28,0x130)
        SQ(30,0x140)
        SQ(31,0x150)
        .endm
        .macro RESTOREALL
        LQ(1,0x000)
        LQ(2,0x010)
        LQ(3,0x020)
        LQ(4,0x030)
        LQ(5,0x040)
        LQ(6,0x050)
        LQ(7,0x060)
        LQ(8,0x070)
        LQ(9,0x080)
        LQ(10,0x090)
        LQ(11,0x0A0)
        LQ(16,0x0B0)
        LQ(17,0x0C0)
        LQ(18,0x0D0)
        LQ(19,0x0E0)
        LQ(20,0x0F0)
        LQ(21,0x100)
        LQ(22,0x110)
        LQ(23,0x120)
        LQ(28,0x130)
        LQ(30,0x140)
        LQ(31,0x150)
        addiu $sp, $sp, 0x180
        .endm
        .globl hook
hook:
        SAVEALL
        jal menu_tick
        nop
        RESTOREALL
        # displaced: addiu sp,sp,-0x40 ; sd s1,0x28(sp)
        addiu $sp, $sp, -0x40
        j 0x2E33F8
        .word (0x3F<<26)|(29<<21)|(17<<16)|0x28

# u32 gcall(u32 fn, u32 a0, u32 a1, u32 a2): call a game function on a 16-byte aligned stack.
        .text
        .globl gcall
gcall:
        addiu $sp, $sp, -16
        sw    $ra, 0($sp)
        sw    $s0, 4($sp)
        move  $s0, $sp
        addiu $t0, $zero, -16
        and   $sp, $sp, $t0
        addiu $sp, $sp, -64
        move  $t9, $a0
        move  $a0, $a1
        move  $a1, $a2
        jalr  $t9
        move  $a2, $a3
        move  $sp, $s0
        lw    $ra, 0($sp)
        lw    $s0, 4($sp)
        jr    $ra
        addiu $sp, $sp, 16

# Entry of the play-state draw (0x145670): state_tick(this) remembers the state and applies the free camera.
        .globl hook_state
hook_state:
        SAVEALL
        jal   state_tick
        nop
        RESTOREALL
        addiu $sp, $sp, -0x2B0
        j     0x145678
        .word (0x39<<26)|(29<<21)|(20<<16)|0x24C

# Entry of the PS2 controller update (0x317480). Runs the original, then pad_after(ctrl).
        .globl hook_pad
hook_pad:
        lui   $at, %hi(pad_ret)
        sw    $ra, %lo(pad_ret)($at)
        lui   $at, %hi(pad_obj)
        sw    $a0, %lo(pad_obj)($at)
        lui   $ra, %hi(pad_post)
        addiu $ra, $ra, %lo(pad_post)
        addiu $sp, $sp, -0x50
        j     0x317488
        lw    $a1, 0x54($a0)
pad_post:
        SAVEALL
        lui   $at, %hi(pad_obj)
        jal   pad_after
        lw    $a0, %lo(pad_obj)($at)
        RESTOREALL
        lui   $at, %hi(pad_ret)
        lw    $ra, %lo(pad_ret)($at)
        jr    $ra
        nop
        .bss
        .align 2
pad_ret: .space 4
pad_obj: .space 4
        .text

# Entry of the state manager's transition processing (0x101160): safe point for state changes.
        .globl hook_mgr
hook_mgr:
        SAVEALL
        jal   mgr_tick
        nop
        RESTOREALL
        addiu $sp, $sp, -0x100
        j     0x101168
        .word (0x3F<<26)|(29<<21)|(16<<16)|0xA8

# Entry of the app update (0x105A98). app_gate(app) decides whether this frame's game update
# runs (pause / frame advance); when it returns 0 the update is skipped.
        .globl hook_app
hook_app:
        SAVEALL
        jal   app_gate
        nop
        lui   $at, %hi(g_gate)
        sw    $v0, %lo(g_gate)($at)
        RESTOREALL
        lui   $at, %hi(g_gate)
        lw    $at, %lo(g_gate)($at)
        beqz  $at, 1f
        nop
        addiu $sp, $sp, -0x30
        j     0x105AA0
        .word (0x39<<26)|(29<<21)|(20<<16)|0x10
1:      jr    $ra
        move  $v0, $zero

# Replaces "jal 0x2CE918" (frame time) at 0x105AA8: dt = app->dt * g_dtscale (if set).
        .globl dt_hook
dt_hook:
        .word (0x31<<26)|(4<<21)|(0<<16)|0x10          # lwc1 $f0, 0x10($a0)
        lui   $at, %hi(g_dtscale)
        lw    $t0, %lo(g_dtscale)($at)
        beqz  $t0, 2f
        nop
        .word (0x11<<26)|(4<<21)|(8<<16)|(1<<11)        # mtc1 $t0, $f1
        nop
        .word (0x11<<26)|(0x10<<21)|(1<<16)|(0<<11)|(0<<6)|2   # mul.s $f0, $f0, $f1
2:      jr    $ra
        nop

        .bss
        .align 2
        .globl g_gate
g_gate: .space 4
        .text

# u32 gcall5(u32 fn, u32 a0, u32 a1, u32 a2, u32 a3, u32 t0): like gcall, for game functions
# taking up to five integer arguments (EABI passes the fifth in $t0).
        .text
        .globl gcall5
gcall5:
        lw    $t1, 16($sp)
        lw    $t2, 20($sp)
        addiu $sp, $sp, -16
        sw    $ra, 0($sp)
        sw    $s0, 4($sp)
        move  $s0, $sp
        addiu $t3, $zero, -16
        and   $sp, $sp, $t3
        addiu $sp, $sp, -64
        move  $t9, $a0
        move  $a0, $a1
        move  $a1, $a2
        move  $a2, $a3
        move  $a3, $t1
        jalr  $t9
        move  $t0, $t2
        move  $sp, $s0
        lw    $ra, 0($sp)
        lw    $s0, 4($sp)
        jr    $ra
        addiu $sp, $sp, 16

# Replaces "jal 0x12DB30" (draw player HUD) at 0x145AE8: skipped while g_hide_hud is set.
        .globl hud_hook
hud_hook:
        lui   $at, %hi(g_hide_hud)
        lw    $at, %lo(g_hide_hud)($at)
        bnez  $at, 3f
        nop
        j     0x12DB30
        nop
3:      jr    $ra
        nop

# Instance setEnabled(db, inst, on) at 0x2FE828: tell scenery.c the game set this one itself.
        .globl hook_enable
hook_enable:
        SAVEALL
        jal   scenery_on_set
        move  $a0, $a1
        RESTOREALL
        addiu $sp, $sp, -0xA0
        j     0x2FE830
        move  $a3, $a1
