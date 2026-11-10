/*
 * ======================================================================================
 *  ImGui Lab  -  "mini engine" host used by the ImGUI learning platform
 * ======================================================================================
 *  This is the same Win32 + OpenGL + Dear ImGui skeleton as the starter OGL.c, grown
 *  into a tiny game-engine runtime: a scene of entities, an orbit camera, a light,
 *  mouse picking, a log, and procedural textures.
 *
 *  YOUR lesson code lives in "Lesson.inl" (written by the website) and is #included
 *  at the very bottom of this file. It must define:
 *
 *      void Lesson_Init(void);   // called once, after ImGui is initialised
 *      void Lesson_UI(void);     // called every frame between NewFrame() and Render()
 *
 *  Compiled as C++ (/TP) because Dear ImGui is a C++ library.
 *
 *  Controls (when the mouse/keyboard is NOT captured by ImGui):
 *      RMB drag = orbit   MMB drag = pan   Wheel = zoom   LMB = select entity
 *      W/A/S/D/Q/E = move camera target    F11 = fullscreen   ESC = quit
 * ======================================================================================
 */
#define _CRT_SECURE_NO_WARNINGS
#define _USE_MATH_DEFINES
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

#include <gl/GL.h>
#include <gl/GLU.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl2.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ======================================================================================
//  ENGINE API  (everything in this block is available to your lesson code)
// ======================================================================================

enum MeshType { MESH_CUBE = 0, MESH_SPHERE, MESH_PYRAMID, MESH_CYLINDER, MESH_PLANE, MESH_COUNT };
const char *gMeshNames[MESH_COUNT] = { "Cube", "Sphere", "Pyramid", "Cylinder", "Plane" };

struct Entity
{
    char  name[32];
    int   mesh;          // MeshType
    float position[3];
    float rotation[3];   // Euler angles in degrees (X, Y, Z)
    float scale[3];
    float color[4];      // RGBA, 0..1
    int   texture;       // index into gTextures[], -1 = none
    bool  visible;
    bool  spin;          // auto-rotate around Y
    float spinSpeed;     // degrees per second
};

#define MAX_ENTITIES 128
Entity gEntities[MAX_ENTITIES];
int    gEntityCount    = 0;
int    gSelectedEntity = -1;     // -1 = nothing selected (LMB in the 3D view changes it)

struct Camera
{
    float target[3];     // orbit centre
    float yaw;           // degrees
    float pitch;         // degrees
    float distance;
    float fov;           // vertical field of view, degrees
    float moveSpeed;     // units per second for WASD
};
Camera gCamera = { { 0.0f, 0.5f, 0.0f }, 35.0f, 25.0f, 12.0f, 45.0f, 5.0f };

struct Light
{
    float direction[3];  // direction the light travels
    float color[3];
    float ambient[3];
    bool  enabled;
};
Light gLight = { { -0.5f, -1.0f, -0.35f }, { 1.0f, 0.97f, 0.9f }, { 0.22f, 0.22f, 0.26f }, true };

// Render / runtime settings
float gClearColor[4]       = { 0.09f, 0.10f, 0.12f, 1.0f };
bool  gShowGrid            = true;
bool  gWireframe           = false;
bool  gShowSelection       = true;   // yellow outline on selected entity
bool  gEnablePicking       = true;   // LMB in the viewport selects entities
bool  gCameraInputEnabled  = true;   // mouse/keyboard camera controls
bool  gShowHelpOverlay     = true;   // hint text bottom-left
bool  gPaused              = false;  // freezes spin animation
bool  gVSync               = true;
float gTimeScale           = 1.0f;

// Timing
float gDeltaTime = 0.0f;              // seconds since last frame (unscaled)
float gTime      = 0.0f;              // seconds since start (scaled, stops when paused)
int   gFrameCount = 0;
#define FRAME_HISTORY 120
float gFrameTimes[FRAME_HISTORY];     // milliseconds, ring buffer
int   gFrameTimeOffset = 0;           // next write position in gFrameTimes

// Window
HWND ghwnd = NULL;
int  gWindowWidth = 1600, gWindowHeight = 900;

// Procedural texture "assets"
#define MAX_TEXTURES 16
struct TextureAsset { char name[32]; GLuint glId; int width, height; };
TextureAsset gTextures[MAX_TEXTURES];
int gTextureCount = 0;

// Log
enum LogLevel { LOG_INFO = 0, LOG_WARN, LOG_ERROR };
struct LogLine { int level; float time; char text[240]; };

// ---- Engine functions ----
int          Engine_AddEntity(const char *name, int mesh);        // returns index or -1 if full
void         Engine_RemoveEntity(int index);
int          Engine_DuplicateEntity(int index);
void         Engine_ResetScene(void);
void         Engine_ClearScene(void);
void         Engine_FocusEntity(int index);                       // move camera target to entity
bool         Engine_WorldToScreen(const float world[3], ImVec2 *outScreen); // false if behind camera
void         Engine_Log(const char *fmt, ...);                    // info
void         Engine_LogWarn(const char *fmt, ...);
void         Engine_LogError(const char *fmt, ...);
int          Engine_GetLogCount(void);
const LogLine *Engine_GetLog(int i);                              // 0 = oldest
void         Engine_ClearLog(void);
int          Engine_CreateTexture(const char *name, int w, int h, const unsigned char *rgba); // returns asset index
ImTextureID  Engine_TexID(int textureIndex);                      // for ImGui::Image / ImageButton
float        Engine_AverageFrameTimeMs(void);
void         Engine_Quit(void);

// ---- Lesson hooks (defined in Lesson.inl) ----
void Lesson_Init(void);
void Lesson_UI(void);

// ======================================================================================
//  Internal state
// ======================================================================================
static HDC   ghdc = NULL;
static HGLRC ghrc = NULL;
static bool  gImGuiReady = false;
static bool  gFullscreen = false;
static DWORD gSavedStyle = 0;
static WINDOWPLACEMENT gSavedPlacement = { sizeof(WINDOWPLACEMENT) };
static LARGE_INTEGER gFreq, gLastCounter;
static GLUquadric *gQuadric = NULL;
static GLdouble gModelView[16], gProjection[16];
static GLint    gViewport[4];
static char     gWindowTitle[256] = "ImGui Lab";

#define LOG_CAPACITY 512
static LogLine gLog[LOG_CAPACITY];
static int     gLogStart = 0, gLogSize = 0;

typedef BOOL (WINAPI *PFNWGLSWAPINTERVALEXTPROC_)(int);
static PFNWGLSWAPINTERVALEXTPROC_ pwglSwapIntervalEXT = NULL;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
static int  Initialize(void);
static void Resize(int, int);
static void Frame(void);
static void Uninitialize(void);
static void ToggleFullscreen(void);
static void CreateBuiltinTextures(void);

// ======================================================================================
//  Entry point
// ======================================================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR lpszCmdLine, int iCmdShow)
{
    if (lpszCmdLine && lpszCmdLine[0])
    {
        char arg[200] = { 0 };
        strncpy(arg, lpszCmdLine[0] == '"' ? lpszCmdLine + 1 : lpszCmdLine, sizeof(arg) - 1);
        size_t n = strlen(arg);
        if (n && arg[n - 1] == '"') arg[n - 1] = 0;          // strip the quotes Windows adds
        _snprintf(gWindowTitle, sizeof(gWindowTitle) - 1, "ImGui Lab - %s", arg);
    }

    WNDCLASSEXA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = "ImGuiLabWindow";
    RegisterClassExA(&wc);

    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    if (gWindowWidth  > sw - 40) gWindowWidth  = sw - 40;
    if (gWindowHeight > sh - 80) gWindowHeight = sh - 80;

    ghwnd = CreateWindowExA(WS_EX_APPWINDOW, wc.lpszClassName, gWindowTitle,
                            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                            (sw - gWindowWidth) / 2, (sh - gWindowHeight) / 2, gWindowWidth, gWindowHeight,
                            NULL, NULL, hInstance, NULL);

    if (Initialize() != 0)
    {
        MessageBoxA(NULL, "OpenGL initialisation failed.", "ImGui Lab", MB_OK | MB_ICONERROR);
        DestroyWindow(ghwnd);
        return -1;
    }

    ShowWindow(ghwnd, (iCmdShow == SW_HIDE) ? SW_SHOWNORMAL : iCmdShow);   // never start hidden
    SetForegroundWindow(ghwnd);
    SetFocus(ghwnd);

    MSG msg = { 0 };
    bool done = false;
    while (!done)
    {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) { done = true; break; }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (!done && ghrc)
            Frame();
    }
    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, iMsg, wParam, lParam))
        return TRUE;

    switch (iMsg)
    {
    case WM_ERASEBKGND: return 0;
    case WM_SIZE:
        gWindowWidth  = LOWORD(lParam);
        gWindowHeight = HIWORD(lParam);
        Resize(gWindowWidth, gWindowHeight);
        break;
    case WM_KEYDOWN:
        if (gImGuiReady && ImGui::GetIO().WantCaptureKeyboard) break;
        if (wParam == VK_ESCAPE) DestroyWindow(hwnd);
        if (wParam == VK_F11)    ToggleFullscreen();
        break;
    case WM_DESTROY:
        Uninitialize();
        PostQuitMessage(0);
        break;
    }
    return DefWindowProc(hwnd, iMsg, wParam, lParam);
}

static void ToggleFullscreen(void)
{
    MONITORINFO mi = { sizeof(mi) };
    if (!gFullscreen)
    {
        gSavedStyle = GetWindowLong(ghwnd, GWL_STYLE);
        if (GetWindowPlacement(ghwnd, &gSavedPlacement) &&
            GetMonitorInfo(MonitorFromWindow(ghwnd, MONITOR_DEFAULTTOPRIMARY), &mi))
        {
            SetWindowLong(ghwnd, GWL_STYLE, gSavedStyle & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(ghwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                         mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
                         SWP_NOZORDER | SWP_FRAMECHANGED);
        }
        gFullscreen = true;
    }
    else
    {
        SetWindowLong(ghwnd, GWL_STYLE, gSavedStyle | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(ghwnd, &gSavedPlacement);
        SetWindowPos(ghwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        gFullscreen = false;
    }
}

// ======================================================================================
//  Initialise
// ======================================================================================
static int Initialize(void)
{
    PIXELFORMATDESCRIPTOR pfd;
    ZeroMemory(&pfd, sizeof(pfd));
    pfd.nSize      = sizeof(pfd);
    pfd.nVersion   = 1;
    pfd.dwFlags    = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;

    ghdc = GetDC(ghwnd);
    if (!ghdc) return -1;
    int pf = ChoosePixelFormat(ghdc, &pfd);
    if (pf == 0 || !SetPixelFormat(ghdc, pf, &pfd)) return -2;
    ghrc = wglCreateContext(ghdc);
    if (!ghrc || !wglMakeCurrent(ghdc, ghrc)) return -3;

    pwglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC_)wglGetProcAddress("wglSwapIntervalEXT");
    if (pwglSwapIntervalEXT) pwglSwapIntervalEXT(1);

    glShadeModel(GL_SMOOTH);
    glClearDepth(1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    glEnable(GL_NORMALIZE);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);

    gQuadric = gluNewQuadric();
    gluQuadricNormals(gQuadric, GLU_SMOOTH);
    gluQuadricTexture(gQuadric, GL_TRUE);

    CreateBuiltinTextures();
    Engine_ResetScene();
    for (int i = 0; i < FRAME_HISTORY; i++) gFrameTimes[i] = 16.6f;

    // ---- Dear ImGui ----
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    // NOTE: keyboard navigation (ImGuiConfigFlags_NavEnableKeyboard) is left OFF on purpose:
    // with it on, io.WantCaptureKeyboard is true whenever ANY ImGui window is focused,
    // which would block WASD camera keys after clicking a panel. Off = captured only while typing.
    io.IniFilename = NULL;            // every lesson starts with a fresh layout
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_InitForOpenGL(ghwnd);
    ImGui_ImplOpenGL2_Init();
    gImGuiReady = true;

    Engine_Log("Engine initialised - OpenGL %s on %s", (const char *)glGetString(GL_VERSION),
               (const char *)glGetString(GL_RENDERER));

    Lesson_Init();                    // <-- your code

    QueryPerformanceFrequency(&gFreq);
    QueryPerformanceCounter(&gLastCounter);

    RECT rc;
    GetClientRect(ghwnd, &rc);
    Resize(rc.right - rc.left, rc.bottom - rc.top);
    return 0;
}

static void Resize(int width, int height)
{
    if (height <= 0) height = 1;
    if (ghrc) glViewport(0, 0, width, height);
}

// ======================================================================================
//  Procedural textures
// ======================================================================================
int Engine_CreateTexture(const char *name, int w, int h, const unsigned char *rgba)
{
    if (gTextureCount >= MAX_TEXTURES) return -1;
    TextureAsset *t = &gTextures[gTextureCount];
    strncpy(t->name, name, sizeof(t->name) - 1);
    t->width = w; t->height = h;
    glGenTextures(1, &t->glId);
    glBindTexture(GL_TEXTURE_2D, t->glId);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glBindTexture(GL_TEXTURE_2D, 0);
    return gTextureCount++;
}

ImTextureID Engine_TexID(int i)
{
    if (i < 0 || i >= gTextureCount) return (ImTextureID)0;
    return (ImTextureID)(intptr_t)gTextures[i].glId;
}

static void CreateBuiltinTextures(void)
{
    const int N = 64;
    static unsigned char px[64 * 64 * 4];
    #define PUT(x, y, r, g, b) { unsigned char *p = &px[((y) * N + (x)) * 4]; p[0] = (unsigned char)(r); p[1] = (unsigned char)(g); p[2] = (unsigned char)(b); p[3] = 255; }

    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++)            // Checker
    { int c = ((x / 8) + (y / 8)) & 1 ? 235 : 60; PUT(x, y, c, c, c); }
    Engine_CreateTexture("Checker", N, N, px);

    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++)            // Bricks
    {
        int row = y / 8, off = (row & 1) ? 8 : 0;
        bool mortar = (y % 8 == 0) || (((x + off) % 16) == 0);
        int n = (x * 37 + y * 91) % 23;
        if (mortar) PUT(x, y, 200, 195, 185) else PUT(x, y, 150 + n, 60 + n / 2, 45);
    }
    Engine_CreateTexture("Bricks", N, N, px);

    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++)            // Gradient
        PUT(x, y, x * 4, 80 + y * 2, 255 - x * 3);
    Engine_CreateTexture("Gradient", N, N, px);

    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++)            // Stripes
    { bool s = ((x + y) / 6) & 1; if (s) PUT(x, y, 250, 200, 40) else PUT(x, y, 30, 30, 30); }
    Engine_CreateTexture("Hazard", N, N, px);

    unsigned int seed = 1234;                                          // Noise / grass
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++)
    { seed = seed * 1664525u + 1013904223u; int n = (seed >> 24) & 63; PUT(x, y, 40 + n / 2, 110 + n, 40 + n / 3); }
    Engine_CreateTexture("Grass", N, N, px);

    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++)            // Dots
    {
        int dx = (x % 16) - 8, dy = (y % 16) - 8;
        if (dx * dx + dy * dy < 20) PUT(x, y, 90, 200, 255) else PUT(x, y, 20, 40, 70);
    }
    Engine_CreateTexture("Dots", N, N, px);
    #undef PUT
}

// ======================================================================================
//  Scene management
// ======================================================================================
int Engine_AddEntity(const char *name, int mesh)
{
    if (gEntityCount >= MAX_ENTITIES) { Engine_LogWarn("Scene full (%d entities)", MAX_ENTITIES); return -1; }
    Entity *e = &gEntities[gEntityCount];
    memset(e, 0, sizeof(*e));
    strncpy(e->name, name ? name : "Entity", sizeof(e->name) - 1);
    e->mesh = (mesh >= 0 && mesh < MESH_COUNT) ? mesh : MESH_CUBE;
    e->scale[0] = e->scale[1] = e->scale[2] = 1.0f;
    e->color[0] = 0.8f; e->color[1] = 0.8f; e->color[2] = 0.8f; e->color[3] = 1.0f;
    e->texture = -1;
    e->visible = true;
    e->spinSpeed = 45.0f;
    return gEntityCount++;
}

void Engine_RemoveEntity(int index)
{
    if (index < 0 || index >= gEntityCount) return;
    for (int i = index; i < gEntityCount - 1; i++) gEntities[i] = gEntities[i + 1];
    gEntityCount--;
    if (gSelectedEntity == index) gSelectedEntity = -1;
    else if (gSelectedEntity > index) gSelectedEntity--;
}

int Engine_DuplicateEntity(int index)
{
    if (index < 0 || index >= gEntityCount || gEntityCount >= MAX_ENTITIES) return -1;
    gEntities[gEntityCount] = gEntities[index];
    Entity *e = &gEntities[gEntityCount];
    char buf[32];
    _snprintf(buf, sizeof(buf) - 1, "%.24s (copy)", gEntities[index].name);
    buf[31] = 0;
    strcpy(e->name, buf);
    e->position[0] += 1.5f;
    return gEntityCount++;
}

void Engine_ClearScene(void) { gEntityCount = 0; gSelectedEntity = -1; }

static void SetVec3(float *v, float x, float y, float z) { v[0] = x; v[1] = y; v[2] = z; }
static void SetColor(float *c, float r, float g, float b) { c[0] = r; c[1] = g; c[2] = b; c[3] = 1.0f; }

void Engine_ResetScene(void)
{
    Engine_ClearScene();
    int i;
    i = Engine_AddEntity("Ground", MESH_PLANE);   SetVec3(gEntities[i].scale, 12, 1, 12); SetColor(gEntities[i].color, 0.35f, 0.38f, 0.42f); gEntities[i].texture = 0;
    i = Engine_AddEntity("Crate", MESH_CUBE);     SetVec3(gEntities[i].position, -3.0f, 0.5f, 0); SetColor(gEntities[i].color, 0.95f, 0.55f, 0.20f); gEntities[i].texture = 1;
    i = Engine_AddEntity("Planet", MESH_SPHERE);  SetVec3(gEntities[i].position, 0, 1.0f, 0); SetVec3(gEntities[i].scale, 2, 2, 2); SetColor(gEntities[i].color, 0.25f, 0.6f, 1.0f); gEntities[i].spin = true;
    i = Engine_AddEntity("Pyramid", MESH_PYRAMID);SetVec3(gEntities[i].position, 3.0f, 0.75f, 0); SetVec3(gEntities[i].scale, 1.5f, 1.5f, 1.5f); SetColor(gEntities[i].color, 0.95f, 0.85f, 0.3f);
    i = Engine_AddEntity("Pillar", MESH_CYLINDER);SetVec3(gEntities[i].position, 0, 1.0f, -3.5f); SetVec3(gEntities[i].scale, 0.8f, 2.0f, 0.8f); SetColor(gEntities[i].color, 0.6f, 0.9f, 0.5f);
}

void Engine_FocusEntity(int index)
{
    if (index < 0 || index >= gEntityCount) return;
    for (int k = 0; k < 3; k++) gCamera.target[k] = gEntities[index].position[k];
}

void Engine_Quit(void) { if (ghwnd) PostMessage(ghwnd, WM_CLOSE, 0, 0); }

// ======================================================================================
//  Log
// ======================================================================================
static void LogV(int level, const char *fmt, va_list args)
{
    int idx;
    if (gLogSize < LOG_CAPACITY) idx = (gLogStart + gLogSize++) % LOG_CAPACITY;
    else { idx = gLogStart; gLogStart = (gLogStart + 1) % LOG_CAPACITY; }
    gLog[idx].level = level;
    gLog[idx].time  = gTime;
    _vsnprintf(gLog[idx].text, sizeof(gLog[idx].text) - 1, fmt, args);
    gLog[idx].text[sizeof(gLog[idx].text) - 1] = 0;
}
void Engine_Log(const char *fmt, ...)      { va_list a; va_start(a, fmt); LogV(LOG_INFO,  fmt, a); va_end(a); }
void Engine_LogWarn(const char *fmt, ...)  { va_list a; va_start(a, fmt); LogV(LOG_WARN,  fmt, a); va_end(a); }
void Engine_LogError(const char *fmt, ...) { va_list a; va_start(a, fmt); LogV(LOG_ERROR, fmt, a); va_end(a); }
int  Engine_GetLogCount(void) { return gLogSize; }
const LogLine *Engine_GetLog(int i) { return (i < 0 || i >= gLogSize) ? NULL : &gLog[(gLogStart + i) % LOG_CAPACITY]; }
void Engine_ClearLog(void) { gLogStart = gLogSize = 0; }

float Engine_AverageFrameTimeMs(void)
{
    float s = 0; for (int i = 0; i < FRAME_HISTORY; i++) s += gFrameTimes[i];
    return s / FRAME_HISTORY;
}

// ======================================================================================
//  Rendering helpers
// ======================================================================================
static void DrawCube(void)
{
    static const float n[6][3] = { {0,0,1},{0,0,-1},{1,0,0},{-1,0,0},{0,1,0},{0,-1,0} };
    static const float v[6][4][3] = {
        {{-.5f,-.5f, .5f},{ .5f,-.5f, .5f},{ .5f, .5f, .5f},{-.5f, .5f, .5f}},
        {{ .5f,-.5f,-.5f},{-.5f,-.5f,-.5f},{-.5f, .5f,-.5f},{ .5f, .5f,-.5f}},
        {{ .5f,-.5f, .5f},{ .5f,-.5f,-.5f},{ .5f, .5f,-.5f},{ .5f, .5f, .5f}},
        {{-.5f,-.5f,-.5f},{-.5f,-.5f, .5f},{-.5f, .5f, .5f},{-.5f, .5f,-.5f}},
        {{-.5f, .5f, .5f},{ .5f, .5f, .5f},{ .5f, .5f,-.5f},{-.5f, .5f,-.5f}},
        {{-.5f,-.5f,-.5f},{ .5f,-.5f,-.5f},{ .5f,-.5f, .5f},{-.5f,-.5f, .5f}} };
    static const float uv[4][2] = { {0,0},{1,0},{1,1},{0,1} };
    glBegin(GL_QUADS);
    for (int f = 0; f < 6; f++)
    {
        glNormal3fv(n[f]);
        for (int k = 0; k < 4; k++) { glTexCoord2fv(uv[k]); glVertex3fv(v[f][k]); }
    }
    glEnd();
}

static void DrawPyramid(void)
{
    const float apex[3] = { 0, 0.5f, 0 };
    const float b[4][3] = { {-.5f,-.5f,.5f},{.5f,-.5f,.5f},{.5f,-.5f,-.5f},{-.5f,-.5f,-.5f} };
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < 4; i++)
    {
        const float *p0 = b[i], *p1 = b[(i + 1) % 4];
        float e1[3] = { p1[0]-p0[0], p1[1]-p0[1], p1[2]-p0[2] };
        float e2[3] = { apex[0]-p0[0], apex[1]-p0[1], apex[2]-p0[2] };
        glNormal3f(e1[1]*e2[2]-e1[2]*e2[1], e1[2]*e2[0]-e1[0]*e2[2], e1[0]*e2[1]-e1[1]*e2[0]);
        glTexCoord2f(0, 0); glVertex3fv(p0);
        glTexCoord2f(1, 0); glVertex3fv(p1);
        glTexCoord2f(0.5f, 1); glVertex3fv(apex);
    }
    glEnd();
    glBegin(GL_QUADS);
    glNormal3f(0, -1, 0);
    for (int i = 3; i >= 0; i--) { glTexCoord2f((float)(i == 1 || i == 2), (float)(i >= 2)); glVertex3fv(b[i]); }
    glEnd();
}

static void DrawPlane(void)
{
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glTexCoord2f(0, 0); glVertex3f(-.5f, 0, .5f);
    glTexCoord2f(6, 0); glVertex3f(.5f, 0, .5f);
    glTexCoord2f(6, 6); glVertex3f(.5f, 0, -.5f);
    glTexCoord2f(0, 6); glVertex3f(-.5f, 0, -.5f);
    glEnd();
}

static void DrawMesh(int mesh)
{
    switch (mesh)
    {
    case MESH_CUBE:    DrawCube(); break;
    case MESH_SPHERE:  gluSphere(gQuadric, 0.5, 32, 20); break;
    case MESH_PYRAMID: DrawPyramid(); break;
    case MESH_CYLINDER:
        glPushMatrix();
        glTranslatef(0, 0.5f, 0); glRotatef(90, 1, 0, 0);
        gluCylinder(gQuadric, 0.5, 0.5, 1.0, 28, 1);
        gluDisk(gQuadric, 0, 0.5, 28, 1);
        glTranslatef(0, 0, 1.0f); glRotatef(180, 1, 0, 0);
        gluDisk(gQuadric, 0, 0.5, 28, 1);
        glPopMatrix();
        break;
    case MESH_PLANE:   DrawPlane(); break;
    }
}

static void ApplyTransform(const Entity *e)
{
    glTranslatef(e->position[0], e->position[1], e->position[2]);
    glRotatef(e->rotation[1], 0, 1, 0);
    glRotatef(e->rotation[0], 1, 0, 0);
    glRotatef(e->rotation[2], 0, 0, 1);
    glScalef(e->scale[0], e->scale[1], e->scale[2]);
}

static void DrawGrid(void)
{
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    for (int i = -10; i <= 10; i++)
    {
        float c = (i == 0) ? 0.0f : 0.28f;
        glColor3f(c, c, c + 0.03f);
        if (i != 0) { glVertex3f((float)i, 0.002f, -10); glVertex3f((float)i, 0.002f, 10);
                      glVertex3f(-10, 0.002f, (float)i); glVertex3f(10, 0.002f, (float)i); }
    }
    glColor3f(0.9f, 0.25f, 0.25f); glVertex3f(-10, 0.003f, 0); glVertex3f(10, 0.003f, 0);   // X axis
    glColor3f(0.25f, 0.45f, 0.95f); glVertex3f(0, 0.003f, -10); glVertex3f(0, 0.003f, 10);  // Z axis
    glColor3f(0.3f, 0.9f, 0.3f);  glVertex3f(0, 0, 0); glVertex3f(0, 1.5f, 0);              // Y axis
    glEnd();
}

static void CameraEye(float eye[3])
{
    float yaw = gCamera.yaw * (float)M_PI / 180.0f, pitch = gCamera.pitch * (float)M_PI / 180.0f;
    eye[0] = gCamera.target[0] + gCamera.distance * cosf(pitch) * sinf(yaw);
    eye[1] = gCamera.target[1] + gCamera.distance * sinf(pitch);
    eye[2] = gCamera.target[2] + gCamera.distance * cosf(pitch) * cosf(yaw);
}

static void RenderScene(void)
{
    glClearColor(gClearColor[0], gClearColor[1], gClearColor[2], gClearColor[3]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(gCamera.fov, (double)gWindowWidth / (double)(gWindowHeight > 0 ? gWindowHeight : 1), 0.05, 500.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    float eye[3];
    CameraEye(eye);
    gluLookAt(eye[0], eye[1], eye[2], gCamera.target[0], gCamera.target[1], gCamera.target[2], 0, 1, 0);

    glGetDoublev(GL_MODELVIEW_MATRIX, gModelView);
    glGetDoublev(GL_PROJECTION_MATRIX, gProjection);
    glGetIntegerv(GL_VIEWPORT, gViewport);

    // Light (directional: w = 0, position points TOWARDS the light)
    if (gLight.enabled)
    {
        GLfloat pos[4] = { -gLight.direction[0], -gLight.direction[1], -gLight.direction[2], 0.0f };
        GLfloat dif[4] = { gLight.color[0], gLight.color[1], gLight.color[2], 1.0f };
        GLfloat amb[4] = { gLight.ambient[0], gLight.ambient[1], gLight.ambient[2], 1.0f };
        glLightfv(GL_LIGHT0, GL_POSITION, pos);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, dif);
        glLightfv(GL_LIGHT0, GL_AMBIENT, amb);
        glEnable(GL_LIGHT0);
    }

    if (gShowGrid) DrawGrid();

    glPolygonMode(GL_FRONT_AND_BACK, gWireframe ? GL_LINE : GL_FILL);
    for (int i = 0; i < gEntityCount; i++)
    {
        const Entity *e = &gEntities[i];
        if (!e->visible) continue;
        if (gLight.enabled) glEnable(GL_LIGHTING); else glDisable(GL_LIGHTING);
        if (e->texture >= 0 && e->texture < gTextureCount)
        {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, gTextures[e->texture].glId);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        }
        else glDisable(GL_TEXTURE_2D);

        if (e->color[3] < 1.0f) { glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); }
        glColor4fv(e->color);
        glPushMatrix();
        ApplyTransform(e);
        if (e->mesh == MESH_PLANE) { glEnable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(1, 1); }
        DrawMesh(e->mesh);
        glDisable(GL_POLYGON_OFFSET_FILL);
        glPopMatrix();
        glDisable(GL_BLEND);
    }
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);

    // Selection outline
    if (gShowSelection && gSelectedEntity >= 0 && gSelectedEntity < gEntityCount && gEntities[gSelectedEntity].visible)
    {
        const Entity *e = &gEntities[gSelectedEntity];
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glLineWidth(2.0f);
        glColor3f(1.0f, 0.85f, 0.1f);
        glDepthFunc(GL_ALWAYS);
        glPushMatrix();
        ApplyTransform(e);
        glScalef(1.01f, 1.01f, 1.01f);
        DrawMesh(e->mesh);
        glPopMatrix();
        glDepthFunc(GL_LEQUAL);
        glLineWidth(1.0f);
    }
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);   // ImGui must render filled
}

bool Engine_WorldToScreen(const float world[3], ImVec2 *out)
{
    GLdouble sx, sy, sz;
    if (!gluProject(world[0], world[1], world[2], gModelView, gProjection, gViewport, &sx, &sy, &sz)) return false;
    if (sz < 0.0 || sz > 1.0) return false;
    out->x = (float)sx;
    out->y = (float)(gViewport[3] - sy);   // OpenGL y-up -> ImGui y-down
    return true;
}

static int PickEntity(float mx, float my)
{
    GLdouble nx, ny, nz, fx, fy, fz;
    double wy = gViewport[3] - my;
    gluUnProject(mx, wy, 0.0, gModelView, gProjection, gViewport, &nx, &ny, &nz);
    gluUnProject(mx, wy, 1.0, gModelView, gProjection, gViewport, &fx, &fy, &fz);
    double d[3] = { fx - nx, fy - ny, fz - nz };
    double len = sqrt(d[0]*d[0] + d[1]*d[1] + d[2]*d[2]);
    for (int k = 0; k < 3; k++) d[k] /= len;

    int best = -1; double bestT = 1e30;
    for (int i = 0; i < gEntityCount; i++)
    {
        const Entity *e = &gEntities[i];
        if (!e->visible || e->mesh == MESH_PLANE) continue;   // planes are "ground", skip
        float r = e->scale[0]; if (e->scale[1] > r) r = e->scale[1]; if (e->scale[2] > r) r = e->scale[2];
        r *= 0.6f;
        double oc[3] = { nx - e->position[0], ny - e->position[1], nz - e->position[2] };
        double b = oc[0]*d[0] + oc[1]*d[1] + oc[2]*d[2];
        double c = oc[0]*oc[0] + oc[1]*oc[1] + oc[2]*oc[2] - r*r;
        double h = b*b - c;
        if (h < 0) continue;
        double t = -b - sqrt(h);
        if (t > 0 && t < bestT) { bestT = t; best = i; }
    }
    return best;
}

// ======================================================================================
//  Per-frame
// ======================================================================================
static void UpdateCamera(float dt)
{
    ImGuiIO &io = ImGui::GetIO();
    if (!gCameraInputEnabled) return;

    if (!io.WantCaptureMouse)
    {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f))
        {
            gCamera.yaw   -= io.MouseDelta.x * 0.3f;
            gCamera.pitch += io.MouseDelta.y * 0.3f;
            if (gCamera.pitch >  89.0f) gCamera.pitch =  89.0f;
            if (gCamera.pitch < -89.0f) gCamera.pitch = -89.0f;
        }
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f))
        {
            float yaw = gCamera.yaw * (float)M_PI / 180.0f;
            float k = gCamera.distance * 0.0015f;
            gCamera.target[0] -= cosf(yaw) * io.MouseDelta.x * k;
            gCamera.target[2] += sinf(yaw) * io.MouseDelta.x * k;
            gCamera.target[1] += io.MouseDelta.y * k;
        }
        if (io.MouseWheel != 0.0f)
        {
            gCamera.distance *= (io.MouseWheel > 0) ? 0.9f : 1.1f;
            if (gCamera.distance < 1.0f)   gCamera.distance = 1.0f;
            if (gCamera.distance > 200.0f) gCamera.distance = 200.0f;
        }
        if (gEnablePicking && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            int hit = PickEntity(io.MousePos.x, io.MousePos.y);
            gSelectedEntity = hit;
            if (hit >= 0) Engine_Log("Selected '%s' (viewport click)", gEntities[hit].name);
        }
    }

    if (!io.WantCaptureKeyboard && !io.KeyCtrl && !io.KeyAlt)   // Ctrl+D etc. are editor shortcuts, not movement
    {
        float yaw = gCamera.yaw * (float)M_PI / 180.0f;
        float fwd[2] = { -sinf(yaw), -cosf(yaw) }, right[2] = { cosf(yaw), -sinf(yaw) };
        float s = gCamera.moveSpeed * dt;
        if (ImGui::IsKeyDown(ImGuiKey_W)) { gCamera.target[0] += fwd[0] * s;   gCamera.target[2] += fwd[1] * s; }
        if (ImGui::IsKeyDown(ImGuiKey_S)) { gCamera.target[0] -= fwd[0] * s;   gCamera.target[2] -= fwd[1] * s; }
        if (ImGui::IsKeyDown(ImGuiKey_D)) { gCamera.target[0] += right[0] * s; gCamera.target[2] += right[1] * s; }
        if (ImGui::IsKeyDown(ImGuiKey_A)) { gCamera.target[0] -= right[0] * s; gCamera.target[2] -= right[1] * s; }
        if (ImGui::IsKeyDown(ImGuiKey_E)) gCamera.target[1] += s;
        if (ImGui::IsKeyDown(ImGuiKey_Q)) gCamera.target[1] -= s;
    }
}

static void Frame(void)
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    gDeltaTime = (float)(now.QuadPart - gLastCounter.QuadPart) / (float)gFreq.QuadPart;
    gLastCounter = now;
    if (gDeltaTime > 0.25f) gDeltaTime = 0.25f;
    gFrameTimes[gFrameTimeOffset] = gDeltaTime * 1000.0f;
    gFrameTimeOffset = (gFrameTimeOffset + 1) % FRAME_HISTORY;
    gFrameCount++;

    if (pwglSwapIntervalEXT)
    {
        static bool lastVSync = true;
        if (lastVSync != gVSync) { pwglSwapIntervalEXT(gVSync ? 1 : 0); lastVSync = gVSync; }
    }

    float simDt = gPaused ? 0.0f : gDeltaTime * gTimeScale;
    gTime += simDt;
    for (int i = 0; i < gEntityCount; i++)
        if (gEntities[i].spin)
        {
            gEntities[i].rotation[1] += gEntities[i].spinSpeed * simDt;
            if (gEntities[i].rotation[1] >= 360.0f) gEntities[i].rotation[1] -= 360.0f;
            if (gEntities[i].rotation[1] < 0.0f)    gEntities[i].rotation[1] += 360.0f;
        }

    // ---- 1. 3D scene ----
    RenderScene();

    // ---- 2. ImGui frame ----
    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    Lesson_UI();                                   // <-- your code

    // Camera input is processed AFTER the UI so WantCaptureMouse reflects this frame's windows
    UpdateCamera(gDeltaTime);

    if (gShowHelpOverlay)
    {
        ImDrawList *bg = ImGui::GetBackgroundDrawList();
        bg->AddText(ImVec2(10, (float)gWindowHeight - 24), IM_COL32(200, 200, 210, 160),
                    "RMB orbit | MMB pan | Wheel zoom | LMB select | WASD/QE move | F11 fullscreen | ESC quit");
    }

    ImGui::Render();
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
    SwapBuffers(ghdc);
}

// ======================================================================================
//  Shutdown
// ======================================================================================
static void Uninitialize(void)
{
    if (gImGuiReady)
    {
        ImGui_ImplOpenGL2_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        gImGuiReady = false;
    }
    for (int i = 0; i < gTextureCount; i++) glDeleteTextures(1, &gTextures[i].glId);
    gTextureCount = 0;
    if (gQuadric) { gluDeleteQuadric(gQuadric); gQuadric = NULL; }
    if (gFullscreen) ToggleFullscreen();
    if (wglGetCurrentContext() == ghrc) wglMakeCurrent(NULL, NULL);
    if (ghrc) { wglDeleteContext(ghrc); ghrc = NULL; }
    if (ghdc) { ReleaseDC(ghwnd, ghdc); ghdc = NULL; }
}

// ======================================================================================
//  YOUR LESSON CODE
// ======================================================================================
#include "Lesson.inl"
