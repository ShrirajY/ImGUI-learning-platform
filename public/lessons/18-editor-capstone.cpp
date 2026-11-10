// Lesson 18 - Capstone: a mini engine editor
// =====================================================================================
//  Everything from the course in one coherent editor:
//    Main menu bar      File / Edit / View / Create / Play controls
//    Scene Hierarchy    selection, visibility, context menus, rename modal, drop textures
//    Inspector          transform, mesh, material (colour + texture slot), behaviour
//    Console            log viewer with level toggles, text filter and commands
//    Render Settings    clear colour, grid, wireframe, light, camera, timing
//    Assets             texture strip (drag onto entities) + mesh spawn buttons
//    Stats overlay      FPS / frame-time graph pinned to the viewport corner
//    Labels             entity names drawn over the 3D view with the background draw list
//    Theme + fonts      custom theme, Segoe UI + Consolas (with fallbacks)
//
//  Architecture:
//    Ed_State     one struct with all editor-only state (panel visibility, console, play mode...)
//    Ed_Panel*()  one function per panel; each only reads/writes engine data and Ed_State
//    Ed_Queue()   structural changes (create/delete/duplicate/reset...) are queued and run by
//                 Ed_FlushCommands() at the END of the frame, never in the middle of a loop
//
//  Shortcuts: F1 hide/show editor, Ctrl+D duplicate, Delete delete, F focus, Ctrl+P play/stop
// =====================================================================================

#define ED_PAYLOAD_TEXTURE "TEXTURE_INDEX"

// -------------------------------------------------------------------------------------
//  Editor state
// -------------------------------------------------------------------------------------
struct Ed_State
{
    // panels
    bool showHierarchy, showInspector, showConsole, showRender, showAssets, showStats;
    bool showLabels, showDemo, showMetrics;
    bool editorVisible;          // F1
    bool resetLayout;            // force default window positions for one frame
    int  layoutWarmup;           // frames left to force the layout at startup (see Ed_NextWindowRect)

    // play mode
    bool  playing;
    Entity snapshot[MAX_ENTITIES];
    int   snapshotCount, snapshotSelected;

    // hierarchy rename
    int  renameIndex;
    bool openRename;
    char renameBuf[32];

    // console
    ImGuiTextFilter consoleFilter;
    bool  showInfo, showWarn, showError, autoScroll;
    char  consoleInput[128];
    int   lastLogCount;

    // fonts
    ImFont *fontMono;
};
static Ed_State ed;

// -------------------------------------------------------------------------------------
//  Deferred commands
// -------------------------------------------------------------------------------------
enum Ed_CmdType { ED_CMD_CREATE, ED_CMD_DUPLICATE, ED_CMD_DELETE, ED_CMD_FOCUS, ED_CMD_RESET, ED_CMD_CLEAR, ED_CMD_QUIT };
struct Ed_Command { int type; int arg; };
static Ed_Command Ed_queue[32];
static int        Ed_queueCount = 0;

static void Ed_Queue(int type, int arg)
{
    if (Ed_queueCount < IM_ARRAYSIZE(Ed_queue)) { Ed_queue[Ed_queueCount].type = type; Ed_queue[Ed_queueCount].arg = arg; Ed_queueCount++; }
}

static void Ed_CreateEntity(int mesh)
{
    char name[32];
    _snprintf(name, sizeof(name) - 1, "%s %d", gMeshNames[mesh], gEntityCount);
    name[31] = 0;
    int i = Engine_AddEntity(name, mesh);
    if (i < 0) return;
    for (int k = 0; k < 3; k++) gEntities[i].position[k] = gCamera.target[k];   // spawn at the camera target
    if (mesh != MESH_PLANE) gEntities[i].position[1] = 0.5f;
    gSelectedEntity = i;
    Engine_Log("Created '%s'", name);
}

static void Ed_FlushCommands(void)
{
    for (int c = 0; c < Ed_queueCount; c++)
    {
        int arg = Ed_queue[c].arg;
        switch (Ed_queue[c].type)
        {
        case ED_CMD_CREATE:    Ed_CreateEntity(arg); break;
        case ED_CMD_DUPLICATE:
            if (arg >= 0 && arg < gEntityCount) { int n = Engine_DuplicateEntity(arg); if (n >= 0) { gSelectedEntity = n; Engine_Log("Duplicated '%s'", gEntities[arg].name); } }
            break;
        case ED_CMD_DELETE:
            if (arg >= 0 && arg < gEntityCount) { Engine_Log("Deleted '%s'", gEntities[arg].name); Engine_RemoveEntity(arg); }
            break;
        case ED_CMD_FOCUS:     Engine_FocusEntity(arg); break;
        case ED_CMD_RESET:     Engine_ResetScene(); Engine_Log("Scene reset"); break;
        case ED_CMD_CLEAR:     Engine_ClearScene(); Engine_Log("Scene cleared"); break;
        case ED_CMD_QUIT:      Engine_Quit(); break;
        }
    }
    Ed_queueCount = 0;
}

// -------------------------------------------------------------------------------------
//  Play mode: snapshot the scene on Play, restore it on Stop (like every real engine)
// -------------------------------------------------------------------------------------
static void Ed_Play(void)
{
    if (ed.playing) return;
    memcpy(ed.snapshot, gEntities, sizeof(Entity) * gEntityCount);
    ed.snapshotCount    = gEntityCount;
    ed.snapshotSelected = gSelectedEntity;
    ed.playing = true;
    gPaused = false;
    Engine_Log("> Play (scene snapshot: %d entities)", gEntityCount);
}

static void Ed_Stop(void)
{
    if (!ed.playing) return;
    memcpy(gEntities, ed.snapshot, sizeof(Entity) * ed.snapshotCount);
    gEntityCount    = ed.snapshotCount;
    gSelectedEntity = ed.snapshotSelected;
    ed.playing = false;
    gPaused = true;
    Engine_Log("[] Stop (scene restored)");
}

// -------------------------------------------------------------------------------------
//  Theme
// -------------------------------------------------------------------------------------
static void Ed_ApplyTheme(void)
{
    ImGuiStyle &s = ImGui::GetStyle();
    ImGui::StyleColorsDark(&s);
    s.WindowRounding = 4.0f; s.FrameRounding = 3.0f; s.GrabRounding = 3.0f; s.PopupRounding = 3.0f;
    s.TabRounding = 3.0f; s.ScrollbarRounding = 4.0f;
    s.WindowBorderSize = 1.0f; s.FramePadding = ImVec2(6, 4); s.ItemSpacing = ImVec2(7, 5);
    s.WindowTitleAlign = ImVec2(0.02f, 0.5f);
    ImVec4 *c = s.Colors;
    ImVec4 accent = ImVec4(0.26f, 0.62f, 0.98f, 1.0f);
    c[ImGuiCol_WindowBg]        = ImVec4(0.12f, 0.12f, 0.13f, 0.97f);
    c[ImGuiCol_ChildBg]         = ImVec4(0.10f, 0.10f, 0.11f, 1.00f);
    c[ImGuiCol_PopupBg]         = ImVec4(0.13f, 0.13f, 0.14f, 0.98f);
    c[ImGuiCol_Border]          = ImVec4(0.22f, 0.22f, 0.24f, 1.00f);
    c[ImGuiCol_FrameBg]         = ImVec4(0.18f, 0.18f, 0.19f, 1.00f);
    c[ImGuiCol_FrameBgHovered]  = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
    c[ImGuiCol_FrameBgActive]   = ImVec4(0.27f, 0.27f, 0.29f, 1.00f);
    c[ImGuiCol_TitleBg]         = ImVec4(0.09f, 0.09f, 0.10f, 1.00f);
    c[ImGuiCol_TitleBgActive]   = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
    c[ImGuiCol_MenuBarBg]       = ImVec4(0.10f, 0.10f, 0.11f, 1.00f);
    c[ImGuiCol_Button]          = ImVec4(0.21f, 0.21f, 0.23f, 1.00f);
    c[ImGuiCol_ButtonHovered]   = ImVec4(0.29f, 0.29f, 0.32f, 1.00f);
    c[ImGuiCol_ButtonActive]    = ImVec4(accent.x, accent.y, accent.z, 0.6f);
    c[ImGuiCol_Header]          = ImVec4(accent.x, accent.y, accent.z, 0.28f);
    c[ImGuiCol_HeaderHovered]   = ImVec4(accent.x, accent.y, accent.z, 0.38f);
    c[ImGuiCol_HeaderActive]    = ImVec4(accent.x, accent.y, accent.z, 0.55f);
    c[ImGuiCol_CheckMark]       = accent;
    c[ImGuiCol_SliderGrab]      = ImVec4(accent.x, accent.y, accent.z, 0.7f);
    c[ImGuiCol_SliderGrabActive]= accent;
}

static void Ed_LoadFonts(void)
{
    ImGuiIO &io = ImGui::GetIO();
    FILE *f = fopen("C:\\Windows\\Fonts\\segoeui.ttf", "rb");
    if (f) { fclose(f); io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 17.0f); }
    else   io.Fonts->AddFontDefault();
    f = fopen("C:\\Windows\\Fonts\\consola.ttf", "rb");
    if (f) { fclose(f); ed.fontMono = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", 15.0f); }
    else   ed.fontMono = NULL;                          // PushFont(NULL) = default font
}

// -------------------------------------------------------------------------------------
//  Layout helper: place panels around the edges of the work area so the 3D view
//  stays visible in the middle. FirstUseEver = the user can move them afterwards.
// -------------------------------------------------------------------------------------
#define ED_LEFT_W    270.0f
#define ED_RIGHT_W   330.0f
#define ED_BOTTOM_H  210.0f

static void Ed_NextWindowRect(float x, float y, float w, float h)
{
    // The main menu bar shrinks WorkPos/WorkSize only from the NEXT frame on, so for the first
    // couple of frames we force the layout (otherwise panels would start under the menu bar).
    ImGuiCond cond = (ed.resetLayout || ed.layoutWarmup > 0) ? ImGuiCond_Always : ImGuiCond_FirstUseEver;
    const ImGuiViewport *vp = ImGui::GetMainViewport();     // WorkPos already excludes the menu bar
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + x, vp->WorkPos.y + y), cond);
    ImGui::SetNextWindowSize(ImVec2(w, h), cond);
}

static bool Ed_HasSelection(void) { return gSelectedEntity >= 0 && gSelectedEntity < gEntityCount; }

// =====================================================================================
//  Panels
// =====================================================================================
static void Ed_MainMenuBar(void)
{
    // During play mode, tint the menu bar so it's obvious edits will be lost on Stop
    if (ed.playing) ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(0.35f, 0.14f, 0.12f, 1.0f));
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Reset scene")) Ed_Queue(ED_CMD_RESET, 0);
            if (ImGui::MenuItem("Clear scene")) Ed_Queue(ED_CMD_CLEAR, 0);
            ImGui::Separator();
            if (ImGui::MenuItem("Quit")) Ed_Queue(ED_CMD_QUIT, 0);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit"))
        {
            bool sel = Ed_HasSelection();
            if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, sel)) Ed_Queue(ED_CMD_DUPLICATE, gSelectedEntity);
            if (ImGui::MenuItem("Delete", "Del", false, sel))       Ed_Queue(ED_CMD_DELETE, gSelectedEntity);
            if (ImGui::MenuItem("Focus", "F", false, sel))          Ed_Queue(ED_CMD_FOCUS, gSelectedEntity);
            ImGui::Separator();
            if (ImGui::MenuItem("Deselect", NULL, false, sel))      gSelectedEntity = -1;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("Scene Hierarchy", NULL, &ed.showHierarchy);
            ImGui::MenuItem("Inspector",       NULL, &ed.showInspector);
            ImGui::MenuItem("Console",         NULL, &ed.showConsole);
            ImGui::MenuItem("Assets",          NULL, &ed.showAssets);
            ImGui::MenuItem("Render Settings", NULL, &ed.showRender);
            ImGui::MenuItem("Stats Overlay",   NULL, &ed.showStats);
            ImGui::MenuItem("Entity Labels",   NULL, &ed.showLabels);
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Layout")) ed.resetLayout = true;
            if (ImGui::MenuItem("Hide Editor", "F1")) ed.editorVisible = false;
            ImGui::Separator();
            ImGui::MenuItem("ImGui Demo",    NULL, &ed.showDemo);
            ImGui::MenuItem("ImGui Metrics", NULL, &ed.showMetrics);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Create"))
        {
            for (int m = 0; m < MESH_COUNT; m++)
                if (ImGui::MenuItem(gMeshNames[m])) Ed_Queue(ED_CMD_CREATE, m);
            ImGui::EndMenu();
        }

        // Play controls in the middle of the bar
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() * 0.5f - 70.0f);
        if (!ed.playing) { if (ImGui::Button("  Play  ")) Ed_Play(); }
        else
        {
            if (ImGui::Button("  Stop  ")) Ed_Stop();
            ImGui::SameLine();
            if (ImGui::Button(gPaused ? "Resume" : "Pause")) gPaused = !gPaused;
        }

        // Right-aligned status text
        char status[64];
        float avg = Engine_AverageFrameTimeMs();
        _snprintf(status, sizeof(status) - 1, "%s | %d entities | %.0f FPS", ed.playing ? "PLAYING" : "EDIT", gEntityCount, avg > 0 ? 1000.0f / avg : 0.0f);
        status[63] = 0;
        ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::CalcTextSize(status).x - 16.0f);
        ImGui::TextDisabled("%s", status);
        ImGui::EndMainMenuBar();
    }
    if (ed.playing) ImGui::PopStyleColor();
}

// Shared drop target: textures onto an entity
static void Ed_TextureDropTarget(int entity)
{
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload *p = ImGui::AcceptDragDropPayload(ED_PAYLOAD_TEXTURE))
        {
            int t = *(const int *)p->Data;
            gEntities[entity].texture = t;
            Engine_Log("'%s' texture -> %s", gEntities[entity].name, gTextures[t].name);
        }
        ImGui::EndDragDropTarget();
    }
}

static void Ed_PanelHierarchy(void)
{
    Ed_NextWindowRect(0, 0, ED_LEFT_W, 420);
    if (!ImGui::Begin("Scene Hierarchy", &ed.showHierarchy)) { ImGui::End(); return; }

    static ImGuiTextFilter filter;
    filter.Draw("##find", -FLT_MIN);           // type to filter by name ("-word" excludes)
    ImGui::Separator();

    ImGui::BeginChild("##entities");
    for (int i = 0; i < gEntityCount; i++)
    {
        Entity *e = &gEntities[i];
        if (!filter.PassFilter(e->name)) continue;
        ImGui::PushID(i);

        // visibility toggle (small checkbox) + name
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 1));
        ImGui::Checkbox("##vis", &e->visible);
        ImGui::PopStyleVar();
        ImGui::SameLine();
        if (!e->visible) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        char label[64];
        _snprintf(label, sizeof(label) - 1, "%s##row", e->name); label[63] = 0;
        if (ImGui::Selectable(label, gSelectedEntity == i, ImGuiSelectableFlags_AllowDoubleClick))
        {
            gSelectedEntity = i;
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) Ed_Queue(ED_CMD_FOCUS, i);
        }
        if (!e->visible) ImGui::PopStyleColor();
        Ed_TextureDropTarget(i);

        // right-click on a row
        if (ImGui::BeginPopupContextItem("##rowctx"))
        {
            gSelectedEntity = i;
            ImGui::TextDisabled("%s", e->name);
            ImGui::Separator();
            if (ImGui::MenuItem("Rename..."))
            {
                ed.renameIndex = i;
                strncpy(ed.renameBuf, e->name, sizeof(ed.renameBuf) - 1);
                ed.renameBuf[sizeof(ed.renameBuf) - 1] = 0;
                ed.openRename = true;                     // open the modal OUTSIDE this popup/ID scope
            }
            if (ImGui::MenuItem("Focus", "F"))          Ed_Queue(ED_CMD_FOCUS, i);
            if (ImGui::MenuItem("Duplicate", "Ctrl+D")) Ed_Queue(ED_CMD_DUPLICATE, i);
            ImGui::Separator();
            if (ImGui::MenuItem("Delete", "Del"))       Ed_Queue(ED_CMD_DELETE, i);
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }

    // right-click on empty space in the list
    if (ImGui::BeginPopupContextWindow("##hierctx", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
    {
        if (ImGui::BeginMenu("Create"))
        {
            for (int m = 0; m < MESH_COUNT; m++)
                if (ImGui::MenuItem(gMeshNames[m])) Ed_Queue(ED_CMD_CREATE, m);
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Deselect")) gSelectedEntity = -1;
        ImGui::EndPopup();
    }
    ImGui::EndChild();

    // Rename modal (same ID scope as OpenPopup)
    if (ed.openRename) { ImGui::OpenPopup("Rename Entity"); ed.openRename = false; }
    if (ImGui::BeginPopupModal("Rename Entity", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        bool enter = ImGui::InputText("##name", ed.renameBuf, sizeof(ed.renameBuf), ImGuiInputTextFlags_EnterReturnsTrue);
        if ((ImGui::Button("OK", ImVec2(90, 0)) || enter) && ed.renameIndex >= 0 && ed.renameIndex < gEntityCount)
        {
            strcpy(gEntities[ed.renameIndex].name, ed.renameBuf);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(90, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::End();
}

static void Ed_PanelInspector(void)
{
    const ImGuiViewport *vp = ImGui::GetMainViewport();
    Ed_NextWindowRect(vp->WorkSize.x - ED_RIGHT_W, 0, ED_RIGHT_W, vp->WorkSize.y);
    if (!ImGui::Begin("Inspector", &ed.showInspector)) { ImGui::End(); return; }

    if (!Ed_HasSelection())
    {
        ImGui::TextDisabled("Nothing selected.");
        ImGui::TextWrapped("Click an entity in the 3D view or the Scene Hierarchy.");
        ImGui::End();
        return;
    }
    Entity *e = &gEntities[gSelectedEntity];
    ImGui::PushID(gSelectedEntity);          // widget IDs per entity: no state bleeding between selections

    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputText("##name", e->name, sizeof(e->name));
    ImGui::Checkbox("Visible", &e->visible);
    ImGui::SameLine(); ImGui::TextDisabled("  #%d", gSelectedEntity);

    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::DragFloat3("Position", e->position, 0.05f, 0, 0, "%.2f");
        ImGui::DragFloat3("Rotation", e->rotation, 0.5f, 0, 0, "%.1f");
        ImGui::DragFloat3("Scale",    e->scale, 0.02f, 0.01f, 100.0f, "%.2f");
        if (ImGui::Button("Reset transform"))
        {
            for (int k = 0; k < 3; k++) { e->position[k] = 0; e->rotation[k] = 0; e->scale[k] = 1; }
        }
    }
    if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen))
        ImGui::Combo("Mesh", &e->mesh, gMeshNames, MESH_COUNT);

    if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::ColorEdit4("Colour", e->color, ImGuiColorEditFlags_AlphaPreviewHalf);
        // Texture slot: thumbnail is a drop target, combo picks from the list
        if (e->texture >= 0) ImGui::Image(Engine_TexID(e->texture), ImVec2(56, 56));
        else                 ImGui::Button("none", ImVec2(56 + ImGui::GetStyle().ImageBorderSize * 2, 56));
        Ed_TextureDropTarget(gSelectedEntity);
        ImGui::SameLine();
        ImGui::BeginGroup();
        const char *preview = e->texture >= 0 ? gTextures[e->texture].name : "(none)";
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##tex", preview))
        {
            if (ImGui::Selectable("(none)", e->texture < 0)) e->texture = -1;
            for (int t = 0; t < gTextureCount; t++)
            {
                ImGui::PushID(t);
                ImGui::Image(Engine_TexID(t), ImVec2(16, 16)); ImGui::SameLine();
                if (ImGui::Selectable(gTextures[t].name, e->texture == t)) e->texture = t;
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        ImGui::TextDisabled("Drop a texture on the slot");
        ImGui::EndGroup();
    }
    if (ImGui::CollapsingHeader("Behaviour", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Spin", &e->spin);
        ImGui::BeginDisabled(!e->spin);
        ImGui::SliderFloat("Speed", &e->spinSpeed, -360.0f, 360.0f, "%.0f deg/s");
        ImGui::EndDisabled();
        if (!ed.playing) ImGui::TextDisabled("Behaviours run in Play mode.");
    }
    ImGui::Spacing();
    if (ImGui::Button("Focus"))     Ed_Queue(ED_CMD_FOCUS, gSelectedEntity);
    ImGui::SameLine();
    if (ImGui::Button("Duplicate")) Ed_Queue(ED_CMD_DUPLICATE, gSelectedEntity);
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.18f, 0.18f, 1.0f));
    if (ImGui::Button("Delete"))    Ed_Queue(ED_CMD_DELETE, gSelectedEntity);
    ImGui::PopStyleColor();

    ImGui::PopID();
    ImGui::End();
}

// ---- console commands ----
static int Ed_FindMesh(const char *name)
{
    for (int m = 0; m < MESH_COUNT; m++) if (_stricmp(name, gMeshNames[m]) == 0) return m;
    return -1;
}

static void Ed_ExecCommand(const char *line)
{
    char cmd[32] = "", arg[96] = "";
    sscanf(line, "%31s %95[^\n]", cmd, arg);
    Engine_Log("> %s", line);

    if (_stricmp(cmd, "help") == 0)
        Engine_Log("commands: help, clear, list, spawn <mesh>, select <name>, delete, focus, reset, timescale <x>, vsync on|off, play, stop");
    else if (_stricmp(cmd, "clear") == 0) Engine_ClearLog();
    else if (_stricmp(cmd, "list") == 0)
        for (int i = 0; i < gEntityCount; i++) Engine_Log("  #%d %s (%s)", i, gEntities[i].name, gMeshNames[gEntities[i].mesh]);
    else if (_stricmp(cmd, "spawn") == 0)
    {
        int m = Ed_FindMesh(arg);
        if (m >= 0) Ed_Queue(ED_CMD_CREATE, m); else Engine_LogError("unknown mesh '%s'", arg);
    }
    else if (_stricmp(cmd, "select") == 0)
    {
        int found = -1;
        for (int i = 0; i < gEntityCount && found < 0; i++) if (_stricmp(gEntities[i].name, arg) == 0) found = i;
        if (found >= 0) gSelectedEntity = found; else Engine_LogWarn("no entity named '%s'", arg);
    }
    else if (_stricmp(cmd, "delete") == 0) { if (Ed_HasSelection()) Ed_Queue(ED_CMD_DELETE, gSelectedEntity); else Engine_LogWarn("nothing selected"); }
    else if (_stricmp(cmd, "focus") == 0)  { if (Ed_HasSelection()) Ed_Queue(ED_CMD_FOCUS, gSelectedEntity); }
    else if (_stricmp(cmd, "reset") == 0)  Ed_Queue(ED_CMD_RESET, 0);
    else if (_stricmp(cmd, "timescale") == 0) { gTimeScale = (float)atof(arg); Engine_Log("time scale = %.2f", gTimeScale); }
    else if (_stricmp(cmd, "vsync") == 0)  { gVSync = (_stricmp(arg, "off") != 0); Engine_Log("vsync %s", gVSync ? "on" : "off"); }
    else if (_stricmp(cmd, "play") == 0)   Ed_Play();
    else if (_stricmp(cmd, "stop") == 0)   Ed_Stop();
    else if (cmd[0])                       Engine_LogError("unknown command '%s' (try 'help')", cmd);
}

static void Ed_PanelConsole(void)
{
    const ImGuiViewport *vp = ImGui::GetMainViewport();
    float midW = vp->WorkSize.x - ED_LEFT_W - ED_RIGHT_W;
    Ed_NextWindowRect(ED_LEFT_W, vp->WorkSize.y - ED_BOTTOM_H, midW * 0.6f, ED_BOTTOM_H);
    if (!ImGui::Begin("Console", &ed.showConsole)) { ImGui::End(); return; }

    // toolbar
    if (ImGui::SmallButton("Clear")) Engine_ClearLog();
    ImGui::SameLine(); ImGui::Checkbox("Info", &ed.showInfo);
    ImGui::SameLine(); ImGui::Checkbox("Warn", &ed.showWarn);
    ImGui::SameLine(); ImGui::Checkbox("Error", &ed.showError);
    ImGui::SameLine(); ImGui::Checkbox("Auto-scroll", &ed.autoScroll);
    ImGui::SameLine(); ed.consoleFilter.Draw("Filter", -60.0f);

    // log area: leave one line for the input
    float footer = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("##log", ImVec2(0, -footer), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PushFont(ed.fontMono);
    static const ImVec4 levelCol[3] = { ImVec4(0.80f, 0.82f, 0.86f, 1), ImVec4(1.0f, 0.78f, 0.3f, 1), ImVec4(1.0f, 0.42f, 0.42f, 1) };
    static const char *levelTag[3] = { "INFO ", "WARN ", "ERROR" };
    int n = Engine_GetLogCount();
    for (int i = 0; i < n; i++)
    {
        const LogLine *l = Engine_GetLog(i);
        if ((l->level == LOG_INFO && !ed.showInfo) || (l->level == LOG_WARN && !ed.showWarn) || (l->level == LOG_ERROR && !ed.showError)) continue;
        if (!ed.consoleFilter.PassFilter(l->text)) continue;
        ImGui::TextColored(levelCol[l->level], "%7.2f %s %s", l->time, levelTag[l->level], l->text);
    }
    // stick to the bottom when new lines arrive (and the user hasn't scrolled up)
    if (ed.autoScroll && n != ed.lastLogCount && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 20.0f)
        ImGui::SetScrollHereY(1.0f);
    ed.lastLogCount = n;
    ImGui::PopFont();
    ImGui::EndChild();

    // command line
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputTextWithHint("##cmd", "type a command, e.g. 'help' or 'spawn sphere'", ed.consoleInput, sizeof(ed.consoleInput), ImGuiInputTextFlags_EnterReturnsTrue))
    {
        if (ed.consoleInput[0]) Ed_ExecCommand(ed.consoleInput);
        ed.consoleInput[0] = 0;
        ImGui::SetKeyboardFocusHere(-1);       // keep typing in the box after Enter
    }
    ImGui::End();
}

static void Ed_PanelAssets(void)
{
    const ImGuiViewport *vp = ImGui::GetMainViewport();
    float midW = vp->WorkSize.x - ED_LEFT_W - ED_RIGHT_W;
    Ed_NextWindowRect(ED_LEFT_W + midW * 0.6f, vp->WorkSize.y - ED_BOTTOM_H, midW * 0.4f, ED_BOTTOM_H);
    if (!ImGui::Begin("Assets", &ed.showAssets)) { ImGui::End(); return; }

    ImGui::TextDisabled("Textures: drag onto an entity, double-click = apply to selected");
    ImGui::BeginChild("##strip", ImVec2(0, 92), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
    for (int t = 0; t < gTextureCount; t++)
    {
        if (t > 0) ImGui::SameLine();
        ImGui::PushID(t);
        ImGui::BeginGroup();
        bool used = Ed_HasSelection() && gEntities[gSelectedEntity].texture == t;
        if (used) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
        ImGui::ImageButton("##tex", Engine_TexID(t), ImVec2(52, 52));
        if (used) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && Ed_HasSelection())
            gEntities[gSelectedEntity].texture = t;
        if (ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload(ED_PAYLOAD_TEXTURE, &t, sizeof(int));
            ImGui::Image(Engine_TexID(t), ImVec2(40, 40)); ImGui::SameLine(); ImGui::Text("%s", gTextures[t].name);
            ImGui::EndDragDropSource();
        }
        ImGui::SetItemTooltip("%s (%dx%d)", gTextures[t].name, gTextures[t].width, gTextures[t].height);
        ImGui::TextDisabled("%.8s", gTextures[t].name);
        ImGui::EndGroup();
        ImGui::PopID();
    }
    ImGui::EndChild();

    ImGui::TextDisabled("Meshes: click to create at the camera target");
    for (int m = 0; m < MESH_COUNT; m++)
    {
        if (m > 0) ImGui::SameLine();
        if (ImGui::Button(gMeshNames[m])) Ed_Queue(ED_CMD_CREATE, m);
    }
    ImGui::End();
}

static void Ed_PanelRender(void)
{
    Ed_NextWindowRect(0, 420, ED_LEFT_W, ImGui::GetMainViewport()->WorkSize.y - 420);
    if (!ImGui::Begin("Render Settings", &ed.showRender)) { ImGui::End(); return; }

    if (ImGui::CollapsingHeader("Viewport", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::ColorEdit3("Background", gClearColor);
        ImGui::Checkbox("Grid", &gShowGrid); ImGui::SameLine();
        ImGui::Checkbox("Wireframe", &gWireframe);
        ImGui::Checkbox("Selection outline", &gShowSelection);
        ImGui::Checkbox("Picking", &gEnablePicking); ImGui::SameLine();
        ImGui::Checkbox("Help text", &gShowHelpOverlay);
    }
    if (ImGui::CollapsingHeader("Light"))
    {
        ImGui::Checkbox("Enabled", &gLight.enabled);
        ImGui::SliderFloat3("Direction", gLight.direction, -1.0f, 1.0f);
        ImGui::ColorEdit3("Diffuse", gLight.color);
        ImGui::ColorEdit3("Ambient", gLight.ambient);
    }
    if (ImGui::CollapsingHeader("Camera"))
    {
        ImGui::SliderFloat("FOV", &gCamera.fov, 20.0f, 100.0f, "%.0f deg");
        ImGui::DragFloat("Distance", &gCamera.distance, 0.1f, 1.0f, 200.0f);
        ImGui::DragFloat3("Target", gCamera.target, 0.05f);
        ImGui::SliderFloat("Move speed", &gCamera.moveSpeed, 1.0f, 30.0f);
        ImGui::Checkbox("Camera input", &gCameraInputEnabled);
    }
    if (ImGui::CollapsingHeader("Time"))
    {
        ImGui::Checkbox("VSync", &gVSync);
        ImGui::SliderFloat("Time scale", &gTimeScale, 0.0f, 4.0f, "%.2fx");
        ImGui::Text("Game time %.1f s", gTime);
    }
    ImGui::End();
}

static void Ed_StatsOverlay(void)
{
    const ImGuiViewport *vp = ImGui::GetMainViewport();
    // top-right corner of the 3D area (left of the Inspector)
    ImVec2 pos(vp->WorkPos.x + vp->WorkSize.x - (ed.showInspector ? ED_RIGHT_W : 0.0f) - 10.0f, vp->WorkPos.y + 10.0f);
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.35f);
    ImGuiWindowFlags f = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
    if (ImGui::Begin("##stats", NULL, f))
    {
        float avg = Engine_AverageFrameTimeMs();
        ImGui::Text("%.0f FPS  %.2f ms", avg > 0 ? 1000.0f / avg : 0.0f, avg);
        ImGui::PlotLines("##ft", gFrameTimes, FRAME_HISTORY, gFrameTimeOffset, NULL, 0.0f, 40.0f, ImVec2(150, 28));
        ImGui::Text("Entities %d / %d", gEntityCount, MAX_ENTITIES);
        if (Ed_HasSelection()) ImGui::Text("Selected: %s", gEntities[gSelectedEntity].name);
    }
    ImGui::End();
}

static void Ed_EntityLabels(void)
{
    ImDrawList *bg = ImGui::GetBackgroundDrawList();
    for (int i = 0; i < gEntityCount; i++)
    {
        const Entity *e = &gEntities[i];
        if (!e->visible || e->mesh == MESH_PLANE) continue;
        float top[3] = { e->position[0], e->position[1] + e->scale[1] * 0.5f + 0.2f, e->position[2] };
        ImVec2 s;
        if (!Engine_WorldToScreen(top, &s)) continue;
        bool sel = (i == gSelectedEntity);
        ImVec2 ts = ImGui::CalcTextSize(e->name);
        ImVec2 p(s.x - ts.x * 0.5f, s.y - ts.y);
        bg->AddRectFilled(ImVec2(p.x - 4, p.y - 2), ImVec2(p.x + ts.x + 4, p.y + ts.y + 2), sel ? IM_COL32(60, 120, 220, 200) : IM_COL32(0, 0, 0, 130), 3.0f);
        bg->AddText(p, sel ? IM_COL32(255, 255, 255, 255) : IM_COL32(210, 215, 225, 230), e->name);
    }
}

// -------------------------------------------------------------------------------------
//  Shortcuts
// -------------------------------------------------------------------------------------
static void Ed_HandleShortcuts(void)
{
    ImGuiIO &io = ImGui::GetIO();
    if (io.WantTextInput) return;        // typing in a text field: no editor hotkeys

    if (ImGui::IsKeyPressed(ImGuiKey_F1, false)) ed.editorVisible = !ed.editorVisible;
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_P)) { if (ed.playing) Ed_Stop(); else Ed_Play(); }
    if (!ed.editorVisible || !Ed_HasSelection()) return;
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_D)) Ed_Queue(ED_CMD_DUPLICATE, gSelectedEntity);
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))           Ed_Queue(ED_CMD_DELETE, gSelectedEntity);
    if (ImGui::IsKeyPressed(ImGuiKey_F, false) && !io.KeyCtrl) Ed_Queue(ED_CMD_FOCUS, gSelectedEntity);
}

// =====================================================================================
//  Lesson hooks
// =====================================================================================
void Lesson_Init(void)
{
    ed.showHierarchy = ed.showInspector = ed.showConsole = ed.showRender = ed.showAssets = ed.showStats = true;
    ed.showLabels = true;
    ed.editorVisible = true;
    ed.showInfo = ed.showWarn = ed.showError = ed.autoScroll = true;
    ed.renameIndex = -1;
    ed.layoutWarmup = 2;

    Ed_LoadFonts();
    Ed_ApplyTheme();
    gPaused = true;                       // edit mode: behaviours (spin) only run in Play mode
    gShowHelpOverlay = false;             // the editor has its own status bar
    Engine_Log("Editor ready. F1 = toggle editor, Ctrl+P = play/stop. Type 'help' in the console.");
}

void Lesson_UI(void)
{
    Ed_HandleShortcuts();

    if (ed.editorVisible)
    {
        Ed_MainMenuBar();                 // first: it shrinks the viewport WorkPos/WorkSize
        if (ed.showHierarchy) Ed_PanelHierarchy();
        if (ed.showRender)    Ed_PanelRender();
        if (ed.showInspector) Ed_PanelInspector();
        if (ed.showConsole)   Ed_PanelConsole();
        if (ed.showAssets)    Ed_PanelAssets();
        if (ed.showStats)     Ed_StatsOverlay();
        if (ed.showLabels && !ed.playing) Ed_EntityLabels();
        if (ed.showDemo)      ImGui::ShowDemoWindow(&ed.showDemo);
        if (ed.showMetrics)   ImGui::ShowMetricsWindow(&ed.showMetrics);
        ed.resetLayout = false;
        if (ed.layoutWarmup > 0) ed.layoutWarmup--;
    }
    else
    {
        ImGui::GetForegroundDrawList()->AddText(ImVec2(10, 8), IM_COL32(255, 255, 255, 150),
                                                ed.playing ? "PLAYING - F1 to show editor, Ctrl+P to stop" : "Editor hidden - F1 to show");
    }

    Ed_FlushCommands();                   // structural changes happen here, after all panels
}
