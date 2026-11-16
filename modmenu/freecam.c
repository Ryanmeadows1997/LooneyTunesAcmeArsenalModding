/* Free camera: overrides the active game camera just before the play state draws.
 * World is Z-up. Camera object layout (+0x70 position, +0x80 up, +0x90 forward, +0xA0 right). */
typedef unsigned int u32;
typedef unsigned char u8;
#define RD32(a) (*(volatile u32 *)(a))
#define RDF(a)  (*(volatile float *)(a))

u32 gcall(u32 fn, u32 a0, u32 a1, u32 a2);
u32 pad_raw(u8 *sticks);   /* menu.c: pressed buttons, sticks = rx ry lx ly */

u32 g_state;               /* play state seen this frame (menu.c consumes it) */
u32 g_freecam;             /* on/off, set by the menu */
u32 g_block_input;         /* menu.c sets while the menu is open */
float g_fov_deg;           /* 0 = game's own field of view */
float g_far;               /* 0 = game's own far clip */
u32 g_bookmark_set;
u32 g_fc_active;           /* free camera drove a camera this frame (menu.c latches it into g_fc_live) */
u32 g_fc_live;

enum { B_L1 = 0x0400, B_R1 = 0x0800, B_L2 = 0x0100, B_R2 = 0x0200 };

#define PI      3.14159265f
#define HALF_PI 1.57079633f

static float fsin(float x)
{
    while (x > PI)  x -= 2.0f * PI;
    while (x < -PI) x += 2.0f * PI;
    if (x > HALF_PI)  x = PI - x;
    if (x < -HALF_PI) x = -PI - x;
    float x2 = x * x;
    return x * (1.0f + x2 * (-1.0f / 6 + x2 * (1.0f / 120 + x2 * (-1.0f / 5040 + x2 * (1.0f / 362880)))));
}
static float fcos(float x) { return fsin(x + HALF_PI); }
static float fabsf_(float x) { return x < 0 ? -x : x; }
static float fatan(float z)
{
    float z2 = z * z;
    return z * (0.9998660f + z2 * (-0.3302995f + z2 * (0.1801410f + z2 * (-0.0851330f + z2 * 0.0208351f))));
}
static float fatan2(float y, float x)
{
    if (x == 0.0f && y == 0.0f) return 0.0f;
    if (fabsf_(x) >= fabsf_(y)) {
        float a = fatan(y / x);
        if (x < 0) a += (y >= 0) ? PI : -PI;
        return a;
    }
    return ((y > 0) ? HALF_PI : -HALF_PI) - fatan(x / y);
}

static u32 valid(u32 p) { return p >= 0x100000 && p < 0x2000000 && !(p & 3); }

/* active camera for player 0: game->vtbl[0x48](game, 0) */
static u32 find_camera(u32 st)
{
    u32 game = RD32(st + 0x30);
    if (!valid(game)) return 0;
    u32 x = game + 0x30;
    u32 vt = RD32(x + 0x57C);
    if (!valid(vt)) return 0;
    short adj = *(volatile short *)(vt + 0x48);
    u32 fn = RD32(vt + 0x4C);
    if (fn < 0x100000 || fn >= 0x450000) return 0;
    u32 cam = gcall(fn, x + adj, 0, 0);
    return valid(cam) ? cam : 0;
}

static u32 s_init;
static float s_x, s_y, s_z, s_yaw, s_pitch;

static float stick(u8 v)
{
    int d = (int)v - 128;
    if (d > -24 && d < 24) return 0.0f;
    return (float)d / 128.0f;
}

static float b_x, b_y, b_z, b_yaw, b_pitch;

void freecam_bookmark(int load)
{
    if (!load) {
        b_x = s_x; b_y = s_y; b_z = s_z; b_yaw = s_yaw; b_pitch = s_pitch;
        g_bookmark_set = 1;
    } else if (g_bookmark_set) {
        s_x = b_x; s_y = b_y; s_z = b_z; s_yaw = b_yaw; s_pitch = b_pitch;
    }
}

void state_tick(u32 st)
{
    g_state = st;
    if (!g_freecam) s_init = 0;
    if (!g_freecam && g_fov_deg == 0 && g_far == 0) return;
    u32 cam = find_camera(st);
    if (!cam) return;
    if (g_fov_deg != 0) RDF(cam + 0xB0) = g_fov_deg * (PI / 180.0f);
    if (g_far != 0)     RDF(cam + 0xC0) = g_far;
    if (!g_freecam) return;
    g_fc_active = 1;

    if (!s_init) {
        s_x = RDF(cam + 0x70); s_y = RDF(cam + 0x74); s_z = RDF(cam + 0x78);
        float fx = RDF(cam + 0x90), fy = RDF(cam + 0x94), fz = RDF(cam + 0x98);
        float h = fx * fx + fy * fy;
        s_yaw = fatan2(fy, fx);
        s_pitch = fatan2(fz, h > 0 ? __builtin_sqrtf(h) : 0.0001f);
        s_init = 1;
    }

    {
        u8 sk[4];
        u32 b = pad_raw(sk);
        float rx = stick(sk[0]), ry = stick(sk[1]), lx = stick(sk[2]), ly = stick(sk[3]);
        float speed = 4.0f;
        if (b & B_R2) speed *= 4.0f;
        if (b & B_L2) speed *= 0.25f;

        s_yaw   -= rx * 0.045f;
        s_pitch -= ry * 0.035f;
        if (s_pitch > 1.5f)  s_pitch = 1.5f;
        if (s_pitch < -1.5f) s_pitch = -1.5f;

        float cy = fcos(s_yaw), sy = fsin(s_yaw), cp = fcos(s_pitch), sp = fsin(s_pitch);
        float fx = cp * cy, fy = cp * sy, fz = sp;          /* forward */
        float rxv = sy, ryv = -cy;                          /* right (horizontal) */
        float fwd = -ly * speed, side = lx * speed;
        s_x += fx * fwd + rxv * side;
        s_y += fy * fwd + ryv * side;
        s_z += fz * fwd;
        if (b & B_R1) s_z += speed;
        if (b & B_L1) s_z -= speed;
    }

    float cy = fcos(s_yaw), sy = fsin(s_yaw), cp = fcos(s_pitch), sp = fsin(s_pitch);
    float F[3] = { cp * cy, cp * sy, sp };
    float U[3] = { -sp * cy, -sp * sy, cp };
    float R[3] = { F[1] * U[2] - F[2] * U[1], F[2] * U[0] - F[0] * U[2], F[0] * U[1] - F[1] * U[0] };

    RDF(cam + 0x70) = s_x;  RDF(cam + 0x74) = s_y;  RDF(cam + 0x78) = s_z;
    RDF(cam + 0x80) = U[0]; RDF(cam + 0x84) = U[1]; RDF(cam + 0x88) = U[2];
    RDF(cam + 0x90) = F[0]; RDF(cam + 0x94) = F[1]; RDF(cam + 0x98) = F[2];
    RDF(cam + 0xA0) = R[0]; RDF(cam + 0xA4) = R[1]; RDF(cam + 0xA8) = R[2];
}

/* Runs after the game's PS2 controller update: hide input from the game while the
 * menu is open or the free camera is on. */
void pad_after(u32 ctrl)
{
    u32 fc = g_freecam && g_fc_live;
    if (!fc && !g_block_input) return;
    if (!valid(ctrl)) return;
    RD32(ctrl + 0x24) = 0; RD32(ctrl + 0x28) = 0; RD32(ctrl + 0x2C) = 0; RD32(ctrl + 0x30) = 0;
    for (u32 i = 0x95; i < 0x95 + 0x34; i++) *(volatile u8 *)(ctrl + i) = 0;
}
