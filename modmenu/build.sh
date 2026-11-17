#!/bin/sh
set -e
cd "$(dirname "$0")"
CF="-target mipsel-none-elf -march=mips2 -mabi=32 -mhard-float -msingle-float -O2 -G0 -fno-pic -mno-abicalls -ffreestanding -fno-builtin -nostdlib -fno-jump-tables -mno-check-zero-division"
clang $CF -c hook.S -o hook.o
# regenerate vu/overlay.h when the game's executable is available (the committed copy is used otherwise)
ELF="${ACME_ELF:-/mnt/user-data/uploads/Looney Tunes - Acme Arsenal (Europe) (EnFrDeEsIt)/SLES_549.31}"
if [ -f "$ELF" ]; then python3 vu/overlay.py "$ELF"; fi
clang $CF -fno-math-errno -c menu.c -o menu.o
clang $CF -fno-math-errno -c freecam.c -o freecam.o
clang $CF -fno-math-errno -c hitbox.c -o hitbox.o
clang $CF -fno-math-errno -c scenery.c -o scenery.o
ld.lld -T link.ld hook.o menu.o freecam.o hitbox.o scenery.o -o menu.elf
llvm-objcopy -O binary --only-section=.text --only-section=.rodata menu.elf menu.bin
python3 mkpnach.py
# level select layout fix
clang -target mipsel-none-elf -march=mips2 -mabi=32 -mhard-float -fno-pic -mno-abicalls -c hubfix/hubgrid.S -o hubfix/hubgrid.o
ld.lld -T hubfix/hub.ld hubfix/hubgrid.o -o hubfix/hubgrid.elf
llvm-objcopy -O binary --only-section=.text hubfix/hubgrid.elf hubfix/hubgrid.bin
python3 - <<'PY'
import struct
b = open('hubfix/hubgrid.bin', 'rb').read()
lines = ['patch=1,EE,%08X,word,%08X' % (0x12BED8 + i, struct.unpack('<I', b[i:i+4])[0]) for i in range(0, len(b), 4)]
lines.append('// profiler text position: the developers used 30.0, but this text system is 0-1 screen units')
lines.append('patch=1,EE,0014A5B0,word,3C043D4C')
open('hubfix/hubgrid.pnach', 'w').write('// developer level select: 3x5 chapter grid (fixes the last two chapters overlapping top-left)\n' + '\n'.join(lines) + '\n')
PY
python3 package.py ../patches/SLES-54931_91C2DF01.pnach
