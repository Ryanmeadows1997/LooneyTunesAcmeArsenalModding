"""Builds the VU1 'lines on top' routine and the per-program hook table, and writes overlay.h.

The game draws every model batch with VU1: the microprogram builds a GS packet in VU1 memory
([material: GIF A+D tag + 6 regs][vertex GIF tag][vertices][EOP tag]) and XGKICKs it. At the
batch-end kick sites the branch back to the program end (delay slot = the XGKICK) is pointed
at our routine instead. It waits for the solid packet to finish, rewrites the material in place
(PRMODE: fog only, CLAMP slot -> FOGCOL, ALPHA slot -> FOG = 0) and the vertex tag's PRIM to
line strip, kicks the same packet again, waits, and returns to the program end."""
import struct, sys, os

NOPL, NOPU = 0x8000033C, 0x000002FF
D = {'x': 8, 'y': 4, 'z': 2, 'w': 1}
def dest(s): return sum(D[c] for c in s)
def i15(op, it, is_, imm): return (op << 25) | (((imm >> 11) & 0xF) << 21) | (it << 16) | (is_ << 11) | (imm & 0x7FF)
def iaddiu(it, is_, imm): return i15(0x08, it, is_, imm)
def isubiu(it, is_, imm): return i15(0x09, it, is_, imm)
def iaddi(it, is_, imm5): return (0x40 << 25) | (it << 16) | (is_ << 11) | ((imm5 & 31) << 6) | 0x32
def ialu(f, id_, is_, it): return (0x40 << 25) | (it << 16) | (is_ << 11) | (id_ << 6) | f
def iadd(d, s, t): return ialu(0x30, d, s, t)
def ior(d, s, t): return ialu(0x35, d, s, t)
def ilw(dst, it, imm, is_): return (0x04 << 25) | (dest(dst) << 21) | (it << 16) | (is_ << 11) | (imm & 0x7FF)
def isw(dst, it, imm, is_): return (0x05 << 25) | (dest(dst) << 21) | (it << 16) | (is_ << 11) | (imm & 0x7FF)
def sq(dst, fs, imm, it): return (0x01 << 25) | (dest(dst) << 21) | (it << 16) | (fs << 11) | (imm & 0x7FF)
def ext(e, it=0, is_=0, dst=''): return (0x40 << 25) | (dest(dst) << 21) | (it << 16) | (is_ << 11) | ((e >> 2) << 6) | 0x3C | (e & 3)
def xgkick(is_): return ext(0x6C, 0, is_)
def mfir(dst, ft, is_): return ext(0x3D, ft, is_, dst)
def br(op, it, is_, off): return (op << 25) | (it << 16) | (is_ << 11) | (off & 0x7FF)
LOI = 0x80000000 | NOPU                                         # upper NOP, lower field -> I
ADDI_Y_VF31 = (dest('y') << 21) | (31 << 6) | 0x22               # addi.y vf31, vf0, I
# The I register written by a LOI is only usable from the NEXT instruction (the game's own code
# does LOI then MULi). Using it in the same pair reads the previous value.

BASE = 0x2A0          # free micro memory: the largest program ends at 0x293
prog = []             # (lower, upper, label-or-None, fixup)
labels = {}
def L(name): labels[name] = len(prog)
def emit(lo, up=NOPU): prog.append([lo, up, None])
def emitb(op, it, is_, target): prog.append([op, it, ('br', is_, target)])

# stubs: A = packet in vi8, program end at 0x10; B = packet in vi4, program end at 0x0
# after the body, put back the I value each program sets (in case it relies on it surviving)
L('stubA'); emitb(0x21, 14, 0, 'body'); emit(iaddiu(15, 8, 0)); emit(0x3F000000, LOI); emitb(0x20, 0, 0, 0x10); emit(NOPL)
L('stubB'); emitb(0x21, 14, 0, 'body'); emit(iaddiu(15, 4, 0)); emit(0x00808001, LOI); emitb(0x20, 0, 0, 0x00); emit(NOPL)
L('body')
emit(iaddiu(10, 0, 3))          # qword 3 holds an empty EOP tag
emit(xgkick(10))                # stalls until the solid packet has gone
emit(ilw('x', 11, 0, 15))       # material tag NLOOP = 1
emit(ilw('z', 12, 1, 15))       # first A+D register = PRMODE (0x1B)
emit(isubiu(11, 11, 1))
emit(isubiu(12, 12, 0x1B))
emit(ior(11, 11, 12))
emit(NOPL)
emitb(0x29, 11, 0, 'done'); emit(NOPL)          # ibne vi11, vi0
emit(ilw('y', 12, 7, 15))       # vertex tag bits 32..47
emit(iaddiu(11, 0, 0x4000))
emit(NOPL)
emitb(0x28, 11, 12, 'strip'); emit(NOPL)       # ibeq: strip tag (PRIM 4)
emit(iaddiu(11, 0, 0x6000))
emit(iadd(11, 11, 11))          # 0xC000: triangle list tag (PRIM 3)
emit(NOPL)
emitb(0x29, 11, 12, 'done'); emit(NOPL)
emit(0x90014000, LOI)           # line strip, NREG 9
emit(NOPL, ADDI_Y_VF31)
emitb(0x20, 0, 0, 'common'); emit(NOPL)
L('strip')
emit(0x30014000, LOI)           # line strip, NREG 3
emit(NOPL, ADDI_Y_VF31)
L('common')
emit(sq('y', 31, 7, 15))
emit(iaddiu(11, 0, 0x20))       # PRMODE: fog only (no texture, no blending)
emit(isw('x', 11, 1, 15))
L('colour'); emit(iaddi(13, 0, -1))   # -1 -> FOGCOL 0xFFFFFFFF (white); patched to 0 for black
emit(mfir('x', 31, 13))
emit(sq('x', 31, 2, 15))
emit(iaddiu(11, 0, 0x3D))       # CLAMP slot -> FOGCOL
emit(isw('z', 11, 2, 15))
emit(isw('y', 0, 4, 15))        # ALPHA slot -> FOG, F = 0 (all fog colour)
emit(iaddiu(11, 0, 0x0A))
emit(isw('z', 11, 4, 15))
emit(xgkick(15))                # lines
emit(xgkick(10))                # wait for them before the program ends
L('done')
emitb(0x24, 0, 14, None); emit(NOPL)          # jr vi14

words = []
for k, (a, b, fix) in enumerate(prog):
    if fix and fix[0] == 'br':
        op, it, (_, is_, tgt) = a, b, fix
        if tgt is None: lo = (0x24 << 25) | (14 << 11)
        else:
            t = labels[tgt] + BASE if isinstance(tgt, str) else tgt
            lo = br(op, it, is_, t - (BASE + k + 1))
        words.append((lo, NOPU))
    else:
        words.append((a, b))

# hook sites: (MPG packet VA of the program's first packet, site of the branch, stub)
E = sys.argv[1]
elf = open(E, 'rb').read()
def ea(v): return v - 0x100000 + 0x1000
def packets(first):
    out = {}; va = first
    while True:
        for skip in range(0, 20, 4):                    # DMA tag / VIF NOPs between packets
            w = struct.unpack_from('<I', elf, ea(va + skip))[0]
            if (w >> 24) & 0x7F == 0x4A: va += skip; break
        else: break
        num = ((w >> 16) & 0xFF) or 256; addr = w & 0xFFFF
        if addr == 0 and out: break
        out[addr] = (va + 4, num); va += 4 + num * 8
    return out
def site_va(first, k):
    for addr, (data, num) in packets(first).items():
        if addr <= k < addr + num: return data + (k - addr) * 8
    raise SystemExit('site not in program')
sites = []
for first, k, stub in ((0x49650C, 0xC9, 'stubA'), (0x49650C, 0x1DB, 'stubA'),
                       (0x4986CC, 0xDA, 'stubA'), (0x4986CC, 0x1FA, 'stubA'),
                       (0x49571C, 0xF9, 'stubB'), (0x49571C, 0x1B4, 'stubB'),
                       (0x4978DC, 0xF9, 'stubB'), (0x4978DC, 0x1B4, 'stubB')):
    va = site_va(first, k)
    orig, = struct.unpack_from('<I', elf, ea(va))
    kick, = struct.unpack_from('<I', elf, ea(va) + 8)
    assert orig >> 25 == 0x20, hex(orig)
    assert ((kick >> 6) & 31, kick & 0x3F) == (0x1B, 0x3C), hex(kick)
    new = (orig & ~0x7FF) | ((labels[stub] + BASE - (k + 1)) & 0x7FF)
    sites.append((va, k, orig, new, kick))

if '--list' in sys.argv:
    sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../../tools'))
    import vudis
    for k, (lo, up) in enumerate(words):
        l = ('loi %08x' % lo) if up >> 31 & 1 else vudis.lower(lo)
        print(f'{BASE + k:04x} {vudis.upper(up):26} | {l}')
    for s in sites: print('site', hex(s[0]), hex(s[1]), vudis.lower(s[2]), '->', vudis.lower(s[3]))

with open(os.path.join(os.path.dirname(__file__), 'overlay.h'), 'w') as f:
    f.write('/* generated by vu/overlay.py */\n')
    f.write('#define OV_BASE 0x%X\n#define OV_COLOUR_INDEX %d\n' % (BASE, labels['colour']))
    f.write('#define OV_WHITE 0x%08Xu\n#define OV_BLACK 0x%08Xu\n' % (iaddi(13, 0, -1), iaddiu(13, 0, 0)))
    f.write('static const u32 kOvCode[][2] = {\n' + ''.join('    { 0x%08X, 0x%08X },\n' % w for w in words) + '};\n')
    f.write('typedef struct { u32 va, site, orig, hooked, kick; } OvSite;\n')
    f.write('static const OvSite kOvSites[] = {\n' + ''.join('    { 0x%06X, 0x%03X, 0x%08X, 0x%08X, 0x%08X },\n' % s for s in sites) + '};\n')
