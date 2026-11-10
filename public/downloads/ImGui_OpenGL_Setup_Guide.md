# Dear ImGui + OpenGL: Setup Guide

This guide explains the starter project in **ImGui_OpenGL_Template.zip**. It covers what is inside, how to build it, and how ImGui is connected to a normal Win32 + OpenGL program (`OGL.c`).

---

## 1. What you need

| Requirement | Notes |
|---|---|
| Windows 10 / 11 | The project uses the Win32 API and WGL. |
| Visual Studio 2022 (Community is free) | Install the **"Desktop development with C++"** workload. It provides `cl.exe`, `link.exe` and the Windows SDK. |
| A GPU driver with OpenGL | Every normal Windows driver works. Only OpenGL 1.1 (fixed-function) is needed. |

No CMake, no package manager, and no extra libraries are needed. Everything ImGui needs is in `external/imgui`.

---

## 2. What is in the zip

```
ImGui_OpenGL_Template/
├── external/
│   └── imgui/                  Dear ImGui (1.92.0 WIP)
│       ├── imgui.h, imconfig.h, imgui_internal.h, imstb_*.h
│       ├── imgui.cpp, imgui_draw.cpp, imgui_widgets.cpp, imgui_tables.cpp
│       ├── imgui_demo.cpp              (ImGui::ShowDemoWindow - the best reference)
│       ├── imgui_impl_win32.h/.cpp     platform backend (window, mouse, keyboard)
│       └── imgui_impl_opengl2.h/.cpp   renderer backend (draws ImGui with OpenGL)
├── OGL.c        the program: Win32 window + OpenGL triangle + one ImGui window
├── run.bat      compiles, links and runs
└── README.txt   short version of this guide
```

Only the **Win32** and **OpenGL2** backends are included. The many other backends in the official repository (DirectX, Vulkan, GLFW, SDL...) were removed on purpose.

---

## 3. Build and run

1. Unzip `ImGui_OpenGL_Template.zip` anywhere.
2. From the Start menu, open **"x64 Native Tools Command Prompt for VS 2022"**. This prompt has `cl.exe` and `link.exe` on its PATH.
3. Go to the folder and run the script:
   ```bat
   cd path\to\ImGui_OpenGL_Template
   run.bat
   ```
4. A window opens with a rotating triangle and an ImGui window titled **"Hello, ImGui!"**. Change the background colour or stop the rotation from that window.

**Keys:** `Esc` quits, `F` toggles fullscreen.

### What run.bat does

```bat
cl.exe /c /EHsc /std:c++17 /TP /I external/imgui  OGL.c  external/imgui/*.cpp ...  /Fo"intermediate\\"
link.exe intermediate\*.obj  User32.lib GDI32.lib OpenGL32.lib  /SUBSYSTEM:WINDOWS /OUT:bin\OGL.exe
bin\OGL.exe
```

- **`/TP`** compiles every file as C++. Dear ImGui is a C++ library (it uses namespaces and default arguments), so `OGL.c` has to be compiled as C++ even though it keeps the `.c` name.
- `/I external/imgui` lets `#include "imgui.h"` find the headers.
- Object files go to `intermediate\` and the exe goes to `bin\`.

> Note: two `C4005 APIENTRY / WINGDIAPI macro redefinition` warnings come from `imgui_impl_opengl2.cpp`. They are harmless.

---

## 4. How ImGui is connected to OGL.c

Everything ImGui-related in `OGL.c` is marked with this banner:

```c
//================================IMGUI==============================================
...
//===================================================================================
```

There are **5 marked places** in the normal program, plus **3 functions** at the bottom of the file that do the actual work.

### 4.1 Includes and declarations (top of file)

```c
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl2.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

void IMGUI_initialize(void);
void IMGUI_display(void);
void IMGUI_uninitialize(void);

float gClearColor[3] = { 0.1f, 0.1f, 0.1f };   // values controlled from the ImGui window
bool  gRotate = true;
```

### 4.2 WndProc: give input to ImGui

```c
if (ImGui_ImplWin32_WndProcHandler(hwnd, iMsg, wParam, lParam))
    return TRUE;
```

Without this line ImGui never sees the mouse or keyboard, and the window cannot be clicked.

### 4.3 initialize(): after the OpenGL context exists

```c
IMGUI_initialize();          // called after wglMakeCurrent()
```

### 4.4 display(): draw ImGui last, then swap

```c
IMGUI_display();             // after your OpenGL drawing, before SwapBuffers()
SwapBuffers(ghdc);
```

### 4.5 uninitialize(): shut down before the GL context is deleted

```c
IMGUI_uninitialize();        // before wglDeleteContext()
```

### 4.6 The three IMGUI functions

```c
void IMGUI_initialize(void)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();                 // 1. create the ImGui context
    ImGui::StyleColorsDark();               // 2. pick a theme
    ImGui_ImplWin32_InitForOpenGL(ghwnd);   // 3. platform backend
    ImGui_ImplOpenGL2_Init();               // 4. renderer backend
}

void IMGUI_display(void)
{
    ImGui_ImplOpenGL2_NewFrame();           // start a new ImGui frame
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Hello, ImGui!");          // ---- your UI goes here ----
    ImGui::Text("This is my first ImGui window");
    ImGui::ColorEdit3("Background", gClearColor);
    ImGui::Checkbox("Rotate triangle", &gRotate);
    ImGui::End();

    ImGui::Render();                        // build the draw lists
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());   // draw them with OpenGL
}

void IMGUI_uninitialize(void)
{
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}
```

### The key idea: immediate mode

There are no button objects or callbacks. Every frame you call the widget functions again:

- `ImGui::ColorEdit3("Background", gClearColor)` **draws** the colour editor **and** writes straight into `gClearColor` when the user changes it.
- `ImGui::Checkbox("Rotate triangle", &gRotate)` does the same with a `bool`.
- `ImGui::Button("...")` returns `true` on the frame it was clicked, so you write `if (ImGui::Button("Reset")) { ... }`.

Your program's variables are the UI state, so there is nothing to keep in sync.

---

## 5. Using it in your own OpenGL project

1. Copy `external/imgui` into your project.
2. Copy the 5 banner blocks and the 3 `IMGUI_*` functions from `OGL.c` into your code.
3. Add the 7 ImGui `.cpp` files to your compile line and compile with `/TP` (or rename your file to `.cpp`).
4. Link `User32.lib GDI32.lib OpenGL32.lib`.
5. Put your own widgets inside `IMGUI_display()`, between `ImGui::NewFrame()` and `ImGui::Render()`.

To explore every widget, add `ImGui::ShowDemoWindow();` inside `IMGUI_display()`. Its source code is in `imgui_demo.cpp`.

---

## 6. Troubleshooting

| Problem | Fix |
|---|---|
| `'cl.exe' is not recognized` | You are in a normal command prompt. Use **x64 Native Tools Command Prompt for VS 2022**. |
| `cannot open include file 'imgui.h'` | Run `run.bat` from inside the template folder, so that `external/imgui` is found. |
| Lots of C errors about `namespace` or `::` | `/TP` is missing, so the file is being compiled as C. |
| ImGui window is drawn but cannot be clicked | The `ImGui_ImplWin32_WndProcHandler` call in WndProc is missing. |
| ImGui window is drawn as lines | `glPolygonMode(GL_LINE)` was left on. Set `GL_FILL` before `IMGUI_display()`. |
| Crash on exit | `IMGUI_uninitialize()` must run **before** `wglDeleteContext()`. |
| `LNK2019 unresolved external ImGui::...` | One of the 7 ImGui `.obj` files is missing from the link line. |

---

## 7. Next step: the learning platform

The **ImGUI-learning-platform** folder contains a local website. It teaches ImGui step by step for game-engine work: widgets, IDs, layout, scene hierarchy and inspector, menus, tables, console, drag & drop, profiler, draw lists, textures, styling, and a complete mini editor. Each lesson has a live code editor that compiles your code into an OpenGL window with one click.

```
ImGUI-learning-platform\setup.bat   (once: checks/installs Node.js + Visual Studio C++ tools)
ImGUI-learning-platform\run.bat     (starts the site at http://localhost:5173)
```
