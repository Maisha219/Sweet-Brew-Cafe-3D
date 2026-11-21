// ===============================================================
//  SWEET BREW CAFE 3D  -  VIEWER (main.cpp)
//  Eta ekta "test viewer": jar jar part (maisha_part.h / afrina_part.h)
//  thakbe sheta-i dekha jabe. Je part nai, sheta khali thakbe (crash korbe na).
//
//  Keys:  W A S D = move    Q / Z = up / down    B = boost (hold)
//         Mouse drag / Arrow keys = look around
//         1..6 = camera view   F = fire ON/OFF   G = sprinkler ON/OFF
//         H = help on/off      ESC = exit
// ===============================================================
#include "common.h"

// ---------- je part file ache sheta include hobe ----------
#if defined(__has_include)
  #if __has_include("maisha_part.h")
    #include "maisha_part.h"
    #define HAVE_MAISHA 1
  #endif
  #if __has_include("afrina_part.h")
    #include "afrina_part.h"
    #define HAVE_AFRINA 1
  #endif
#endif

// Jodi purano compiler hoy ar part dekha jachche na (blank screen),
// tahole nicher line gulo thekei "//" tule dao (jar jar part tar line):
// #define FORCE_MAISHA
// #define FORCE_AFRINA
#if defined(FORCE_MAISHA) && !defined(HAVE_MAISHA)
  #include "maisha_part.h"
  #define HAVE_MAISHA 1
#endif
#if defined(FORCE_AFRINA) && !defined(HAVE_AFRINA)
  #include "afrina_part.h"
  #define HAVE_AFRINA 1
#endif

// ---------- je part nai tar khali (empty) function ----------
#ifndef HAVE_MAISHA
static void drawShell() {}
static void drawInterior() {}
static void drawKitchen() {}
static void drawBakery() {}
static void drawTable(int) {}
static void drawGlass() {}
#endif
#ifndef HAVE_AFRINA
static void drawOutdoor() {}
static void drawBillboard() {}
static void drawPerson(const Person&) {}
static void drawShadows() {}
static void drawEffects() {}
#endif

// ---------------------------------------------------------------
//  Demo scene (simulation chhara, sudhu dekhar jonno)
// ---------------------------------------------------------------
static void demoPerson(Person& p, int type, float x, float z, float ang) {
    memset(&p, 0, sizeof(Person));
    p.active = 1; p.type = type; p.x = x; p.z = z; p.ang = p.tAng = ang;
    p.skin = 1 + rand() % 3; p.hair = rand() % 3;
    p.shirt = rand() % 10; p.pants = rand() % 6;
    p.hairStyle = (type == P_WOMAN || type == P_CASHIER) ? 1 + rand() % 2 : (type == P_GIRL) ? 3 : 0;
    p.beard = (type == P_MAN && rand() % 3 == 0);
    p.tbl = -1; p.speed = 1.25f; p.actT = frand() * 6; p.mode = M_HOME;
}

static void demoSeat(int t, int n, int state, const int* kinds) {
    Table& T = Tb[t];
    T.n = n; T.state = state; T.eatT = 6; T.eatTotal = 30; T.bill = 0;
    for (int k = 0; k < n; k++) {
        int id = FIRST_CUST + t * 4 + k;
        float sx, sz, sa; seatPos(t, k, sx, sz, sa);
        demoPerson(P[id], kinds[k], sx, sz, sa);
        P[id].sit = 1; P[id].tbl = t; P[id].seat = k; P[id].mode = M_SEATED;
        P[id].act = (state == T_EATING) ? 1 : 0;
        T.mem[k] = id; T.seatOf[k] = k; T.dish[k] = (t + k) % 4; T.drink[k] = (t + k) % 3;
    }
}

static void initDemo() {
    memset(P, 0, sizeof(P)); memset(Tb, 0, sizeof(Tb)); memset(pf, 0, sizeof(pf));
    float tc[8][3] = {{0.95f,0.92f,0.85f},{0.55f,0.30f,0.20f},{0.95f,0.92f,0.85f},{0.20f,0.40f,0.35f},
                      {0.95f,0.92f,0.85f},{0.55f,0.30f,0.20f},{0.20f,0.40f,0.35f},{0.95f,0.92f,0.85f}};
    for (int i = 0; i < 8; i++) {
        Tb[i].x = TPOS[i][0]; Tb[i].z = TPOS[i][1]; Tb[i].state = T_FREE; Tb[i].waiter = -1;
        copy3(Tb[i].col, tc[i]);
    }
    // staff
    float cx[3] = {-11.0f, -7.0f, -3.2f};
    for (int i = 0; i < 3; i++) { demoPerson(P[i], P_CHEF, cx[i], -7.95f, 0); P[i].skin = 1 + i % 2; P[i].hair = 0; }
    P[0].act = 5; P[1].act = 4; P[2].act = 8;                       // stir, chop, pizza
    demoPerson(P[WT0], P_WAITER, -2.0f, -3.2f, 0); P[WT0].hasTray = 1; P[WT0].act = 3;
    demoPerson(P[WT1], P_WAITER, 0.8f, -3.2f, 0);
    demoPerson(P[CASH], P_CASHIER, 8.6f, -5.9f, 0); P[CASH].hairStyle = 2; P[CASH].act = 10;
    // customers
    int fam[4] = {P_MAN, P_WOMAN, P_BOY, P_GIRL}, fr[3] = {P_MAN, P_WOMAN, P_MAN}, cp[2] = {P_MAN, P_WOMAN};
    demoSeat(0, 4, T_EATING, fam);
    demoSeat(2, 3, T_EATING, fr);
    demoSeat(4, 2, T_WAIT_ORDER, cp);
    demoSeat(5, 3, T_WAIT_BILL, fr);
    demoSeat(1, 2, T_READY, cp);                                    // tray ready on pass counter
    Tb[3].state = T_COOKING; Tb[3].n = 2;                           // kitchen busy -> flame + steam
    for (int k = 0; k < 2; k++) { Tb[3].dish[k] = k; Tb[3].drink[k] = k; }
}

// ---------------------------------------------------------------
//  Camera (free fly) + views
// ---------------------------------------------------------------
static void setView(float ex, float ey, float ez, float cx, float cy, float cz) {
    float f[3] = {cx - ex, cy - ey, cz - ez};
    float l = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]); if (l < 0.001f) l = 1;
    mYaw = atan2f(f[0] / l, -f[2] / l); mPitch = asinf(clampf(f[1] / l, -1, 1));
    mPos[0] = ex; mPos[1] = ey; mPos[2] = ez;
}
static void presetView(int n) {
    switch (n) {
    case 0: setView(-18, 5.5f, 36, 0, 5.5f, 10); break;          // 1 : bahir theke building
    case 1: setView(0, 2.4f, 8.8f, 0, 3.5f, -4); break;          // 2 : dhukei interior
    case 2: setView(-3, 4.8f, 8.5f, -2, 2.5f, -6); break;        // 3 : upor theke full cafe
    case 3: setView(-6, 2.3f, -1.5f, -9, 1.8f, -8); break;       // 4 : kitchen
    case 4: setView(6, 1.9f, -0.5f, 8, 1.8f, -8); break;         // 5 : bakery + cashier
    case 5: setView(-2, 2.0f, 2.5f, -6, 1.0f, 6); break;         // 6 : table close-up
    }
}
static void updateCamera(float dt) {
    float sp = (keys['b'] || keys['B']) ? 14.0f : 5.0f;
    float f[3] = {sinf(mYaw) * cosf(mPitch), sinf(mPitch), -cosf(mYaw) * cosf(mPitch)};
    float r[3] = {cosf(mYaw), 0, sinf(mYaw)};
    if (keys['w'] || keys['W']) for (int k = 0; k < 3; k++) mPos[k] += f[k] * sp * dt;
    if (keys['s'] || keys['S']) for (int k = 0; k < 3; k++) mPos[k] -= f[k] * sp * dt;
    if (keys['d'] || keys['D']) for (int k = 0; k < 3; k++) mPos[k] += r[k] * sp * dt;
    if (keys['a'] || keys['A']) for (int k = 0; k < 3; k++) mPos[k] -= r[k] * sp * dt;
    if (keys['q'] || keys['Q']) mPos[1] += sp * dt;
    if (keys['z'] || keys['Z']) mPos[1] -= sp * dt;
    if (skeys[GLUT_KEY_LEFT])  mYaw -= 1.6f * dt;
    if (skeys[GLUT_KEY_RIGHT]) mYaw += 1.6f * dt;
    if (skeys[GLUT_KEY_UP])    mPitch += 1.2f * dt;
    if (skeys[GLUT_KEY_DOWN])  mPitch -= 1.2f * dt;
    mPitch = clampf(mPitch, -1.4f, 1.4f);
    if (mPos[1] < 0.4f) mPos[1] = 0.4f;
    camE[0] = mPos[0]; camE[1] = mPos[1]; camE[2] = mPos[2];
    camC[0] = mPos[0] + sinf(mYaw) * cosf(mPitch);
    camC[1] = mPos[1] + sinf(mPitch);
    camC[2] = mPos[2] - cosf(mYaw) * cosf(mPitch);
    int inside = (camE[2] < 9.8f && fabsf(camE[0]) < 14 && camE[1] < 7);
    insideF += ((inside ? 1.0f : 0.0f) - insideF) * clampf(dt * 3.0f, 0, 1);
}

// ---------------------------------------------------------------
//  Update (sudhu animation: steam, fire, sprinkler)
// ---------------------------------------------------------------
static float fireTarget = 0;
static void update(float dt) {
    simTime += dt;
    for (int i = 0; i < MAXP; i++) if (P[i].active) P[i].actT += dt;
    // fire / sprinkler demo
    fireLevel += (fireTarget - fireLevel) * clampf(dt * 2.0f, 0, 1);
    if (fireTarget == 0 && fireLevel < 0.03f) fireLevel = 0;
    fireState = (fireTarget > 0) ? 1 : 0;
    alarmOn = (fireTarget > 0);
    if (sprinklerOn) wetAmt = clampf(wetAmt + dt * 0.12f, 0, 1); else wetAmt = clampf(wetAmt - dt * 0.05f, 0, 1);
    // kitchen steam + smoke
    int busy = (cookingCount() > 0 && fireState == 0);
    if (busy && frand() < dt * 8.0f) {
        emitPuff(0, -11.9f + frand() * 0.2f, 1.3f, -7.2f); emitPuff(0, -11.0f, 1.6f, -7.0f);
        if (frand() < 0.5f) emitPuff(0, -3.0f + frand() * 0.4f, 1.3f, -7.0f);
    }
    if (fireLevel > 0.05f && frand() < dt * 30.0f * fireLevel) emitPuff(1, -11.0f + frand() * 0.8f, 1.8f, -6.9f);
    for (int i = 0; i < 200; i++) if (pf[i].life > 0) {
        Puff& p = pf[i];
        p.x += p.vx * dt; p.y += p.vy * dt; p.z += p.vz * dt; p.life -= dt;
        if (p.y > 6.6f) { p.y = 6.6f; p.vy = 0; p.vx *= 1.01f; }
    }
    // sliding door open/close demo
    doorOpen = 0.5f + 0.5f * sinf(simTime * 0.5f);
    if (payFlash > 0) payFlash -= dt;
    updateCamera(dt);
}

// ---------------------------------------------------------------
//  HUD (help text)
// ---------------------------------------------------------------
static void drawText(float x, float y, const char* s) {
    glRasterPos2f(x, y);
    for (const char* c = s; *c; c++) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
}
static void drawHUD() {
    glDisable(GL_LIGHTING); glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); gluOrtho2D(0, winW, 0, winH);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    glColor4f(0, 0, 0, 0.45f);
    glBegin(GL_QUADS); glVertex2f(0, winH); glVertex2f(winW, winH); glVertex2f(winW, winH - 30); glVertex2f(0, winH - 30); glEnd();
    glColor3f(1, 0.9f, 0.5f);
    char buf[200];
#ifdef HAVE_MAISHA
    const char* m = "MAISHA part: ON";
#else
    const char* m = "MAISHA part: (nai)";
#endif
#ifdef HAVE_AFRINA
    const char* a = "AFRINA part: ON";
#else
    const char* a = "AFRINA part: (nai)";
#endif
    sprintf(buf, "SWEET BREW CAFE 3D - VIEWER   |   %s   |   %s", m, a);
    drawText(12, winH - 20, buf);
    if (showHelp) {
        glColor3f(1, 1, 1);
        drawText(12, 14, "W A S D / Q Z: move | Mouse drag or Arrows: look | B: boost | 1-6: views | F: fire | G: sprinkler | H: help | ESC: exit");
    }
    glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW); glPopMatrix();
    glEnable(GL_DEPTH_TEST); glEnable(GL_LIGHTING);
}

// ---------------------------------------------------------------
//  GLUT callbacks
// ---------------------------------------------------------------
static void display() {
    glClearColor(0.55f, 0.76f, 0.95f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluPerspective(58.0, (double)winW / (double)winH, 0.1, 400.0);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    gluLookAt(camE[0], camE[1], camE[2], camC[0], camC[1], camC[2], 0, 1, 0);
    setupLights();

    drawOutdoor();
    drawShell();
    drawBillboard();
    drawInterior();
    drawKitchen();
    drawBakery();
    for (int i = 0; i < 8; i++) drawTable(i);
    for (int i = 0; i < MAXP; i++) if (P[i].active) drawPerson(P[i]);

    drawShadows();
    drawGlass();
    drawEffects();
    drawHUD();
    glutSwapBuffers();
}

static void reshape(int w, int h) { winW = w; winH = h > 0 ? h : 1; glViewport(0, 0, winW, winH); }

static void timerFunc(int) {
    static int last = 0;
    int now = glutGet(GLUT_ELAPSED_TIME);
    float dt = (now - last) / 1000.0f; last = now;
    if (dt > 0.05f) dt = 0.05f;
    update(dt);
    glutPostRedisplay();
    glutTimerFunc(16, timerFunc, 0);
}

static void keyDown(unsigned char k, int, int) {
    keys[k] = 1;
    if (k == 27) exit(0);
    if (k == 'f' || k == 'F') fireTarget = (fireTarget > 0) ? 0.0f : 1.0f;
    if (k == 'g' || k == 'G') sprinklerOn = !sprinklerOn;
    if (k == 'h' || k == 'H') showHelp = !showHelp;
    if (k >= '1' && k <= '6') presetView(k - '1');
}
static void keyUp(unsigned char k, int, int) { keys[k] = 0; }
static void specialDown(int k, int, int) { if (k >= 0 && k < 256) skeys[k] = 1; }
static void specialUp(int k, int, int) { if (k >= 0 && k < 256) skeys[k] = 0; }
static void mouseBtn(int b, int s, int x, int y) {
    if (b == GLUT_LEFT_BUTTON) { mouseDown = (s == GLUT_DOWN); lastMx = x; lastMy = y; }
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
    GLfloat spec[4] = {0.25f, 0.25f, 0.25f, 1};
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec); glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 30);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    Q = gluNewQuadric(); gluQuadricNormals(Q, GLU_SMOOTH);
    makeTexture(texWood, 0); makeTexture(texTile, 1); makeTexture(texBrick, 2); makeTexture(texPave, 3);
}

int main(int argc, char** argv) {
    srand(7);
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(winW, winH);
    glutCreateWindow("Sweet Brew Cafe 3D - Viewer");
    initGL(); initDemo();
    presetView(0);
    glutDisplayFunc(display); glutReshapeFunc(reshape);
    glutKeyboardFunc(keyDown); glutKeyboardUpFunc(keyUp);
    glutSpecialFunc(specialDown); glutSpecialUpFunc(specialUp);
    glutMouseFunc(mouseBtn); glutMotionFunc(mouseMove);
    glutIgnoreKeyRepeat(1);
    glutTimerFunc(16, timerFunc, 0);
    glutMainLoop();
    return 0;
}
