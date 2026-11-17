/* Show all scenery: switches on every visual instance the game has switched off.
 *
 * Levels hide whole groups of scenery until Bugs reaches them (trigger zones toggle layers),
 * which is why the free camera finds parts of the level missing. Instances are switched on and
 * off through one function, setEnabled(scene db, instance, on) at 0x2FE828; bit 1 of
 * instance+4 holds the result. While this is on, every visual instance (kVisualInstance, vtable
 * 0x49353C) that is off gets switched on, and remembered. Switching it off again puts those
 * back, except any the game itself has set since (a hook on 0x2FE828 forgets them). */
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
#define INST_ENABLE  0x2FB0E0
#define INST_DISABLE 0x2FB118

#define MAX_FORCED 3000
static u32 s_forced[MAX_FORCED];
static u32 s_n, s_busy, s_world;

static u32 valid(u32 p) { return p >= 0x100000 && p < 0x2000000 && !(p & 3); }

/* hook on setEnabled: the game decided this instance's state itself, so it's no longer ours */
void scenery_on_set(u32 inst)
{
    if (s_busy) return;
    for (u32 i = 0; i < s_n; i++)
        if (s_forced[i] == inst) { s_forced[i] = s_forced[--s_n]; return; }
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

static u32 s_frame;

/* Diagnostic pace: one instance every 4 frames, nearest to the camera first, with its address
 * on the status line so a crash shows which one caused it. */
void scenery_tick(u32 st)
{
    u32 world = 0, db = level_db(st, &world);
    if (world != s_world) { s_n = 0; s_world = world; }     /* new level: the old list is gone */
    if (!db) return;

    if (g_all_scenery) {
        if ((s_frame++ & 3) || s_n >= MAX_FORCED) return;
        u32 cam = game_camera(st);
        if (!cam) return;
        float cx = *(volatile float *)(cam + 0x70), cy = *(volatile float *)(cam + 0x74), cz = *(volatile float *)(cam + 0x78);
        u32 p = RD32(db + DB_ALL), n = 0, best = 0, left = 0;
        float bd = 0;
        while (valid(p) && n < 20000) {
            if (RD32(p) == VISUAL_VT && !(RD32(p + 4) & 2)) {
                int seen = 0;
                for (u32 i = 0; i < s_n; i++) if (s_forced[i] == p) { seen = 1; break; }
                if (!seen) {
                    float dx = *(volatile float *)(p + 0x10) - cx, dy = *(volatile float *)(p + 0x14) - cy, dz = *(volatile float *)(p + 0x18) - cz;
                    float d = dx * dx + dy * dy + dz * dz;
                    if (!best || d < bd) { best = p; bd = d; }
                    left++;
                }
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
        s_forced[s_n++] = best;
        s_busy = 1;
        gcall(INST_ENABLE, best, 0, 0);
        s_busy = 0;
    } else if (s_n) {
        for (u32 i = 0; i < s_n; i++) {
            u32 p = s_forced[i];
            if (valid(p) && RD32(p) == VISUAL_VT && (RD32(p + 4) & 2)) {
                s_busy = 1;
                gcall(INST_DISABLE, p, 0, 0);
                s_busy = 0;
            }
        }
        s_n = 0;
    }
}
