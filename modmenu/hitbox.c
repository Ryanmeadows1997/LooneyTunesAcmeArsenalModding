/* Collision hitboxes: outlines every collision instance near the camera.
 *
 * Collision instances (kCollisionInstance, vtable 0x492290) live in the world's scene database
 * list together with every other kind of instance. Each one has a world position, a uniform
 * scale and a rotation quaternion, and points at its element: a box (half extents) or a sphere.
 *
 * The lines are drawn by adding one REF tag to the renderer's VIF1 chain for this frame. It
 * points at a packet of our own: FLUSH + DIRECT, then GIF data (PRMODECONT on, line lists in
 * REGLIST mode, PRMODECONT back off). Two packet buffers alternate, because the chain for one
 * frame is still being sent while the next one is built. */
typedef unsigned int u32;
typedef unsigned char u8;
#ifndef RD32
#define RD32(a) (*(volatile u32 *)(a))
#define RDF(a)  (*(volatile float *)(a))
#define PTR(a)  ((volatile u32 *)(a))
#endif

u32 gcall(u32 fn, u32 a0, u32 a1, u32 a2);
u32 game_camera(u32 st);           /* freecam.c */

u32 g_hitboxes;                    /* menu choice: 0 off, 1 all, 2 level boxes, 3 triggers, 4 spheres */
float g_hb_range;                  /* slider: 0 = default */

#define GAME_WORLD    0x4FC        /* game + 0x4FC: the level's world */
#define WORLD_VT      0x493A14
#define WORLD_DB      0x2C         /* scene database; its list of every instance starts at +0xA8 */
#define DB_ALL        0xA8
#define INST_NEXT     0x40
#define INST_VT       0x492290
#define BOX_VT        0x493F3C
#define SPHERE_VT     0x492330
#define RENDER_CORE   0x470158     /* -> PS2 renderer (vtable 0x494D90) */
#define RENDER_VT     0x494D90
#define SYNC_DCACHE   0x368DD8

#define HB_BUF0       0x00098000u
#define HB_BUF1       0x000AC000u
#define HB_QWORDS     0x1400u      /* 80 KB each (0x98000-0xC0000) */

static u32 valid(u32 p) { return p >= 0x100000 && p < 0x2000000 && !(p & 3); }

/* ---- packet writing (qword = 4 words) ---- */
static volatile u32 *s_w, *s_end;
static void q4(u32 a, u32 b, u32 c, u32 d) { s_w[0] = a; s_w[1] = b; s_w[2] = c; s_w[3] = d; s_w += 4; }
static void ad(u32 reg, u32 lo, u32 hi, u32 eop)
{
    q4(1 | (eop << 15), 0x10000000, 0xE, 0);                /* A+D tag, NLOOP 1, NREG 1 */
    q4(lo, hi, reg, 0);
}

/* ---- camera ---- */
static float s_px, s_py, s_pz, s_rx, s_ry, s_rz, s_ux, s_uy, s_uz, s_fx, s_fy, s_fz, s_sx, s_sy, s_cx, s_cy, s_zk;
#define NEAR 0.5f
#define GS_LO 1024.0f
#define GS_HI 3072.0f


/* Screen scale: the renderer keeps its VU1 projection constants in its own object. Screen
 * position = centre + C0 * (S * view) / depth, with C0 = core+0x180 (192, -256 at 512x512)
 * and S = core+0x190 (2, 2 at the default field of view). */
static int setup_camera(u32 cam, u32 core)
{
    s_px = RDF(cam + 0x70); s_py = RDF(cam + 0x74); s_pz = RDF(cam + 0x78);
    s_ux = RDF(cam + 0x80); s_uy = RDF(cam + 0x84); s_uz = RDF(cam + 0x88);
    s_fx = RDF(cam + 0x90); s_fy = RDF(cam + 0x94); s_fz = RDF(cam + 0x98);
    s_rx = RDF(cam + 0xA0); s_ry = RDF(cam + 0xA4); s_rz = RDF(cam + 0xA8);
    float sx = RDF(core + 0x180) * RDF(core + 0x190);
    float sy = -RDF(core + 0x184) * RDF(core + 0x194);
    s_cx = RDF(core + 0x170); s_cy = RDF(core + 0x174);
    if (!(sx > 20.0f && sx < 20000.0f && sy > 20.0f && sy < 20000.0f)) { sx = 384.0f; sy = 512.0f; }
    if (!(s_cx > 1500.0f && s_cx < 2600.0f && s_cy > 1500.0f && s_cy < 2600.0f)) { s_cx = 2048.0f; s_cy = 2048.0f; }
    s_sx = sx; s_sy = sy;
    /* depth: GS Z = C0.z / view depth (C0.z = core+0x188, 2^32 x near plane); the game tests
     * Z GEQUAL. Pulled 2% towards the camera so lines on a surface aren't hidden by it. */
    float zk = RDF(core + 0x188);
    if (!(zk > 1.0e6f && zk < 1.0e13f)) zk = 4.2949673e10f;
    s_zk = zk * 1.02f;
    return 1;
}

typedef struct { float x, y, z; } V3;
static void to_view(float wx, float wy, float wz, V3 *o)
{
    float dx = wx - s_px, dy = wy - s_py, dz = wz - s_pz;
    o->x = dx * s_rx + dy * s_ry + dz * s_rz;
    o->y = dx * s_ux + dy * s_uy + dz * s_uz;
    o->z = dx * s_fx + dy * s_fy + dz * s_fz;
}

/* float -> unsigned int from the bit pattern (the EE has no trunc.w.s) */
static u32 f2u(float f)
{
    union { float f; u32 u; } v = { f };
    if (!(f >= 1.0f)) return 0;
    int e = (int)((v.u >> 23) & 0xFF) - 127;
    if (e > 31) return 0xFFFFFFFF;
    u32 m = (v.u & 0x7FFFFF) | 0x800000;
    return e > 23 ? m << (e - 23) : m >> (23 - e);
}

static u32 s_lines;
#define MAX_LINES 4800

/* clip a view-space segment to the near plane, project, clip to a safe GS window, emit */
static void seg(V3 a, V3 b)
{
    if (a.z < NEAR && b.z < NEAR) return;
    if (a.z < NEAR || b.z < NEAR) {
        float t = (NEAR - a.z) / (b.z - a.z);
        V3 c = { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, NEAR };
        if (a.z < NEAR) a = c; else b = c;
    }
    float x0 = s_cx + s_sx * a.x / a.z, y0 = s_cy - s_sy * a.y / a.z, z0 = s_zk / a.z;
    float x1 = s_cx + s_sx * b.x / b.z, y1 = s_cy - s_sy * b.y / b.z, z1 = s_zk / b.z;
    /* Liang-Barsky against [GS_LO, GS_HI] */
    float t0 = 0, t1 = 1, dx = x1 - x0, dy = y1 - y0;
    float p[4] = { -dx, dx, -dy, dy }, q[4] = { x0 - GS_LO, GS_HI - x0, y0 - GS_LO, GS_HI - y0 };
    for (int i = 0; i < 4; i++) {
        if (p[i] == 0) { if (q[i] < 0) return; continue; }
        float r = q[i] / p[i];
        if (p[i] < 0) { if (r > t1) return; if (r > t0) t0 = r; }
        else          { if (r < t0) return; if (r < t1) t1 = r; }
    }
    float ax = x0 + dx * t0, ay = y0 + dy * t0, bx = x0 + dx * t1, by = y0 + dy * t1;
    float az = z0 + (z1 - z0) * t0, bz = z0 + (z1 - z0) * t1;   /* 1/depth is linear on screen */
    u32 X0 = f2u(ax * 16.0f), Y0 = f2u(ay * 16.0f), X1 = f2u(bx * 16.0f), Y1 = f2u(by * 16.0f);
    q4(X0 | (Y0 << 16), f2u(az), X1 | (Y1 << 16), f2u(bz));     /* XYZ2, XYZ2 */
    s_lines++;
}

static const u8 kEdges[12][2] = { {0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7} };

/* rotation matrix columns from quaternion (x, y, z, w) */
static void qcols(const float *q, float *c)
{
    float x = q[0], y = q[1], z = q[2], w = q[3];
    c[0] = 1 - 2 * (y * y + z * z); c[1] = 2 * (x * y + z * w); c[2] = 2 * (x * z - y * w);   /* local X */
    c[3] = 2 * (x * y - z * w); c[4] = 1 - 2 * (x * x + z * z); c[5] = 2 * (y * z + x * w);   /* local Y */
    c[6] = 2 * (x * z + y * w); c[7] = 2 * (y * z - x * w); c[8] = 1 - 2 * (x * x + y * y);   /* local Z */
}

static const float kCos12[12] = { 1, 0.8660254f, 0.5f, 0, -0.5f, -0.8660254f, -1, -0.8660254f, -0.5f, 0, 0.5f, 0.8660254f };
#define KSIN(i) kCos12[((i) + 9) % 12]

static u32 s_colour;
static volatile u32 *s_tag;
static void begin_group(u32 rgba)
{
    if (rgba != s_colour) { ad(0x01, rgba, 0x3F800000, 0); s_colour = rgba; }
    s_tag = s_w; s_w += 4;             /* REGLIST tag, filled in by end_group */
    s_lines = 0;
}
static void end_group(void)
{
    if (!s_lines) { s_w = s_tag; return; }
    /* NLOOP = lines, FLG = 1 (REGLIST), NREG = 2; REGS = XYZ2, XYZ2. The tag's PRE/PRIM only
     * work in PACKED mode, so PRIM is set by the A+D write at the start of the packet. */
    s_tag[0] = s_lines; s_tag[1] = 0x24000000; s_tag[2] = 0x55; s_tag[3] = 0;
}

static u32 s_total;

/* Is this instance shown and in view? Returns a distance bucket 0..31 (nearest first), or -1. */
#define NBUCKETS 32
static int cull_instance(u32 in, float range)
{
    u32 el = RD32(in + 0x48);
    if (!valid(el)) return -1;
    u32 evt = RD32(el);
    float rad;
    if (evt == SPHERE_VT) {
        if (g_hitboxes == 2 || g_hitboxes == 3) return -1;
        rad = RDF(el + 0x30);
    } else if (evt == BOX_VT) {
        u32 trig = RD32(in + 0x6C) != 0;
        if (g_hitboxes == 4 || (g_hitboxes == 2 && trig) || (g_hitboxes == 3 && !trig)) return -1;
        rad = RDF(el + 0x40);
    } else return -1;
    float s = RDF(in + 0x1C);
    if (!(s > 0.001f && s < 1000.0f)) s = 1.0f;
    rad *= s;
    if (!(rad > 0 && rad < 100000.0f)) return -1;
    V3 c; to_view(RDF(in + 0x10), RDF(in + 0x14), RDF(in + 0x18), &c);
    if (c.z + rad < NEAR || c.z - rad > range) return -1;
    float d2 = c.x * c.x + c.y * c.y + c.z * c.z;
    if (d2 > (range + rad) * (range + rad)) return -1;
    if (c.x - rad > c.z * 1.8f || -c.x - rad > c.z * 1.8f) return -1;
    if (c.y - rad > c.z * 1.4f || -c.y - rad > c.z * 1.4f) return -1;
    /* bucket by distance to the nearest point of the bounding sphere */
    float d = c.z - rad;
    if (d < 0) d = 0;
    float b = d * (float)NBUCKETS / range;
    return b >= (float)(NBUCKETS - 1) ? NBUCKETS - 1 : (int)f2u(b);
}

static void draw_instance(u32 in)
{
    u32 el = RD32(in + 0x48);
    u32 evt = RD32(el);
    float px = RDF(in + 0x10), py = RDF(in + 0x14), pz = RDF(in + 0x18), s = RDF(in + 0x1C);
    if (!(s > 0.001f && s < 1000.0f)) s = 1.0f;
    float rad = (evt == SPHERE_VT ? RDF(el + 0x30) : RDF(el + 0x40)) * s;
    if ((u32)(s_end - s_w) < 4 * 64) return;
    if (s_total > MAX_LINES) return;

    float q[4] = { RDF(in + 0x20), RDF(in + 0x24), RDF(in + 0x28), RDF(in + 0x2C) };
    float m[9]; qcols(q, m);
    u32 active = RD32(in + 0x6C) != 0;     /* collides-with mask: triggers and zones */

    if (evt == BOX_VT) {
        float ex = RDF(el + 0x30) * s, ey = RDF(el + 0x34) * s, ez = RDF(el + 0x38) * s;
        V3 v[8];
        for (int i = 0; i < 8; i++) {
            float lx = i & 1 ? ex : -ex, ly = i & 2 ? ey : -ey, lz = i & 4 ? ez : -ez;
            to_view(px + m[0] * lx + m[3] * ly + m[6] * lz,
                    py + m[1] * lx + m[4] * ly + m[7] * lz,
                    pz + m[2] * lx + m[5] * ly + m[8] * lz, &v[i]);
        }
        begin_group(active ? 0x8000C0FF : 0x8000FF40);         /* yellow : green (0xAABBGGRR) */
        for (int i = 0; i < 12; i++) seg(v[kEdges[i][0]], v[kEdges[i][1]]);
    } else {
        /* three rings of 12 segments, around the instance's own axes */
        begin_group(0x80FFFF00);                                   /* cyan */
        for (int axis = 0; axis < 3; axis++) {
            const float *a = &m[((axis + 1) % 3) * 3], *b = &m[((axis + 2) % 3) * 3];
            V3 prev, first;
            for (int i = 0; i <= 12; i++) {
                float cs = kCos12[i % 12] * rad, sn = KSIN(i % 12) * rad;
                V3 cur;
                to_view(px + a[0] * cs + b[0] * sn, py + a[1] * cs + b[1] * sn, pz + a[2] * cs + b[2] * sn, &cur);
                if (i) seg(prev, cur); else first = cur;
                prev = cur;
            }
            (void)first;
        }
    }
    s_total += s_lines;
    end_group();
}

/* Does this chain entry draw into an off-screen buffer? (A REF tag whose data writes FRAME_1
 * with a width other than the 512-pixel frame: the bloom/post-processing passes.) */
static int tag_offscreen(u32 t)
{
    u32 lo = RD32(t), d = RD32(t + 4);
    if ((lo >> 28) != 3 || !valid(d)) return 0;
    u32 n = lo & 0xFFFF;
    if (n > 0x80) n = 0x80;
    for (u32 k = 0; k < n; k++) {
        u32 q = d + k * 16;
        if (RD32(q + 8) == 0x4C && RD32(q + 12) == 0 && ((RD32(q) >> 16) & 0x3F) != 8) return 1;
    }
    return 0;
}

static u32 s_frame;
#define MAX_CAND 2048
static u32 s_cand[MAX_CAND];
static u8 s_bucket[MAX_CAND];

void hitbox_tick(u32 st)
{
    if (!g_hitboxes || !valid(st)) return;
    u32 game = RD32(st + 0x30);
    if (!valid(game)) return;
    u32 world = RD32(game + GAME_WORLD);
    if (!valid(world) || RD32(world) != WORLD_VT) return;
    u32 db = RD32(world + WORLD_DB);
    if (!valid(db)) return;
    u32 core = RD32(RENDER_CORE);
    if (!valid(core) || RD32(core) != RENDER_VT) return;
    u32 cam = game_camera(st);
    if (!cam || !setup_camera(cam, core)) return;
    u32 tp = RD32(core + 0xC), tend = RD32(core + 0x8);
    if ((tp & 0x0FFFFFFF) + 16 > (tend & 0x0FFFFFFF) || (tp & 15)) return;

    u32 buf = (s_frame++ & 1) ? HB_BUF1 : HB_BUF0;
    s_w = PTR(buf);
    s_end = PTR(buf + HB_QWORDS * 16 - 16 * 4);   /* room for the closing packet */
    s_w += 4;                                                   /* VIF codes, filled in below */
    ad(0x1A, 1, 0, 0);                                          /* PRMODECONT: attributes from PRIM */
    ad(0x00, 1, 0, 0);                                          /* PRIM: line, flat, untextured */
    ad(0x47, 0x50000, 0, 0);                                    /* TEST: depth GEQUAL, no alpha test (the next pass sets its own) */
    s_colour = 0; s_total = 0;

    /* collect what's in view, then draw nearest first so any cut-off happens far away */
    float range = g_hb_range > 0 ? g_hb_range : 400.0f;
    u32 p = RD32(db + DB_ALL), n = 0, nc = 0;
    while (valid(p) && n < 20000) {
        if (RD32(p) == INST_VT && nc < MAX_CAND) {
            int b = cull_instance(p, range);
            if (b >= 0) { s_cand[nc] = p; s_bucket[nc] = (u8)b; nc++; }
        }
        p = RD32(p + INST_NEXT);
        n++;
    }
    for (u32 b = 0; b < NBUCKETS && s_total <= MAX_LINES; b++)
        for (u32 i = 0; i < nc; i++)
            if (s_bucket[i] == b) draw_instance(s_cand[i]);
    ad(0x1A, 0, 0, 1);                                          /* back to PRMODE, end of packet */

    u32 qwc = (u32)((s_w - PTR(buf)) >> 2);
    volatile u32 *v = PTR(buf);
    v[0] = 0x11000000;                 /* FLUSH: wait for VU1 and PATH1 */
    v[1] = 0; v[2] = 0;
    v[3] = 0x50000000 | (qwc - 1);     /* DIRECT */
    gcall(SYNC_DCACHE, buf, buf + qwc * 16 - 1, 0);

    /* Our REF tag goes into this frame's chain just before the post-processing passes: those
     * draw full-screen and leave the depth buffer useless for testing against. Going back
     * from the end, find the first run of off-screen passes and insert before it. */
    u32 tstart = RD32(core + 0x4), ins = tp, found = 0;
    for (u32 k = 1; k <= 64; k++) {
        u32 t = tp - 16 * k;
        if (t < tstart) break;
        if (tag_offscreen(t)) { ins = t; found = 1; }
        else if (found) break;
    }
    for (u32 t = tp; t > ins; t -= 16) {          /* shift the later tags up one slot */
        RD32(t) = RD32(t - 16); RD32(t + 4) = RD32(t - 12); RD32(t + 8) = RD32(t - 8); RD32(t + 12) = RD32(t - 4);
    }
    volatile u32 *t = PTR(ins);
    t[0] = 0x30000000 | qwc; t[1] = buf; t[2] = 0; t[3] = 0;
    RD32(core + 0xC) = tp + 16;
}
