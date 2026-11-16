/* Acme Arsenal mod menu (PAL SLES-54931). Runs once per frame from the text-system draw hook. */
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

#define RD32(a) (*(volatile u32 *)(a))
#define RDF(a)  (*(volatile float *)(a))

/* ---- game functions ---- */
u32 gcall(u32 fn, u32 a0, u32 a1, u32 a2);
u32 gcall5(u32 fn, u32 a0, u32 a1, u32 a2, u32 a3, u32 t0);
#define G(fn, a, b, c) gcall((fn), (u32)(a), (u32)(b), (u32)(c))
#define text_system()       ((void *)G(0x392ED4, 0, 0, 0))
#define text_create(s)      ((void *)G(0x2E3B50, s, 0, 0))
#define text_destroy(s, e)  G(0x2E3B70, s, e, 0)
#define text_update(e)      G(0x2E3D30, e, 0, 0)
#define kstr_from_c(k, c)   G(0x2F5540, k, c, 0)
#define kstr_assign(d, k)   G(0x2F5610, d, k, 0)
#define kstr_free(k, f)     G(0x2F55A0, k, f, 0)

/* player debug handlers left in the retail game (aiPlayer message handlers) */
#define PL_INVINCIBLE_ON    0x2434D8
#define PL_INVINCIBLE_OFF   0x243508
#define PL_SUICIDE          0x243530
#define PL_POWER_UP         0x2B0480    /* this = entity + 0x5D0 */
#define PL_POWER_DOWN       0x2B04D0
#define AIPLAYER_TO_ENTITY  0x510       /* aiPlayer sub-object -> entity base */
#define PL_CHEAT_INVISIBLE  0x1F5C48    /* (entity, on): the retail Invisibility cheat */
#define MENU_UNLOCK_ALL     0x111F60    /* (menuState + 0xB0): what DEVELOPER_LEVEL_UNLOCK does */
#define MENU_STATE_VTABLE   0x47BA14    /* main menu state (vtable at state + 0x18) */
#define APP_PTR             0x47B040    /* app; state manager at app + 0x40 */

/* ---- pad (libpad2 socket 0 DMA buffer, double buffered, active-low) ---- */
#define PAD_SOCKETS 0x008385C8
enum {
    B_SELECT = 0x0001, B_L3 = 0x0002, B_R3 = 0x0004, B_START = 0x0008,
    B_UP = 0x0010, B_RIGHT = 0x0020, B_DOWN = 0x0040, B_LEFT = 0x0080,
    B_L2 = 0x0100, B_R2 = 0x0200, B_L1 = 0x0400, B_R1 = 0x0800,
    B_TRI = 0x1000, B_CIRCLE = 0x2000, B_CROSS = 0x4000, B_SQUARE = 0x8000,
};

/* pressed buttons; if sticks != 0 also fills rx, ry, lx, ly (0..255, 128 = centre) */
u32 pad_raw(u8 *sticks)
{
    u32 buf = RD32(PAD_SOCKETS + 0x10);
    if (sticks) sticks[0] = sticks[1] = sticks[2] = sticks[3] = 128;
    if (buf < 0x100000 || buf >= 0x2000000)
        return 0;
    u32 a = buf, b = buf + 0x80;
    u32 cur = ((int)(RD32(b + 0x7C) - RD32(a + 0x7C)) > 0) ? b : a;
    u32 raw = *(volatile u8 *)(cur + 0x1C) | (*(volatile u8 *)(cur + 0x1D) << 8);
    if (sticks)
        for (int i = 0; i < 4; i++) sticks[i] = *(volatile u8 *)(cur + 0x1E + i);
    return ~raw & 0xFFFF;
}

/* shared with freecam.c */
extern u32 g_state, g_freecam, g_block_input, g_fc_active, g_fc_live, g_fc_entity;
void freecam_pre_update(void);
extern float g_fov_deg, g_far;          /* 0 = leave the game's value */
void freecam_bookmark(int load);
int freecam_pos(float *out);
extern u32 g_bookmark_set;

static u32 valid(u32 p) { return p >= 0x100000 && p < 0x2000000 && !(p & 3); }

/* ---- small string helpers (no libc) ---- */
static void scpy(char *d, const char *s) { while ((*d++ = *s++)) ; }
static char *sapp(char *d, const char *s) { while (*s) *d++ = *s++; *d = 0; return d; }
static int  seq(const char *a, const char *b) { while (*a && *a == *b) a++, b++; return *a == *b; }
static char *dec(char *d, int v)
{
    char t[12]; int n = 0;
    if (v < 0) { *d++ = '-'; v = -v; }
    do { t[n++] = '0' + v % 10; v /= 10; } while (v);
    while (n) *d++ = t[--n];
    *d = 0;
    return d;
}
/* float -> int (truncate) from the bit pattern: the EE has no trunc.w.s, so avoid the compiler's */
static int f2i(float f)
{
    union { float f; u32 u; } c = { f };
    u32 b = c.u;
    int e = (int)((b >> 23) & 0xFF) - 127;
    if (e < 0) return 0;
    if (e > 30) e = 30;
    u32 m = (b & 0x7FFFFF) | 0x800000;
    int v = (int)(e >= 23 ? m << (e - 23) : m >> (23 - e));
    return (b >> 31) ? -v : v;
}

/* ---- text ---- */
#define NLINES 11
#define LINELEN 60

static void *g_sys;
static u32 g_tmp[4] __attribute__((aligned(16)));
static void *g_elem[NLINES];
static char  g_shown[NLINES][LINELEN];
static char  g_line[NLINES][LINELEN];
static void *g_info;                 /* info overlay element (menu closed) */
static void *g_tab[8];               /* one element per page tab, so the active one can be coloured */
static char  g_tab_shown[8][16];
static void *g_loading;              /* "Loading..." message shown across a state switch */
static char  g_loading_shown[LINELEN];
static char  g_info_shown[LINELEN], g_info_line[LINELEN];

static u32 g_prev, g_open, g_page, g_cursor;
static const char *g_status;
static u32 g_player, g_entity, g_ent_frames, g_last_play;

static const u32 kWhite[4]  = { 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000 };
static const u32 kGreen[4]  = { 0x00000000, 0x3F800000, 0x00000000, 0x3F800000 };
static const u32 kYellow[4] = { 0x3F800000, 0x3F800000, 0x00000000, 0x3F800000 };
static const u32 kGrey[4]   = { 0x3F333333, 0x3F333333, 0x3F333333, 0x3F800000 };

static void set_color(void *e, const u32 *c)
{
    u32 *p = (u32 *)e;
    p[0] = c[0]; p[1] = c[1]; p[2] = c[2]; p[3] = c[3];
}

static void *make_text(float x, float y, float scale)
{
    void *e = text_create(g_sys);
    if (!e) return 0;
    RDF((u32)e + 0x20) = x;
    RDF((u32)e + 0x24) = y;
    RDF((u32)e + 0x28) = scale;
    *(volatile u8 *)((u32)e + 0x2C) = 1;
    set_color(e, kWhite);
    return e;
}

static void set_text(void *e, char *shown, const char *s)
{
    if (!e) return;
    if (!seq(shown, s)) {
        u32 *tmp = g_tmp;
        tmp[0] = tmp[1] = tmp[2] = tmp[3] = 0;
        kstr_from_c(tmp, s);
        kstr_assign((char *)e + 0x18, tmp);
        kstr_free(tmp, 2);
        scpy(shown, s);
    }
    text_update(e);
}

#define LINE_Y(i) (0.07f + 0.058f * (float)(i))
#define NTABS 5

static void open_menu(void)
{
    for (int i = 0; i < NLINES; i++) {
        g_elem[i] = make_text(0.06f, LINE_Y(i), 2.0f);
        g_shown[i][0] = 1;
        g_shown[i][1] = 0;
    }
    for (int i = 0; i < NTABS; i++) {
        g_tab[i] = make_text(0.06f + 0.165f * (float)i, LINE_Y(1), 1.7f);
        g_tab_shown[i][0] = 1;
        g_tab_shown[i][1] = 0;
    }
    g_open = 1;
}

static void close_menu(void)
{
    for (int i = 0; i < NLINES; i++) {
        if (g_elem[i]) text_destroy(g_sys, g_elem[i]);
        g_elem[i] = 0;
    }
    for (int i = 0; i < NTABS; i++) {
        if (g_tab[i]) text_destroy(g_sys, g_tab[i]);
        g_tab[i] = 0;
    }
    g_open = 0;
    g_block_input = 0;
}

static void show_loading(const char *msg)
{
    if (!g_loading) {
        g_loading = make_text(0.30f, 0.45f, 3.0f);
        g_loading_shown[0] = 1; g_loading_shown[1] = 0;
    }
    if (g_loading) {
        set_color(g_loading, kYellow);
        set_text(g_loading, g_loading_shown, msg);
    }
}
static void hide_loading(void)
{
    if (g_loading) text_destroy(g_sys, g_loading);
    g_loading = 0;
}

/* ---- deferred actions: run from the state manager hook, outside rendering ---- */
enum { ACT_NONE, ACT_WARP, ACT_PREVIEWER, ACT_INV_ON, ACT_INV_OFF, ACT_KILL, ACT_POWER_UP, ACT_POWER_DOWN,
       ACT_INVIS_ON, ACT_INVIS_OFF, ACT_FINISH, ACT_RESTART, ACT_TELEPORT, ACT_PROFILE };
static u32 g_action;
static u32 g_action_entity;

static u32 g_invincible;
static u32 g_inv_entity;      /* entity we last made invincible (re-apply on respawn / new level) */
static u32 g_info_on;
static u32 g_invisible, g_invis_entity;
static u32 g_unlock_levels;
u32 g_hide_hud;                    /* read by hud_hook */

/* ---- level timer: game time while a level is drawing, reset when a new level starts ---- */
static u32 g_timer_on, g_timer_frames, g_timer_state;
static void *g_timer_el;
static char g_timer_shown[24], g_timer_line[24];

/* ---- pause / frame advance / game speed ---- */
static u32 g_paused, g_step;
static float g_speed_pct;          /* slider: 0 = normal */
u32 g_dtscale;                     /* float bits read by dt_hook; 0 = unscaled */

#define MGR_TRANSITIONS 0x101160

u32 app_gate(u32 app)
{
    freecam_pre_update();
    if (!g_paused) return 1;
    if (g_step) { g_step--; return 1; }
    /* paused: still let state changes (and the menu's deferred actions) happen */
    G(MGR_TRANSITIONS, app + 0x40, 0, 0);
    return 0;
}

static void request(u32 act)
{
    g_action = act;
    g_action_entity = g_entity;
}

static u32 g_pending, g_pending_wait, g_loading_hold;

/* ---- menu model ---- */
enum { K_ACTION, K_TOGGLE, K_SLIDER };
typedef struct {
    const char *label;
    u8 kind;
    u8 arg;             /* action id or toggle id */
    float *val;         /* slider */
    float lo, hi, step;
    const char *help;
} Item;

enum { T_INV, T_FREECAM, T_INFO, T_INVIS, T_UNLOCK, T_PAUSE, T_HUD, T_TIMER };

static const Item kPlayer[] = {
    { "Invincible",         K_TOGGLE, T_INV,   0, 0, 0, 0, "Bugs takes no damage" },
    { "Invisible",          K_TOGGLE, T_INVIS, 0, 0, 0, 0, "The retail Invisibility cheat" },
    { "Power up weapon",    K_ACTION, ACT_POWER_UP,   0, 0, 0, 0, "One weapon level up" },
    { "Power down weapon",  K_ACTION, ACT_POWER_DOWN, 0, 0, 0, 0, "One weapon level down" },
    { "Kill / respawn",     K_ACTION, ACT_KILL,       0, 0, 0, 0, "Respawn at the last checkpoint" },
    { "Info overlay",       K_TOGGLE, T_INFO,  0, 0, 0, 0, "Position, speed and free memory" },
    { "Level timer",        K_TOGGLE, T_TIMER, 0, 0, 0, 0, "Game time in this level (pauses and loads don't count)" },
};
static const Item kCamera[] = {
    { "Free camera",        K_TOGGLE, T_FREECAM, 0, 0, 0, 0, "Sticks fly/look, R1/L1 up/down, R2 fast, L2 slow" },
    { "Field of view",      K_SLIDER, 0, &g_fov_deg, 20, 120, 5,  "Left/Right to change (game = 45)" },
    { "Draw distance",      K_SLIDER, 0, &g_far,     500, 20000, 500, "Left/Right to change (game = 1000)" },
    { "Save camera spot",   K_ACTION, 100, 0, 0, 0, 0, "Free camera: remember this view" },
    { "Go to camera spot",  K_ACTION, 101, 0, 0, 0, 0, "Free camera: jump to the saved view" },
    { "Bring Bugs here",    K_ACTION, ACT_TELEPORT, 0, 0, 0, 0, "Free camera: move Bugs to the camera" },
};
static const Item kGame[] = {
    { "Pause",             K_TOGGLE, T_PAUSE, 0, 0, 0, 0, "Freeze the game (menu and free camera still work)" },
    { "Step one frame",    K_ACTION, 102, 0, 0, 0, 0, "While paused. Shortcut: R3 with the menu closed" },
    { "Game speed %",      K_SLIDER, 0, &g_speed_pct, 10, 400, 10, "Left/Right to change (game = 100)" },
};
static const Item kLevels[] = {
    { "Level warp",          K_ACTION, ACT_WARP,      0, 0, 0, 0, "Developer level select (every level)" },
    { "Character Previewer", K_ACTION, ACT_PREVIEWER, 0, 0, 0, 0, "Developer model/animation viewer" },
    { "Unlock all levels",   K_TOGGLE, T_UNLOCK,      0, 0, 0, 0, "Every level in the main menu's level select" },
    { "Finish level",        K_ACTION, ACT_FINISH,    0, 0, 0, 0, "Levels started from the developer level select" },
    { "Restart level",       K_ACTION, ACT_RESTART,   0, 0, 0, 0, "Levels started from the developer level select" },
    { "Profile level",       K_ACTION, ACT_PROFILE,   0, 0, 0, 0, "Developers' memory sweep of this level (Start+Circle exits)" },
};
static const Item kVisual[] = {
    { "Hide HUD",            K_TOGGLE, T_HUD, 0, 0, 0, 0, "Health and power bars off, for clean screenshots" },
    { "Wireframe",           K_ACTION, 200, 0, 0, 0, 0, "Coming soon" },
    { "Collision hitboxes",  K_ACTION, 200, 0, 0, 0, 0, "Coming soon" },
};

typedef struct { const char *name; const Item *items; u32 count; } Page;
static const Page kPages[] = {
    { "PLAYER", kPlayer, sizeof(kPlayer) / sizeof(Item) },
    { "CAMERA", kCamera, sizeof(kCamera) / sizeof(Item) },
    { "GAME",   kGame,   sizeof(kGame) / sizeof(Item) },
    { "LEVELS", kLevels, sizeof(kLevels) / sizeof(Item) },
    { "VISUAL", kVisual, sizeof(kVisual) / sizeof(Item) },
};
#define NPAGES (sizeof(kPages) / sizeof(Page))

static u32 *toggle_ptr(u32 id)
{
    switch (id) {
    case T_INV: return &g_invincible;
    case T_FREECAM: return &g_freecam;
    case T_INVIS: return &g_invisible;
    case T_UNLOCK: return &g_unlock_levels;
    case T_PAUSE: return &g_paused;
    case T_HUD: return &g_hide_hud;
    case T_TIMER: return &g_timer_on;
    default: return &g_info_on;
    }
}

static void activate(const Item *it)
{
    if (it->kind == K_TOGGLE) {
        u32 *p = toggle_ptr(it->arg);
        *p = !*p;
        if (it->arg == T_INV) request(*p ? ACT_INV_ON : ACT_INV_OFF);
        if (it->arg == T_INVIS) request(*p ? ACT_INVIS_ON : ACT_INVIS_OFF);
        if (it->arg == T_FREECAM && *p) close_menu();
        return;
    }
    if (it->kind != K_ACTION) return;
    switch (it->arg) {
    case ACT_WARP:
    case ACT_PREVIEWER:
        close_menu();
        g_freecam = 0;
        show_loading(it->arg == ACT_WARP ? "Loading level select..." : "Loading previewer...");
        g_pending = it->arg;
        g_pending_wait = 3;          /* let a few frames draw the message before the switch */
        return;
    case ACT_TELEPORT:
        if (!g_freecam || !g_entity) { g_status = "Turn on the free camera in a level first"; return; }
        request(ACT_TELEPORT);
        g_status = "Moved Bugs to the camera";
        return;
    case ACT_FINISH:
    case ACT_RESTART:
    case ACT_PROFILE:
        request(it->arg);
        close_menu();
        return;
    case ACT_KILL: case ACT_POWER_UP: case ACT_POWER_DOWN:
        if (!g_entity) { g_status = "No player here"; return; }
        request(it->arg);
        g_status = "Done";
        return;
    case 102:
        if (!g_paused) { g_status = "Pause first"; return; }
        g_step++;
        g_status = "Stepped one frame";
        return;
    case 100:
    case 101:
        if (!g_freecam) { g_status = "Turn on the free camera first"; return; }
        if (it->arg == 101 && !g_bookmark_set) { g_status = "No camera spot saved yet"; return; }
        freecam_bookmark(it->arg == 101);
        g_status = it->arg == 100 ? "Camera spot saved" : "Moved to camera spot";
        return;
    default:
        g_status = "Not built yet";
    }
}

/* sliders: 0 means "use the game's value"; stepping below the minimum returns to 0 */
static void adjust(const Item *it, int dir)
{
    float v = *it->val;
    if (v == 0) v = dir > 0 ? it->lo : 0;
    else {
        v += it->step * (float)dir;
        if (v < it->lo) v = 0;
        if (v > it->hi) v = it->hi;
    }
    *it->val = v;
}

static void build_item_line(char *d, const Item *it, int selected)
{
    d = sapp(d, selected ? "> " : "  ");
    d = sapp(d, it->label);
    if (it->kind == K_TOGGLE)
        sapp(d, *toggle_ptr(it->arg) ? ": ON" : ": OFF");
    else if (it->kind == K_SLIDER) {
        d = sapp(d, ": < ");
        if (*it->val == 0) d = sapp(d, "game");
        else d = dec(d, f2i(*it->val));
        sapp(d, " >");
    }
}

/* ---- level warp / previewer: drop every running state and push one built from XML ---- */
/* One TinyXML document per state description, parsed once; the node is reused for every switch.
 * (Re-parsing into one document appends elements, and picking the right one proved unreliable.) */
static u32 g_doc_hub[0x40] __attribute__((aligned(16)));
static u32 g_doc_prev[0x40] __attribute__((aligned(16)));
static u32 g_node_hub, g_node_prev;
static const char kHubXml[] =
    "<GameState name=\"Hub\" type=\"aHubState\" file=\"GAMEDATA\\BOOT\\Hub.xml\"/>";
static const char kPreviewerXml[] =
    "<GameState name=\"Character\" type=\"aPreviewerState\"/>";

#define XML_DOC_CTOR    0x307C00
#define XML_DOC_PARSE   0x30B7F8
#define XML_DOC_ROOT    0x305758
#define HEAP_FOR        0x2EF870
#define STATE_FROM_XML  0x101E58
#define MGR_POP_TO      0x101750
#define MGR_PUSH        0x101568

static u32 parse_once(u32 *doc, u32 *node, const char *xml)
{
    if (!*node) {
        G(XML_DOC_CTOR, doc, 0, 0);
        G(XML_DOC_PARSE, doc, xml, 0);
        u32 n = G(XML_DOC_ROOT, doc, 0, 0);
        if (valid(n)) *node = n;
    }
    return *node;
}

static void switch_state(u32 mgr, u32 node)
{
    if (!valid(node)) return;
    u32 st = G(STATE_FROM_XML, node, G(HEAP_FOR, mgr, 0, 0), 0);
    if (!st) return;
    G(MGR_POP_TO, mgr, 0, 0);
    G(MGR_PUSH, mgr, st, 0);
}

static void unlock_levels(u32 mgr)
{
    u32 n = RD32(mgr + 4), arr = mgr + 8;
    if (n > 32) return;
    for (u32 i = 0; i < n; i++) {
        u32 st = RD32(arr + i * 4);
        if (valid(st) && RD32(st + 0x18) == MENU_STATE_VTABLE)
            G(MENU_UNLOCK_ALL, st + 0xB0, 0, 0);
    }
}

#define PLAY_STATE_VTABLE   0x47CAE8
#define HUB_STATE_VTABLE    0x47C59C
#define GAME_RESULT         0x146650    /* (game) -> result: +0xB4 ended, +0xB5 failed */
#define HUB_START_LEVEL     0x12CF30    /* (hub, mgr, chapter, act, profile) */

static u32 find_state(u32 mgr, u32 vtable)
{
    u32 n = RD32(mgr + 4);
    if (n > 32) return 0;
    for (u32 i = n; i-- > 0;) {
        u32 st = RD32(mgr + 8 + i * 4);
        if (valid(st) && RD32(st + 0x18) == vtable) return st;
    }
    return 0;
}

static const char *g_flow_msg;

/* Bugs' position lives in several places (entity, aiPlayer copy, physics body, render/AI copies).
 * Write all the ones found so physics doesn't pull him back. */
static void put_pos(u32 p, const float *v)
{
    if (!valid(p)) return;
    RDF(p) = v[0]; RDF(p + 4) = v[1]; RDF(p + 8) = v[2];
}
static void teleport_entity(u32 ent, const float *v)
{
    put_pos(ent + 0x20, v);
    put_pos(ent + 0x530, v);
    u32 phys = RD32(ent + 0x5C);
    if (valid(phys)) { put_pos(phys, v); put_pos(phys + 0x230, v); }
    u32 a = RD32(ent + 0x58);   if (valid(a)) put_pos(a, v);
    u32 b = RD32(ent + 0x6C);   if (valid(b)) put_pos(b + 0x10, v);
    u32 c = RD32(ent + 0x4C4);  if (valid(c)) { put_pos(c + 0x10, v); put_pos(c + 0xB0, v); }
}   /* shown after a finish/restart attempt fails */

static void finish_level(u32 mgr)
{
    u32 play = find_state(mgr, PLAY_STATE_VTABLE);
    if (!play) { g_flow_msg = "Not in a level"; return; }
    if (!find_state(mgr, HUB_STATE_VTABLE)) {
        g_flow_msg = "Finish needs a level started from the level select";
        return;
    }
    u32 game = RD32(play + 0x30);
    if (!valid(game)) return;
    u32 res = G(GAME_RESULT, game, 0, 0);
    if (!valid(res)) return;
    *(volatile u8 *)(res + 0xB4) = 1;     /* ended */
    *(volatile u8 *)(res + 0xB5) = 0;     /* not failed */
    G(MGR_POP_TO, mgr, play, 0);
    G(0x1016B8, mgr, play, 0);            /* remove the play state; the level flow carries on */
}

static void restart_level(u32 mgr, u32 profile)
{
    u32 hub = find_state(mgr, HUB_STATE_VTABLE);
    if (!hub) { g_flow_msg = "Restart needs a level started from the level select"; return; }
    u32 chapter = RD32(hub + 0x3C), act = RD32(hub + 0x38);
    if (!valid(chapter) || !valid(act)) return;
    G(MGR_POP_TO, mgr, hub, 0);
    gcall5(HUB_START_LEVEL, hub, mgr, chapter + 8, act + 8, profile);
}

void mgr_tick(u32 mgr)
{
    if (g_unlock_levels) unlock_levels(mgr);
    u32 act = g_action;
    u32 ent = g_action_entity;
    g_action = 0;

    switch (act) {
    case ACT_WARP:      switch_state(mgr, parse_once(g_doc_hub, &g_node_hub, kHubXml)); return;
    case ACT_PREVIEWER: switch_state(mgr, parse_once(g_doc_prev, &g_node_prev, kPreviewerXml)); return;
    case ACT_FINISH:    finish_level(mgr); return;
    case ACT_RESTART:   restart_level(mgr, 0); return;
    case ACT_PROFILE:   restart_level(mgr, 1); return;
    }
    if (!valid(ent) || ent != g_entity)     /* player changed since the request: skip */
        return;
    switch (act) {
    case ACT_INV_ON:     G(PL_INVINCIBLE_ON, ent, 0, 0); g_inv_entity = ent; break;
    case ACT_INV_OFF:    G(PL_INVINCIBLE_OFF, ent, 0, 0); g_inv_entity = 0; break;
    case ACT_KILL:       G(PL_SUICIDE, ent, 0, 0); break;
    case ACT_POWER_UP:   G(PL_POWER_UP, ent + 0x5D0, 0, 0); break;
    case ACT_POWER_DOWN: G(PL_POWER_DOWN, ent + 0x5D0, 0, 0); break;
    case ACT_INVIS_ON:   G(PL_CHEAT_INVISIBLE, ent, 1, 0); g_invis_entity = ent; break;
    case ACT_INVIS_OFF:  G(PL_CHEAT_INVISIBLE, ent, 0, 0); g_invis_entity = 0; break;
    case ACT_TELEPORT: {
        float v[3];
        if (freecam_pos(v)) { v[2] -= 10.0f; teleport_entity(ent, v); }
        break;
    }
    }
}

/* ---- free memory, the way the developers' profiler measures it ---- */
#define MEM_SYSTEM   0x2EE320
#define MEM_FREE     0x2EE4F0
static u32 g_mem_min;

static char *mem_text(char *d)
{
    u32 sys = G(MEM_SYSTEM, 0, 0, 0);
    if (!valid(sys)) return d;
    u32 bytes = G(MEM_FREE, sys, 0, 0);
    if (!g_mem_min || bytes < g_mem_min) g_mem_min = bytes;
    u32 kb = bytes >> 10, mb100 = (kb * 100) >> 10;     /* MB with two decimals */
    d = sapp(d, "  MEM ");
    d = dec(d, (int)(mb100 / 100)); d = sapp(d, ".");
    if (mb100 % 100 < 10) d = sapp(d, "0");
    d = dec(d, (int)(mb100 % 100));
    return sapp(d, "MB");
}

/* ---- info overlay ---- */
static float g_lastpos[2];
static u32 g_lastpos_ok, g_since_move, g_speed;

static void update_info(void)
{
    if (!g_info_on || g_open || !g_player) {     /* nothing to show outside gameplay */
        if (g_info) { text_destroy(g_sys, g_info); g_info = 0; }
        return;
    }
    if (!g_info) {
        g_info = make_text(0.06f, 0.86f, 1.8f);
        g_info_shown[0] = 1; g_info_shown[1] = 0;
        if (!g_info) return;
    }
    char *d = g_info_line;
    if (!g_player) {
        sapp(d, "no player");
    } else {
        u32 e = g_entity ? g_entity : g_player;
        float x = RDF(e + 0x20), y = RDF(e + 0x24), z = RDF(e + 0x28);
        d = sapp(d, "X "); d = dec(d, f2i(x));
        d = sapp(d, "  Y "); d = dec(d, f2i(y));
        d = sapp(d, "  Z "); d = dec(d, f2i(z));
        /* horizontal speed in units per second (PAL, 50 fps). If the position hasn't changed
         * this frame, keep showing the last speed; after half a second still, show 0. */
        if (g_lastpos_ok) {
            float dx = x - g_lastpos[0], dy = y - g_lastpos[1];
            float h = dx * dx + dy * dy;
            g_since_move++;
            if (h > 0) {
                g_speed = f2i(__builtin_sqrtf(h) * 50.0f / (float)g_since_move);
                g_since_move = 0;
            } else if (g_since_move > 25) g_speed = 0;
        }
        d = sapp(d, "  SPD "); d = dec(d, (int)g_speed);
        mem_text(d);
        g_lastpos[0] = x; g_lastpos[1] = y;
        g_lastpos_ok = 1;
    }
    set_text(g_info, g_info_shown, g_info_line);
}

static void update_timer(u32 st, u32 counting)
{
    /* Counts frames in which a level was drawn and the game wasn't paused, scaled by game speed.
     * Resets when a level starts: the play state wasn't drawn last frame (loading) or changed.
     * (A restarted level can reuse the same address, so the gap is what really matters.) */
    if (!st) g_timer_state = 0;
    else {
        if (st != g_timer_state) { g_timer_state = st; g_timer_frames = 0; }
        if (counting && !g_paused) {
            u32 add = 100;                       /* hundredths of a frame */
            if (g_speed_pct != 0) add = (u32)f2i(g_speed_pct);
            g_timer_frames += add;
        }
    }
    if (!g_timer_on || !st || g_open) {
        if (g_timer_el) { text_destroy(g_sys, g_timer_el); g_timer_el = 0; }
        return;
    }
    if (!g_timer_el) {
        g_timer_el = make_text(0.72f, 0.05f, 2.0f);
        g_timer_shown[0] = 1; g_timer_shown[1] = 0;
        if (!g_timer_el) return;
        set_color(g_timer_el, kYellow);
    }
    /* PAL: 50 frames a second -> 2 centiseconds per frame */
    u32 cs = g_timer_frames / 50;               /* (frames*100)/50 /100*... = centiseconds */
    u32 m = cs / 6000, sec = (cs / 100) % 60, c = cs % 100;
    char *d = g_timer_line;
    d = dec(d, (int)m); d = sapp(d, ":");
    if (sec < 10) d = sapp(d, "0");
    d = dec(d, (int)sec); d = sapp(d, ".");
    if (c < 10) d = sapp(d, "0");
    dec(d, (int)c);
    set_text(g_timer_el, g_timer_shown, g_timer_line);
}

void menu_tick(void *sys)
{
    (void)sys;
    g_sys = text_system();

    /* player for this frame (only valid if the play state drew this frame) */
    {
        u32 st = g_state, pl = 0, ent = 0;
        g_state = 0;
        g_last_play = valid(st) ? st : 0;
        g_fc_live = g_fc_active;     /* free camera only blocks the pad while it's actually live */
        g_fc_active = 0;
        if (valid(st)) {
            u32 game = RD32(st + 0x30);
            if (valid(game)) pl = G(0x146660, game, 0, 0);
        }
        if (valid(pl)) {
            ent = pl - AIPLAYER_TO_ENTITY;
            u32 vt = RD32(ent + 0x90);
            if (vt < 0x470000 || vt >= 0x4C0000) ent = 0;
        } else pl = 0;
        if (!pl) g_lastpos_ok = 0;
        g_player = pl;
        if (ent != g_entity) g_ent_frames = 0; else if (g_ent_frames < 1000) g_ent_frames++;
        g_entity = ent;
        g_fc_entity = ent;
        /* keep invincibility on through respawns and new levels */
        /* wait until a (re)spawned Bugs has existed for half a second */
        if (g_ent_frames < 25) ;
        else if (g_invincible && ent && ent != g_inv_entity && !g_action)
            request(ACT_INV_ON);
        else if (g_invisible && ent && ent != g_invis_entity && !g_action)
            request(ACT_INVIS_ON);
    }

    /* deferred state switch, and removing the loading message once the new state is drawing */
    if (g_pending) {
        if (g_loading) set_text(g_loading, g_loading_shown, g_loading_shown);
        if (--g_pending_wait == 0) { request(g_pending); g_pending = 0; g_loading_hold = 10; }
    } else if (g_loading && !g_action) {
        set_text(g_loading, g_loading_shown, g_loading_shown);
        if (g_loading_hold && --g_loading_hold == 0) hide_loading();
    }

    u32 pad = pad_raw(0);
    u32 hit = pad & ~g_prev;
    g_prev = pad;

    /* game speed: dt_hook multiplies the frame time by this */
    {
        union { float f; u32 u; } sc;
        sc.f = g_speed_pct / 100.0f;
        g_dtscale = g_speed_pct == 0 ? 0 : sc.u;
    }
    /* frame advance shortcut: R3 alone while paused and the menu is closed */
    if (g_paused && !g_open && (hit & B_R3) && !(pad & B_L3))
        g_step++;

    if ((pad & (B_L3 | B_R3)) == (B_L3 | B_R3) && (hit & (B_L3 | B_R3))) {
        if (g_open) close_menu();
        else { open_menu(); g_status = g_flow_msg; g_flow_msg = 0; }
        g_block_input = g_open;
        update_info();
        return;
    }
    g_block_input = g_open;
    update_info();
    update_timer(g_last_play, g_player != 0);
    if (!g_open)
        return;

    const Page *pg = &kPages[g_page];
    if (g_cursor >= pg->count) g_cursor = 0;
    const Item *it = &pg->items[g_cursor];

    if (hit & B_UP)   { g_cursor = (g_cursor + pg->count - 1) % pg->count; g_status = 0; }
    if (hit & B_DOWN) { g_cursor = (g_cursor + 1) % pg->count; g_status = 0; }
    if (hit & (B_LEFT | B_RIGHT)) {
        int dir = (hit & B_RIGHT) ? 1 : -1;
        if (it->kind == K_SLIDER) adjust(it, dir);
        else { g_page = (g_page + NPAGES + dir) % NPAGES; g_cursor = 0; g_status = 0; }
    }
    if (hit & B_CROSS) activate(it);
    if (!g_open) return;                 /* activate() may close the menu */

    pg = &kPages[g_page];
    it = &pg->items[g_cursor];

    /* line 0: title, line 1: page tabs, then items, then help/status and controls */
    scpy(g_line[0], "ACME ARSENAL MOD MENU");
    g_line[1][0] = 0;
    for (u32 i = 0; i < NTABS && i < NPAGES; i++) {
        if (!g_tab[i]) continue;
        char t[16], *d = t;
        d = sapp(d, i == g_page ? "[" : " ");
        d = sapp(d, kPages[i].name);
        sapp(d, i == g_page ? "]" : " ");
        set_color(g_tab[i], i == g_page ? kYellow : kGrey);
        set_text(g_tab[i], g_tab_shown[i], t);
    }
    for (u32 i = 0; i < NLINES - 4; i++) {
        if (i < pg->count) build_item_line(g_line[2 + i], &pg->items[i], i == g_cursor);
        else g_line[2 + i][0] = 0;
    }
    scpy(g_line[NLINES - 2], g_status ? g_status : it->help);
    scpy(g_line[NLINES - 1], "X select  Left/Right page  L3+R3 close");

    for (u32 i = 0; i < NLINES; i++) {
        if (!g_elem[i]) continue;
        const u32 *c = kWhite;
        if (i <= 1) c = kYellow;
        else if (i == 2 + g_cursor) c = kGreen;
        else if (i >= NLINES - 2) c = kGrey;
        set_color(g_elem[i], c);
        set_text(g_elem[i], g_shown[i], g_line[i]);
    }
}
