import struct, subprocess
BASE = 0xC0000
code = open('menu.bin', 'rb').read()
code += b'\0' * (-len(code) % 4)
# The EE's FPU only has C.F/C.EQ/C.LT/C.LE. Clang emits the 'unordered' compare variants
# (c.ult.s etc.), which the EE doesn't implement. There are no NaNs on the EE, so map them
# to the ordered forms.
_fix = 0
_w = list(struct.unpack('<%dI' % (len(code) // 4), code))
for _i, _x in enumerate(_w):
    if (_x >> 26) == 0x11 and ((_x >> 21) & 31) == 0x10 and (_x & 0x30) == 0x30:
        _f = _x & 0x3F
        _nf = (_f & ~0x09) | 0x30          # drop 'unordered' (bit0) and 'signalling' (bit3)
        if _f != _nf:
            _w[_i] = (_x & ~0x3F) | _nf; _fix += 1
# The EE's SQRT.S takes its operand from the ft field (bits 16-20); standard MIPS uses fs
# (bits 11-15) with ft = 0, which on the EE means sqrt($f0). Move the operand across.
_sq = 0
for _i, _x in enumerate(_w):
    if (_x >> 26) == 0x11 and ((_x >> 21) & 31) == 0x10 and (_x & 0x3F) == 0x04 and ((_x >> 16) & 31) == 0:
        _fs = (_x >> 11) & 31
        _w[_i] = (_x & ~(0x1F << 11)) | (_fs << 16); _sq += 1
code = struct.pack('<%dI' % len(_w), *_w)
print('fixed', _fix, 'FPU compares,', _sq, 'sqrt.s')
hook = 0xC0000
def j(t): return (0x02 << 26) | ((t >> 2) & 0x3FFFFFF)
lines = ['[Mod menu (L3+R3)]',
         'author=Glitch_Me_Up',
         'description=In-game mod menu. Press L3+R3 (click both sticks) to open. PAL only.',
         '// menu code at 0x%X (%d bytes)' % (BASE, len(code))]
for i in range(0, len(code), 4):
    w = struct.unpack('<I', code[i:i+4])[0]
    lines.append('patch=1,EE,%08X,word,%08X' % (BASE + i, w))
import subprocess as sp
syms=dict((l.split()[2],int(l.split()[0],16)) for l in sp.run(['llvm-nm','menu.elf'],capture_output=True,text=True).stdout.splitlines() if len(l.split())==3)
lines.append('// hook: play-state draw 0x145670 -> remember state')
lines.append('patch=1,EE,00145670,word,%08X' % j(syms['hook_state']))
lines.append('patch=1,EE,00145674,word,00000000')
lines.append('// hook: PS2 controller update 0x317480 -> hide input from the game while menu/free camera is active')
lines.append('patch=1,EE,00317480,word,%08X' % j(syms['hook_pad']))
lines.append('patch=1,EE,00317484,word,00000000')
lines.append('// hook: app update 0x105A98 -> pause / frame advance')
lines.append('patch=1,EE,00105A98,word,%08X' % j(syms['hook_app']))
lines.append('patch=1,EE,00105A9C,word,00000000')
lines.append('// frame time read at 0x105AA8 -> scaled by the game speed setting')
lines.append('patch=1,EE,00105AA8,word,%08X' % ((0x03 << 26) | ((syms['dt_hook'] >> 2) & 0x3FFFFFF)))
lines.append('// player HUD draw call at 0x145AE8 -> skipped when the HUD is hidden')
lines.append('patch=1,EE,00145AE8,word,%08X' % ((0x03 << 26) | ((syms['hud_hook'] >> 2) & 0x3FFFFFF)))
lines.append('// hook: state manager transitions 0x101160 -> level warp')
lines.append('patch=1,EE,00101160,word,%08X' % j(syms['hook_mgr']))
lines.append('patch=1,EE,00101164,word,00000000')
lines.append('// hook: text-system draw 0x2E33F0 -> menu')
lines.append('patch=1,EE,002E33F0,word,%08X' % j(hook))
lines.append('patch=1,EE,002E33F4,word,00000000')
open('modmenu.pnach', 'w').write('\n'.join(lines) + '\n')
print(len(code), 'bytes,', len(lines), 'lines')
