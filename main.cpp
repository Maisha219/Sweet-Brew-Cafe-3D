// ===============================================================
//  SWEET BREW CAFE  -  3D OUTDOOR DESIGN    (OpenGL + GLUT)
//  Code::Blocks e ekta file hishebe run hobe (F9).
//
//  Keys:  W A S D = move      Q = up      E / Z = down      B = boost
//         Arrow keys / Mouse drag = look around
//         C = auto camera ON/OFF       N = day / night
//         1..4 = camera views          R = reset       H = help     ESC = exit
// ===============================================================
#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/glut.h>
#include <GL/glu.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PI 3.14159265f
#define RAD(d) ((d) * PI / 180.0f)
#define DEG(r) ((r) * 180.0f / PI)
#define YP   0.12f          // plaza / footpath level
#define ZF   (-4.0f)        // cafe front wall (z)

// ---------------------------------------------------------------
//  Small utilities
// ---------------------------------------------------------------
static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static float hashf(int n) {                          // -1..1 noise
    unsigned int h = (unsigned int)n;
    h = (h << 13) ^ h;
    return 1.0f - (float)((h * (h * h * 15731u + 789221u) + 1376312589u) & 0x7fffffffu) / 1073741824.0f;
}

// ---------------------------------------------------------------
//  Globals
// ---------------------------------------------------------------
static GLUquadricObj* Q;
static int   winW = 1280, winH = 720;
static float simT = 0, nightF = 0;
static int   nightTarget = 0, camAuto = 1, showHelp = 1;
static float eye[3] = {0, 6, 26}, ctr[3] = {0, 3.5f, -8};
static float mYaw = 0, mPitch = -0.1f, mPos[3] = {0, 6, 26};
static int   keys[256], skeys[256];
static int   mouseDown = 0, lastMx = 0, lastMy = 0;
static GLuint texBrick, texPave, texRoad, texGrass;

// glow halos are collected and drawn at the end (night effect)
struct Halo { float x, y, z, r, cr, cg, cb; };
static Halo hl[200];
static int  nh = 0;
static void addHalo(float x, float y, float z, float r, float cr, float cg, float cb) {
    if (nightF < 0.08f || nh >= 200) return;
    hl[nh].x = x; hl[nh].y = y; hl[nh].z = z; hl[nh].r = r; hl[nh].cr = cr; hl[nh].cg = cg; hl[nh].cb = cb; nh++;
}

// ---------------------------------------------------------------
//  Primitive helpers
// ---------------------------------------------------------------
static void emitOn()  { glDisable(GL_LIGHTING); }
static void emitOff() { glEnable(GL_LIGHTING); }
static void box(float x, float y, float z, float sx, float sy, float sz) {
    glPushMatrix(); glTranslatef(x, y, z); glScalef(sx, sy, sz); glutSolidCube(1.0); glPopMatrix();
}
static void ball(float x, float y, float z, float r) {
    glPushMatrix(); glTranslatef(x, y, z); glutSolidSphere(r, 18, 14); glPopMatrix();
}
static void ellip(float x, float y, float z, float rx, float ry, float rz) {
    glPushMatrix(); glTranslatef(x, y, z); glScalef(rx, ry, rz); glutSolidSphere(1.0, 18, 14); glPopMatrix();
}
static void cylY(float x, float y, float z, float r1, float r2, float h) {      // upright, capped
    glPushMatrix(); glTranslatef(x, y, z); glRotatef(-90, 1, 0, 0);
    gluCylinder(Q, r1, r2, h, 22, 1); gluDisk(Q, 0, r1, 22, 1);
    glTranslatef(0, 0, h); gluDisk(Q, 0, r2, 22, 1); glPopMatrix();
}
static void discUp(float x, float y, float z, float r) {
    glPushMatrix(); glTranslatef(x, y, z); glRotatef(-90, 1, 0, 0); gluDisk(Q, 0, r, 24, 1); glPopMatrix();
}
static void flatQuad(float x0, float z0, float x1, float z1, float y) {
    glNormal3f(0, 1, 0);
    glBegin(GL_QUADS); glVertex3f(x0, y, z0); glVertex3f(x1, y, z0); glVertex3f(x1, y, z1); glVertex3f(x0, y, z1); glEnd();
}
static void quadZ(float x0, float x1, float y0, float y1, float z, float nz) {
    glNormal3f(0, 0, nz);
    glBegin(GL_QUADS); glVertex3f(x0, y0, z); glVertex3f(x1, y0, z); glVertex3f(x1, y1, z); glVertex3f(x0, y1, z); glEnd();
}
static void quadX(float z0, float z1, float y0, float y1, float x, float nx) {
    glNormal3f(nx, 0, 0);
    glBegin(GL_QUADS); glVertex3f(x, y0, z0); glVertex3f(x, y0, z1); glVertex3f(x, y1, z1); glVertex3f(x, y1, z0); glEnd();
}

// ---------------------------------------------------------------
//  Procedural textures (brick, paving, asphalt, grass)
// ---------------------------------------------------------------
static void makeTexture(GLuint& id, int kind) {
    const int N = 128;
    static unsigned char img[128][128][3];
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
        float r = 200, g = 200, b = 200;
        float nz = hashf(x * 73 + y * 179 + kind * 977) * 6.0f;
        if (kind == 0) {                                   // brick
            int row = y / 16, off = (row & 1) * 16, bx = (x + off) % 32;
            float v = hashf(row * 31 + (x + off) / 32 + 5) * 20.0f;
            if (y % 16 < 2 || bx < 2) { r = 214; g = 206; b = 190; }
            else { r = 170 + v; g = 78 + v * 0.5f; b = 58 + v * 0.3f; }
        } else if (kind == 1) {                            // paving stones
            int row = y / 32, off = (row & 1) * 32, px = (x + off) / 64;
            float v = hashf(row * 17 + px * 13 + 3) * 14.0f;
            int jx = (x + off) % 64, jy = y % 32;
            if (jx < 2 || jy < 2) { r = 128; g = 122; b = 114; }
            else { r = 198 + v; g = 188 + v; b = 172 + v; }
        } else if (kind == 2) {                            // asphalt
            r = g = b = 58 + nz * 1.5f;
            if (hashf(x * 31 + y * 57 + 11) > 0.92f) { r += 40; g += 40; b += 40; }
        } else {                                           // grass
            float t = hashf(x * 7 + y * 13) * 16.0f + hashf((x / 4) * 5 + (y / 4) * 3) * 14.0f;
            r = 64 + t * 0.5f; g = 130 + t; b = 44 + t * 0.3f;
        }
        if (kind != 2) { r += nz; g += nz; b += nz; }
        img[y][x][0] = (unsigned char)clampf(r, 0, 255);
        img[y][x][1] = (unsigned char)clampf(g, 0, 255);
        img[y][x][2] = (unsigned char)clampf(b, 0, 255);
    }
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gluBuild2DMipmaps(GL_TEXTURE_2D, 3, N, N, GL_RGB, GL_UNSIGNED_BYTE, img);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}
static void texFloor(GLuint t, float x0, float z0, float x1, float z1, float y, float tile) {
    glBindTexture(GL_TEXTURE_2D, t); glEnable(GL_TEXTURE_2D); glColor3f(1, 1, 1);
    float u = (x1 - x0) / tile, v = (z1 - z0) / tile;
    glNormal3f(0, 1, 0);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex3f(x0, y, z0); glTexCoord2f(u, 0); glVertex3f(x1, y, z0);
    glTexCoord2f(u, v); glVertex3f(x1, y, z1); glTexCoord2f(0, v); glVertex3f(x0, y, z1);
    glEnd(); glDisable(GL_TEXTURE_2D);
}
static void texWallZ(GLuint t, float x0, float x1, float y0, float y1, float z, float nz, float tile) {
    glBindTexture(GL_TEXTURE_2D, t); glEnable(GL_TEXTURE_2D); glColor3f(1, 1, 1);
    float u = (x1 - x0) / tile, v = (y1 - y0) / tile;
    glNormal3f(0, 0, nz);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex3f(x0, y0, z); glTexCoord2f(u, 0); glVertex3f(x1, y0, z);
    glTexCoord2f(u, v); glVertex3f(x1, y1, z); glTexCoord2f(0, v); glVertex3f(x0, y1, z);
    glEnd(); glDisable(GL_TEXTURE_2D);
}
static void texWallX(GLuint t, float z0, float z1, float y0, float y1, float x, float nx, float tile) {
    glBindTexture(GL_TEXTURE_2D, t); glEnable(GL_TEXTURE_2D); glColor3f(1, 1, 1);
    float u = (z1 - z0) / tile, v = (y1 - y0) / tile;
    glNormal3f(nx, 0, 0);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex3f(x, y0, z0); glTexCoord2f(u, 0); glVertex3f(x, y0, z1);
    glTexCoord2f(u, v); glVertex3f(x, y1, z1); glTexCoord2f(0, v); glVertex3f(x, y1, z0);
    glEnd(); glDisable(GL_TEXTURE_2D);
}

// ---------------------------------------------------------------
//  3D stroke text (signboard letters)
// ---------------------------------------------------------------
static float strokeW(const char* s) {
    float w = 0; while (*s) { w += glutStrokeWidth(GLUT_STROKE_ROMAN, *s); s++; } return w;
}
static void textFit(const char* s, float cx, float cy, float z, float maxW, float maxH,
                    float r, float g, float b, float lw) {
    float sc = maxH / 119.05f, w = strokeW(s) * sc;
    if (w > maxW) { sc *= maxW / w; w = strokeW(s) * sc; }
    emitOn(); glColor3f(r, g, b); glLineWidth(lw);
    glPushMatrix(); glTranslatef(cx - w * 0.5f, cy - 52.0f * sc, z); glScalef(sc, sc, sc);
    for (const char* c = s; *c; c++) glutStrokeCharacter(GLUT_STROKE_ROMAN, *c);
    glPopMatrix(); glLineWidth(1); emitOff();
}

// ===============================================================
//  SKY, SUN / MOON, STARS, CLOUDS
// ===============================================================
static void skyColors(float* top, float* hor) {
    float dt[3] = {0.14f, 0.40f, 0.82f}, dh[3] = {0.74f, 0.89f, 0.98f};
    float nt[3] = {0.01f, 0.02f, 0.10f}, nhh[3] = {0.10f, 0.12f, 0.28f};
    for (int i = 0; i < 3; i++) { top[i] = lerpf(dt[i], nt[i], nightF); hor[i] = lerpf(dh[i], nhh[i], nightF); }
}
static void drawSky() {
    float top[3], hor[3]; skyColors(top, hor);
    glDisable(GL_LIGHTING); glDepthMask(GL_FALSE);
    glPushMatrix(); glTranslatef(eye[0], eye[1], eye[2]);
    const int SEG = 36, RING = 12; const float R = 220.0f;
    for (int i = 0; i < RING; i++) {
        float e0 = RAD(-12.0f + 102.0f * i / RING), e1 = RAD(-12.0f + 102.0f * (i + 1) / RING);
        float t0 = powf(clampf(sinf(e0), 0, 1), 0.55f), t1 = powf(clampf(sinf(e1), 0, 1), 0.55f);
        glBegin(GL_QUAD_STRIP);
        for (int s = 0; s <= SEG; s++) {
            float az = 2 * PI * s / SEG;
            glColor3f(lerpf(hor[0], top[0], t0), lerpf(hor[1], top[1], t0), lerpf(hor[2], top[2], t0));
            glVertex3f(R * cosf(e0) * sinf(az), R * sinf(e0), -R * cosf(e0) * cosf(az));
            glColor3f(lerpf(hor[0], top[0], t1), lerpf(hor[1], top[1], t1), lerpf(hor[2], top[2], t1));
            glVertex3f(R * cosf(e1) * sinf(az), R * sinf(e1), -R * cosf(e1) * cosf(az));
        }
        glEnd();
    }
    if (nightF > 0.4f) {                                           // stars
        glPointSize(2.0f); glBegin(GL_POINTS);
        for (int i = 0; i < 220; i++) {
            float az = (hashf(i * 3 + 1) + 1) * PI, el = (hashf(i * 5 + 2) + 1) * 0.5f * 1.3f + 0.15f;
            float tw = 0.6f + 0.4f * sinf(simT * 2 + i);
            glColor4f(1, 1, 1, tw * (nightF - 0.4f) * 1.6f);
            glVertex3f(200 * cosf(el) * sinf(az), 200 * sinf(el), -200 * cosf(el) * cosf(az));
        }
        glEnd();
    }
    // sun (day) / moon (night)
    float sx = 78, sy = 72, sz = -165;
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    if (nightF < 0.5f) { glColor4f(1.0f, 0.9f, 0.5f, 0.18f * (1 - nightF * 2)); ball(sx, sy, sz, 22); ball(sx, sy, sz, 14); }
    else               { glColor4f(0.7f, 0.8f, 1.0f, 0.14f * (nightF * 2 - 1)); ball(sx, sy, sz, 18); }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (nightF < 0.5f) glColor3f(1.0f, 0.95f, 0.65f); else glColor3f(0.93f, 0.94f, 1.0f);
    ball(sx, sy, sz, 7.5f);
    glPopMatrix();
    glDepthMask(GL_TRUE); glEnable(GL_LIGHTING);
}
static void drawClouds() {
    glDisable(GL_LIGHTING);
    float c = lerpf(1.0f, 0.22f, nightF);
    glColor4f(c, c, lerpf(1.0f, 0.34f, nightF), 0.93f);
    for (int i = 0; i < 8; i++) {
        float cx = fmodf(i * 53.0f + simT * 1.3f, 300.0f) - 150.0f;
        float cy = 52 + (i % 3) * 9, cz = -110 - (i % 4) * 14;
        ellip(cx, cy, cz, 11, 3.0f, 5); ellip(cx + 8, cy + 1.2f, cz, 8, 3.2f, 4.5f); ellip(cx - 8, cy + 0.6f, cz + 1, 7, 2.6f, 4);
    }
    glEnable(GL_LIGHTING);
}

// ===============================================================
//  GROUND : grass, footpath, road, markings
// ===============================================================
static void drawGround() {
    texFloor(texGrass, -220, -220, 220, 80, -0.02f, 6.0f);
    glColor3f(0.62f, 0.60f, 0.56f); box(0, YP / 2, -1.0f, 150, YP, 6.0f);                 // near footpath base
    texFloor(texPave, -75, -4.0f, 75, 2.0f, YP + 0.003f, 2.5f);
    glColor3f(0.80f, 0.79f, 0.76f); box(0, YP / 2 + 0.02f, 2.12f, 150, YP + 0.04f, 0.24f);  // curb
    texFloor(texRoad, -100, 2.24f, 100, 10.6f, 0.02f, 4.0f);                              // road
    glColor3f(0.80f, 0.79f, 0.76f); box(0, YP / 2 + 0.02f, 10.7f, 150, YP + 0.04f, 0.24f);
    glColor3f(0.62f, 0.60f, 0.56f); box(0, YP / 2, 12.7f, 150, YP, 3.7f);                 // far footpath
    texFloor(texPave, -75, 10.82f, 75, 14.5f, YP + 0.003f, 2.5f);
    glColor3f(0.96f, 0.96f, 0.92f);                                                       // markings
    for (float x = -96; x < 96; x += 4.0f) flatQuad(x, 6.3f, x + 2.0f, 6.46f, 0.03f);
    flatQuad(-100, 2.75f, 100, 2.9f, 0.03f); flatQuad(-100, 9.95f, 100, 10.1f, 0.03f);
    for (float x = -3.5f; x < 3.4f; x += 1.0f) flatQuad(x, 3.2f, x + 0.55f, 9.6f, 0.031f); // zebra crossing
}

// ===============================================================
//  BACKDROP : far skyline + neighbour shops
// ===============================================================
static void drawBackdrop() {
    float dn = 1.0f - nightF;
    for (int i = 0; i < 16; i++) {
        float x = -105 + i * 14.0f, h = 24 + hashf(i * 31 + 7) * 10, w = 11 + hashf(i * 17) * 2;
        glColor3f(lerpf(0.58f, 0.06f, nightF), lerpf(0.66f, 0.07f, nightF), lerpf(0.78f, 0.14f, nightF));
        box(x, h / 2, -78, w, h, 10);
        emitOn();
        for (float wy = 3; wy < h - 2; wy += 4) for (float wx = x - w / 2 + 1.5f; wx < x + w / 2 - 1; wx += 3.0f) {
            float on = hashf((int)(wx * 3 + wy * 7 + i * 101));
            if (nightF > 0.5f && on > 0.1f) glColor3f(1.0f, 0.85f, 0.45f);
            else glColor3f(0.40f * dn + 0.05f, 0.55f * dn + 0.06f, 0.72f * dn + 0.12f);
            quadZ(wx - 0.6f, wx + 0.6f, wy, wy + 1.8f, -72.9f, 1);
        }
        emitOff();
    }
}
static void drawNeighbor(float x0, float x1, float h, float r, float g, float b, int seed) {
    float cx = (x0 + x1) / 2, w = x1 - x0;
    glColor3f(r, g, b); box(cx, h / 2, -8, w, h, 8);
    glColor3f(r * 0.7f, g * 0.7f, b * 0.7f); box(cx, h + 0.2f, -7.9f, w + 0.4f, 0.4f, 8.4f);
    glColor3f(r * 0.6f, g * 0.6f, b * 0.6f); box(cx, 2.65f, ZF + 0.03f, w, 0.25f, 0.1f);
    emitOn();
    int idx = 0;
    for (float wy = 3.4f; wy < h - 1.5f; wy += 2.6f)
        for (float wx = x0 + 1.5f; wx < x1 - 1.0f; wx += 2.8f, idx++) {
            if (nightF > 0.5f && hashf(seed + idx * 13) > -0.2f) glColor3f(1.0f, 0.84f, 0.45f);
            else glColor3f(lerpf(0.50f, 0.07f, nightF), lerpf(0.68f, 0.09f, nightF), lerpf(0.82f, 0.18f, nightF));
            quadZ(wx - 0.8f, wx + 0.8f, wy, wy + 1.6f, ZF + 0.09f, 1);
        }
    // ground floor shop window
    glColor3f(nightF > 0.5f ? 1.0f : 0.55f, nightF > 0.5f ? 0.85f : 0.72f, nightF > 0.5f ? 0.5f : 0.85f);
    quadZ(x0 + 1.2f, x1 - 1.2f, 0.6f, 2.2f, ZF + 0.09f, 1);
    emitOff();
}

// ===============================================================
//  TREES, PLANTS, FLOWER BOXES
// ===============================================================
static void drawTree(float x, float y, float z, float s, float ph) {
    float sw = sinf(simT * 0.9f + ph) * 1.3f;
    glPushMatrix(); glTranslatef(x, y, z); glScalef(s, s, s);
    glColor3f(0.36f, 0.22f, 0.12f); cylY(0, 0, 0, 0.28f, 0.17f, 2.4f);
    glTranslatef(0, 2.2f, 0); glRotatef(sw, 0, 0, 1); glRotatef(sw * 0.6f, 1, 0, 0);
    glColor3f(0.13f, 0.44f, 0.17f); ball(0, 1.2f, 0, 1.5f);
    glColor3f(0.20f, 0.57f, 0.22f); ball(0.9f, 0.8f, 0.25f, 1.1f); ball(-0.85f, 0.7f, -0.3f, 1.0f);
    glColor3f(0.28f, 0.65f, 0.26f); ball(0.1f, 2.1f, 0, 0.95f);
    glPopMatrix();
}
static void drawPlanter(float x, float z) {
    glColor3f(0.62f, 0.30f, 0.18f); cylY(x, YP, z, 0.42f, 0.32f, 0.7f);
    glColor3f(0.40f, 0.22f, 0.14f); cylY(x, YP + 0.66f, z, 0.46f, 0.46f, 0.08f);
    glColor3f(0.2f, 0.12f, 0.08f); discUp(x, YP + 0.75f, z, 0.38f);
    glColor3f(0.35f, 0.22f, 0.12f); cylY(x, YP + 0.7f, z, 0.05f, 0.04f, 0.5f);
    glColor3f(0.12f, 0.48f, 0.18f); ball(x, YP + 1.25f, z, 0.46f);
    glColor3f(0.18f, 0.58f, 0.22f); ball(x, YP + 1.78f, z, 0.32f); ball(x, YP + 2.15f, z, 0.2f);
}
static void drawFlowerBox(float cx, float w) {
    glColor3f(0.32f, 0.18f, 0.09f); box(cx, 0.88f, ZF + 0.38f, w, 0.45f, 0.55f);
    const float FC[5][3] = {{0.95f,0.25f,0.40f},{1.0f,0.82f,0.15f},{0.95f,0.95f,0.95f},{0.65f,0.30f,0.85f},{1.0f,0.5f,0.15f}};
    int n = (int)(w / 0.42f);
    for (int i = 0; i < n; i++) {
        float x = cx - w / 2 + 0.3f + i * (w - 0.6f) / (n - 1);
        glColor3f(0.12f, 0.50f, 0.17f); ball(x, 1.22f, ZF + 0.38f, 0.26f);
        const float* c = FC[(i + (int)(cx + 20)) % 5];
        glColor3f(c[0], c[1], c[2]); ball(x + 0.05f * sinf(simT + i), 1.46f, ZF + 0.42f, 0.11f);
    }
}

// ===============================================================
//  COFFEE CUP (roof logo, table decor)
// ===============================================================
static void drawCup(float x, float y, float z, float s, float rotY) {
    glPushMatrix(); glTranslatef(x, y, z); glRotatef(rotY, 0, 1, 0); glScalef(s, s, s);
    glColor3f(0.92f, 0.92f, 0.94f); cylY(0, -0.08f, 0, 1.05f, 1.05f, 0.08f);          // saucer
    glColor3f(0.98f, 0.98f, 0.98f); cylY(0, 0, 0, 0.55f, 0.75f, 1.0f);                  // cup
    glColor3f(0.80f, 0.15f, 0.12f); cylY(0, 0.28f, 0, 0.615f, 0.635f, 0.14f);           // red band
    glColor3f(0.35f, 0.18f, 0.08f); discUp(0, 0.985f, 0, 0.68f);                        // coffee
    glColor3f(0.98f, 0.98f, 0.98f);
    glPushMatrix(); glTranslatef(0.8f, 0.52f, 0); glutSolidTorus(0.09, 0.28, 8, 16); glPopMatrix();
    emitOn(); glColor4f(1, 1, 1, 0.6f); glLineWidth(3);                                // steam
    for (int i = 0; i < 3; i++) {
        glBegin(GL_LINE_STRIP);
        for (int k = 0; k < 14; k++)
            glVertex3f(-0.3f + i * 0.3f + 0.1f * sinf(k * 0.7f + simT * 3 + i * 2), 1.15f + k * 0.12f, 0);
        glEnd();
    }
    glLineWidth(1); emitOff();
    glPopMatrix();
}

// ===============================================================
//  CAFE BUILDING
// ===============================================================
static void frameRect(float x0, float x1, float y0, float y1, float t) {
    glColor3f(0.20f, 0.11f, 0.06f);
    box((x0 + x1) / 2, y1, ZF - 0.08f, x1 - x0 + t, t, 0.3f);
    box((x0 + x1) / 2, y0, ZF - 0.08f, x1 - x0 + t, t, 0.3f);
    box(x0, (y0 + y1) / 2, ZF - 0.08f, t, y1 - y0, 0.3f);
    box(x1, (y0 + y1) / 2, ZF - 0.08f, t, y1 - y0, 0.3f);
}
static void drawWindow(float x0, float x1) {
    frameRect(x0, x1, 1.1f, 4.5f, 0.16f);
    glColor3f(0.20f, 0.11f, 0.06f);
    box((x0 + x1) / 2, 2.8f, ZF - 0.1f, x1 - x0, 0.09f, 0.1f);
    box((x0 + x1) / 2, 2.8f, ZF - 0.1f, 0.09f, 3.4f, 0.1f);
    glColor3f(0.86f, 0.83f, 0.76f); box((x0 + x1) / 2, 1.05f, ZF + 0.12f, x1 - x0 + 0.5f, 0.12f, 0.55f);   // sill
}
static void drawAwning(float cx, float w) {
    int n = (int)(w / 0.5f + 0.5f); float sw = w / n;
    const float y0 = 4.5f, z0 = ZF + 0.05f, y1 = 3.85f, z1 = ZF + 1.7f;
    for (int i = 0; i < n; i++) {
        float x = cx - w / 2 + i * sw;
        if (i % 2) glColor3f(0.97f, 0.95f, 0.90f); else glColor3f(0.78f, 0.10f, 0.13f);
        glNormal3f(0, 0.93f, 0.37f);
        glBegin(GL_QUADS); glVertex3f(x, y0, z0); glVertex3f(x + sw, y0, z0); glVertex3f(x + sw, y1, z1); glVertex3f(x, y1, z1); glEnd();
        glNormal3f(0, 0, 1);
        glBegin(GL_QUADS); glVertex3f(x, y1, z1); glVertex3f(x + sw, y1, z1); glVertex3f(x + sw, y1 - 0.22f, z1); glVertex3f(x, y1 - 0.22f, z1); glEnd();
        glBegin(GL_TRIANGLES); glVertex3f(x, y1 - 0.22f, z1); glVertex3f(x + sw, y1 - 0.22f, z1); glVertex3f(x + sw * 0.5f, y1 - 0.5f, z1); glEnd();
    }
    glColor3f(0.25f, 0.2f, 0.18f);
    box(cx - w / 2 + 0.05f, 4.15f, ZF + 0.9f, 0.06f, 0.06f, 1.9f); box(cx + w / 2 - 0.05f, 4.15f, ZF + 0.9f, 0.06f, 0.06f, 1.9f);
}
static void drawWallLamp(float x) {
    glColor3f(0.15f, 0.12f, 0.1f); box(x, 3.0f, ZF + 0.18f, 0.1f, 0.1f, 0.3f); box(x, 2.78f, ZF + 0.3f, 0.12f, 0.08f, 0.12f);
    emitOn(); glColor3f(1.0f, 0.88f, 0.55f); ball(x, 3.1f, ZF + 0.36f, 0.17f); emitOff();
    addHalo(x, 3.1f, ZF + 0.36f, 1.1f, 1.0f, 0.75f, 0.35f);
}
static void drawSignBulbs() {
    int tick = (int)(simT * 6), idx = 0;
    emitOn();
    for (float x = -7.2f; x <= 7.21f; x += 0.6f, idx++) {
        for (int k = 0; k < 2; k++) {
            float y = k ? 6.72f : 4.78f;
            int on = ((idx + tick) % 3 == 0);
            if (on) glColor3f(1.0f, 0.92f, 0.5f); else glColor3f(0.5f, 0.4f, 0.18f);
            ball(x, y, ZF + 0.38f, 0.07f);
            if (on) addHalo(x, y, ZF + 0.38f, 0.45f, 1.0f, 0.85f, 0.3f);
        }
    }
    for (float y = 5.0f; y < 6.6f; y += 0.4f, idx++) for (int k = 0; k < 2; k++) {
        int on = ((idx + tick) % 3 == 0);
        if (on) glColor3f(1.0f, 0.92f, 0.5f); else glColor3f(0.5f, 0.4f, 0.18f);
        ball(k ? 7.2f : -7.2f, y, ZF + 0.38f, 0.07f);
    }
    emitOff();
}
static void drawNeon() {
    int on = (fmodf(simT, 3.0f) < 2.5f);
    emitOn(); glLineWidth(3);
    if (on) { glColor3f(1.0f, 0.2f, 0.25f); glBegin(GL_LINE_LOOP); glVertex3f(-7.2f, 3.2f, ZF - 0.5f); glVertex3f(-4.8f, 3.2f, ZF - 0.5f); glVertex3f(-4.8f, 4.1f, ZF - 0.5f); glVertex3f(-7.2f, 4.1f, ZF - 0.5f); glEnd(); }
    if (on) glLineWidth(4);
    emitOff();
    if (on) { textFit("OPEN", -6.0f, 3.65f, ZF - 0.5f, 2.0f, 0.55f, 1.0f, 0.3f, 0.35f, 4.0f); addHalo(-6.0f, 3.65f, ZF - 0.7f, 1.8f, 1.0f, 0.2f, 0.25f); }
    textFit("COFFEE", 6.0f, 3.65f, ZF - 0.5f, 2.4f, 0.55f, 0.3f, 0.9f, 1.0f, 4.0f);
    emitOn(); glColor3f(0.3f, 0.9f, 1.0f); glLineWidth(3);
    glBegin(GL_LINE_LOOP); glVertex3f(4.6f, 3.2f, ZF - 0.5f); glVertex3f(7.4f, 3.2f, ZF - 0.5f); glVertex3f(7.4f, 4.1f, ZF - 0.5f); glVertex3f(4.6f, 4.1f, ZF - 0.5f); glEnd();
    glLineWidth(1); emitOff();
    addHalo(6.0f, 3.65f, ZF - 0.7f, 1.9f, 0.2f, 0.8f, 1.0f);
}
static void drawChalkboard(float x, float z) {
    glPushMatrix(); glTranslatef(x, YP, z);
    for (int side = -1; side <= 1; side += 2) {
        glPushMatrix(); glTranslatef(0, 0.72f, side * 0.26f); glRotatef(-side * 11.0f, 1, 0, 0);
        glColor3f(0.45f, 0.28f, 0.14f); box(0, 0, 0, 0.98f, 1.4f, 0.06f);
        glColor3f(0.08f, 0.14f, 0.11f); box(0, 0, side * 0.032f, 0.86f, 1.28f, 0.02f);
        if (side == 1) {
            textFit("TODAY'S SPECIAL", 0, 0.42f, 0.05f, 0.78f, 0.13f, 1.0f, 0.85f, 0.4f, 1.5f);
            textFit("COFFEE   80 TK", 0, 0.12f, 0.05f, 0.76f, 0.12f, 0.95f, 0.95f, 0.9f, 1.4f);
            textFit("FRESH CAKE 120", 0, -0.14f, 0.05f, 0.76f, 0.12f, 0.95f, 0.95f, 0.9f, 1.4f);
            textFit("WELCOME", 0, -0.42f, 0.05f, 0.7f, 0.14f, 1.0f, 0.55f, 0.65f, 1.6f);
        }
        glPopMatrix();
    }
    glPopMatrix();
}
static void drawRoomPeek() {                                    // what is seen through the glass
    glColor3f(0.52f, 0.34f, 0.20f); flatQuad(-11.9f, -12, 11.9f, ZF, YP + 0.01f);                // wood floor
    glColor3f(0.62f, 0.42f, 0.26f); for (float z = -12; z < ZF; z += 0.9f) flatQuad(-11.9f, z, 11.9f, z + 0.04f, YP + 0.012f);
    glColor3f(0.93f, 0.87f, 0.75f); flatQuad(-11.9f, -12, 11.9f, ZF, 6.85f);   // ceiling
    glColor3f(0.96f, 0.90f, 0.78f); quadZ(-11.9f, 11.9f, YP, 6.9f, -11.85f, 1);                   // back wall
    glColor3f(0.92f, 0.85f, 0.72f); quadX(-12, ZF, YP, 6.9f, -11.9f, 1); quadX(-12, ZF, YP, 6.9f, 11.9f, -1);
    glColor3f(0.38f, 0.24f, 0.13f); box(0, 0.95f, -11.75f, 23.8f, 1.7f, 0.12f);                 // dark wainscot
    // counter + espresso machine + pastry case
    glColor3f(0.45f, 0.28f, 0.15f); box(-3, 0.75f, -10.4f, 10, 1.2f, 1.3f);
    glColor3f(0.88f, 0.86f, 0.82f); box(-3, 1.4f, -10.4f, 10.4f, 0.1f, 1.5f);
    glColor3f(0.65f, 0.67f, 0.7f); box(-6.0f, 1.85f, -10.6f, 1.5f, 0.8f, 0.7f); box(-6.0f, 2.35f, -10.6f, 1.2f, 0.2f, 0.6f);
    glColor3f(0.15f, 0.15f, 0.17f); box(-6.0f, 1.55f, -10.2f, 0.25f, 0.15f, 0.2f);
    glColor3f(0.95f, 0.72f, 0.80f); cylY(-1.5f, 1.45f, -10.3f, 0.32f, 0.32f, 0.22f);
    glColor3f(0.45f, 0.24f, 0.12f); cylY(-0.6f, 1.45f, -10.3f, 0.30f, 0.30f, 0.26f);
    glColor3f(0.9f, 0.72f, 0.3f); cylY(0.3f, 1.45f, -10.3f, 0.28f, 0.28f, 0.16f);
    // shelves with jars
    for (int s = 0; s < 2; s++) {
        float y = 3.1f + s * 0.9f;
        glColor3f(0.4f, 0.26f, 0.14f); box(-3, y, -11.5f, 10, 0.08f, 0.45f);
        for (float x = -7.5f; x < 1.6f; x += 0.65f) {
            int k = (int)((hashf((int)(x * 10) + s * 77) + 1) * 3.0f) % 3;
            if (k == 0) glColor3f(0.8f, 0.55f, 0.25f); else if (k == 1) glColor3f(0.35f, 0.2f, 0.12f); else glColor3f(0.9f, 0.85f, 0.7f);
            cylY(x, y + 0.04f, -11.5f, 0.17f, 0.17f, 0.38f);
        }
    }
    glColor3f(0.1f, 0.18f, 0.14f); box(6.6f, 3.7f, -11.75f, 5.0f, 2.5f, 0.1f);                  // menu board
    glColor3f(0.4f, 0.27f, 0.15f); box(6.6f, 4.98f, -11.75f, 5.2f, 0.1f, 0.14f); box(6.6f, 2.42f, -11.75f, 5.2f, 0.1f, 0.14f);
    textFit("MENU", 6.6f, 4.5f, -11.68f, 1.6f, 0.4f, 1, 1, 1, 2.0f);
    textFit("BURGER  PIZZA  PASTA", 6.6f, 3.8f, -11.68f, 4.4f, 0.28f, 0.95f, 0.9f, 0.7f, 1.5f);
    textFit("CAKE  COFFEE  JUICE", 6.6f, 3.3f, -11.68f, 4.4f, 0.28f, 0.95f, 0.9f, 0.7f, 1.5f);
    // tables + chairs inside
    float tp[4][2] = {{-6.5f, -7.5f}, {6.5f, -7.5f}, {-6.5f, -5.6f}, {6.5f, -5.6f}};
    for (int i = 0; i < 4; i++) {
        float tx = tp[i][0], tz = tp[i][1];
        glColor3f(0.12f, 0.1f, 0.09f); cylY(tx, YP, tz, 0.3f, 0.3f, 0.04f); cylY(tx, YP, tz, 0.06f, 0.06f, 0.72f);
        glColor3f(i % 2 ? 0.55f : 0.9f, i % 2 ? 0.3f : 0.85f, i % 2 ? 0.2f : 0.78f); cylY(tx, YP + 0.72f, tz, 0.7f, 0.7f, 0.05f);
        for (int c = 0; c < 2; c++) {
            glColor3f(0.48f, 0.3f, 0.16f);
            float cx = tx + (c ? 1.0f : -1.0f); cylY(cx, YP, tz, 0.2f, 0.2f, 0.42f); box(cx + (c ? 0.2f : -0.2f), YP + 0.75f, tz, 0.05f, 0.7f, 0.38f);
        }
    }
    // pendant lamps
    for (int i = 0; i < 3; i++) {
        float x = -6 + i * 6.0f;
        glColor3f(0.1f, 0.1f, 0.1f); cylY(x, 5.0f, -8, 0.012f, 0.012f, 1.9f);
        glColor3f(0.88f, 0.55f, 0.2f); cylY(x, 4.55f, -8, 0.5f, 0.14f, 0.4f);
        emitOn(); glColor3f(1.0f, 0.93f, 0.7f); ball(x, 4.55f, -8, 0.12f); emitOff();
        addHalo(x, 4.5f, -8, 1.6f, 1.0f, 0.8f, 0.4f);
    }
}
static void drawCafe() {
    drawRoomPeek();
    // side + back walls (brick)
    texWallX(texBrick, -12, ZF, 0, 7, -12.0f, -1, 2.0f); texWallX(texBrick, -12, ZF, 0, 7, 12.0f, 1, 2.0f);
    texWallZ(texBrick, -12, 12, 0, 7, -12.0f, -1, 2.0f);
    // front facade (brick with window + door openings)
    texWallZ(texBrick, -12, -1.4f, 0, 1.1f, ZF, 1, 2.0f); texWallZ(texBrick, 1.4f, 12, 0, 1.1f, ZF, 1, 2.0f);
    texWallZ(texBrick, -12, -9, 1.1f, 4.5f, ZF, 1, 2.0f); texWallZ(texBrick, -3, -1.4f, 1.1f, 4.5f, ZF, 1, 2.0f);
    texWallZ(texBrick, 1.4f, 3, 1.1f, 4.5f, ZF, 1, 2.0f);  texWallZ(texBrick, 9, 12, 1.1f, 4.5f, ZF, 1, 2.0f);
    texWallZ(texBrick, -12, 12, 4.5f, 7.0f, ZF, 1, 2.0f);
    // roof, cornice, parapet, chimney
    glColor3f(0.30f, 0.28f, 0.30f); box(0, 7.1f, -8, 24.6f, 0.3f, 8.6f);
    glColor3f(0.93f, 0.88f, 0.78f); box(0, 6.98f, ZF + 0.1f, 24.8f, 0.28f, 0.5f);
    glColor3f(0.42f, 0.23f, 0.13f); box(0, 7.5f, ZF - 0.1f, 24.4f, 0.55f, 0.3f); box(-12.0f, 7.5f, -8, 0.3f, 0.55f, 8.2f); box(12.0f, 7.5f, -8, 0.3f, 0.55f, 8.2f); box(0, 7.5f, -12, 24.4f, 0.55f, 0.3f);
    glColor3f(0.93f, 0.88f, 0.78f); box(0, 7.82f, ZF - 0.1f, 24.8f, 0.1f, 0.46f);
    glColor3f(0.62f, 0.3f, 0.22f); box(-8, 8.3f, -10, 1.0f, 2.0f, 1.0f); glColor3f(0.2f, 0.2f, 0.22f); box(-8, 9.35f, -10, 1.2f, 0.12f, 1.2f);
    drawCup(-9.5f, 7.75f, -6.2f, 1.5f, 180); drawCup(9.5f, 7.75f, -6.2f, 1.5f, 0);       // big roof cups
    // corner pilasters
    glColor3f(0.93f, 0.88f, 0.78f); box(-11.85f, 3.5f, ZF + 0.1f, 0.5f, 7.0f, 0.35f); box(11.85f, 3.5f, ZF + 0.1f, 0.5f, 7.0f, 0.35f);
    // signboard
    glColor3f(0.13f, 0.07f, 0.04f); box(0, 5.75f, ZF + 0.2f, 14.4f, 1.9f, 0.3f);
    glColor3f(0.86f, 0.66f, 0.22f); box(0, 6.72f, ZF + 0.2f, 14.6f, 0.1f, 0.34f); box(0, 4.78f, ZF + 0.2f, 14.6f, 0.1f, 0.34f);
    box(-7.25f, 5.75f, ZF + 0.2f, 0.1f, 1.9f, 0.34f); box(7.25f, 5.75f, ZF + 0.2f, 0.1f, 1.9f, 0.34f);
    textFit("SWEET BREW CAFE", 0.06f, 5.9f, ZF + 0.355f, 12.9f, 1.0f, 0.12f, 0.06f, 0.02f, 7.0f);   // shadow
    textFit("SWEET BREW CAFE", 0, 5.95f, ZF + 0.37f, 12.9f, 1.0f, 1.0f, 0.82f, 0.28f, 4.5f);          // gold
    textFit("COFFEE & BAKERY", 0, 5.12f, ZF + 0.37f, 6.6f, 0.36f, 1.0f, 0.97f, 0.9f, 2.5f);
    drawSignBulbs();
    // windows, door, awnings
    drawWindow(-9, -3); drawWindow(3, 9);
    frameRect(-1.4f, 1.4f, YP, 3.5f, 0.16f); frameRect(-1.4f, 1.4f, 3.5f, 4.5f, 0.16f);
    glColor3f(0.20f, 0.11f, 0.06f); box(0, 1.8f, ZF - 0.14f, 0.08f, 3.4f, 0.1f);
    glColor3f(0.52f, 0.42f, 0.22f); box(-0.7f, 0.4f, ZF - 0.14f, 1.2f, 0.5f, 0.05f); box(0.7f, 0.4f, ZF - 0.14f, 1.2f, 0.5f, 0.05f);
    glColor3f(0.9f, 0.7f, 0.2f); box(-0.18f, 1.8f, ZF - 0.22f, 0.05f, 0.9f, 0.05f); box(0.18f, 1.8f, ZF - 0.22f, 0.05f, 0.9f, 0.05f);
    textFit("WELCOME", 0, 4.0f, ZF - 0.2f, 2.2f, 0.4f, 1.0f, 0.82f, 0.3f, 3.0f);
    drawAwning(-6, 6.8f); drawAwning(6, 6.8f);
    drawFlowerBox(-6, 6.0f); drawFlowerBox(6, 6.0f);
    drawWallLamp(-2.2f); drawWallLamp(2.2f);
    glColor3f(0.38f, 0.1f, 0.1f); flatQuad(-1.1f, -3.7f, 1.1f, -2.7f, YP + 0.006f);    // door mat
    drawNeon();
}

// ===============================================================
//  OUTDOOR SEATING, LAMPS, BUNTING
// ===============================================================
static void drawBistro(float x, float z, int style) {
    glPushMatrix(); glTranslatef(x, 0, z);
    glColor3f(0.22f, 0.22f, 0.24f); cylY(0, YP, 0, 0.32f, 0.3f, 0.05f); cylY(0, YP, 0, 0.05f, 0.05f, 0.72f);
    glColor3f(0.45f, 0.26f, 0.12f); cylY(0, YP + 0.72f, 0, 0.82f, 0.82f, 0.06f);
    glColor3f(0.25f, 0.25f, 0.27f); cylY(0, YP, 0, 0.03f, 0.03f, 3.2f);                         // umbrella pole
    drawCup(0.35f, YP + 0.78f, 0.2f, 0.14f, 0); drawCup(-0.35f, YP + 0.78f, -0.2f, 0.14f, 180);
    glColor3f(0.9f, 0.9f, 0.6f); cylY(0, YP + 0.78f, 0, 0.04f, 0.05f, 0.12f);
    glColor3f(0.95f, 0.3f, 0.4f); ball(0, YP + 0.98f, 0, 0.06f);
    for (int c = 0; c < 3; c++) {                                                            // chairs
        float a = 90.0f + c * 120.0f;
        glPushMatrix(); glTranslatef(sinf(RAD(a)) * 1.2f, 0, cosf(RAD(a)) * 1.2f); glRotatef(a + 180, 0, 1, 0);
        glColor3f(0.28f, 0.28f, 0.30f);
        for (int l = 0; l < 4; l++) box((l % 2 ? 0.17f : -0.17f), YP + 0.22f, (l < 2 ? 0.17f : -0.17f), 0.04f, 0.44f, 0.04f);
        glColor3f(0.78f, 0.35f, 0.16f); cylY(0, YP + 0.44f, 0, 0.26f, 0.26f, 0.05f);
        glColor3f(0.28f, 0.28f, 0.30f); box(0, YP + 0.78f, -0.22f, 0.4f, 0.5f, 0.04f);
        glPopMatrix();
    }
    for (int i = 0; i < 8; i++) {                                                            // umbrella canopy
        float a0 = RAD(i * 45.0f), a1 = RAD((i + 1) * 45.0f), am = (a0 + a1) / 2;
        if ((i + style) % 2) glColor3f(0.97f, 0.95f, 0.88f); else glColor3f(0.78f, 0.10f, 0.13f);
        glNormal3f(cosf(am) * 0.55f, 0.83f, sinf(am) * 0.55f);
        glBegin(GL_TRIANGLES); glVertex3f(0, YP + 3.75f, 0);
        glVertex3f(cosf(a0) * 1.9f, YP + 3.1f, sinf(a0) * 1.9f); glVertex3f(cosf(a1) * 1.9f, YP + 3.1f, sinf(a1) * 1.9f); glEnd();
        glNormal3f(cosf(am), 0, sinf(am));
        glBegin(GL_QUADS); glVertex3f(cosf(a0) * 1.9f, YP + 3.1f, sinf(a0) * 1.9f); glVertex3f(cosf(a1) * 1.9f, YP + 3.1f, sinf(a1) * 1.9f);
        glVertex3f(cosf(a1) * 1.9f, YP + 2.92f, sinf(a1) * 1.9f); glVertex3f(cosf(a0) * 1.9f, YP + 2.92f, sinf(a0) * 1.9f); glEnd();
    }
    glPopMatrix();
}
static void drawStreetLamp(float x, float z) {
    glColor3f(0.14f, 0.14f, 0.16f);
    cylY(x, YP, z, 0.22f, 0.14f, 0.5f); cylY(x, YP + 0.5f, z, 0.08f, 0.05f, 4.9f);
    box(x, 5.45f, z + 0.5f, 0.08f, 0.08f, 1.0f);
    glColor3f(0.22f, 0.22f, 0.24f); cylY(x, 5.2f, z + 1.0f, 0.3f, 0.12f, 0.22f);
    emitOn(); glColor3f(1.0f, 0.92f, 0.6f); ball(x, 5.15f, z + 1.0f, 0.19f); emitOff();
    addHalo(x, 5.1f, z + 1.0f, 1.8f, 1.0f, 0.85f, 0.45f);
}
static void drawBunting(float xa, float xb, float y, float z, int phase) {
    const float FC[5][3] = {{0.9f,0.2f,0.25f},{0.98f,0.8f,0.2f},{0.2f,0.6f,0.9f},{0.3f,0.75f,0.4f},{0.95f,0.95f,0.95f}};
    emitOn(); glColor3f(0.2f, 0.2f, 0.2f); glLineWidth(1.5f);
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i <= 20; i++) { float u = -1 + 2 * i / 20.0f; glVertex3f(lerpf(xa, xb, i / 20.0f), y - 0.8f * (1 - u * u), z); }
    glEnd(); emitOff();
    int n = 12;
    for (int i = 1; i < n; i++) {
        float t = i / (float)n, u = -1 + 2 * t, x = lerpf(xa, xb, t), yy = y - 0.8f * (1 - u * u);
        float sw = sinf(simT * 1.6f + i * 0.9f) * 0.06f;
        const float* c = FC[(i + phase) % 5]; glColor3f(c[0], c[1], c[2]);
        glNormal3f(0, 0, 1);
        glBegin(GL_TRIANGLES); glVertex3f(x - 0.23f, yy, z); glVertex3f(x + 0.23f, yy, z); glVertex3f(x + sw, yy - 0.55f, z + sw); glEnd();
    }
}

// ===============================================================
//  PEOPLE + CARS
// ===============================================================
static void drawWalker(float x, float z, float dirDeg, float ph, int k) {
    const float SH[4][3] = {{0.85f,0.25f,0.25f},{0.25f,0.5f,0.85f},{0.3f,0.7f,0.4f},{0.9f,0.7f,0.2f}};
    float sw = sinf(ph) * 28.0f;
    glPushMatrix(); glTranslatef(x, YP, z); glRotatef(dirDeg, 0, 1, 0);
    for (int s = -1; s <= 1; s += 2) {
        glPushMatrix(); glTranslatef(s * 0.1f, 0.92f, 0); glRotatef(s * sw, 1, 0, 0);
        glColor3f(0.18f, 0.2f, 0.32f); glTranslatef(0, -0.45f, 0); box(0, 0, 0, 0.14f, 0.9f, 0.15f);
        glColor3f(0.1f, 0.08f, 0.08f); box(0, -0.47f, 0.05f, 0.15f, 0.08f, 0.26f); glPopMatrix();
        glPushMatrix(); glTranslatef(s * 0.27f, 1.5f, 0); glRotatef(-s * sw, 1, 0, 0); glTranslatef(0, -0.27f, 0);
        glColor3f(0.85f, 0.65f, 0.5f); box(0, 0, 0, 0.1f, 0.54f, 0.1f); glPopMatrix();
    }
    glColor3f(SH[k % 4][0], SH[k % 4][1], SH[k % 4][2]); box(0, 1.22f, 0, 0.42f, 0.62f, 0.24f);
    glColor3f(0.85f, 0.65f, 0.5f); ball(0, 1.74f, 0, 0.14f);
    glColor3f(0.1f, 0.08f, 0.06f); ellip(0, 1.8f, -0.02f, 0.15f, 0.12f, 0.15f);
    glPopMatrix();
}
static void drawCar(float x, float z, float dirDeg, float r, float g, float b) {
    glPushMatrix(); glTranslatef(x, 0.02f, z); glRotatef(dirDeg, 0, 1, 0);
    glColor3f(r, g, b); box(0, 0.62f, 0, 4.2f, 0.6f, 1.8f); box(1.55f, 0.95f, 0, 1.0f, 0.1f, 1.7f);
    box(-0.2f, 1.5f, 0, 2.0f, 0.07f, 1.5f);
    glColor3f(0.12f, 0.2f, 0.28f); box(-0.2f, 1.2f, 0, 2.25f, 0.4f, 1.64f);
    glColor3f(r, g, b); box(-0.2f, 1.2f, 0.82f, 0.08f, 0.4f, 0.06f); box(-0.2f, 1.2f, -0.82f, 0.08f, 0.4f, 0.06f);
    glColor3f(0.15f, 0.15f, 0.17f); box(2.12f, 0.42f, 0, 0.14f, 0.2f, 1.7f); box(-2.12f, 0.42f, 0, 0.14f, 0.2f, 1.7f);
    emitOn(); glColor3f(1.0f, 0.95f, 0.7f); box(2.12f, 0.72f, 0.6f, 0.05f, 0.16f, 0.3f); box(2.12f, 0.72f, -0.6f, 0.05f, 0.16f, 0.3f);
    glColor3f(0.95f, 0.1f, 0.08f); box(-2.12f, 0.72f, 0.6f, 0.05f, 0.16f, 0.3f); box(-2.12f, 0.72f, -0.6f, 0.05f, 0.16f, 0.3f); emitOff();
    float spin = -x * 160.0f;
    for (int w = 0; w < 4; w++) {
        float wx = (w < 2) ? 1.3f : -1.3f; int pos = (w % 2 == 0);
        glPushMatrix(); glTranslatef(wx, 0.36f, pos ? 0.76f : -1.0f);
        glColor3f(0.04f, 0.04f, 0.04f); gluCylinder(Q, 0.36f, 0.36f, 0.24f, 18, 1);
        gluDisk(Q, 0, 0.36f, 18, 1); glTranslatef(0, 0, 0.24f); gluDisk(Q, 0, 0.36f, 18, 1);
        glColor3f(0.72f, 0.74f, 0.78f); glTranslatef(0, 0, 0.003f); gluDisk(Q, 0, 0.2f, 14, 1);
        glRotatef(spin, 0, 0, 1); glColor3f(0.35f, 0.35f, 0.38f); box(0, 0, 0.006f, 0.34f, 0.05f, 0.01f); box(0, 0, 0.006f, 0.05f, 0.34f, 0.01f);
        glPopMatrix();
    }
    glPopMatrix();
    float dx = (dirDeg == 0) ? 2.2f : -2.2f;
    addHalo(x + dx, 0.75f, z + 0.6f, 1.4f, 1.0f, 0.95f, 0.7f); addHalo(x + dx, 0.75f, z - 0.6f, 1.4f, 1.0f, 0.95f, 0.7f);
}

// ===============================================================
//  GLASS (drawn last, transparent) + HALOS
// ===============================================================
static void drawGlass() {
    emitOn(); glDepthMask(GL_FALSE);
    float z = ZF - 0.14f;
    glColor4f(0.62f, 0.82f, 0.94f, 0.14f);
    quadZ(-9, -3, 1.1f, 4.5f, z, 1); quadZ(3, 9, 1.1f, 4.5f, z, 1); quadZ(-1.4f, 1.4f, 3.5f, 4.5f, z, 1);
    glColor4f(0.62f, 0.82f, 0.94f, 0.18f); quadZ(-1.4f, 0, YP, 3.5f, z, 1); quadZ(0, 1.4f, YP, 3.5f, z, 1);
    glColor4f(1, 1, 1, 0.14f * (1 - nightF * 0.6f));
    for (int w = 0; w < 2; w++) {
        float x = w ? 3.0f : -9.0f;
        glBegin(GL_QUADS); glVertex3f(x + 0.8f, 1.2f, z + 0.01f); glVertex3f(x + 1.5f, 1.2f, z + 0.01f); glVertex3f(x + 3.1f, 4.4f, z + 0.01f); glVertex3f(x + 2.4f, 4.4f, z + 0.01f); glEnd();
    }
    glDepthMask(GL_TRUE); emitOff();
}
static void drawHalos() {
    if (nh == 0) return;
    glDisable(GL_LIGHTING); glDepthMask(GL_FALSE); glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    for (int i = 0; i < nh; i++) {
        Halo& h = hl[i];
        glColor4f(h.cr, h.cg, h.cb, 0.10f * nightF); ball(h.x, h.y, h.z, h.r);
        glColor4f(h.cr, h.cg, h.cb, 0.18f * nightF); ball(h.x, h.y, h.z, h.r * 0.5f);
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_TRUE); glEnable(GL_LIGHTING);
    nh = 0;
}

// ===============================================================
//  LIGHTS, HUD, CAMERA
// ===============================================================
static void setLight(int id, float x, float y, float z, float w, float r, float g, float b, float a1, float a2) {
    GLfloat pos[4] = {x, y, z, w}, dif[4] = {r, g, b, 1}, amb[4] = {0, 0, 0, 1}, spc[4] = {r * 0.3f, g * 0.3f, b * 0.3f, 1};
    glEnable(id);
    glLightfv(id, GL_POSITION, pos); glLightfv(id, GL_DIFFUSE, dif); glLightfv(id, GL_AMBIENT, amb); glLightfv(id, GL_SPECULAR, spc);
    glLightf(id, GL_CONSTANT_ATTENUATION, 1.0f); glLightf(id, GL_LINEAR_ATTENUATION, a1); glLightf(id, GL_QUADRATIC_ATTENUATION, a2);
}
static void setupLights() {
    GLfloat ga[4] = {lerpf(0.50f, 0.14f, nightF), lerpf(0.50f, 0.16f, nightF), lerpf(0.53f, 0.28f, nightF), 1};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ga);
    float d = 1 - nightF;
    setLight(GL_LIGHT0, 0.45f, 0.75f, 0.6f, 0, 0.88f * d + 0.10f * nightF, 0.85f * d + 0.12f * nightF, 0.78f * d + 0.25f * nightF, 0, 0);
    float wl = 0.45f + 0.8f * nightF;
    setLight(GL_LIGHT1, 0, 5.5f, -7.5f, 1, 1.0f * wl, 0.82f * wl, 0.5f * wl, 0.02f, 0.004f);
    setLight(GL_LIGHT2, -5, 5.2f, 3.0f, 1, 1.0f * nightF, 0.85f * nightF, 0.5f * nightF, 0.03f, 0.008f);
    setLight(GL_LIGHT3, 5, 5.2f, 3.0f, 1, 1.0f * nightF, 0.85f * nightF, 0.5f * nightF, 0.03f, 0.008f);
}
static void drawText(float x, float y, const char* s) {
    glRasterPos2f(x, y); for (const char* c = s; *c; c++) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
}
static void drawHUD() {
    glDisable(GL_LIGHTING); glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); gluOrtho2D(0, winW, 0, winH);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    glColor4f(0, 0, 0, 0.45f);
    glBegin(GL_QUADS); glVertex2f(0, winH); glVertex2f(winW, winH); glVertex2f(winW, winH - 30); glVertex2f(0, winH - 30); glEnd();
    char buf[160];
    sprintf(buf, "SWEET BREW CAFE  -  3D OUTDOOR     |     %s     |     Camera: %s",
            nightF > 0.5f ? "NIGHT" : "DAY", camAuto ? "AUTO TOUR" : "MANUAL");
    glColor3f(1, 0.9f, 0.5f); drawText(12, winH - 20, buf);
    if (showHelp) {
        glColor3f(1, 1, 1);
        drawText(12, 14, "W A S D / Q E: move | Mouse drag or Arrows: look | B: boost | C: auto camera | N: day/night | 1-4: views | R: reset | H: help | ESC: exit");
    }
    glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW); glPopMatrix();
    glEnable(GL_DEPTH_TEST); glEnable(GL_LIGHTING);
}
static void setManualFrom(const float* e, const float* c) {
    float f[3] = {c[0] - e[0], c[1] - e[1], c[2] - e[2]};
    float l = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]); if (l < 0.001f) l = 1;
    mYaw = atan2f(f[0] / l, -f[2] / l); mPitch = asinf(clampf(f[1] / l, -1, 1));
    mPos[0] = e[0]; mPos[1] = e[1]; mPos[2] = e[2];
}
static void setView(float ex, float ey, float ez, float cx, float cy, float cz) {
    float e[3] = {ex, ey, ez}, c[3] = {cx, cy, cz}; camAuto = 0; setManualFrom(e, c);
}
static void toManual() { if (camAuto) { camAuto = 0; setManualFrom(eye, ctr); } }
static void updateCamera(float dt) {
    if (camAuto) {
        float t = simT * 0.12f;
        eye[0] = 17 * sinf(t); eye[1] = 5.6f + 1.4f * sinf(t * 0.8f); eye[2] = 22 + 3.0f * sinf(t * 0.7f);
        ctr[0] = 2.5f * sinf(t * 0.5f); ctr[1] = 3.7f; ctr[2] = -8;
    } else {
        float sp = (keys['b'] || keys['B']) ? 14.0f : 6.0f;
        float f[3] = {sinf(mYaw) * cosf(mPitch), sinf(mPitch), -cosf(mYaw) * cosf(mPitch)};
        float r[3] = {cosf(mYaw), 0, sinf(mYaw)};
        if (keys['w'] || keys['W']) for (int k = 0; k < 3; k++) mPos[k] += f[k] * sp * dt;
        if (keys['s'] || keys['S']) for (int k = 0; k < 3; k++) mPos[k] -= f[k] * sp * dt;
        if (keys['d'] || keys['D']) for (int k = 0; k < 3; k++) mPos[k] += r[k] * sp * dt;
        if (keys['a'] || keys['A']) for (int k = 0; k < 3; k++) mPos[k] -= r[k] * sp * dt;
        if (keys['q'] || keys['Q']) mPos[1] += sp * dt;
        if (keys['e'] || keys['E'] || keys['z'] || keys['Z']) mPos[1] -= sp * dt;
        if (skeys[GLUT_KEY_LEFT])  mYaw -= 1.6f * dt;
        if (skeys[GLUT_KEY_RIGHT]) mYaw += 1.6f * dt;
        if (skeys[GLUT_KEY_UP])    mPitch += 1.2f * dt;
        if (skeys[GLUT_KEY_DOWN])  mPitch -= 1.2f * dt;
        mPitch = clampf(mPitch, -1.4f, 1.4f);
        if (mPos[1] < 0.5f) mPos[1] = 0.5f;
        eye[0] = mPos[0]; eye[1] = mPos[1]; eye[2] = mPos[2];
        ctr[0] = mPos[0] + sinf(mYaw) * cosf(mPitch); ctr[1] = mPos[1] + sinf(mPitch); ctr[2] = mPos[2] - cosf(mYaw) * cosf(mPitch);
    }
}

// ===============================================================
//  DISPLAY / CALLBACKS
// ===============================================================
static void display() {
    float top[3], hor[3]; skyColors(top, hor);
    glClearColor(hor[0], hor[1], hor[2], 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluPerspective(58.0, (double)winW / (double)winH, 0.3, 600.0);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    gluLookAt(eye[0], eye[1], eye[2], ctr[0], ctr[1], ctr[2], 0, 1, 0);
    nh = 0;
    drawSky(); drawClouds();
    setupLights();
    drawGround(); drawBackdrop();
    drawNeighbor(-34, -12, 10.0f, 0.74f, 0.68f, 0.58f, 11);
    drawNeighbor(12, 34, 8.5f, 0.60f, 0.67f, 0.76f, 29);
    drawCafe();
    // trees and plants
    drawTree(-13.8f, YP, 0.8f, 1.0f, 0); drawTree(13.8f, YP, 0.8f, 1.1f, 1.5f);
    for (int i = 0; i < 6; i++) drawTree(-27 + i * 11.0f, YP, 12.6f, 1.0f + 0.15f * hashf(i), i * 0.8f);
    for (int i = 0; i < 9; i++) drawTree(-44 + i * 11.0f, 0, -17, 1.4f, i * 0.5f);
    drawPlanter(-2.2f, ZF + 0.8f); drawPlanter(2.2f, ZF + 0.8f);
    drawChalkboard(3.4f, -2.6f);
    // outdoor seating
    drawBistro(-9, -0.4f, 0); drawBistro(-5, -0.4f, 1); drawBistro(5, -0.4f, 0); drawBistro(9, -0.4f, 1);
    // street lamps + bunting
    for (int i = 0; i < 4; i++) drawStreetLamp(-15 + i * 10.0f, 1.9f);
    for (int i = 0; i < 3; i++) drawBunting(-15 + i * 10.0f, -5 + i * 10.0f, 4.9f, 1.9f, i * 2);
    // people
    drawWalker(fmodf(simT * 1.1f + 10, 60) - 30, 1.35f, 90, simT * 5.0f, 0);
    drawWalker(30 - fmodf(simT * 0.9f + 25, 60), 0.95f, -90, simT * 4.5f + 1, 1);
    drawWalker(fmodf(simT * 1.4f + 40, 60) - 30, 1.15f, 90, simT * 6.0f + 2, 2);
    // traffic
    drawCar(fmodf(simT * 6.0f + 10, 140) - 70, 4.6f, 0, 0.82f, 0.12f, 0.14f);
    drawCar(fmodf(simT * 6.0f + 80, 140) - 70, 4.6f, 0, 0.18f, 0.6f, 0.3f);
    drawCar(70 - fmodf(simT * 5.5f + 20, 140), 8.2f, 180, 0.2f, 0.4f, 0.8f);
    drawCar(70 - fmodf(simT * 5.5f + 90, 140), 8.2f, 180, 0.95f, 0.78f, 0.15f);
    drawGlass();
    drawHalos();
    drawHUD();
    glutSwapBuffers();
}
static void reshape(int w, int h) { winW = w; winH = h > 0 ? h : 1; glViewport(0, 0, winW, winH); }
static void timerFunc(int) {
    static int last = 0;
    int now = glutGet(GLUT_ELAPSED_TIME);
    float dt = (now - last) / 1000.0f; last = now;
    if (dt > 0.05f) dt = 0.05f;
    simT += dt;
    nightF += ((float)nightTarget - nightF) * clampf(dt * 1.2f, 0, 1);
    updateCamera(dt);
    glutPostRedisplay();
    glutTimerFunc(16, timerFunc, 0);
}
static void keyDown(unsigned char k, int, int) {
    keys[k] = 1;
    if (k == 27) exit(0);
    if (k == 'c' || k == 'C') { if (camAuto) toManual(); else camAuto = 1; }
    if (k == 'n' || k == 'N') nightTarget = !nightTarget;
    if (k == 'h' || k == 'H') showHelp = !showHelp;
    if (k == 'r' || k == 'R') { camAuto = 1; simT = 0; }
    if (k == 'w' || k == 'W' || k == 'a' || k == 'A' || k == 's' || k == 'S' || k == 'd' || k == 'D' ||
        k == 'q' || k == 'Q' || k == 'e' || k == 'E' || k == 'z' || k == 'Z') toManual();
    if (k == '1') setView(0, 6, 30, 0, 3.5f, -8);
    if (k == '2') setView(0, 5.5f, 9, 0, 5.6f, -4);
    if (k == '3') setView(3, 2.2f, 7, 0, 2.0f, -4);
    if (k == '4') setView(-24, 4.5f, 16, 0, 3.0f, -4);
}
static void keyUp(unsigned char k, int, int) { keys[k] = 0; }
static void specialDown(int k, int, int) { if (k >= 0 && k < 256) skeys[k] = 1; if (k >= 100 && k <= 103) toManual(); }
static void specialUp(int k, int, int) { if (k >= 0 && k < 256) skeys[k] = 0; }
static void mouseBtn(int b, int s, int x, int y) {
    if (b == GLUT_LEFT_BUTTON) { mouseDown = (s == GLUT_DOWN); lastMx = x; lastMy = y; if (mouseDown) toManual(); }
}
static void mouseMove(int x, int y) {
    if (!mouseDown) return;
    mYaw += (x - lastMx) * 0.005f; mPitch -= (y - lastMy) * 0.005f; mPitch = clampf(mPitch, -1.4f, 1.4f);
    lastMx = x; lastMy = y;
}
static void initGL() {
    glEnable(GL_DEPTH_TEST); glEnable(GL_LIGHTING); glEnable(GL_NORMALIZE);
    glEnable(GL_COLOR_MATERIAL); glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glShadeModel(GL_SMOOTH); glEnable(GL_LINE_SMOOTH);
    GLfloat spec[4] = {0.2f, 0.2f, 0.2f, 1};
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec); glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 25);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    Q = gluNewQuadric(); gluQuadricNormals(Q, GLU_SMOOTH);
    makeTexture(texBrick, 0); makeTexture(texPave, 1); makeTexture(texRoad, 2); makeTexture(texGrass, 3);
}
int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(winW, winH);
    glutCreateWindow("Sweet Brew Cafe - 3D Outdoor Design");
    initGL();
    glutDisplayFunc(display); glutReshapeFunc(reshape);
    glutKeyboardFunc(keyDown); glutKeyboardUpFunc(keyUp);
    glutSpecialFunc(specialDown); glutSpecialUpFunc(specialUp);
    glutMouseFunc(mouseBtn); glutMotionFunc(mouseMove);
    glutIgnoreKeyRepeat(1);
    glutTimerFunc(16, timerFunc, 0);
    glutMainLoop();
    return 0;
}
