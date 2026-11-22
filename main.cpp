
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
#define YP   0.12f          // floor level
#define ZF   (-4.0f)        // front wall
#define ZB   (-20.0f)       // back wall
#define WH   7.0f           // wall height

static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static void emitOn()  { glDisable(GL_LIGHTING); }
static void emitOff() { glEnable(GL_LIGHTING); }

static GLUquadricObj* Q;
static int   winW = 1280, winH = 720;
static float simT = 0, nightF = 0;
static int   nightTarget = 0, camAuto = 1, showHelp = 1;
static float eye[3] = {0, 16, 10}, ctr[3] = {0, 1.5f, -13};
static float mYaw = 0, mPitch = -0.2f, mPos[3] = {0, 16, 10};
static int   keys[256], skeys[256];
static int   mouseDown = 0, lastMx = 0, lastMy = 0;
static GLuint texBrick, texPave, texRoad, texGrass, texWood, texTile, texMarble;
static float gA = 1.0f;                       // alpha (ceiling / front wall fade korar jonno)
static bool  wL = true, wR = true, wB = true; // kon kon dewal ekhon dekhano hobe

struct Halo { float x, y, z, r, cr, cg, cb; };
static Halo hl[220];
static int  nh = 0;
static void addHalo(float x, float y, float z, float r, float cr, float cg, float cb) {
    if (nightF < 0.08f || nh >= 220) return;
    hl[nh].x = x; hl[nh].y = y; hl[nh].z = z; hl[nh].r = r; hl[nh].cr = cr; hl[nh].cg = cg; hl[nh].cb = cb; nh++;
}
static void col(float r, float g, float b) { glColor4f(r, g, b, gA); }


// ---------------- primitives ----------------
static float hashf(int n) {                          // -1..1 noise
    unsigned int h = (unsigned int)n;
    h = (h << 13) ^ h;
    return 1.0f - (float)((h * (h * h * 15731u + 789221u) + 1376312589u) & 0x7fffffffu) / 1073741824.0f;
}

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

static float strokeW(const char* s) {
    float w = 0; while (*s) { w += glutStrokeWidth(GLUT_STROKE_ROMAN, *s); s++; } return w;
}


// ---------------------------------------------------------------
//  Quads (winding nijei thik hoy) + textured quads
// ---------------------------------------------------------------
static void quadN(float x0, float y0, float z0, float x1, float y1, float z1,
                  float x2, float y2, float z2, float x3, float y3, float z3,
                  float u, float v, float nx, float ny, float nz) {
    float p[4][3] = {{x0,y0,z0},{x1,y1,z1},{x2,y2,z2},{x3,y3,z3}};
    float t[4][2] = {{0,0},{u,0},{u,v},{0,v}};
    float ax = x1-x0, ay = y1-y0, az = z1-z0, bx = x2-x0, by = y2-y0, bz = z2-z0;
    float cx = ay*bz - az*by, cy = az*bx - ax*bz, cz = ax*by - ay*bx;
    int o[4] = {0, 1, 2, 3};
    if (cx*nx + cy*ny + cz*nz < 0) { o[1] = 3; o[3] = 1; }
    glNormal3f(nx, ny, nz);
    glBegin(GL_QUADS);
    for (int k = 0; k < 4; k++) { glTexCoord2f(t[o[k]][0], t[o[k]][1]); glVertex3f(p[o[k]][0], p[o[k]][1], p[o[k]][2]); }
    glEnd();
}
static void flatQuad(float x0, float z0, float x1, float z1, float y) { quadN(x0,y,z0, x1,y,z0, x1,y,z1, x0,y,z1, 1,1, 0,1,0); }
static void ceilQuad(float x0, float z0, float x1, float z1, float y) { quadN(x0,y,z0, x1,y,z0, x1,y,z1, x0,y,z1, 1,1, 0,-1,0); }
static void quadZ(float x0, float x1, float y0, float y1, float z, float nz) { quadN(x0,y0,z, x1,y0,z, x1,y1,z, x0,y1,z, 1,1, 0,0,nz); }
static void quadX(float z0, float z1, float y0, float y1, float x, float nx) { quadN(x,y0,z0, x,y0,z1, x,y1,z1, x,y1,z0, 1,1, nx,0,0); }
static void texOn(GLuint t) { glBindTexture(GL_TEXTURE_2D, t); glEnable(GL_TEXTURE_2D); glColor4f(1, 1, 1, gA); }
static void texOff() { glDisable(GL_TEXTURE_2D); }
static void texFloor(GLuint t, float x0, float z0, float x1, float z1, float y, float tile) {
    texOn(t); quadN(x0,y,z0, x1,y,z0, x1,y,z1, x0,y,z1, (x1-x0)/tile, (z1-z0)/tile, 0,1,0); texOff();
}
static void texWallZ(GLuint t, float x0, float x1, float y0, float y1, float z, float nz, float tile) {
    texOn(t); quadN(x0,y0,z, x1,y0,z, x1,y1,z, x0,y1,z, (x1-x0)/tile, (y1-y0)/tile, 0,0,nz); texOff();
}
static void texWallX(GLuint t, float z0, float z1, float y0, float y1, float x, float nx, float tile) {
    texOn(t); quadN(x,y0,z0, x,y0,z1, x,y1,z1, x,y1,z0, (z1-z0)/tile, (y1-y0)/tile, nx,0,0); texOff();
}

// ---------------------------------------------------------------
//  Procedural textures
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
            if (y % 16 < 2 || bx < 2) { r = 214; g = 206; b = 190; } else { r = 170 + v; g = 78 + v * 0.5f; b = 58 + v * 0.3f; }
        } else if (kind == 1) {                            // paving
            int row = y / 32, off = (row & 1) * 32, px = (x + off) / 64;
            float v = hashf(row * 17 + px * 13 + 3) * 14.0f; int jx = (x + off) % 64, jy = y % 32;
            if (jx < 2 || jy < 2) { r = 128; g = 122; b = 114; } else { r = 198 + v; g = 188 + v; b = 172 + v; }
        } else if (kind == 2) {                            // asphalt
            r = g = b = 58 + nz * 1.5f; if (hashf(x * 31 + y * 57 + 11) > 0.92f) { r += 40; g += 40; b += 40; }
        } else if (kind == 3) {                            // grass
            float t = hashf(x * 7 + y * 13) * 16.0f + hashf((x / 4) * 5 + (y / 4) * 3) * 14.0f;
            r = 64 + t * 0.5f; g = 130 + t; b = 44 + t * 0.3f;
        } else if (kind == 4) {                            // wood planks
            int row = y / 16, jx = (x + row * 37) % 64; int pl = (x + row * 37) / 64;
            float v = hashf(row * 7 + pl * 3 + 1) * 16.0f;
            float grain = sinf(x * 0.35f + hashf(row * 11) * 6.0f + y * 0.9f) * 4.0f;
            if (y % 16 < 1 || jx < 1) { r = 70; g = 42; b = 22; } else { r = 150 + v + grain; g = 98 + v * 0.7f + grain * 0.7f; b = 58 + v * 0.4f; }
        } else if (kind == 5) {                            // kitchen tile (checker)
            int cx = x / 32, cy = y / 32;
            if (x % 32 < 1 || y % 32 < 1) { r = 125; g = 130; b = 134; }
            else if ((cx + cy) & 1) { r = 232; g = 232; b = 228; } else { r = 172; g = 182; b = 190; }
        } else {                                           // marble
            float vv = sinf(x * 0.09f + y * 0.05f + hashf((x / 8) * 3 + (y / 8)) * 2.0f);
            r = 236; g = 234; b = 230;
            if (fabsf(vv) < 0.07f) { r = 158; g = 158; b = 168; } else if (fabsf(vv) < 0.14f) { r = 205; g = 205; b = 212; }
        }
        if (kind != 2 && kind != 5) { r += nz; g += nz; b += nz; }
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

// ---------------------------------------------------------------
//  Text (alpha-aware) + small effects
// ---------------------------------------------------------------
static void textFit(const char* s, float cx, float cy, float z, float maxW, float maxH,
                    float r, float g, float b, float lw) {
    float sc = maxH / 119.05f, w = strokeW(s) * sc;
    if (w > maxW) { sc *= maxW / w; w = strokeW(s) * sc; }
    glDisable(GL_LIGHTING); glColor4f(r, g, b, gA); glLineWidth(lw);
    glPushMatrix(); glTranslatef(cx - w * 0.5f, cy - 52.0f * sc, z); glScalef(sc, sc, sc);
    for (const char* c = s; *c; c++) glutStrokeCharacter(GLUT_STROKE_ROMAN, *c);
    glPopMatrix(); glLineWidth(1); glEnable(GL_LIGHTING);
}
static void textWall(const char* s, float x, float y, float z, float rotY, float maxW, float maxH,
                     float r, float g, float b, float lw) {
    glPushMatrix(); glTranslatef(x, y, z); glRotatef(rotY, 0, 1, 0);
    textFit(s, 0, 0, 0, maxW, maxH, r, g, b, lw); glPopMatrix();
}
static void steamAt(float x, float y, float z, float s, int n) {
    glDisable(GL_LIGHTING); glColor4f(0.95f, 0.95f, 1.0f, 0.55f); glLineWidth(2.5f);
    for (int i = 0; i < n; i++) {
        glBegin(GL_LINE_STRIP);
        for (int k = 0; k < 12; k++)
            glVertex3f(x + (i - (n - 1) * 0.5f) * 0.12f * s + 0.07f * s * sinf(k * 0.7f + simT * 3 + i * 2), y + k * 0.1f * s, z);
        glEnd();
    }
    glLineWidth(1); glEnable(GL_LIGHTING);
}

// ---------------- outside world (outdoor design theke) ----------------
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

static void drawStreetLamp(float x, float z) {
    glColor3f(0.14f, 0.14f, 0.16f);
    cylY(x, YP, z, 0.22f, 0.14f, 0.5f); cylY(x, YP + 0.5f, z, 0.08f, 0.05f, 4.9f);
    box(x, 5.45f, z + 0.5f, 0.08f, 0.08f, 1.0f);
    glColor3f(0.22f, 0.22f, 0.24f); cylY(x, 5.2f, z + 1.0f, 0.3f, 0.12f, 0.22f);
    emitOn(); glColor3f(1.0f, 0.92f, 0.6f); ball(x, 5.15f, z + 1.0f, 0.19f); emitOff();
    addHalo(x, 5.1f, z + 1.0f, 1.8f, 1.0f, 0.85f, 0.45f);
}

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

static void setLight(int id, float x, float y, float z, float w, float r, float g, float b, float a1, float a2) {
    GLfloat pos[4] = {x, y, z, w}, dif[4] = {r, g, b, 1}, amb[4] = {0, 0, 0, 1}, spc[4] = {r * 0.3f, g * 0.3f, b * 0.3f, 1};
    glEnable(id);
    glLightfv(id, GL_POSITION, pos); glLightfv(id, GL_DIFFUSE, dif); glLightfv(id, GL_AMBIENT, amb); glLightfv(id, GL_SPECULAR, spc);
    glLightf(id, GL_CONSTANT_ATTENUATION, 1.0f); glLightf(id, GL_LINEAR_ATTENUATION, a1); glLightf(id, GL_QUADRATIC_ATTENUATION, a2);
}

static void drawText(float x, float y, const char* s) {
    glRasterPos2f(x, y); for (const char* c = s; *c; c++) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
}


// ===============================================================
//  DATA : dining tables
// ===============================================================
struct Tbl { float x, z; int cloth; };
static Tbl TB[7] = {
    {-5.6f, -6.8f, 1}, {5.6f, -6.8f, 2},
    {-2.8f, -10.6f, 0}, {2.6f, -10.6f, 1}, {6.6f, -13.2f, 2},
    {-2.8f, -14.6f, 2}, {2.4f, -14.6f, 0}
};
static const float CUSH[5][3] = {{0.75f,0.20f,0.20f},{0.20f,0.45f,0.60f},{0.25f,0.55f,0.30f},{0.85f,0.60f,0.20f},{0.55f,0.30f,0.60f}};

// ===============================================================
//  ROOM SHELL : floor, walls, wainscot, rugs
// ===============================================================
static void drawRoomShell() {
    col(0.55f, 0.53f, 0.50f); box(0, YP / 2, -12, 24.4f, YP, 16.4f);
    texFloor(texWood, -12, ZB, 12, ZF - 0.15f, YP + 0.002f, 3.0f);
    texFloor(texTile, -12, ZB, -6, -12, YP + 0.006f, 2.0f);                           // kitchen tile
    if (wL) texWallX(texBrick, ZB, ZF - 0.15f, 0, WH, -12.0f, 1, 2.0f);              // brick left wall
    col(0.94f, 0.88f, 0.76f); if (wR) quadX(ZB, ZF - 0.15f, 0, WH, 12.0f, -1);       // cream right wall
    col(0.86f, 0.57f, 0.42f); if (wB) quadZ(-12, 12, 0, WH, ZB, 1);                  // terracotta back wall
    col(0.38f, 0.24f, 0.13f);                                                         // wood wainscot
    if (wB) box(0, 0.85f, ZB + 0.06f, 24, 1.5f, 0.1f);
    if (wR) box(11.94f, 0.85f, -12, 0.1f, 1.5f, 15.7f);
    if (wL) box(-11.94f, 0.85f, -12, 0.1f, 1.5f, 15.7f);
    col(0.86f, 0.66f, 0.22f);                                                         // gold trim
    if (wB) box(0, 1.62f, ZB + 0.07f, 24, 0.06f, 0.12f);
    if (wR) box(11.93f, 1.62f, -12, 0.12f, 0.06f, 15.7f);
    if (wL) box(-11.93f, 1.62f, -12, 0.12f, 0.06f, 15.7f);
    col(0.38f, 0.10f, 0.10f); flatQuad(-1.3f, -6.1f, 1.3f, -4.4f, YP + 0.008f);      // door mat
    col(0.55f, 0.15f, 0.12f); flatQuad(-1.15f, -5.95f, 1.15f, -4.55f, YP + 0.010f);
    col(0.30f, 0.12f, 0.12f); flatQuad(8.8f, -17.8f, 11.6f, -8.2f, YP + 0.008f);     // bakery runner
    col(0.62f, 0.24f, 0.18f); flatQuad(9.0f, -17.6f, 11.4f, -8.4f, YP + 0.010f);
}

// ===============================================================
//  BACK SIGN + BACK BAR (coffee counter)
// ===============================================================
static void drawBackSign() {
    if (!wB) return;
    col(0.13f, 0.07f, 0.04f); box(0, 5.45f, ZB + 0.14f, 12.6f, 1.9f, 0.22f);
    col(0.86f, 0.66f, 0.22f);
    box(0, 6.42f, ZB + 0.14f, 12.8f, 0.1f, 0.26f); box(0, 4.48f, ZB + 0.14f, 12.8f, 0.1f, 0.26f);
    box(-6.35f, 5.45f, ZB + 0.14f, 0.1f, 1.9f, 0.26f); box(6.35f, 5.45f, ZB + 0.14f, 0.1f, 1.9f, 0.26f);
    textFit("SWEET BREW CAFE", 0.06f, 5.6f, ZB + 0.255f, 11.6f, 1.0f, 0.12f, 0.06f, 0.02f, 7.0f);
    textFit("SWEET BREW CAFE", 0, 5.65f, ZB + 0.27f, 11.6f, 1.0f, 1.0f, 0.82f, 0.28f, 4.5f);
    textFit("COFFEE & BAKERY", 0, 4.85f, ZB + 0.27f, 6.0f, 0.34f, 1.0f, 0.97f, 0.9f, 2.5f);
    int tick = (int)(simT * 6), idx = 0;
    emitOn();
    for (float x = -6.3f; x <= 6.31f; x += 0.6f, idx++) for (int k = 0; k < 2; k++) {
        float y = k ? 6.42f : 4.48f; int on = ((idx + tick) % 3 == 0);
        if (on) glColor3f(1.0f, 0.92f, 0.5f); else glColor3f(0.5f, 0.4f, 0.18f);
        ball(x, y, ZB + 0.28f, 0.07f); if (on) addHalo(x, y, ZB + 0.3f, 0.45f, 1.0f, 0.85f, 0.3f);
    }
    emitOff();
}
static void drawBackBar() {
    col(0.43f, 0.26f, 0.13f); box(0, 0.62f, -18.95f, 11.2f, 1.0f, 1.3f);
    col(0.30f, 0.17f, 0.08f); box(0, 0.25f, -18.28f, 11.0f, 0.25f, 0.05f); box(0, 0.8f, -18.28f, 11.0f, 0.08f, 0.04f);
    col(0.92f, 0.91f, 0.88f); box(0, 1.15f, -18.95f, 11.5f, 0.08f, 1.5f);
    texFloor(texMarble, -5.75f, -19.7f, 5.75f, -18.2f, 1.196f, 1.5f);
    // espresso machine
    col(0.66f, 0.68f, 0.72f); box(-3.0f, 1.65f, -19.0f, 2.0f, 0.9f, 0.8f);
    col(0.85f, 0.85f, 0.88f); box(-3.0f, 2.14f, -19.0f, 2.1f, 0.08f, 0.85f);
    col(0.12f, 0.12f, 0.14f);
    for (int i = 0; i < 2; i++) {
        float x = -3.5f + i * 1.0f; box(x, 1.6f, -18.55f, 0.36f, 0.16f, 0.2f); box(x, 1.52f, -18.35f, 0.07f, 0.07f, 0.38f);
        col(0.85f, 0.85f, 0.88f); cylY(x, 1.2f, -18.5f, 0.1f, 0.1f, 0.14f); col(0.12f, 0.12f, 0.14f);
    }
    col(0.2f, 0.2f, 0.22f); box(-3.0f, 1.24f, -18.58f, 1.8f, 0.04f, 0.4f);
    emitOn(); glColor3f(0.95f, 0.95f, 0.85f); ball(-3.9f, 1.95f, -18.58f, 0.09f); glColor3f(0.4f, 0.9f, 0.5f); ball(-2.3f, 1.95f, -18.58f, 0.04f); emitOff();
    col(0.7f, 0.7f, 0.74f); box(-2.0f, 1.55f, -18.6f, 0.04f, 0.5f, 0.04f);
    steamAt(-2.0f, 1.85f, -18.6f, 0.9f, 2);
    for (int i = 0; i < 4; i++) drawCup(-3.7f + i * 0.5f, 2.2f, -19.0f, 0.13f, 0);
    // grinder
    col(0.15f, 0.15f, 0.17f); box(-0.7f, 1.65f, -19.1f, 0.5f, 0.9f, 0.5f);
    col(0.45f, 0.27f, 0.14f); cylY(-0.7f, 2.1f, -19.1f, 0.22f, 0.16f, 0.4f);
    // cake stands
    col(0.9f, 0.9f, 0.92f); cylY(1.5f, 1.2f, -18.8f, 0.5f, 0.5f, 0.04f); cylY(1.5f, 1.24f, -18.8f, 0.05f, 0.05f, 0.3f);
    cylY(1.5f, 1.54f, -18.8f, 0.42f, 0.42f, 0.03f);
    col(0.95f, 0.72f, 0.82f); cylY(1.5f, 1.57f, -18.8f, 0.34f, 0.34f, 0.2f);
    col(0.9f, 0.15f, 0.2f); for (int i = 0; i < 6; i++) ball(1.5f + cosf(i * 1.05f) * 0.22f, 1.8f, -18.8f + sinf(i * 1.05f) * 0.22f, 0.05f);
    col(0.9f, 0.9f, 0.92f); cylY(3.0f, 1.2f, -18.8f, 0.5f, 0.5f, 0.04f);
    col(0.85f, 0.55f, 0.22f); for (int i = 0; i < 5; i++) ellip(2.75f + i * 0.13f, 1.34f, -18.8f + (i % 2) * 0.12f, 0.14f, 0.07f, 0.09f);
    // register + bell + vase
    col(0.12f, 0.12f, 0.14f); box(4.6f, 1.32f, -18.7f, 0.7f, 0.22f, 0.5f); box(4.6f, 1.55f, -18.85f, 0.5f, 0.35f, 0.04f);
    emitOn(); glColor3f(0.3f, 0.75f, 0.45f); quadZ(4.38f, 4.82f, 1.4f, 1.7f, -18.82f, 1); emitOff();
    col(0.9f, 0.7f, 0.2f); cylY(5.2f, 1.2f, -18.5f, 0.1f, 0.1f, 0.03f); ball(5.2f, 1.26f, -18.5f, 0.09f);
    col(0.8f, 0.85f, 0.92f); cylY(-5.0f, 1.2f, -18.6f, 0.1f, 0.08f, 0.3f);
    col(0.15f, 0.5f, 0.2f); cylY(-5.0f, 1.5f, -18.6f, 0.01f, 0.01f, 0.35f);
    col(0.95f, 0.3f, 0.4f); ball(-5.0f, 1.88f, -18.6f, 0.09f); ball(-4.9f, 1.8f, -18.55f, 0.07f);
    // back shelves with jars, mugs, plants
    if (wB) {
        for (int s = 0; s < 2; s++) {
            float y = 2.75f + s * 0.8f;
            col(0.4f, 0.25f, 0.13f); box(0, y, ZB + 0.3f, 11.6f, 0.07f, 0.5f);
            for (float x = -5.4f; x < 5.5f; x += 0.62f) {
                int k = (int)((hashf((int)(x * 10) + s * 77) + 1) * 2.99f) % 3;
                if (k == 0) col(0.8f, 0.55f, 0.25f); else if (k == 1) col(0.35f, 0.2f, 0.12f); else col(0.92f, 0.88f, 0.75f);
                cylY(x, y + 0.04f, ZB + 0.3f, 0.16f, 0.16f, 0.34f);
            }
        }
        col(0.1f, 0.18f, 0.14f); box(8.5f, 3.9f, ZB + 0.1f, 3.4f, 2.2f, 0.1f);          // chalk menu
        col(0.4f, 0.27f, 0.15f); box(8.5f, 5.03f, ZB + 0.1f, 3.6f, 0.1f, 0.14f); box(8.5f, 2.77f, ZB + 0.1f, 3.6f, 0.1f, 0.14f);
        textFit("TODAY'S MENU", 8.5f, 4.6f, ZB + 0.17f, 2.8f, 0.34f, 1.0f, 0.85f, 0.4f, 2.0f);
        textFit("ESPRESSO  100", 8.5f, 4.05f, ZB + 0.17f, 2.8f, 0.24f, 0.95f, 0.95f, 0.9f, 1.5f);
        textFit("LATTE  150", 8.5f, 3.6f, ZB + 0.17f, 2.8f, 0.24f, 0.95f, 0.95f, 0.9f, 1.5f);
        textFit("BURGER  350", 8.5f, 3.15f, ZB + 0.17f, 2.8f, 0.24f, 0.95f, 0.95f, 0.9f, 1.5f);
    }
}

// ===============================================================
//  KITCHEN (left-back, tiled floor)
// ===============================================================
static void drawKitchen() {
    // left-wall cabinet run with stove
    col(0.40f, 0.43f, 0.47f); box(-11.3f, 0.6f, -16.0f, 1.4f, 1.0f, 7.6f);
    col(0.80f, 0.82f, 0.85f); box(-11.2f, 1.15f, -16.0f, 1.6f, 0.08f, 7.8f);
    col(0.30f, 0.32f, 0.36f); for (int i = 0; i < 6; i++) box(-10.58f, 0.6f, -19.0f + i * 1.25f, 0.03f, 0.8f, 0.03f);
    col(0.08f, 0.08f, 0.09f); box(-11.2f, 1.21f, -16.0f, 1.3f, 0.04f, 1.7f);          // hob
    const float bx[4][2] = {{-11.5f, -16.4f}, {-10.95f, -16.4f}, {-11.5f, -15.6f}, {-10.95f, -15.6f}};
    for (int i = 0; i < 4; i++) { col(0.2f, 0.2f, 0.22f); cylY(bx[i][0], 1.23f, bx[i][1], 0.18f, 0.18f, 0.02f); }
    emitOn();
    for (int i = 0; i < 4; i++) for (int k = 0; k < 10; k++) {
        float a = k * 2 * PI / 10, f = 0.8f + 0.4f * sinf(simT * 14 + k * 1.7f + i);
        glColor3f(0.35f, 0.55f, 1.0f); ball(bx[i][0] + cosf(a) * 0.12f, 1.26f, bx[i][1] + sinf(a) * 0.12f, 0.02f * f);
    }
    emitOff();
    col(0.72f, 0.74f, 0.78f); cylY(bx[0][0], 1.25f, bx[0][1], 0.24f, 0.24f, 0.3f); cylY(bx[0][0], 1.55f, bx[0][1], 0.25f, 0.2f, 0.03f);
    col(0.2f, 0.2f, 0.22f); cylY(bx[1][0], 1.25f, bx[1][1], 0.2f, 0.2f, 0.1f); box(bx[1][0] + 0.45f, 1.36f, bx[1][1], 0.4f, 0.03f, 0.04f);
    steamAt(bx[0][0], 1.9f, bx[0][1], 1.0f, 3);
    col(0.12f, 0.12f, 0.14f); box(-10.55f, 0.58f, -16.0f, 0.05f, 0.62f, 1.4f);         // oven door
    col(0.75f, 0.75f, 0.78f); box(-10.50f, 0.8f, -16.0f, 0.04f, 0.04f, 1.0f);
    col(0.65f, 0.67f, 0.70f); box(-11.3f, 3.3f, -16.0f, 1.4f, 0.5f, 2.2f); box(-11.7f, 5.2f, -16.0f, 0.6f, 3.3f, 0.9f);   // hood
    emitOn(); glColor3f(1.0f, 0.95f, 0.75f); box(-10.62f, 3.04f, -16.0f, 0.04f, 0.04f, 1.8f); emitOff();
    if (wL) {
        texWallX(texTile, ZB + 0.3f, -12.3f, 1.25f, 3.2f, -11.98f, 1, 1.5f);
        textWall("LIVE KITCHEN", -11.9f, 4.9f, -13.4f, 90, 2.6f, 0.4f, 1.0f, 0.82f, 0.3f, 3.0f);
    }
    // back-wall cabinet + sink + fridge
    col(0.40f, 0.43f, 0.47f); box(-9.0f, 0.6f, -19.3f, 3.6f, 1.0f, 1.3f);
    col(0.80f, 0.82f, 0.85f); box(-9.0f, 1.15f, -19.3f, 3.8f, 0.08f, 1.5f);
    col(0.45f, 0.48f, 0.52f); box(-9.0f, 1.2f, -19.3f, 1.2f, 0.03f, 0.7f);
    col(0.85f, 0.87f, 0.9f); cylY(-9.0f, 1.2f, -19.6f, 0.03f, 0.03f, 0.4f); box(-9.0f, 1.58f, -19.45f, 0.04f, 0.04f, 0.32f);
    if (wB) texWallZ(texTile, -10.8f, -7.2f, 1.25f, 3.2f, ZB + 0.02f, 1, 1.5f);
    col(0.78f, 0.74f, 0.66f); box(-9.0f, 4.0f, ZB + 0.35f, 3.6f, 1.1f, 0.7f);
    col(0.5f, 0.46f, 0.4f); box(-9.9f, 4.0f, ZB + 0.72f, 0.03f, 1.0f, 0.03f); box(-9.0f, 4.0f, ZB + 0.72f, 0.03f, 1.0f, 0.03f); box(-8.1f, 4.0f, ZB + 0.72f, 0.03f, 1.0f, 0.03f);
    col(0.82f, 0.84f, 0.87f); box(-6.6f, 1.2f, -19.2f, 1.2f, 2.4f, 1.2f);
    col(0.25f, 0.25f, 0.27f); box(-6.0f, 1.5f, -18.65f, 0.04f, 0.7f, 0.04f); box(-6.0f, 0.8f, -18.65f, 0.04f, 0.4f, 0.04f);
    // prep island + marble top + food
    col(0.45f, 0.29f, 0.17f); box(-8.6f, 0.6f, -14.0f, 3.8f, 1.0f, 1.1f);
    col(0.9f, 0.9f, 0.88f); box(-8.6f, 1.15f, -14.0f, 4.0f, 0.08f, 1.3f);
    texFloor(texMarble, -10.6f, -14.65f, -6.6f, -13.35f, 1.196f, 1.5f);
    col(0.6f, 0.4f, 0.2f); box(-9.6f, 1.22f, -14.0f, 0.8f, 0.04f, 0.5f);
    col(0.85f, 0.15f, 0.10f); ball(-8.5f, 1.3f, -14.1f, 0.1f); ball(-8.3f, 1.3f, -13.9f, 0.09f); ball(-8.2f, 1.3f, -14.3f, 0.09f);
    col(0.25f, 0.65f, 0.2f); ball(-7.6f, 1.31f, -14.0f, 0.12f); col(0.95f, 0.6f, 0.15f); ball(-7.2f, 1.3f, -13.8f, 0.09f);
    col(0.9f, 0.9f, 0.95f); cylY(-7.0f, 1.2f, -14.3f, 0.18f, 0.22f, 0.12f);
    for (int i = 0; i < 3; i++) {                                                     // stools
        float sx = -10.0f + i * 1.4f;
        col(0.2f, 0.2f, 0.22f); for (int l = 0; l < 3; l++) box(sx + cosf(l * 2.09f) * 0.15f, 0.5f, -12.7f + sinf(l * 2.09f) * 0.15f, 0.04f, 0.76f, 0.04f);
        col(0.75f, 0.30f, 0.18f); cylY(sx, 0.9f, -12.7f, 0.24f, 0.24f, 0.08f);
    }
}

// ===============================================================
//  BAKERY + CASHIER (right side)
// ===============================================================
static void drawBakery() {
    col(0.43f, 0.26f, 0.13f); box(10.4f, 0.6f, -13.0f, 1.4f, 1.0f, 9.0f);
    col(0.30f, 0.17f, 0.08f); box(9.68f, 0.6f, -13.0f, 0.04f, 0.8f, 8.6f);
    col(0.92f, 0.91f, 0.88f); box(10.3f, 1.14f, -13.0f, 1.7f, 0.08f, 9.2f);
    texFloor(texMarble, 9.45f, -17.6f, 11.15f, -8.4f, 1.186f, 1.5f);
    col(0.25f, 0.18f, 0.12f);                                                         // glass case frame
    for (int i = 0; i < 5; i++) box(9.78f, 1.67f, -17.4f + i * 2.15f, 0.05f, 0.9f, 0.05f);
    box(9.78f, 1.2f, -13.0f, 0.05f, 0.05f, 8.7f); box(9.78f, 2.12f, -13.0f, 0.05f, 0.05f, 8.7f);
    col(0.88f, 0.88f, 0.9f); box(10.4f, 1.58f, -13.0f, 1.2f, 0.03f, 8.4f); box(10.4f, 2.14f, -13.0f, 1.3f, 0.04f, 8.6f);
    emitOn(); glColor3f(1.0f, 0.93f, 0.75f); box(10.4f, 2.08f, -13.0f, 0.9f, 0.03f, 8.0f); emitOff();
    for (int i = 0; i < 8; i++) {                                                     // items
        float z = -17.0f + i * 1.0f;
        int k = i % 4;
        if (k == 0) { col(0.92f, 0.68f, 0.82f); cylY(10.4f, 1.24f, z, 0.26f, 0.26f, 0.18f); col(0.9f, 0.15f, 0.2f); ball(10.4f, 1.46f, z, 0.05f); }
        else if (k == 1) { col(0.55f, 0.32f, 0.18f); cylY(10.4f, 1.24f, z, 0.26f, 0.26f, 0.18f); col(0.98f, 0.9f, 0.8f); cylY(10.4f, 1.42f, z, 0.26f, 0.26f, 0.02f); }
        else if (k == 2) { col(0.88f, 0.58f, 0.22f); for (int j = 0; j < 3; j++) ellip(10.4f, 1.3f, z - 0.25f + j * 0.25f, 0.2f, 0.09f, 0.12f); }
        else { col(0.8f, 0.5f, 0.22f); ellip(10.4f, 1.34f, z, 0.35f, 0.11f, 0.14f); }
        int m = (i + 2) % 4;                                                          // upper shelf
        if (m == 0) { col(0.95f, 0.45f, 0.55f); for (int j = 0; j < 3; j++) { cylY(10.4f, 1.62f, z - 0.25f + j * 0.25f, 0.09f, 0.1f, 0.1f); col(0.98f, 0.85f, 0.9f); ball(10.4f, 1.75f, z - 0.25f + j * 0.25f, 0.09f); col(0.95f, 0.45f, 0.55f); } }
        else if (m == 1) { col(0.85f, 0.50f, 0.22f); for (int j = 0; j < 3; j++) { glPushMatrix(); glTranslatef(10.4f, 1.7f, z - 0.25f + j * 0.25f); glRotatef(90, 1, 0, 0); glutSolidTorus(0.03, 0.08, 10, 16); glPopMatrix(); } }
        else if (m == 2) { col(0.9f, 0.62f, 0.25f); ellip(10.4f, 1.68f, z, 0.3f, 0.1f, 0.14f); ellip(10.4f, 1.68f, z + 0.3f, 0.28f, 0.1f, 0.13f); }
        else { const float MC[3][3] = {{0.95f,0.5f,0.6f},{0.5f,0.85f,0.55f},{0.95f,0.85f,0.4f}}; for (int j = 0; j < 3; j++) { col(MC[j][0], MC[j][1], MC[j][2]); ball(10.4f, 1.64f, z - 0.2f + j * 0.2f, 0.07f); } }
    }
    col(0.12f, 0.12f, 0.14f); box(10.3f, 1.34f, -8.9f, 0.5f, 0.24f, 0.6f);          // register
    emitOn(); glColor3f(0.3f, 0.75f, 0.45f); quadX(-9.1f, -8.7f, 1.4f, 1.62f, 10.04f, -1); emitOff();
    col(0.2f, 0.2f, 0.22f); box(10.7f, 1.22f, -9.4f, 0.2f, 0.06f, 0.14f);
    col(0.85f, 0.9f, 0.95f); cylY(10.7f, 1.18f, -8.45f, 0.1f, 0.1f, 0.22f); col(0.9f, 0.75f, 0.2f); ball(10.7f, 1.3f, -8.45f, 0.05f);
    col(0.92f, 0.8f, 0.55f); box(10.0f, 1.28f, -8.45f, 0.3f, 0.2f, 0.18f);
    if (wR) {
        for (int s = 0; s < 3; s++) {
            float y = 2.8f + s * 0.75f;
            col(0.4f, 0.25f, 0.13f); box(11.7f, y, -13.0f, 0.6f, 0.06f, 8.6f);
            for (int i = 0; i < 6; i++) {
                float z = -16.8f + i * 1.45f;
                col(0.66f, 0.5f, 0.28f); box(11.7f, y + 0.18f, z, 0.45f, 0.28f, 0.7f);
                col(0.82f, 0.52f, 0.22f); ellip(11.7f, y + 0.36f, z - 0.12f, 0.17f, 0.1f, 0.3f); ellip(11.7f, y + 0.36f, z + 0.2f, 0.17f, 0.1f, 0.26f);
            }
        }
        col(0.13f, 0.07f, 0.04f); box(11.88f, 5.95f, -13.0f, 0.12f, 1.1f, 6.6f);
        textWall("FRESH BAKERY", 11.8f, 5.95f, -13.0f, -90, 6.0f, 0.75f, 1.0f, 0.82f, 0.28f, 4.0f);
    }
}

// ===============================================================
//  LOUNGE (sofa, armchair, coffee table, bookshelf, lamp)
// ===============================================================
static void drawLounge() {
    col(0.50f, 0.16f, 0.14f); flatQuad(-11.6f, -10.8f, -6.4f, -5.4f, YP + 0.008f);   // rug
    col(0.93f, 0.86f, 0.70f); flatQuad(-11.35f, -10.55f, -6.65f, -5.65f, YP + 0.010f);
    col(0.70f, 0.28f, 0.20f); flatQuad(-11.1f, -10.3f, -6.9f, -5.9f, YP + 0.012f);
    // sofa
    col(0.20f, 0.26f, 0.40f);
    box(-11.0f, 0.4f, -8.2f, 1.0f, 0.35f, 3.4f); box(-11.55f, 0.85f, -8.2f, 0.3f, 0.95f, 3.4f);
    box(-11.0f, 0.55f, -6.55f, 1.0f, 0.6f, 0.3f); box(-11.0f, 0.55f, -9.85f, 1.0f, 0.6f, 0.3f);
    col(0.15f, 0.1f, 0.08f); for (int i = 0; i < 4; i++) box(-10.6f + (i % 2) * -0.8f, 0.12f, -9.7f + (i / 2) * 3.0f, 0.08f, 0.14f, 0.08f);
    col(0.30f, 0.36f, 0.52f);
    for (int k = -1; k <= 1; k++) { box(-10.85f, 0.64f, -8.2f + k * 1.05f, 0.8f, 0.18f, 0.95f); box(-11.3f, 1.0f, -8.2f + k * 1.05f, 0.22f, 0.55f, 0.95f); }
    col(0.9f, 0.7f, 0.2f); ellip(-11.0f, 1.0f, -9.3f, 0.12f, 0.25f, 0.25f); col(0.8f, 0.25f, 0.25f); ellip(-11.0f, 1.0f, -7.1f, 0.12f, 0.25f, 0.25f);
    // coffee table
    col(0.35f, 0.2f, 0.1f); cylY(-8.9f, YP + 0.4f, -8.2f, 0.62f, 0.62f, 0.05f);
    for (int l = 0; l < 3; l++) box(-8.9f + cosf(l * 2.09f) * 0.4f, YP + 0.2f, -8.2f + sinf(l * 2.09f) * 0.4f, 0.05f, 0.4f, 0.05f);
    col(0.7f, 0.2f, 0.2f); box(-9.1f, YP + 0.47f, -8.1f, 0.4f, 0.05f, 0.3f); col(0.2f, 0.4f, 0.6f); box(-9.1f, YP + 0.52f, -8.1f, 0.35f, 0.05f, 0.26f);
    drawCup(-8.6f, YP + 0.46f, -8.3f, 0.13f, 0);
    col(0.15f, 0.5f, 0.2f); ball(-8.8f, YP + 0.6f, -8.5f, 0.1f);
    // armchair
    glPushMatrix(); glTranslatef(-8.9f, YP, -10.4f);
    col(0.62f, 0.28f, 0.2f); box(0, 0.38f, 0, 0.9f, 0.3f, 0.9f); box(0, 0.85f, -0.4f, 0.9f, 0.9f, 0.14f);
    box(-0.45f, 0.6f, 0, 0.14f, 0.55f, 0.9f); box(0.45f, 0.6f, 0, 0.14f, 0.55f, 0.9f);
    col(0.72f, 0.36f, 0.26f); box(0, 0.58f, 0.02f, 0.7f, 0.14f, 0.7f);
    col(0.15f, 0.1f, 0.08f); for (int i = 0; i < 4; i++) box((i % 2 ? 0.35f : -0.35f), 0.1f, (i < 2 ? 0.35f : -0.35f), 0.07f, 0.2f, 0.07f);
    glPopMatrix();
    // floor lamp
    col(0.15f, 0.15f, 0.16f); cylY(-11.2f, YP, -5.6f, 0.15f, 0.15f, 0.03f); cylY(-11.2f, YP, -5.6f, 0.025f, 0.025f, 1.7f);
    col(0.95f, 0.85f, 0.6f); cylY(-11.2f, YP + 1.55f, -5.6f, 0.3f, 0.2f, 0.34f);
    emitOn(); glColor3f(1.0f, 0.92f, 0.65f); ball(-11.2f, YP + 1.62f, -5.6f, 0.12f); emitOff();
    addHalo(-11.2f, YP + 1.65f, -5.6f, 1.6f, 1.0f, 0.8f, 0.4f);
    // bookshelf
    if (wL) {
        col(0.35f, 0.2f, 0.1f); box(-11.75f, 1.5f, -11.1f, 0.5f, 3.0f, 1.9f);
        col(0.22f, 0.12f, 0.06f); box(-11.97f, 1.5f, -11.1f, 0.06f, 2.9f, 1.8f);
        for (int s = 0; s < 4; s++) {
            float y = 0.45f + s * 0.68f;
            col(0.4f, 0.24f, 0.12f); box(-11.7f, y, -11.1f, 0.48f, 0.05f, 1.85f);
            for (int b = 0; b < 12; b++) {
                float h = 0.32f + 0.1f * hashf(b * 3 + s * 17), z = -11.95f + b * 0.145f; int c = (b + s * 3) % 5;
                col(CUSH[c][0], CUSH[c][1], CUSH[c][2]); box(-11.7f, y + 0.03f + h / 2, z, 0.3f, h, 0.1f);
            }
        }
    }
}

// ===============================================================
//  DINING : chairs, tables, items
// ===============================================================
static void drawChairI(float x, float z, float yaw, int ci) {
    glPushMatrix(); glTranslatef(x, YP, z); glRotatef(yaw, 0, 1, 0);
    col(0.28f, 0.16f, 0.08f);
    for (int i = 0; i < 4; i++) box((i % 2 ? 0.2f : -0.2f), 0.24f, (i < 2 ? 0.2f : -0.2f), 0.05f, 0.48f, 0.05f);
    col(0.34f, 0.2f, 0.1f); box(0, 0.5f, 0, 0.5f, 0.06f, 0.5f);
    col(CUSH[ci % 5][0], CUSH[ci % 5][1], CUSH[ci % 5][2]); box(0, 0.56f, 0, 0.44f, 0.07f, 0.44f);
    col(0.28f, 0.16f, 0.08f); box(-0.21f, 0.8f, -0.23f, 0.04f, 0.6f, 0.05f); box(0.21f, 0.8f, -0.23f, 0.04f, 0.6f, 0.05f);
    box(0, 1.08f, -0.23f, 0.5f, 0.08f, 0.05f); box(0, 0.7f, -0.23f, 0.38f, 0.04f, 0.03f);
    col(CUSH[ci % 5][0], CUSH[ci % 5][1], CUSH[ci % 5][2]); box(0, 0.9f, -0.2f, 0.38f, 0.26f, 0.05f);
    glPopMatrix();
}
static void drawTableI(int i) {
    Tbl t = TB[i];
    glPushMatrix(); glTranslatef(t.x, YP, t.z);
    col(0.18f, 0.12f, 0.08f); cylY(0, 0, 0, 0.42f, 0.42f, 0.05f); cylY(0, 0.05f, 0, 0.08f, 0.08f, 0.72f);
    if (t.cloth == 0) { col(0.50f, 0.28f, 0.12f); cylY(0, 0.77f, 0, 0.95f, 0.95f, 0.07f); }
    else {
        if (t.cloth == 1) col(0.93f, 0.9f, 0.82f); else col(0.78f, 0.14f, 0.14f);
        cylY(0, 0.5f, 0, 0.99f, 0.99f, 0.3f); cylY(0, 0.76f, 0, 0.96f, 0.96f, 0.07f);
        if (t.cloth == 1) col(0.85f, 0.8f, 0.7f); else col(0.62f, 0.1f, 0.1f);
        cylY(0, 0.5f, 0, 1.0f, 1.0f, 0.04f);
    }
    float top = 0.84f;
    col(0.97f, 0.97f, 0.97f); cylY(0.45f, top, 0.1f, 0.18f, 0.18f, 0.02f); cylY(-0.45f, top, -0.1f, 0.18f, 0.18f, 0.02f);
    drawCup(0.3f, top + 0.02f, 0.45f, 0.11f, 0); drawCup(-0.3f, top + 0.02f, -0.45f, 0.11f, 180);
    col(0.8f, 0.85f, 0.9f); cylY(0, top, 0, 0.07f, 0.06f, 0.16f);
    col(0.15f, 0.5f, 0.2f); cylY(0, top + 0.14f, 0, 0.008f, 0.008f, 0.2f);
    const float FC[4][3] = {{0.95f,0.25f,0.4f},{1.0f,0.8f,0.15f},{0.95f,0.95f,0.95f},{0.65f,0.3f,0.85f}};
    col(FC[i % 4][0], FC[i % 4][1], FC[i % 4][2]); ball(0, top + 0.37f, 0, 0.07f); ball(0.06f, top + 0.3f, 0.02f, 0.05f);
    col(0.95f, 0.9f, 0.75f); cylY(0.5f, top, -0.4f, 0.04f, 0.04f, 0.1f);                // candle
    emitOn(); glColor3f(1.0f, 0.8f, 0.3f); ball(0.5f, top + 0.13f, -0.4f, 0.025f + 0.005f * sinf(simT * 12 + i)); emitOff();
    addHalo(t.x + 0.5f, YP + top + 0.13f, t.z - 0.4f, 0.5f, 1.0f, 0.75f, 0.3f);
    glPopMatrix();
    drawChairI(t.x, t.z - 1.3f, 0, i % 5); drawChairI(t.x, t.z + 1.3f, 180, (i + 1) % 5);
    drawChairI(t.x - 1.3f, t.z, 90, (i + 2) % 5); drawChairI(t.x + 1.3f, t.z, -90, (i + 3) % 5);
}
static void drawDining() { for (int i = 0; i < 7; i++) drawTableI(i); }

// ===============================================================
//  DECOR : plants, wall art, clock
// ===============================================================
static void drawPlantI(float x, float z, float s) {
    glPushMatrix(); glTranslatef(x, YP, z); glScalef(s, s, s);
    col(0.70f, 0.32f, 0.16f); cylY(0, 0, 0, 0.3f, 0.25f, 0.55f);
    col(0.86f, 0.66f, 0.22f); cylY(0, 0.5f, 0, 0.33f, 0.33f, 0.05f);
    col(0.2f, 0.12f, 0.07f); discUp(0, 0.56f, 0, 0.27f);
    for (int i = 0; i < 9; i++) {
        float a = i * 40.0f * PI / 180.0f; float g = 0.40f + 0.12f * (i % 3);
        col(0.12f, g, 0.18f); ellip(sinf(a) * 0.28f, 1.1f + 0.15f * (i % 3), cosf(a) * 0.28f, 0.14f, 0.5f, 0.14f);
    }
    col(0.16f, 0.55f, 0.2f); ellip(0, 1.5f, 0, 0.18f, 0.6f, 0.18f);
    glPopMatrix();
}
static void drawFrameArt(float x, float y, float z, float rotY, float w, float h, int idx) {
    glPushMatrix(); glTranslatef(x, y, z); glRotatef(rotY, 0, 1, 0);
    col(0.30f, 0.17f, 0.08f); box(0, 0, 0, w, h, 0.08f);
    col(0.96f, 0.93f, 0.86f); box(0, 0, 0.03f, w - 0.12f, h - 0.12f, 0.03f);
    const float P[4][3] = {{0.85f,0.45f,0.25f},{0.25f,0.5f,0.6f},{0.9f,0.75f,0.3f},{0.6f,0.3f,0.5f}};
    col(P[idx % 4][0], P[idx % 4][1], P[idx % 4][2]); box(-w * 0.12f, h * 0.08f, 0.055f, w * 0.35f, h * 0.4f, 0.01f);
    col(P[(idx + 1) % 4][0], P[(idx + 1) % 4][1], P[(idx + 1) % 4][2]); box(w * 0.14f, -h * 0.1f, 0.06f, w * 0.3f, h * 0.3f, 0.01f);
    glPopMatrix();
}
static void drawClock(float x, float y, float z) {
    col(0.3f, 0.17f, 0.08f); cylY(x, y, z, 0.62f, 0.62f, 0.1f);
    glPushMatrix(); glTranslatef(x, y, z); glRotatef(0, 0, 1, 0);
    col(0.97f, 0.95f, 0.9f); glTranslatef(0, 0, 0.11f); gluDisk(Q, 0, 0.54f, 28, 1);
    glDisable(GL_LIGHTING); glColor4f(0.1f, 0.1f, 0.1f, gA);
    for (int k = 0; k < 12; k++) { float a = k * PI / 6; glPushMatrix(); glTranslatef(sinf(a) * 0.46f, cosf(a) * 0.46f, 0.005f); glScalef(0.03f, 0.08f, 0.01f); glRotatef(0, 0, 0, 1); glutSolidCube(1); glPopMatrix(); }
    float sec = fmodf(simT, 60.0f) * 6.0f, mn = 120.0f + simT * 0.1f, hr = 300.0f + simT * 0.00833f;
    glPushMatrix(); glRotatef(-hr, 0, 0, 1); glTranslatef(0, 0.15f, 0.01f); glScalef(0.04f, 0.3f, 0.01f); glutSolidCube(1); glPopMatrix();
    glPushMatrix(); glRotatef(-mn, 0, 0, 1); glTranslatef(0, 0.21f, 0.015f); glScalef(0.03f, 0.42f, 0.01f); glutSolidCube(1); glPopMatrix();
    glColor4f(0.8f, 0.1f, 0.1f, gA);
    glPushMatrix(); glRotatef(-sec, 0, 0, 1); glTranslatef(0, 0.22f, 0.02f); glScalef(0.015f, 0.46f, 0.01f); glutSolidCube(1); glPopMatrix();
    glEnable(GL_LIGHTING); glPopMatrix();
}
static void drawDecor() {
    drawPlantI(-5.2f, -19.0f, 1.1f); drawPlantI(5.4f, -19.0f, 1.1f);
    drawPlantI(-1.9f, -5.0f, 1.0f);  drawPlantI(1.9f, -5.0f, 1.0f);
    drawPlantI(-11.2f, -4.9f, 0.9f); drawPlantI(11.2f, -5.4f, 1.1f); drawPlantI(-11.0f, -19.0f, 0.8f);
    if (wR) { drawFrameArt(11.92f, 3.6f, -6.6f, -90, 1.5f, 1.1f, 0); drawFrameArt(11.92f, 3.6f, -5.0f, -90, 1.0f, 1.4f, 1); }
    if (wB) { drawClock(8.5f, 6.0f, ZB + 0.06f); drawFrameArt(-9.0f, 5.9f, ZB + 0.06f, 0, 1.6f, 0.9f, 2); }
    if (wL) { drawFrameArt(-11.92f, 3.9f, -7.4f, 90, 1.6f, 1.2f, 3); drawFrameArt(-11.92f, 3.9f, -9.4f, 90, 1.0f, 1.0f, 1); }
}

// ===============================================================
//  CEILING GROUP (beams, fans, pendants, string lights) - fades when camera goes up
// ===============================================================
static void drawFan(float x, float z) {
    glPushMatrix(); glTranslatef(x, 6.15f, z);
    col(0.2f, 0.14f, 0.1f); cylY(0, 0, 0, 0.03f, 0.03f, 0.8f); cylY(0, -0.1f, 0, 0.2f, 0.2f, 0.16f);
    glPushMatrix(); glRotatef(simT * 220.0f, 0, 1, 0);
    for (int k = 0; k < 4; k++) { glPushMatrix(); glRotatef(k * 90.0f, 0, 1, 0); col(0.45f, 0.28f, 0.14f); box(0.75f, -0.03f, 0, 1.1f, 0.03f, 0.22f); glPopMatrix(); }
    glPopMatrix();
    emitOn(); glColor4f(1.0f, 0.93f, 0.7f, gA); ball(0, -0.22f, 0, 0.11f); emitOff();
    if (gA > 0.6f) addHalo(x, 5.9f, z, 1.2f, 1.0f, 0.85f, 0.45f);
    glPopMatrix();
}
static void drawCeilingGroup() {
    glDepthMask(gA < 0.99f ? GL_FALSE : GL_TRUE);
    col(0.94f, 0.89f, 0.78f); ceilQuad(-12, ZB, 12, ZF - 0.15f, WH - 0.05f);
    col(0.3f, 0.17f, 0.08f);
    for (int i = 0; i < 4; i++) box(0, 6.72f, -6.0f - i * 4.0f, 24, 0.3f, 0.34f);
    for (int i = -1; i <= 1; i++) box(i * 8.0f, 6.8f, -12, 0.28f, 0.18f, 16);
    col(0.93f, 0.88f, 0.78f); box(0, 6.85f, ZB + 0.1f, 24, 0.2f, 0.2f); box(-11.9f, 6.85f, -12, 0.2f, 0.2f, 16); box(11.9f, 6.85f, -12, 0.2f, 0.2f, 16);
    drawFan(-3.5f, -12.5f); drawFan(4.0f, -12.5f);
    // pendants over tables
    for (int i = 0; i < 7; i++) {
        float x = TB[i].x, z = TB[i].z;
        col(0.1f, 0.1f, 0.1f); cylY(x, 3.75f, z, 0.012f, 0.012f, 3.1f);
        col(0.92f, 0.58f, 0.22f); cylY(x, 3.3f, z, 0.45f, 0.12f, 0.4f);
        emitOn(); glColor4f(1.0f, 0.93f, 0.7f, gA); ball(x, 3.32f, z, 0.11f); emitOff();
        if (gA > 0.6f) addHalo(x, 3.2f, z, 1.5f, 1.0f, 0.8f, 0.4f);
    }
    // pot rack above kitchen island
    col(0.15f, 0.15f, 0.16f); box(-8.6f, 4.5f, -14.0f, 3.0f, 0.05f, 0.05f);
    box(-9.8f, 5.7f, -14.0f, 0.02f, 2.3f, 0.02f); box(-7.4f, 5.7f, -14.0f, 0.02f, 2.3f, 0.02f);
    for (int i = 0; i < 4; i++) { col(0.7f, 0.72f, 0.76f); cylY(-9.7f + i * 0.8f, 3.95f, -14.0f, 0.2f, 0.18f, 0.12f); col(0.15f, 0.15f, 0.16f); box(-9.7f + i * 0.8f, 4.3f, -14.0f, 0.02f, 0.4f, 0.02f); }
    // hanging plants
    for (int i = 0; i < 3; i++) {
        float x = -7.5f + i * 7.5f, z = -17.2f;
        col(0.12f, 0.12f, 0.12f); box(x, 5.6f, z, 0.015f, 2.5f, 0.015f);
        col(0.7f, 0.32f, 0.16f); cylY(x, 4.3f, z, 0.3f, 0.2f, 0.3f);
        for (int k = 0; k < 7; k++) { col(0.12f, 0.5f, 0.18f); float a = k * 0.9f; ellip(x + sinf(a) * 0.3f, 4.1f - 0.15f * (k % 3), z + cosf(a) * 0.3f, 0.08f, 0.3f + 0.1f * (k % 3), 0.08f); }
    }
    // string lights
    for (int row = 0; row < 2; row++) {
        float z = row ? -16.0f : -7.2f;
        emitOn(); glColor4f(0.2f, 0.2f, 0.2f, gA); glBegin(GL_LINE_STRIP);
        for (int i = 0; i <= 24; i++) { float x = -11.8f + i * 0.98f, u = (x / 11.8f); glVertex3f(x, 6.55f - 0.5f * (1 - u * u), z); }
        glEnd();
        for (int i = 0; i <= 24; i++) {
            float x = -11.8f + i * 0.98f, u = (x / 11.8f), y = 6.5f - 0.5f * (1 - u * u);
            int on = (sinf(simT * 2.2f + i * 0.9f + row) > -0.4f);
            if (on) glColor4f(1.0f, 0.88f, 0.5f, gA); else glColor4f(0.55f, 0.45f, 0.25f, gA);
            ball(x, y, z, 0.06f); if (on && gA > 0.6f && (i % 2 == 0)) addHalo(x, y, z, 0.5f, 1.0f, 0.8f, 0.4f);
        }
        emitOff();
    }
    glDepthMask(GL_TRUE);
}

// ===============================================================
//  FRONT WALL (outside = brick facade + sign, inside = cream plaster, curtains)
// ===============================================================
static const float FR[7][4] = {{-12,-1.4f,0,1.1f},{1.4f,12,0,1.1f},{-12,-9,1.1f,4.5f},{-3,-1.4f,1.1f,4.5f},{1.4f,3,1.1f,4.5f},{9,12,1.1f,4.5f},{-12,12,4.5f,7.0f}};
static void frameRect(float x0, float x1, float y0, float y1, float t) {
    col(0.20f, 0.11f, 0.06f);
    box((x0 + x1) / 2, y1, ZF - 0.08f, x1 - x0 + t, t, 0.3f); box((x0 + x1) / 2, y0, ZF - 0.08f, x1 - x0 + t, t, 0.3f);
    box(x0, (y0 + y1) / 2, ZF - 0.08f, t, y1 - y0, 0.3f);     box(x1, (y0 + y1) / 2, ZF - 0.08f, t, y1 - y0, 0.3f);
}
static void drawAwningI(float cx, float w) {
    int n = (int)(w / 0.5f + 0.5f); float sw = w / n;
    const float y0 = 4.5f, z0 = ZF + 0.05f, y1 = 3.85f, z1 = ZF + 1.7f;
    for (int i = 0; i < n; i++) {
        float x = cx - w / 2 + i * sw;
        if (i % 2) col(0.97f, 0.95f, 0.90f); else col(0.78f, 0.10f, 0.13f);
        quadN(x,y0,z0, x+sw,y0,z0, x+sw,y1,z1, x,y1,z1, 1,1, 0,0.93f,0.37f);
        quadZ(x, x + sw, y1 - 0.22f, y1, z1, 1);
        glNormal3f(0, 0, 1); glBegin(GL_TRIANGLES); glVertex3f(x, y1 - 0.22f, z1); glVertex3f(x + sw, y1 - 0.22f, z1); glVertex3f(x + sw * 0.5f, y1 - 0.5f, z1); glEnd();
    }
}
static void drawFrontWall() {
    glDepthMask(gA < 0.99f ? GL_FALSE : GL_TRUE);
    for (int i = 0; i < 7; i++) {                                                     // outer brick + inner plaster
        texWallZ(texBrick, FR[i][0], FR[i][1], FR[i][2], FR[i][3], ZF, 1, 2.0f);
        col(0.94f, 0.88f, 0.76f); quadZ(FR[i][0], FR[i][1], FR[i][2], FR[i][3], ZF - 0.15f, -1);
    }
    // window + door frames
    for (int w = 0; w < 2; w++) {
        float x0 = w ? 3.0f : -9.0f, x1 = w ? 9.0f : -3.0f;
        frameRect(x0, x1, 1.1f, 4.5f, 0.16f);
        col(0.20f, 0.11f, 0.06f); box((x0 + x1) / 2, 2.8f, ZF - 0.1f, x1 - x0, 0.09f, 0.1f); box((x0 + x1) / 2, 2.8f, ZF - 0.1f, 0.09f, 3.4f, 0.1f);
        col(0.86f, 0.83f, 0.76f); box((x0 + x1) / 2, 1.05f, ZF + 0.12f, x1 - x0 + 0.5f, 0.12f, 0.55f);       // outer sill
        box((x0 + x1) / 2, 1.18f, ZF - 0.32f, x1 - x0 + 0.3f, 0.08f, 0.28f);                                   // inner sill
        // inner curtains + rod
        col(0.80f, 0.25f, 0.22f);
        for (int k = 0; k < 3; k++) { box(x0 + 0.05f + k * 0.17f, 2.9f, ZF - 0.3f, 0.14f, 3.7f, 0.05f); box(x1 - 0.05f - k * 0.17f, 2.9f, ZF - 0.3f, 0.14f, 3.7f, 0.05f); }
        col(0.86f, 0.66f, 0.22f); box((x0 + x1) / 2, 4.85f, ZF - 0.3f, x1 - x0 + 0.5f, 0.06f, 0.06f);
        col(0.7f, 0.32f, 0.16f); cylY((x0 + x1) / 2, 1.26f, ZF - 0.34f, 0.12f, 0.1f, 0.14f); col(0.15f, 0.55f, 0.2f); ball((x0 + x1) / 2, 1.55f, ZF - 0.34f, 0.15f);
    }
    frameRect(-1.4f, 1.4f, YP, 3.5f, 0.16f); frameRect(-1.4f, 1.4f, 3.5f, 4.5f, 0.16f);
    col(0.20f, 0.11f, 0.06f); box(0, 1.8f, ZF - 0.14f, 0.08f, 3.4f, 0.1f);
    col(0.52f, 0.42f, 0.22f); box(-0.7f, 0.4f, ZF - 0.14f, 1.2f, 0.5f, 0.05f); box(0.7f, 0.4f, ZF - 0.14f, 1.2f, 0.5f, 0.05f);
    col(0.9f, 0.7f, 0.2f); box(-0.18f, 1.8f, ZF - 0.22f, 0.05f, 0.9f, 0.05f); box(0.18f, 1.8f, ZF - 0.22f, 0.05f, 0.9f, 0.05f);
    textWall("THANK YOU - COME AGAIN", 0, 4.0f, ZF - 0.22f, 180, 2.3f, 0.3f, 1.0f, 0.82f, 0.3f, 2.5f);
    // outside decoration: cornice, pilasters, sign, awnings
    col(0.93f, 0.88f, 0.78f); box(0, 6.98f, ZF + 0.1f, 24.8f, 0.28f, 0.5f);
    box(-11.85f, 3.5f, ZF + 0.1f, 0.5f, 7.0f, 0.35f); box(11.85f, 3.5f, ZF + 0.1f, 0.5f, 7.0f, 0.35f);
    col(0.42f, 0.23f, 0.13f); box(0, 7.5f, ZF - 0.1f, 24.4f, 0.55f, 0.3f);
    col(0.93f, 0.88f, 0.78f); box(0, 7.82f, ZF - 0.1f, 24.8f, 0.1f, 0.46f);
    col(0.13f, 0.07f, 0.04f); box(0, 5.75f, ZF + 0.2f, 14.4f, 1.9f, 0.3f);
    col(0.86f, 0.66f, 0.22f); box(0, 6.72f, ZF + 0.2f, 14.6f, 0.1f, 0.34f); box(0, 4.78f, ZF + 0.2f, 14.6f, 0.1f, 0.34f);
    box(-7.25f, 5.75f, ZF + 0.2f, 0.1f, 1.9f, 0.34f); box(7.25f, 5.75f, ZF + 0.2f, 0.1f, 1.9f, 0.34f);
    if (gA > 0.9f) {
        textFit("SWEET BREW CAFE", 0.06f, 5.9f, ZF + 0.355f, 12.9f, 1.0f, 0.12f, 0.06f, 0.02f, 7.0f);
        textFit("SWEET BREW CAFE", 0, 5.95f, ZF + 0.37f, 12.9f, 1.0f, 1.0f, 0.82f, 0.28f, 4.5f);
        textFit("COFFEE & BAKERY", 0, 5.12f, ZF + 0.37f, 6.6f, 0.36f, 1.0f, 0.97f, 0.9f, 2.5f);
    }
    drawAwningI(-6, 6.8f); drawAwningI(6, 6.8f);
    glDepthMask(GL_TRUE);
}

// ===============================================================
//  GLASS (transparent, drawn last)
// ===============================================================
static void drawGlass(float cutA) {
    glDisable(GL_LIGHTING); glDepthMask(GL_FALSE);
    glColor4f(0.72f, 0.88f, 0.96f, 0.16f);                                            // bakery display case
    quadX(-17.4f, -8.6f, 1.22f, 2.1f, 9.72f, 1); quadZ(9.75f, 11.0f, 1.22f, 2.1f, -17.4f, 1); quadZ(9.75f, 11.0f, 1.22f, 2.1f, -8.6f, 1);
    glColor4f(0.72f, 0.88f, 0.96f, 0.10f); quadN(9.75f,2.14f,-17.4f, 11.0f,2.14f,-17.4f, 11.0f,2.14f,-8.6f, 9.75f,2.14f,-8.6f, 1,1, 0,1,0);
    if (cutA > 0.02f) {
        float z = ZF - 0.14f;
        glColor4f(0.62f, 0.82f, 0.94f, 0.12f * cutA);
        quadZ(-9, -3, 1.1f, 4.5f, z, 1); quadZ(3, 9, 1.1f, 4.5f, z, 1); quadZ(-1.4f, 1.4f, 3.5f, 4.5f, z, 1);
        glColor4f(0.62f, 0.82f, 0.94f, 0.16f * cutA); quadZ(-1.4f, 0, YP, 3.5f, z, 1); quadZ(0, 1.4f, YP, 3.5f, z, 1);
        glColor4f(1, 1, 1, 0.12f * cutA * (1 - nightF * 0.6f));
        for (int w = 0; w < 2; w++) {
            float x = w ? 3.0f : -9.0f;
            glBegin(GL_QUADS); glVertex3f(x + 0.8f, 1.2f, z + 0.01f); glVertex3f(x + 1.5f, 1.2f, z + 0.01f); glVertex3f(x + 3.1f, 4.4f, z + 0.01f); glVertex3f(x + 2.4f, 4.4f, z + 0.01f); glEnd();
        }
    }
    glDepthMask(GL_TRUE); glEnable(GL_LIGHTING);
}

// ===============================================================
//  OUTSIDE WORLD (window theke dekha jay)
// ===============================================================
static void drawOutside() {
    gA = 1;
    drawTree(-13.8f, YP, 0.8f, 1.0f, 0); drawTree(13.8f, YP, 0.8f, 1.1f, 1.5f);
    for (int i = 0; i < 6; i++) drawTree(-27 + i * 11.0f, YP, 12.6f, 1.0f + 0.15f * hashf(i), i * 0.8f);
    for (int i = 0; i < 4; i++) { drawTree(-18, 0, -4 - i * 7.0f, 1.3f, i); drawTree(18, 0, -4 - i * 7.0f, 1.3f, i + 2); }
    for (int i = 0; i < 9; i++) drawTree(-44 + i * 11.0f, 0, -30, 1.4f, i * 0.5f);
    for (int i = 0; i < 4; i++) drawStreetLamp(-15 + i * 10.0f, 1.9f);
    drawWalker(fmodf(simT * 1.1f + 10, 60) - 30, 1.35f, 90, simT * 5.0f, 0);
    drawWalker(30 - fmodf(simT * 0.9f + 25, 60), 0.95f, -90, simT * 4.5f + 1, 1);
    drawCar(fmodf(simT * 6.0f + 10, 140) - 70, 4.6f, 0, 0.82f, 0.12f, 0.14f);
    drawCar(fmodf(simT * 6.0f + 80, 140) - 70, 4.6f, 0, 0.18f, 0.6f, 0.3f);
    drawCar(70 - fmodf(simT * 5.5f + 20, 140), 8.2f, 180, 0.2f, 0.4f, 0.8f);
    drawCar(70 - fmodf(simT * 5.5f + 90, 140), 8.2f, 180, 0.95f, 0.78f, 0.15f);
}

// ===============================================================
//  LIGHTS, HUD, CAMERA
// ===============================================================
static void setupLights() {
    GLfloat ga[4] = {lerpf(0.42f, 0.16f, nightF), lerpf(0.40f, 0.15f, nightF), lerpf(0.38f, 0.22f, nightF), 1};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ga);
    float d = 1 - nightF, wl = 0.55f + 0.45f * nightF;
    setLight(GL_LIGHT0, 0.45f, 0.75f, 0.6f, 0, 0.62f * d + 0.08f * nightF, 0.60f * d + 0.10f * nightF, 0.56f * d + 0.20f * nightF, 0, 0);
    setLight(GL_LIGHT1, -6, 5.8f, -8, 1, 1.0f * wl, 0.84f * wl, 0.55f * wl, 0.02f, 0.003f);
    setLight(GL_LIGHT2, 6, 5.8f, -8, 1, 1.0f * wl, 0.84f * wl, 0.55f * wl, 0.02f, 0.003f);
    setLight(GL_LIGHT3, 0, 5.8f, -15, 1, 1.0f * wl, 0.84f * wl, 0.55f * wl, 0.02f, 0.003f);
    setLight(GL_LIGHT4, -8.5f, 5.5f, -16.5f, 1, 0.8f * wl, 0.88f * wl, 1.0f * wl, 0.03f, 0.004f);
    setLight(GL_LIGHT5, 8.5f, 5.5f, -14, 1, 1.0f * wl, 0.80f * wl, 0.5f * wl, 0.03f, 0.004f);
    setLight(GL_LIGHT6, -5, 5.2f, 3.0f, 1, 1.0f * nightF, 0.85f * nightF, 0.5f * nightF, 0.03f, 0.008f);
    setLight(GL_LIGHT7, 5, 5.2f, 3.0f, 1, 1.0f * nightF, 0.85f * nightF, 0.5f * nightF, 0.03f, 0.008f);
}
static void drawHUD() {
    glDisable(GL_LIGHTING); glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); gluOrtho2D(0, winW, 0, winH);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    glColor4f(0, 0, 0, 0.45f);
    glBegin(GL_QUADS); glVertex2f(0, winH); glVertex2f(winW, winH); glVertex2f(winW, winH - 30); glVertex2f(0, winH - 30); glEnd();
    char buf[160];
    sprintf(buf, "SWEET BREW CAFE  -  3D INTERIOR     |     %s     |     Camera: %s", nightF > 0.5f ? "NIGHT" : "DAY", camAuto ? "AUTO TOUR" : "MANUAL");
    glColor3f(1, 0.9f, 0.5f); drawText(12, winH - 20, buf);
    if (showHelp) { glColor3f(1, 1, 1); drawText(12, 14, "W A S D / Q E: move | Mouse drag or Arrows: look | B: boost | C: auto tour | N: day/night | 1-6: views | R: restart tour | H: help | ESC: exit"); }
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
static const float TW[9][6] = {
    {0, 16, 10,        0, 1.5f, -13},        // dollhouse (cutaway) top-front
    {-8, 12, 6,        0, 1.0f, -14},
    {0, 2.4f, 5,       0, 2.6f, -12},        // street, door er samne
    {0, 2.3f, -5.2f,   0, 2.8f, -20},        // bhitore dhukei
    {-3.5f, 2.1f, -9.5f, -9.5f, 1.6f, -16.5f},   // kitchen
    {0, 2.5f, -10,     0, 3.6f, -20},        // back bar + sign
    {4, 2.1f, -9,      10.5f, 1.5f, -14},    // bakery
    {-3, 2.2f, -13.5f, -10, 1.1f, -7},       // lounge
    {4, 2.2f, -17,     0, 2.8f, -4}          // jaanala + darja
};
static void updateCamera(float dt) {
    if (camAuto) {
        float seg = simT / 6.5f; int i = (int)seg % 9, j = (i + 1) % 9; float f = seg - floorf(seg);
        f = f * f * f * (f * (f * 6 - 15) + 10);
        for (int k = 0; k < 3; k++) { eye[k] = lerpf(TW[i][k], TW[j][k], f); ctr[k] = lerpf(TW[i][3 + k], TW[j][3 + k], f); }
    } else {
        float sp = (keys['b'] || keys['B']) ? 12.0f : 5.0f;
        float fw[3] = {sinf(mYaw) * cosf(mPitch), sinf(mPitch), -cosf(mYaw) * cosf(mPitch)};
        float r[3] = {cosf(mYaw), 0, sinf(mYaw)};
        if (keys['w'] || keys['W']) for (int k = 0; k < 3; k++) mPos[k] += fw[k] * sp * dt;
        if (keys['s'] || keys['S']) for (int k = 0; k < 3; k++) mPos[k] -= fw[k] * sp * dt;
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
    gluPerspective(58.0, (double)winW / (double)winH, 0.25, 600.0);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    gluLookAt(eye[0], eye[1], eye[2], ctr[0], ctr[1], ctr[2], 0, 1, 0);
    nh = 0;
    wL = eye[0] > -12.2f; wR = eye[0] < 12.2f; wB = eye[2] > ZB + 0.2f;
    float cutA = clampf((7.6f - eye[1]) / 1.4f, 0, 1);
    drawSky(); drawClouds();
    setupLights();
    drawGround(); drawBackdrop(); drawOutside();
    gA = 1;
    drawRoomShell(); drawBackSign(); drawBackBar(); drawKitchen(); drawBakery(); drawLounge(); drawDining(); drawDecor();
    gA = cutA;
    if (gA > 0.02f) { drawCeilingGroup(); drawFrontWall(); }
    gA = 1;
    drawGlass(cutA);
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
    if (k == '1') setView(0, 16, 10, 0, 1.5f, -13);
    if (k == '2') setView(0, 2.3f, -5.2f, 0, 2.8f, -20);
    if (k == '3') setView(-3.5f, 2.1f, -9.5f, -9.5f, 1.6f, -16.5f);
    if (k == '4') setView(4, 2.1f, -9, 10.5f, 1.5f, -14);
    if (k == '5') setView(-3, 2.2f, -13.5f, -10, 1.1f, -7);
    if (k == '6') setView(4, 2.2f, -17, 0, 2.8f, -4);
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
    makeTexture(texWood, 4); makeTexture(texTile, 5); makeTexture(texMarble, 6);
}
int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(winW, winH);
    glutCreateWindow("Sweet Brew Cafe - 3D Interior Design");
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
