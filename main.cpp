
#include <windows.h>
#include <GL/glut.h>
#include <cmath>
#include <cstdio>

#define PI 3.14159265f
#define WH 5.5f

GLUquadric* Q;

float camYaw = 12.0f;
float camPitch = 30.0f;
float camDist = 27.0f;
float camTX = 0, camTY = 1.4f, camTZ = 0;

int lastX = 0, lastY = 0;
bool dragging = false;
int winW = 1200, winH = 720;

// Original cafe table positions
struct Table
{
    float x, z;
    int cloth;
};

Table tab[10] =
{
    {-3.5f, -3.0f, 1},
    { 3.5f, -3.0f, 2},
    {-8.0f,  1.5f, 0},
    {-4.0f,  1.5f, 2},
    { 4.0f,  1.5f, 1},
    { 8.0f,  1.5f, 0},
    {-7.0f,  6.0f, 1},
    {-3.0f,  6.0f, 0},
    { 3.0f,  6.0f, 2},
    { 7.0f,  6.0f, 1}
};

// ------------------------------------------------
// Basic 3D objects
// ------------------------------------------------

void cube(float x, float y, float z,
          float sx, float sy, float sz)
{
    glPushMatrix();
    glTranslatef(x, y, z);
    glScalef(sx, sy, sz);
    glutSolidCube(1);
    glPopMatrix();
}

void sphere(float x, float y, float z, float r)
{
    glPushMatrix();
    glTranslatef(x, y, z);
    gluSphere(Q, r, 20, 16);
    glPopMatrix();
}

void cylinder(float x, float y, float z,
              float r, float h)
{
    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(-90, 1, 0, 0);

    gluCylinder(Q, r, r, h, 20, 4);

    glPopMatrix();
}

void line3D(float ax, float ay, float az,
            float bx, float by, float bz,
            float radius)
{
    float dx = bx-ax;
    float dy = by-ay;
    float dz = bz-az;

    float len = sqrt(dx*dx + dy*dy + dz*dz);

    if(len < 0.001f) return;

    glPushMatrix();

    glTranslatef(ax, ay, az);

    float angle = acos(dz/len)*180.0f/PI;
    glRotatef(angle, -dy, dx, 0);

    gluCylinder(Q, radius, radius, len, 10, 2);

    glPopMatrix();
}

void setMaterial(float r, float g, float b)
{
    glColor3f(r, g, b);
}

// ------------------------------------------------
// Floor
// ------------------------------------------------

void drawFloor()
{
    // Wooden floor
    setMaterial(0.48f, 0.27f, 0.13f);
    cube(0, -0.08f, 0, 24, 0.16f, 18);

    // Wooden plank lines
    setMaterial(0.25f, 0.13f, 0.06f);

    for(int i=-11; i<=11; i++)
        cube(i, 0.006f, 0, 0.025f, 0.01f, 18);

    for(int j=-8; j<=8; j+=2)
        cube(0, 0.008f, j, 24, 0.01f, 0.025f);

    // Kitchen tiled floor
    setMaterial(0.75f, 0.78f, 0.80f);
    cube(-9, 0.015f, -5.5f, 6, 0.025f, 7);

    setMaterial(0.45f, 0.48f, 0.50f);

    for(int i=-12; i<=-6; i++)
        cube(i, 0.032f, -5.5f, 0.025f, 0.01f, 7);

    for(int j=-9; j<=-2; j++)
        cube(-9, 0.033f, j, 6, 0.01f, 0.025f);

    // Entrance mat
    setMaterial(0.45f, 0.08f, 0.08f);
    cube(0, 0.025f, -8.1f, 2.5f, 0.025f, 1.2f);

    // Bakery carpet
    setMaterial(0.45f, 0.10f, 0.08f);
    cube(8.6f, 0.025f, -3.2f, 4.6f, 0.025f, 1.6f);
}

// ------------------------------------------------
// Walls and windows
// ------------------------------------------------

void drawWalls()
{
    // Back wall
    setMaterial(0.65f, 0.28f, 0.18f);
    cube(0, WH/2, -9.15f, 24, WH, 0.3f);

    // Left brick wall
    setMaterial(0.60f, 0.25f, 0.16f);
    cube(-12.1f, WH/2, 0, 0.3f, WH, 18);

    // Right cream wall
    setMaterial(0.92f, 0.85f, 0.72f);
    cube(12.1f, WH/2, 0, 0.3f, WH, 18);

    // Wall wooden border
    setMaterial(0.30f, 0.16f, 0.08f);
    cube(11.9f, 0.55f, 0, 0.08f, 1.1f, 18);

    // Window frames
    for(int s=-1; s<=1; s+=2)
    {
        float x = s*4.0f;

        setMaterial(0.25f, 0.13f, 0.06f);

        cube(x, 1.2f, -9.0f, 3.2f, 0.10f, 0.25f);
        cube(x, 3.6f, -9.0f, 3.2f, 0.10f, 0.25f);

        cube(x-1.55f, 2.4f, -9.0f,
             0.10f, 2.5f, 0.25f);

        cube(x+1.55f, 2.4f, -9.0f,
             0.10f, 2.5f, 0.25f);

        cube(x, 2.4f, -9.0f,
             0.06f, 2.4f, 0.20f);

        // Glass panels
        setMaterial(0.30f, 0.62f, 0.72f);
        cube(x, 2.4f, -9.25f, 2.9f, 2.2f, 0.025f);
    }

    // Wall menu board
    setMaterial(0.22f, 0.12f, 0.07f);
    cube(11.72f, 2.7f, 2.0f, 0.12f, 2.0f, 3.2f);

    setMaterial(0.08f, 0.12f, 0.10f);
    cube(11.64f, 2.7f, 2.0f, 0.06f, 1.8f, 3.0f);

    // Ceiling wooden beams
    setMaterial(0.25f, 0.14f, 0.07f);

    for(int z=-8; z<=8; z+=4)
        cube(0, WH-0.15f, z, 24, 0.28f, 0.3f);

    for(int x=-10; x<=10; x+=5)
        cube(x, WH-0.1f, 0, 0.25f, 0.2f, 18);

    // Cafe name sign
    setMaterial(0.20f, 0.10f, 0.04f);
    cube(0, 4.5f, -8.85f, 7.6f, 0.95f, 0.18f);
}

// ------------------------------------------------
// 3D Text
// ------------------------------------------------

void drawText(float x, float y, float z,
              const char* str)
{
    glDisable(GL_LIGHTING);

    glColor3f(1.0f, 0.82f, 0.35f);

    glRasterPos3f(x, y, z);

    for(int i=0; str[i]!='\0'; i++)
        glutBitmapCharacter(
            GLUT_BITMAP_HELVETICA_18, str[i]
        );

    glEnable(GL_LIGHTING);
}

// ------------------------------------------------
// Cafe tables and chairs
// ------------------------------------------------

void drawChair(float x, float z,
               float yaw, int color)
{
    glPushMatrix();

    glTranslatef(x, 0, z);
    glRotatef(yaw, 0, 1, 0);

    setMaterial(0.30f, 0.17f, 0.08f);

    // Chair legs
    for(int i=0; i<4; i++)
    {
        float px = (i%2==0) ? -0.20f : 0.20f;
        float pz = (i<2) ? -0.20f : 0.20f;

        cylinder(px, 0, pz, 0.025f, 0.47f);
    }

    // Seat
    setMaterial(0.35f, 0.20f, 0.10f);
    cube(0, 0.48f, 0, 0.52f, 0.08f, 0.52f);

    // Cushions
    float colors[5][3] =
    {
        {0.75f,0.20f,0.20f},
        {0.20f,0.45f,0.60f},
        {0.25f,0.55f,0.30f},
        {0.85f,0.60f,0.20f},
        {0.55f,0.30f,0.60f}
    };

    setMaterial(colors[color%5][0],
                colors[color%5][1],
                colors[color%5][2]);

    cube(0, 0.54f, 0, 0.46f, 0.06f, 0.46f);

    // Back support
    setMaterial(0.30f, 0.17f, 0.08f);

    cube(0, 0.83f, -0.23f, 0.48f, 0.10f, 0.05f);
    cube(-0.21f, 0.68f, -0.23f, 0.04f, 0.35f, 0.05f);
    cube(0.21f, 0.68f, -0.23f, 0.04f, 0.35f, 0.05f);

    glPopMatrix();
}

void drawTable(int i)
{
    Table t = tab[i];

    glPushMatrix();
    glTranslatef(t.x, 0, t.z);

    // Table base
    setMaterial(0.20f, 0.14f, 0.09f);
    cylinder(0, 0, 0, 0.40f, 0.05f);
    cylinder(0, 0.05f, 0, 0.07f, 0.75f);

    // Table top
    if(t.cloth==0)
        setMaterial(0.48f, 0.27f, 0.12f);
    else if(t.cloth==1)
        setMaterial(0.90f, 0.87f, 0.78f);
    else
        setMaterial(0.75f, 0.12f, 0.12f);


cylinder(0, 0.82f, 0, 0.95f, 0.08f);


    // Table decoration: flower vase
    setMaterial(0.80f, 0.85f, 0.90f);
    cylinder(-0.55f, 0.90f, -0.10f, 0.05f, 0.14f);

    setMaterial(0.15f, 0.50f, 0.20f);
    line3D(-0.55f, 1.04f, -0.10f,
           -0.55f, 1.20f, -0.10f, 0.008f);

    setMaterial(0.95f, 0.25f, 0.35f);
    sphere(-0.55f, 1.22f, -0.10f, 0.04f);

    // Sugar pot
    setMaterial(0.85f, 0.85f, 0.85f);
    cylinder(0.45f, 0.90f, 0.30f, 0.035f, 0.08f);

    // Menu stand
    setMaterial(0.12f, 0.12f, 0.12f);
    cube(0.4f, 0.96f, -0.35f, 0.10f, 0.12f, 0.03f);

    glPopMatrix();

    // Four chairs around each table
    drawChair(t.x, t.z-1.4f, 0, i%5);
    drawChair(t.x, t.z+1.4f, 180, (i+1)%5);
    drawChair(t.x-1.4f, t.z, 90, (i+2)%5);
    drawChair(t.x+1.4f, t.z, -90, (i+3)%5);
}

void drawDiningArea()
{
    for(int i=0; i<10; i++)
        drawTable(i);
}

// ------------------------------------------------
// Kitchen area
// ------------------------------------------------

void drawKitchen()
{
    // Kitchen cabinets
    setMaterial(0.40f, 0.43f, 0.47f);
    cube(-9.0f, 0.47f, -8.5f, 5.8f, 0.94f, 0.9f);

    setMaterial(0.78f, 0.80f, 0.83f);
    cube(-9.0f, 0.96f, -8.5f, 5.9f, 0.05f, 0.95f);

    // Oven
    setMaterial(0.08f, 0.08f, 0.09f);
    cube(-10.4f, 0.48f, -8.03f, 1.2f, 0.55f, 0.08f);

    setMaterial(0.30f, 0.35f, 0.38f);
    cube(-10.4f, 0.48f, -7.98f, 0.95f, 0.38f, 0.04f);

    // Stove burners
    setMaterial(0.08f, 0.08f, 0.08f);

    for(int i=0; i<4; i++)
        cylinder(-10.3f+(i%2)*1.4f, 0.98f,
                 -8.65f+(i/2)*0.35f, 0.17f, 0.03f);

    // Cooking pot
    setMaterial(0.68f, 0.70f, 0.74f);
    cylinder(-10.3f, 1.0f, -8.65f, 0.22f, 0.30f);

    // Kitchen chimney hood
    setMaterial(0.60f, 0.63f, 0.66f);
    cube(-9.6f, 3.2f, -8.4f, 3.6f, 0.45f, 1.2f);
    cube(-9.6f, 4.5f, -8.7f, 0.9f, 2.2f, 0.8f);

    // Sink
    setMaterial(0.45f, 0.48f, 0.52f);
    cube(-7.2f, 0.98f, -8.45f, 1.0f, 0.04f, 0.55f);

    setMaterial(0.85f, 0.87f, 0.90f);
    line3D(-7.2f, 1.0f, -8.75f,
           -7.2f, 1.35f, -8.75f, 0.025f);

    // Refrigerator
    setMaterial(0.82f, 0.84f, 0.87f);
    cube(-11.4f, 0.95f, -6.2f, 1.0f, 1.9f, 1.0f);

    setMaterial(0.25f, 0.25f, 0.27f);
    cube(-10.88f, 1.2f, -6.55f, 0.04f, 0.6f, 0.04f);

    // Preparation island
    setMaterial(0.45f, 0.29f, 0.17f);
    cube(-9.5f, 0.47f, -4.2f, 3.2f, 0.94f, 1.0f);

    setMaterial(0.85f, 0.86f, 0.88f);
    cube(-9.5f, 0.96f, -4.2f, 3.3f, 0.05f, 1.1f);

    // Vegetables on island
    setMaterial(0.85f, 0.15f, 0.10f);
    sphere(-8.6f, 1.08f, -4.3f, 0.10f);
    sphere(-8.45f, 1.08f, -4.15f, 0.09f);

    setMaterial(0.25f, 0.65f, 0.20f);
    sphere(-8.8f, 1.08f, -4.0f, 0.10f);

    drawText(-10.0f, 3.0f, -8.7f, "LIVE KITCHEN");
}

// ------------------------------------------------
// Bakery and cashier
// ------------------------------------------------

void drawBakery()
{
    // Main wooden counter
    setMaterial(0.42f, 0.25f, 0.13f);
    cube(8.7f, 0.55f, -4.5f, 5.4f, 1.1f, 1.0f);

    setMaterial(0.92f, 0.91f, 0.88f);
    cube(8.7f, 1.12f, -4.5f, 5.6f, 0.06f, 1.1f);

    // Display shelves
    setMaterial(0.30f, 0.32f, 0.35f);
    cube(7.9f, 1.17f, -4.5f, 3.4f, 0.04f, 0.9f);
    cube(7.9f, 1.55f, -4.5f, 3.3f, 0.03f, 0.8f);
    cube(7.9f, 1.86f, -4.5f, 3.4f, 0.03f, 0.9f);

    // Glass display panels
    setMaterial(0.35f, 0.65f, 0.75f);
    cube(7.9f, 1.55f, -4.95f, 3.4f, 0.8f, 0.025f);
    cube(6.2f, 1.55f, -4.5f, 0.025f, 0.8f, 0.9f);
    cube(9.6f, 1.55f, -4.5f, 0.025f, 0.8f, 0.9f);

    // Donuts
    for(int i=0; i<5; i++)
    {
        glPushMatrix();

        glTranslatef(6.7f+i*0.28f, 1.28f, -4.3f);
        glRotatef(90, 1, 0, 0);

        setMaterial(0.85f, 0.48f, 0.20f);
        glutSolidTorus(0.03, 0.065, 12, 20);

        glPopMatrix();
    }

    // Cakes
    setMaterial(0.90f, 0.65f, 0.45f);
    cylinder(9.3f, 1.20f, -4.5f, 0.22f, 0.15f);

    setMaterial(0.90f, 0.20f, 0.25f);
    sphere(9.3f, 1.40f, -4.5f, 0.04f);

    // Bread
    for(int i=0; i<4; i++)
    {
        setMaterial(0.80f, 0.50f, 0.20f);

        glPushMatrix();
        glTranslatef(8.1f+i*0.35f, 1.65f, -4.5f);
        glScalef(1.5f, 0.6f, 0.7f);
        glutSolidSphere(0.13f, 16, 12);
        glPopMatrix();
    }

    // Cashier register
    setMaterial(0.12f, 0.12f, 0.14f);
    cube(10.6f, 1.25f, -4.5f, 0.6f, 0.25f, 0.5f);

    setMaterial(0.15f, 0.35f, 0.20f);
    cube(10.6f, 1.42f, -4.62f, 0.4f, 0.12f, 0.02f);

    // Back coffee counter
    setMaterial(0.42f, 0.26f, 0.14f);
    cube(8.9f, 0.5f, -8.3f, 5.4f, 1.0f, 0.8f);

    setMaterial(0.88f, 0.89f, 0.87f);
    cube(8.9f, 1.02f, -8.3f, 5.5f, 0.05f, 0.9f);

    // Coffee machine
    setMaterial(0.68f, 0.12f, 0.10f);
    cube(10.6f, 1.3f, -8.3f, 0.9f, 0.55f, 0.6f);

    setMaterial(0.75f, 0.77f, 0.80f);
    cube(10.6f, 1.62f, -8.3f, 1.0f, 0.1f, 0.65f);

    // Bakery shelves
    for(int r=0; r<4; r++)
    {
        setMaterial(0.40f, 0.25f, 0.13f);
        cube(8.9f, 1.7f+r*0.62f, -8.7f,
             5.4f, 0.05f, 0.5f);
    }

    drawText(7.1f, 3.15f, -4.42f, "FRESH BAKERY");
}

// ------------------------------------------------
// Entrance door
// ------------------------------------------------

void drawDoor()
{
    setMaterial(0.30f, 0.17f, 0.08f);

    // Door frame
    cube(-1.35f, 1.75f, -9.0f, 0.12f, 3.5f, 0.3f);
    cube( 1.35f, 1.75f, -9.0f, 0.12f, 3.5f, 0.3f);
    cube(0, 3.5f, -9.0f, 2.8f, 0.12f, 0.3f);

    // Glass doors
    setMaterial(0.30f, 0.62f, 0.72f);
    cube(-0.67f, 1.7f, -9.12f, 1.2f, 3.2f, 0.04f);
    cube( 0.67f, 1.7f, -9.12f, 1.2f, 3.2f, 0.04f);

    // Handles
    setMaterial(0.85f, 0.68f, 0.25f);
    sphere(-0.15f, 1.5f, -9.20f, 0.06f);
    sphere( 0.15f, 1.5f, -9.20f, 0.06f);

    drawText(-0.55f, 3.8f, -9.0f, "WELCOME");
}

// ------------------------------------------------
// Pendant lights
// ------------------------------------------------

void drawPendant(float x, float z)
{
    setMaterial(0.12f, 0.12f, 0.12f);

    line3D(x, WH-0.15f, z,
           x, 3.4f, z, 0.012f);

    setMaterial(0.95f, 0.65f, 0.25f);

    glPushMatrix();
    glTranslatef(x, 3.15f, z);
    glRotatef(-90, 1, 0, 0);
    glutSolidCone(0.40f, 0.30f, 20, 8);
    glPopMatrix();

    setMaterial(1.0f, 0.92f, 0.65f);
    sphere(x, 3.12f, z, 0.09f);
}

void drawLights()
{
    for(int i=0; i<10; i++)
        drawPendant(tab[i].x, tab[i].z);

    drawPendant(0, -3);
    drawPendant(0, 4);
}

// ------------------------------------------------
// Decorative plants
// ------------------------------------------------

void drawPlant(float x, float z, float scale)
{
    glPushMatrix();

    glTranslatef(x, 0, z);
    glScalef(scale, scale, scale);

    // Pot
    setMaterial(0.70f, 0.32f, 0.16f);
    cylinder(0, 0, 0, 0.25f, 0.50f);

    // Soil
    setMaterial(0.20f, 0.12f, 0.07f);
    cylinder(0, 0.5f, 0, 0.24f, 0.03f);

    // Green leaves
    for(int i=0; i<9; i++)
    {
        float a = i*40.0f*PI/180.0f;

        setMaterial(0.15f, 0.50f, 0.18f);

        line3D(0, 0.5f, 0,
               sin(a)*0.38f, 1.4f, cos(a)*0.38f,
               0.015f);

        glPushMatrix();
        glTranslatef(sin(a)*0.4f, 1.4f, cos(a)*0.4f);
        glScalef(0.16f, 0.05f, 0.24f);
        glutSolidSphere(1, 16, 12);
        glPopMatrix();
    }

    glPopMatrix();
}

void drawDecorations()
{
    drawPlant(-11.2f, 8.2f, 1.1f);
    drawPlant(11.2f, 8.2f, 1.1f);
    drawPlant(-5.6f, -8.4f, 0.9f);
    drawPlant(-1.8f, -8.35f, 0.8f);
    drawPlant(1.9f, -8.35f, 0.8f);
    drawPlant(11.2f, -1.0f, 0.9f);
    drawPlant(-11.2f, 3.4f, 0.8f);
}

// ------------------------------------------------
// Lighting
// ------------------------------------------------

void setupLights()
{
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    GLfloat ambient[] = {0.45f, 0.40f, 0.34f, 1};
    GLfloat diffuse[] = {0.95f, 0.82f, 0.65f, 1};
    GLfloat position[] = {0, 8, 2, 1};

    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_POSITION, position);

    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
}

// ------------------------------------------------
// Main display
// ------------------------------------------------

void display()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    gluPerspective(
        48,
        (double)winW/(double)(winH ? winH : 1),
        0.3,
        300
    );

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    float cy = camYaw*PI/180.0f;
    float cp = camPitch*PI/180.0f;

    float ex = camTX + camDist*sin(cy)*cos(cp);
    float ey = camTY + camDist*sin(cp);
    float ez = camTZ + camDist*cos(cy)*cos(cp);

    gluLookAt(
        ex, ey, ez,
        camTX, camTY, camTZ,
        0, 1, 0
    );

    setupLights();

    // Interior components
    drawFloor();
    drawWalls();
    drawKitchen();
    drawBakery();
    drawDoor();
    drawDiningArea();
    drawLights();
    drawDecorations();

    glutSwapBuffers();
}

// ------------------------------------------------
// Camera controls
// ------------------------------------------------

void reshape(int w, int h)
{
    winW = w;
    winH = h;

    if(h==0) h=1;

    glViewport(0, 0, w, h);
}

void keyboard(unsigned char key, int x, int y)
{
    if(key=='+') camDist -= 1.5f;
    if(key=='-') camDist += 1.5f;

    if(key=='1')
    {
        camYaw=12;
        camPitch=30;
        camDist=27;
        camTX=0;
        camTY=1.4f;
        camTZ=0;
    }

    if(key=='3')
    {
        camYaw=-22;
        camPitch=24;
        camDist=14;
        camTX=-8.5f;
        camTY=1.4f;
        camTZ=-5.5f;
    }

    if(key=='4')
    {
        camYaw=25;
        camPitch=22;
        camDist=14;
        camTX=8.5f;
        camTY=1.4f;
        camTZ=-5.5f;
    }

    if(key==27)
        exit(0);

    glutPostRedisplay();
}

void specialKeys(int key, int x, int y)
{
    if(key==GLUT_KEY_LEFT) camYaw-=4;
    if(key==GLUT_KEY_RIGHT) camYaw+=4;
    if(key==GLUT_KEY_UP) camPitch+=3;
    if(key==GLUT_KEY_DOWN) camPitch-=3;

    if(camPitch<5) camPitch=5;
    if(camPitch>85) camPitch=85;

    glutPostRedisplay();
}

void mouse(int button, int state, int x, int y)
{
    if(button==GLUT_LEFT_BUTTON)
    {
        dragging=(state==GLUT_DOWN);
        lastX=x;
        lastY=y;
    }

    if(state==GLUT_DOWN && button==3)
        camDist-=1.5f;

    if(state==GLUT_DOWN && button==4)
        camDist+=1.5f;

    glutPostRedisplay();
}

void motion(int x, int y)
{
    if(!dragging) return;

    camYaw-=(x-lastX)*0.4f;
    camPitch+=(y-lastY)*0.3f;

    if(camPitch<5) camPitch=5;
    if(camPitch>85) camPitch=85;

    lastX=x;
    lastY=y;

    glutPostRedisplay();
}

// ------------------------------------------------
// Main
// ------------------------------------------------

int main(int argc, char** argv)
{
    glutInit(&argc, argv);

    glutInitDisplayMode(
        GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH
    );

    glutInitWindowSize(1200, 720);
    glutInitWindowPosition(60, 40);

    glutCreateWindow("Sweet Brew Cafe - Interior Design");

    Q = gluNewQuadric();
    gluQuadricNormals(Q, GLU_SMOOTH);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);

    glClearColor(0.12f, 0.10f, 0.09f, 1);

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKeys);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);

    glutMainLoop();

    return 0;
}
