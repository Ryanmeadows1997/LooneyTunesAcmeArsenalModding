# Builds the full PCSX2 patch file: the repo's base patches + the dev level select layout fix
# + the mod menu section (which also carries the previewer fixes, so its "Character Previewer"
# item works without the separate boot patch).
import sys, os
HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.join(HERE, 'base_patches.pnach')
base = open(REPO).read()
fix = open(os.path.join(HERE, 'hubfix', 'hubgrid.pnach')).read()
mm = open(os.path.join(HERE, 'modmenu.pnach')).read()

def section(text, title):
    i = text.index('[' + title)
    j = text.find('\n[', i + 1)
    return i, (len(text) if j < 0 else j)

# previewer fixes (everything after the string swap)
i, j = section(base, 'Boot: Character Previewer')
prev = base[i:j]
prev_fixes = prev[prev.index('// Fix 1'):].rstrip('\n') + '\n'

# layout fix into the Development boot section
i, j = section(base, 'Boot: Development')
if '0012BED8' not in base[i:j]:
    base = base[:j].rstrip('\n') + '\n' + fix + base[j:]

out = base.rstrip('\n') + '\n\n' + mm.rstrip('\n') + '\n' + fix + '// Character Previewer fixes (for the menu\'s previewer item)\n' + prev_fixes
dst = sys.argv[1]
os.makedirs(os.path.dirname(os.path.abspath(dst)), exist_ok=True)
open(dst, 'w').write(out)
print(dst, len(out.splitlines()), 'lines')
