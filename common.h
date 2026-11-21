#ifndef COMMON_H
#define COMMON_H
// ===============================================================
//  SWEET BREW CAFE 3D   -   OpenGL + GLUT   (Code::Blocks)
//
//  Story:  Bahire building + billboard  ->  customer dhuke  ->  high-ceiling interior
//          -> friends / family table e bose  -> waiter order nei  -> LIVE KITCHEN e ranna
//          -> waiter khabar dey -> khay -> bill -> pay -> berie jay
//          -> kichu customer sudhu parcel nie tk dey chole jay (cashier + bakery shelf)
//          -> AUTO fire alarm -> sprinkler pani -> agun off -> bill -> sobai berie jay
//
//  (SHARED BASE - dujon e use korbe, kew edit korbe na)
//  Keys:  C = auto camera ON/OFF      F = fire start
//         1..6 = camera view          W A S D = move   Q / Z = up / down
//         Arrow keys or Mouse drag = look around      B = boost speed (hold)
//         P = pause     H = help on/off     ESC = exit
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


// ---------------------------------------------------------------
//  Small utilities
// ---------------------------------------------------------------
static float frand() { return rand() / (float)RAND_MAX; }
static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static float smooth01(float t) { t = clampf(t, 0, 1); return t * t * (3 - 2 * t); }
static float hashf(int n) {                       // -1..1 deterministic noise
    unsigned int h = (unsigned int)n;
    h = (h << 13) ^ h;
    return 1.0f - (float)((h * (h * h * 15731u + 789221u) + 1376312589u) & 0x7fffffffu) / 1073741824.0f;
}
static float turnToward(float cur, float target, float maxStep) {
    float d = fmodf(target - cur, 360.0f);
    if (d > 180) d -= 360;
    if (d < -180) d += 360;
    if (d > maxStep) d = maxStep;
    if (d < -maxStep) d = -maxStep;
    return cur + d;
}
static void copy3(float* d, const float* s) { d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; }

// ---------------------------------------------------------------
//  Globals
// ---------------------------------------------------------------
static GLUquadricObj* Q;
static int   winW = 1280, winH = 720;
static float simTime = 0;
static int   paused = 0, showHelp = 1;
static int   camAuto = 1;
static float camE[3] = {0, 5, 30}, camC[3] = {0, 4, 0};
static float mYaw = 0, mPitch = 0, mPos[3] = {0, 3, 30};
static int   keys[256], skeys[256];
static int   mouseDown = 0, lastMx = 0, lastMy = 0;
static int   income = 0, served = 0;
static float payFlash = 0, doorOpen = 0, insideF = 0;
static GLuint texWood, texTile, texBrick, texPave;

// fire system
static int   fireState = 0;          // 0 none, 1 burning, 2 sprinkler, 3 smoke clearing
static float fireT = 0, fireLevel = 0, wetAmt = 0, fireBlend = 0;
static int   alarmOn = 0, sprinklerOn = 0, autoFireDone = 0;
static float beepT = 0;
#define AUTO_FIRE_TIME 75.0f

// ---------------------------------------------------------------
//  Primitive helpers
// ---------------------------------------------------------------
static void boxc(float x, float y, float z, float sx, float sy, float sz) {
    glPushMatrix(); glTranslatef(x, y, z); glScalef(sx, sy, sz); glutSolidCube(1.0); glPopMatrix();
}
static void ball(float x, float y, float z, float r) {
    glPushMatrix(); glTranslatef(x, y, z); glutSolidSphere(r, 16, 12); glPopMatrix();
}
static void ellip(float x, float y, float z, float rx, float ry, float rz) {
    glPushMatrix(); glTranslatef(x, y, z); glScalef(rx, ry, rz); glutSolidSphere(1.0, 16, 12); glPopMatrix();
}
static void cylY(float x, float y, float z, float r1, float r2, float h) {   // upright, capped
    glPushMatrix();
    glTranslatef(x, y, z); glRotatef(-90, 1, 0, 0);
    gluCylinder(Q, r1, r2, h, 20, 1);
    gluDisk(Q, 0, r1, 20, 1);
    glTranslatef(0, 0, h); gluDisk(Q, 0, r2, 20, 1);
    glPopMatrix();
}
static void discUp(float x, float y, float z, float r) {
    glPushMatrix(); glTranslatef(x, y, z); glRotatef(-90, 1, 0, 0); gluDisk(Q, 0, r, 28, 1); glPopMatrix();
}
static void limbDown(float len, float r1, float r2) {
    glPushMatrix(); glRotatef(90, 1, 0, 0); gluCylinder(Q, r1, r2, len, 10, 1); glPopMatrix();
}
static void emitOn()  { glDisable(GL_LIGHTING); }
static void emitOff() { glEnable(GL_LIGHTING); }

// ---------------------------------------------------------------
//  Procedural textures
// ---------------------------------------------------------------
static void makeTexture(GLuint& id, int kind) {
    const int N = 128;
    static unsigned char img[128][128][3];
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
        float r = 200, g = 200, b = 200;
        float nz = hashf(x * 73 + y * 179 + kind * 977) * 5.0f;
        if (kind == 0) {                           // wooden floor
            int row = y / 16;
            float tone = hashf(row * 31 + 7) * 16.0f;
            float grain = sinf(x * 0.1f + hashf(row) * 6 + y * 0.35f) * 5.0f;
            r = 172 + tone + grain; g = 120 + tone * 0.7f + grain; b = 72 + tone * 0.4f;
            if (y % 16 < 1 || (x + row * 37) % 64 == 0) { r *= 0.55f; g *= 0.55f; b *= 0.55f; }
        } else if (kind == 1) {                    // kitchen tile
            int ct = ((x / 32) + (y / 32)) & 1;
            r = ct ? 226 : 150; g = ct ? 229 : 156; b = ct ? 233 : 164;
            if (x % 32 == 0 || y % 32 == 0) { r = 105; g = 110; b = 116; }
        } else if (kind == 2) {                    // brick
            int row = y / 16, off = (row & 1) * 16, bx = (x + off) % 32;
            float v = hashf(row * 31 + (x + off) / 32) * 18.0f;
            if (y % 16 < 2 || bx < 2) { r = 208; g = 200; b = 186; }
            else { r = 168 + v; g = 76 + v * 0.5f; b = 56; }
        } else {                                   // pavement
            int ct = ((x / 32) + (y / 32)) & 1;
            r = g = b = ct ? 170 : 154; b -= 4;
            if (x % 32 == 0 || y % 32 == 0) { r = g = b = 100; }
        }
        r += nz; g += nz; b += nz;
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

static void texFloorQuad(GLuint t, float x0, float z0, float x1, float z1, float y, float tile) {
    glBindTexture(GL_TEXTURE_2D, t); glEnable(GL_TEXTURE_2D); glColor3f(1, 1, 1);
    float u = (x1 - x0) / tile, v = (z1 - z0) / tile;
    glNormal3f(0, 1, 0);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex3f(x0, y, z0);
    glTexCoord2f(u, 0); glVertex3f(x1, y, z0);
    glTexCoord2f(u, v); glVertex3f(x1, y, z1);
    glTexCoord2f(0, v); glVertex3f(x0, y, z1);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}
static void texWallZ(GLuint t, float x0, float x1, float y0, float y1, float z, float nz, float tile) {
    glBindTexture(GL_TEXTURE_2D, t); glEnable(GL_TEXTURE_2D); glColor3f(1, 1, 1);
    float u = (x1 - x0) / tile, v = (y1 - y0) / tile;
    glNormal3f(0, 0, nz);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex3f(x0, y0, z);
    glTexCoord2f(u, 0); glVertex3f(x1, y0, z);
    glTexCoord2f(u, v); glVertex3f(x1, y1, z);
    glTexCoord2f(0, v); glVertex3f(x0, y1, z);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}
static void texWallX(GLuint t, float z0, float z1, float y0, float y1, float x, float nx, float tile) {
    glBindTexture(GL_TEXTURE_2D, t); glEnable(GL_TEXTURE_2D); glColor3f(1, 1, 1);
    float u = (z1 - z0) / tile, v = (y1 - y0) / tile;
    glNormal3f(nx, 0, 0);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex3f(x, y0, z0);
    glTexCoord2f(u, 0); glVertex3f(x, y0, z1);
    glTexCoord2f(u, v); glVertex3f(x, y1, z1);
    glTexCoord2f(0, v); glVertex3f(x, y1, z0);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}
static void flatQuadY(float x0, float z0, float x1, float z1, float y, float ny) {
    glNormal3f(0, ny, 0);
    glBegin(GL_QUADS);
    glVertex3f(x0, y, z0); glVertex3f(x1, y, z0); glVertex3f(x1, y, z1); glVertex3f(x0, y, z1);
    glEnd();
}

// ---------------------------------------------------------------
//  Stroke text (3D signs)
// ---------------------------------------------------------------
static float strokeW(const char* s) {
    float w = 0;
    while (*s) { w += glutStrokeWidth(GLUT_STROKE_ROMAN, *s); s++; }
    return w;
}
static void text3D(const char* s, float x, float y, float z, float h,
                   float r, float g, float b, float lw, float angY) {
    emitOn();
    glColor3f(r, g, b); glLineWidth(lw);
    glPushMatrix();
    glTranslatef(x, y, z); glRotatef(angY, 0, 1, 0);
    float sc = h / 119.05f;
    glScalef(sc, sc, sc);
    glTranslatef(-strokeW(s) * 0.5f, 0, 0);
    for (const char* c = s; *c; c++) glutStrokeCharacter(GLUT_STROKE_ROMAN, *c);
    glPopMatrix();
    glLineWidth(1);
    emitOff();
}

// ===============================================================
//  DATA : people, tables
// ===============================================================
enum { P_MAN, P_WOMAN, P_BOY, P_GIRL, P_CHEF, P_WAITER, P_CASHIER };
enum { M_NONE, M_HOME, M_TO_SEAT, M_SEATED, M_LEAVE, M_EVAC, M_GATHER, M_TAKE_IN, M_QUEUE,
       M_TAKE_PAY, M_TAKE_OUT, M_RETURN };
enum { J_NONE, J_ORDER, J_DELIVER, J_BILL };
enum { T_FREE, T_ARRIVING, T_WAIT_ORDER, T_ORDERING, T_COOKING, T_READY, T_DELIVER, T_EATING,
       T_WAIT_BILL, T_BILLING, T_LEAVING, T_EVAC };

struct Person {
    int active, type, mode;
    float x, z, ang, tAng, walkPh;
    int moving, sit, act; float actT;
    int skin, hair, shirt, pants, hairStyle, beard;
    int tbl, seat;
    float wx[8], wz[8]; int nw, cw; float speed;
    int hasBag, hasTray, hasBill; float timer;
    int job, step, jobTbl;
    float hx, hz, hang;
};
#define MAXP 80
static Person P[MAXP];
#define CH0 0
#define WT0 3
#define WT1 4
#define CASH 5
#define FIRST_CUST 6

struct Table {
    float x, z; int state, n; int mem[4], seatOf[4], dish[4], drink[4];
    int waiter, billAtDoor, bill; float timer, eatT, eatTotal; float col[3];
};
static Table Tb[8];
static const float TPOS[8][2] = {{-10, 0.5f}, {-4.5f, 0.5f}, {1, 0.5f}, {-10, 6.5f},
                                 {-4.5f, 6.5f}, {1, 6.5f}, {11, 6.5f}, {11, 0.5f}};

static const float SKIN[5][3]  = {{0.96f,0.80f,0.66f},{0.88f,0.68f,0.50f},{0.78f,0.56f,0.38f},{0.62f,0.42f,0.28f},{0.46f,0.31f,0.21f}};
static const float HAIRC[5][3] = {{0.07f,0.06f,0.05f},{0.18f,0.11f,0.07f},{0.33f,0.20f,0.10f},{0.72f,0.55f,0.28f},{0.62f,0.62f,0.64f}};
static const float SHIRTC[10][3] = {{0.85f,0.20f,0.20f},{0.20f,0.45f,0.85f},{0.25f,0.65f,0.35f},{0.95f,0.75f,0.20f},
                                    {0.60f,0.30f,0.75f},{0.95f,0.45f,0.65f},{0.20f,0.65f,0.70f},{0.95f,0.50f,0.20f},
                                    {0.90f,0.90f,0.92f},{0.35f,0.35f,0.40f}};
static const float PANTC[6][3] = {{0.18f,0.22f,0.38f},{0.22f,0.22f,0.24f},{0.42f,0.32f,0.20f},{0.10f,0.10f,0.12f},
                                  {0.30f,0.40f,0.55f},{0.55f,0.50f,0.42f}};
static const float SNK[8][3] = {{0.85f,0.2f,0.2f},{0.2f,0.45f,0.85f},{0.25f,0.65f,0.3f},{0.95f,0.75f,0.2f},
                                {0.95f,0.5f,0.15f},{0.6f,0.3f,0.7f},{0.9f,0.4f,0.6f},{0.25f,0.65f,0.7f}};
static const int DISH_PRICE[4] = {150, 250, 200, 120};
static const int DRINK_PRICE[3] = {80, 60, 50};


// ---------------------------------------------------------------
//  Seat / table geometry helpers
// ---------------------------------------------------------------
static void seatPos(int t, int k, float& sx, float& sz, float& ang) {
    float th = RAD(k * 90.0f);
    sx = Tb[t].x + sinf(th) * 1.3f; sz = Tb[t].z + cosf(th) * 1.3f; ang = k * 90.0f + 180.0f;
}
static void approachPos(int t, int k, float& ax, float& az) {
    float th = RAD(k * 90.0f);
    ax = clampf(Tb[t].x + sinf(th) * 2.4f, -13.0f, 13.0f); az = Tb[t].z + cosf(th) * 2.4f;
}
static float gatherX(int t) { return -10.0f + t * 2.8f; }
static float trayX(int t) { return -12.0f + t * 1.3f; }

// ---- shared: plant (inside + outside e lage) ----
static void drawPlant(float x, float z, float s) {
    glColor3f(0.62f, 0.30f, 0.18f); cylY(x, 0, z, 0.38f * s, 0.26f * s, 0.62f * s);
    glColor3f(0.2f, 0.12f, 0.08f); discUp(x, 0.62f * s, z, 0.36f * s);
    for (int i = 0; i < 9; i++) {
        glPushMatrix(); glTranslatef(x, 0.62f * s, z); glRotatef(i * 40.0f, 0, 1, 0); glRotatef(-28 - (i % 3) * 9, 1, 0, 0);
        if (i % 2) glColor3f(0.12f, 0.50f, 0.16f); else glColor3f(0.20f, 0.60f, 0.22f);
        ellip(0, 0.8f * s, 0, 0.13f * s, 0.85f * s, 0.04f * s);
        glPopMatrix();
    }
}

// ---- shared: kitchen busy check ----
static int cookingCount() {
    int c = 0;
    for (int i = 0; i < 8; i++) if (Tb[i].state == T_COOKING) c++;
    return c;
}

// ---- shared: smoke / steam particles + sprinkler positions ----
struct Puff { float x, y, z, vx, vy, vz, life, maxLife, size; int kind; };
static Puff pf[200];

static void emitPuff(int kind, float x, float y, float z) {
    for (int i = 0; i < 200; i++) if (pf[i].life <= 0) {
        Puff& p = pf[i]; p.kind = kind; p.x = x; p.y = y; p.z = z;
        if (kind == 0) { p.vx = (frand() - 0.5f) * 0.1f; p.vy = 0.5f + frand() * 0.4f; p.vz = (frand() - 0.5f) * 0.1f; p.maxLife = p.life = 1.6f + frand(); p.size = 0.06f; }
        else { p.vx = (frand() - 0.5f) * 0.5f; p.vy = 1.0f + frand() * 0.8f; p.vz = (frand() - 0.3f) * 0.5f; p.maxLife = p.life = 4.0f + frand() * 2.0f; p.size = 0.2f; }
        return;
    }
}

static const float SPR[6][3] = {{-8, 6.9f, 2}, {0, 6.9f, 2.5f}, {8, 6.9f, 2}, {-8, 6.9f, 7.5f}, {-11, 6.9f, -7}, {-5, 6.9f, -7}};

// ---- shared: lights ----
static void setLight(int id, float x, float y, float z, float w, float r, float g, float b, float a1, float a2) {
    GLfloat pos[4] = {x, y, z, w}, dif[4] = {r, g, b, 1}, amb[4] = {0, 0, 0, 1}, spc[4] = {r * 0.35f, g * 0.35f, b * 0.35f, 1};
    glEnable(id);
    glLightfv(id, GL_POSITION, pos); glLightfv(id, GL_DIFFUSE, dif); glLightfv(id, GL_AMBIENT, amb); glLightfv(id, GL_SPECULAR, spc);
    glLightf(id, GL_CONSTANT_ATTENUATION, 1.0f); glLightf(id, GL_LINEAR_ATTENUATION, a1); glLightf(id, GL_QUADRATIC_ATTENUATION, a2);
}
static void setupLights() {
    float amb = lerpf(0.40f, 0.30f, insideF);
    GLfloat ga[4] = {amb, amb * 0.97f, amb * 0.92f, 1};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ga);
    float sun = lerpf(0.95f, 0.45f, insideF);
    setLight(GL_LIGHT0, 0.4f, 0.9f, 0.6f, 0, sun, sun * 0.97f, sun * 0.9f, 0, 0);
    setLight(GL_LIGHT1, -8, 6.2f, 3, 1, 1.0f, 0.88f, 0.70f, 0.02f, 0.004f);
    setLight(GL_LIGHT2, 0, 6.2f, 3.5f, 1, 1.0f, 0.88f, 0.70f, 0.02f, 0.004f);
    setLight(GL_LIGHT3, 8, 6.2f, 3, 1, 1.0f, 0.88f, 0.70f, 0.02f, 0.004f);
    setLight(GL_LIGHT4, 0, 6.0f, 9, 1, 0.9f, 0.8f, 0.65f, 0.03f, 0.006f);
    setLight(GL_LIGHT5, -7, 6.0f, -7, 1, 0.95f, 0.97f, 1.0f, 0.02f, 0.004f);
    setLight(GL_LIGHT6, 8, 5.5f, -5, 1, 1.0f, 0.88f, 0.70f, 0.03f, 0.006f);
    if (fireLevel > 0.02f) setLight(GL_LIGHT7, -11, 1.8f, -6.8f, 1, 1.6f * fireLevel, 0.7f * fireLevel, 0.2f * fireLevel, 0.1f, 0.05f);
    else glDisable(GL_LIGHT7);
}

#endif
