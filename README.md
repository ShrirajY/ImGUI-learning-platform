# ImGui Lab: Dear ImGui learning platform (Win32 + OpenGL, game-engine focused)

A local website that teaches Dear ImGui from scratch, starting from the plain `OGL.c` template. Every lesson has a live editor. **Build & Run** compiles your code with MSVC into a real OpenGL + ImGui window.

## Quick start (Windows 10/11)

```bat
git clone <this-repo-url> ImGUI-learning-platform
cd ImGUI-learning-platform
setup.bat      :: once - checks/installs Node.js + Visual Studio C++ Build Tools, test-builds the engine
run.bat        :: every time - starts http://localhost:5173 and opens your browser
```

No `npm install` is needed (the server uses only built-in Node modules). Dear ImGui is already in the repo.
Requirements are installed by `setup.bat` via winget if missing: **Node.js LTS** and **Visual Studio 2022 Build Tools
(C++ workload)**. An existing Visual Studio 2022 install with "Desktop development with C++" also works.

## Your progress

Progress (completed lessons, quiz answers, your edited code) is saved in your browser on your machine.
It survives restarts of the site. The first time `setup.bat` runs on a machine it creates `.install_id`;
when the site sees a new id it starts you from 0. To start over: use **Reset progress** in the sidebar,
or delete `.install_id` and run `setup.bat` again.

## Starter template

`public/downloads/ImGui_OpenGL_Template.zip` is a minimal Win32 + OpenGL + ImGui project (OGL.c + run.bat) you can
forward to anyone; also downloadable from the site's Setup page.

## Contents

- **20 lessons** covering immediate mode, integration into OGL.c, widgets, colours, the ID stack, layout, combos/lists, scene hierarchy + inspector, menus/popups/shortcuts, tables, console, input capture + drag & drop, profiler/plots, ImDrawList, textures/asset browser, styling/fonts, a capstone mini-editor, and next steps. Each lesson has challenges, solutions and a quiz.
- **Playground**: a free editor with insertable snippets.
- **Engine API**: reference for the sandbox engine (entities, camera, light, picking, log, textures).
- **Cheat Sheet**: the most-used ImGui calls on one page.

## Layout

```
server/server.js     local HTTP server (no npm packages), binds 127.0.0.1 only
server/builder.js    MSVC pipeline: vcvars env -> cl /TP OGL.c (+ Lesson.inl) -> link -> launch
server/build-cli.js  compile lesson files from the command line (used by setup.bat)
public/              website + lessons/<id>.html|.cpp|.json + pages/
sandbox/engine/OGL.c mini engine host (your code is #included as Lesson.inl)
sandbox/external/imgui  Dear ImGui 1.92.0 WIP (core + win32 + opengl2 backends)
```

Adding a lesson: create `public/lessons/<id>.html`, `<id>.json` (title, summary, hasCode, quiz) and optionally `<id>.cpp`, then add the id to `public/lessons/manifest.json`.
Checking that lessons compile: `node server\build-cli.js public\lessons\*.cpp`
