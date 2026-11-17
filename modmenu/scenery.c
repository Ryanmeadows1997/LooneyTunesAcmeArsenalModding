/* Show all scenery: shows every visual instance the level is hiding.
 *
 * A visual instance (kVisualInstance, vtable 0x49353C) has a visual state index at +0x70: -1
 * means hidden, 0..n-1 picks one of its element's n looks (element +0x28). The level hides
 * scenery by setting -1 until Bugs gets near (which is why the free camera finds parts of the
 * level missing). setState(inst, _, state) at 0x2F9980 (its state interface's +0x30 method)
 * stores the index and switches the instance on or off to match.
 *
 * Switching an instance on directly, with its state still -1, crashes the game. So this calls
 * setState(.., 0) on hidden instances, nearest the camera first, and remembers them; switching
 * it off sets them back to -1. Any the game sets itself in the meantime are forgotten (a hook
 * on setEnabled at 0x2FE828, which every state change goes through), so its choice stands.
 *
 * Animated scenery (vtables 0x491FC8 and 0x491BC8) has no stored index: its setState at
 * 0x2C7FF0 just switches it off for -1 (or an index past its element's count at +0x160) and
 * on otherwise. Those are shown when switched off, and set back to -1 afterwards. */
typedef unsigned int u32;
#define RD32(a) (*(volatile u32 *)(a))

u32 gcall(u32 fn, u32 a0, u32 a1, u32 a2);

u32 g_all_scenery;                 /* menu toggle */

#define GAME_WORLD  0x4FC
#define WORLD_VT    0x493A14
#define WORLD_DB    0x2C
#define DB_ALL      0xA8
#define INST_NEXT   0x40
#define VISUAL_VT   0x49353C
#define VIS_SET_STATE 0x2F9980
#define VIS_STATE(p)  (*(volatile short *)((p) + 0x70))
#define ANIM_VT1      0x491FC8
#define ANIM_VT2      0x491BC8
#define ANIM_SET_STATE 0x2C7FF0

/* hidden scenery we can show: returns its setState function, or 0 */
static u32 hidden_kind(u32 p)
{
    u32 vt = RD32(p);
    if (RD32(p + 4) & 2) return 0;                         /* already on */
    if (vt == VISUAL_VT) return VIS_STATE(p) == -1 ? VIS_SET_STATE : 0;
    if (vt == ANIM_VT1 || vt == ANIM_VT2) return ANIM_SET_STATE;
    return 0;
}

#define MAX_FORCED 3000
static u32 s_forced[MAX_FORCED], s_fn[MAX_FORCED];
static u32 s_n, s_busy, s_world;

static u32 valid(u32 p) { return p >= 0x100000 && p < 0x2000000 && !(p & 3); }

/* hook on setEnabled: the game decided this instance's state itself, so it's no longer ours */
void scenery_on_set(u32 inst)
{
    if (s_busy) return;
    for (u32 i = 0; i < s_n; i++)
        if (s_forced[i] == inst) { s_n--; s_forced[i] = s_forced[s_n]; s_fn[i] = s_fn[s_n]; return; }
}

static u32 level_db(u32 st, u32 *world)
{
    if (!valid(st)) return 0;
    u32 game = RD32(st + 0x30);
    if (!valid(game)) return 0;
    u32 w = RD32(game + GAME_WORLD);
    if (!valid(w) || RD32(w) != WORLD_VT) return 0;
    *world = w;
    u32 db = RD32(w + WORLD_DB);
    return valid(db) ? db : 0;
}

u32 game_camera(u32 st);           /* freecam.c */
char g_scenery_msg[48];            /* shown on the menu's status line while this is on */

static void hex8(char *d, u32 v)
{
    for (int i = 7; i >= 0; i--) { u32 n = (v >> (i * 4)) & 15; *d++ = (char)(n < 10 ? '0' + n : 'A' + n - 10); }
    *d = 0;
}
static char *app(char *d, const char *s) { while (*s) *d++ = *s++; *d = 0; return d; }
static char *dec(char *d, u32 v)
{
    char t[12]; int n = 0;
    do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) *d++ = t[--n];
    *d = 0; return d;
}

#define PER_FRAME 8
void scenery_tick(u32 st)
{
    u32 world = 0, db = level_db(st, &world);
    if (world != s_world) { s_n = 0; s_world = world; }     /* new level: the old list is gone */
    if (!db) return;

    if (g_all_scenery) {
        for (u32 k = 0; k < PER_FRAME && s_n < MAX_FORCED; k++) {
        u32 cam = game_camera(st);
        if (!cam) return;
        float cx = *(volatile float *)(cam + 0x70), cy = *(volatile float *)(cam + 0x74), cz = *(volatile float *)(cam + 0x78);
        u32 p = RD32(db + DB_ALL), n = 0, best = 0, left = 0;
        float bd = 0;
        while (valid(p) && n < 20000) {
            if (hidden_kind(p)) {
                /* (once shown it's switched on, so it isn't picked again) */
                float dx = *(volatile float *)(p + 0x10) - cx, dy = *(volatile float *)(p + 0x14) - cy, dz = *(volatile float *)(p + 0x18) - cz;
                float d = dx * dx + dy * dy + dz * dz;
                if (!best || d < bd) { best = p; bd = d; }
                left++;
            }
            p = RD32(p + INST_NEXT);
            n++;
        }
        char *m = app(g_scenery_msg, "Scenery on ");
        m = dec(m, s_n);
        m = app(m, ", left ");
        m = dec(m, left);
        if (!best) return;
        m = app(m, ", now ");
        hex8(m, best);
        u32 fn = hidden_kind(best);
        s_forced[s_n] = best; s_fn[s_n++] = fn;
        s_busy = 1;
        gcall(fn, best, 0, 0);
        s_busy = 0;
        }
    } else if (s_n) {
        for (u32 i = 0; i < s_n; i++) {
            u32 p = s_forced[i];
            u32 fn = s_fn[i], vt = valid(p) ? RD32(p) : 0;
            int ours = fn == VIS_SET_STATE ? (vt == VISUAL_VT && VIS_STATE(p) == 0)
                                           : ((vt == ANIM_VT1 || vt == ANIM_VT2) && (RD32(p + 4) & 2));
            if (ours) {
                s_busy = 1;
                gcall(fn, p, 0, 0xFFFFFFFF);
                s_busy = 0;
            }
        }
        s_n = 0;
    }
}
