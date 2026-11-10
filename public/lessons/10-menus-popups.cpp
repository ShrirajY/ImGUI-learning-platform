// Lesson 10 - Menus, shortcuts, popups, modals and tooltips
//   Main menu bar  : File / Edit / View / Create / Help, toggling every panel
//   Shortcuts      : Ctrl+N new scene, Ctrl+D duplicate, Delete (with confirm), F focus, P pause, F1 about, Ctrl+Q quit
//   Modals         : "Delete entity?", "Reset scene?" (unsaved changes), "About"
//   Tooltips       : SetItemTooltip (text) and BeginItemTooltip (rich content)
// Change something, then Build & Run (Ctrl+Enter).

// ======================================================================================
//  Editor state
// ======================================================================================
static bool L10_showScene      = true;    // panel visibility, toggled from the View menu
static bool L10_showProperties = true;
static bool L10_showStats      = false;
static bool L10_showDemo       = false;
static bool L10_showAbout      = false;   // About is a modal with a close [x]

static bool L10_dirty          = false;   // "unsaved changes" - set by anything that edits the scene
static bool L10_confirmDelete  = true;    // user can turn the confirmation off
static bool L10_sceneSortByName = false;  // option in the Scene window's own menu bar

// Requests: menus and shortcuts only SET these. The popups are opened at the top level
// of Lesson_UI so that OpenPopup() and BeginPopupModal() share the same ID stack.
static bool L10_reqDeleteModal = false;
static bool L10_reqResetModal  = false;
static bool L10_reqAboutModal  = false;

// ======================================================================================
//  Commands (the things menus, buttons and shortcuts all call)
// ======================================================================================
static bool L10_HasSelection(void) { return gSelectedEntity >= 0 && gSelectedEntity < gEntityCount; }

static void L10_CmdCreate(int mesh)
{
    int i = Engine_AddEntity(gMeshNames[mesh], mesh);
    if (i < 0) return;
    for (int k = 0; k < 3; k++) gEntities[i].position[k] = gCamera.target[k];
    gSelectedEntity = i;
    L10_dirty = true;
    Engine_Log("Created %s", gEntities[i].name);
}

static void L10_CmdDuplicate(void)
{
    if (!L10_HasSelection()) { Engine_LogWarn("Duplicate: nothing selected"); return; }
    int n = Engine_DuplicateEntity(gSelectedEntity);
    if (n >= 0) { gSelectedEntity = n; L10_dirty = true; Engine_Log("Duplicated -> %s", gEntities[n].name); }
}

static void L10_CmdDeleteNow(void)
{
    if (!L10_HasSelection()) return;
    Engine_Log("Deleted %s", gEntities[gSelectedEntity].name);
    Engine_RemoveEntity(gSelectedEntity);
    L10_dirty = true;
}

static void L10_CmdRequestDelete(void)        // ask first (if enabled)
{
    if (!L10_HasSelection()) return;
    if (L10_confirmDelete) L10_reqDeleteModal = true;
    else                   L10_CmdDeleteNow();
}

static void L10_CmdRequestNewScene(void)      // only ask when there is something to lose
{
    if (L10_dirty) L10_reqResetModal = true;
    else           { Engine_ResetScene(); Engine_Log("New scene"); }
}

// ======================================================================================
//  Main menu bar
// ======================================================================================
static void L10_MainMenuBar(void)
{
    if (!ImGui::BeginMainMenuBar()) return;

    if (ImGui::BeginMenu("File"))
    {
        // The 2nd argument is ONLY display text. The real shortcut is handled in L10_Shortcuts().
        if (ImGui::MenuItem("New Scene", "Ctrl+N")) L10_CmdRequestNewScene();
        if (ImGui::MenuItem("Clear Scene"))         { Engine_ClearScene(); L10_dirty = true; }
        ImGui::MenuItem("Save", "Ctrl+S", false, false);   // enabled = false -> greyed out
        ImGui::SetItemTooltip("Saving arrives in a later lesson");
        ImGui::Separator();
        if (ImGui::MenuItem("Quit", "Ctrl+Q"))      Engine_Quit();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit"))
    {
        bool sel = L10_HasSelection();
        if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, sel)) L10_CmdDuplicate();
        if (ImGui::MenuItem("Delete", "Del", false, sel))       L10_CmdRequestDelete();
        if (ImGui::MenuItem("Focus Selected", "F", false, sel)) Engine_FocusEntity(gSelectedEntity);
        if (ImGui::MenuItem("Deselect", NULL, false, sel))      gSelectedEntity = -1;
        ImGui::Separator();
        ImGui::MenuItem("Confirm before delete", NULL, &L10_confirmDelete);  // bool* = checkmark
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View"))
    {
        // bool* overload: shows a checkmark and toggles the bool when clicked
        ImGui::MenuItem("Scene",      NULL, &L10_showScene);
        ImGui::MenuItem("Properties", NULL, &L10_showProperties);
        ImGui::MenuItem("Stats",      NULL, &L10_showStats);
        ImGui::Separator();
        if (ImGui::BeginMenu("Viewport"))              // nested sub-menu
        {
            ImGui::MenuItem("Grid",         NULL, &gShowGrid);
            ImGui::MenuItem("Wireframe",    NULL, &gWireframe);
            ImGui::MenuItem("Outline",      NULL, &gShowSelection);
            ImGui::MenuItem("Help overlay", NULL, &gShowHelpOverlay);
            ImGui::EndMenu();
        }
        ImGui::MenuItem("Pause simulation", "P", &gPaused);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Create"))
    {
        for (int m = 0; m < MESH_COUNT; m++)
            if (ImGui::MenuItem(gMeshNames[m]))
                L10_CmdCreate(m);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help"))
    {
        ImGui::MenuItem("ImGui Demo Window", NULL, &L10_showDemo);
        // WRONG would be: ImGui::OpenPopup("About") here - this code runs inside the
        // "Help" menu window, so the popup ID would not match BeginPopupModal("About") below.
        if (ImGui::MenuItem("About ImGui Lab...", "F1")) L10_reqAboutModal = true;
        ImGui::EndMenu();
    }

    // Right-aligned status text in the menu bar
    const char *status = L10_dirty ? "* unsaved changes" : "saved";
    float w = ImGui::CalcTextSize(status).x;
    ImGui::SameLine(ImGui::GetWindowWidth() - w - 20);
    if (L10_dirty) ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "%s", status);
    else           ImGui::TextDisabled("%s", status);

    ImGui::EndMainMenuBar();
}

// ======================================================================================
//  Real keyboard shortcuts
// ======================================================================================
static void L10_Shortcuts(void)
{
    ImGuiIO &io = ImGui::GetIO();
    if (io.WantTextInput) return;                                       // typing in a text field
    if (ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopup)) return;       // a menu or modal is open

    // RouteGlobal: fire even when no ImGui window is focused (e.g. after clicking the 3D view).
    // Note: without it, Shortcut() only fires while an ImGui window has focus.
    const ImGuiInputFlags g = ImGuiInputFlags_RouteGlobal;
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_N, g)) L10_CmdRequestNewScene();
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_D, g)) L10_CmdDuplicate();
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Q, g)) Engine_Quit();
    if (ImGui::Shortcut(ImGuiKey_Delete, g))            L10_CmdRequestDelete();
    if (ImGui::Shortcut(ImGuiKey_F, g) && L10_HasSelection()) Engine_FocusEntity(gSelectedEntity);
    if (ImGui::Shortcut(ImGuiKey_P, g))                 gPaused = !gPaused;
    if (ImGui::Shortcut(ImGuiKey_F1, g))                L10_reqAboutModal = true;
}

// ======================================================================================
//  Panels
// ======================================================================================
static void L10_EntityTooltip(const Entity *e)
{
    // Rich tooltip: any widgets you like between BeginItemTooltip / EndTooltip
    if (ImGui::BeginItemTooltip())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "%s", e->name);
        ImGui::Separator();
        ImGui::ColorButton("##c", ImVec4(e->color[0], e->color[1], e->color[2], e->color[3]),
                           ImGuiColorEditFlags_NoTooltip, ImVec2(40, 40));
        if (e->texture >= 0 && e->texture < gTextureCount)
        {
            ImGui::SameLine();
            ImGui::Image(Engine_TexID(e->texture), ImVec2(40, 40));
        }
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::Text("Mesh: %s", gMeshNames[e->mesh]);
        ImGui::Text("Pos: %.1f, %.1f, %.1f", e->position[0], e->position[1], e->position[2]);
        ImGui::Text("Visible: %s", e->visible ? "yes" : "no");
        ImGui::EndGroup();
        ImGui::EndTooltip();
    }
}

static void L10_ScenePanel(void)
{
    if (!L10_showScene) return;
    ImGui::SetNextWindowPos(ImVec2(20, 40), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 420), ImGuiCond_FirstUseEver);
    // MenuBar flag: this window gets its own menu bar under the title
    if (!ImGui::Begin("Scene", &L10_showScene, ImGuiWindowFlags_MenuBar)) { ImGui::End(); return; }

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Add"))
        {
            for (int m = 0; m < MESH_COUNT; m++)
                if (ImGui::MenuItem(gMeshNames[m])) L10_CmdCreate(m);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Options"))
        {
            ImGui::MenuItem("Sort by name", NULL, &L10_sceneSortByName);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // Build the display order (sorted copy of indices, the scene is untouched)
    int order[MAX_ENTITIES];
    for (int i = 0; i < gEntityCount; i++) order[i] = i;
    if (L10_sceneSortByName)
        for (int a = 1; a < gEntityCount; a++)                // tiny insertion sort
            for (int b = a; b > 0 && _stricmp(gEntities[order[b]].name, gEntities[order[b - 1]].name) < 0; b--)
            { int t = order[b]; order[b] = order[b - 1]; order[b - 1] = t; }

    for (int k = 0; k < gEntityCount; k++)
    {
        int i = order[k];
        ImGui::PushID(i);
        if (ImGui::Selectable(gEntities[i].name, gSelectedEntity == i))
            gSelectedEntity = i;
        L10_EntityTooltip(&gEntities[i]);                      // hover a row for details
        ImGui::PopID();
    }
    ImGui::End();
}

static void L10_PropertiesPanel(void)
{
    if (!L10_showProperties) return;
    ImGui::SetNextWindowPos(ImVec2(20, 475), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Properties", &L10_showProperties)) { ImGui::End(); return; }

    if (!L10_HasSelection()) { ImGui::TextDisabled("Nothing selected"); ImGui::End(); return; }
    Entity *e = &gEntities[gSelectedEntity];
    ImGui::PushID(gSelectedEntity);

    if (ImGui::InputText("Name", e->name, sizeof(e->name)))                 L10_dirty = true;
    if (ImGui::DragFloat3("Position", e->position, 0.05f))                  L10_dirty = true;
    if (ImGui::DragFloat3("Scale", e->scale, 0.02f, 0.01f, 100.0f))         L10_dirty = true;
    if (ImGui::ColorEdit4("Color", e->color, ImGuiColorEditFlags_NoInputs)) L10_dirty = true;
    ImGui::SameLine();

    // ---- A plain (non-modal) popup: OpenPopup + BeginPopup in the SAME scope ----
    if (ImGui::Button("Presets..."))
        ImGui::OpenPopup("ColorPresets");
    ImGui::SetItemTooltip("Pick a colour from the palette");      // simple text tooltip
    if (ImGui::BeginPopup("ColorPresets"))
    {
        static const float pal[8][3] = { {0.9f,0.3f,0.3f},{0.95f,0.55f,0.2f},{0.95f,0.85f,0.3f},{0.4f,0.85f,0.4f},
                                         {0.3f,0.7f,0.95f},{0.5f,0.4f,0.95f},{0.85f,0.85f,0.85f},{0.2f,0.2f,0.2f} };
        ImGui::TextDisabled("Palette");
        for (int p = 0; p < 8; p++)
        {
            if (p % 4) ImGui::SameLine();
            ImGui::PushID(p);
            if (ImGui::ColorButton("##p", ImVec4(pal[p][0], pal[p][1], pal[p][2], 1.0f), 0, ImVec2(28, 28)))
            {
                e->color[0] = pal[p][0]; e->color[1] = pal[p][1]; e->color[2] = pal[p][2];
                L10_dirty = true;
                ImGui::CloseCurrentPopup();                         // buttons don't auto-close popups
            }
            ImGui::PopID();
        }
        ImGui::EndPopup();
    }

    ImGui::Checkbox("Visible", &e->visible);
    ImGui::SameLine();
    ImGui::Checkbox("Spin", &e->spin);
    ImGui::Separator();
    if (ImGui::Button("Duplicate")) L10_CmdDuplicate();
    ImGui::SetItemTooltip("Ctrl+D");
    ImGui::SameLine();
    if (ImGui::Button("Delete"))    L10_CmdRequestDelete();
    ImGui::SetItemTooltip("Del (asks first if 'Confirm before delete' is on)");

    ImGui::PopID();
    ImGui::End();
}

static void L10_StatsPanel(void)
{
    if (!L10_showStats) return;
    ImGui::SetNextWindowPos(ImVec2(1250, 40), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(280, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Stats", &L10_showStats, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Frame time: %.2f ms", Engine_AverageFrameTimeMs());
        ImGui::Text("Entities:   %d / %d", gEntityCount, MAX_ENTITIES);
        ImGui::Text("Game time:  %.1f s%s", gTime, gPaused ? " (paused)" : "");
    }
    ImGui::End();
}

// ======================================================================================
//  Modals - opened and begun at the SAME level (top of Lesson_UI's ID stack)
// ======================================================================================
static void L10_CenterNextWindow(void)
{
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
}

static void L10_Modals(void)
{
    // 1. Turn requests into OpenPopup() calls HERE, where the matching Begin* lives
    if (L10_reqDeleteModal) { ImGui::OpenPopup("Delete entity?"); L10_reqDeleteModal = false; }
    if (L10_reqResetModal)  { ImGui::OpenPopup("Reset scene?");   L10_reqResetModal  = false; }
    if (L10_reqAboutModal)  { L10_showAbout = true; ImGui::OpenPopup("About ImGui Lab"); L10_reqAboutModal = false; }

    // 2. Delete confirmation
    L10_CenterNextWindow();
    if (ImGui::BeginPopupModal("Delete entity?", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (L10_HasSelection())
            ImGui::Text("Delete '%s'?\nThis cannot be undone.", gEntities[gSelectedEntity].name);
        else
            ImGui::TextDisabled("(the entity no longer exists)");
        ImGui::Separator();
        bool dontAsk = !L10_confirmDelete;            // checkbox shows the inverse of our setting
        if (ImGui::Checkbox("Don't ask me again", &dontAsk))
            L10_confirmDelete = !dontAsk;
        if (ImGui::Button("Delete", ImVec2(120, 0)))
        {
            L10_CmdDeleteNow();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
            ImGui::CloseCurrentPopup();
        ImGui::SetItemDefaultFocus();               // Enter/Space activates Cancel: the safe choice
        ImGui::EndPopup();
    }

    // 3. Unsaved changes
    L10_CenterNextWindow();
    if (ImGui::BeginPopupModal("Reset scene?", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "You have unsaved changes.");
        ImGui::Text("Resetting will restore the default scene and lose them.");
        ImGui::Separator();
        if (ImGui::Button("Discard and reset", ImVec2(150, 0)))
        {
            Engine_ResetScene();
            L10_dirty = false;
            Engine_Log("Scene reset (changes discarded)");
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
            ImGui::CloseCurrentPopup();
        ImGui::SetItemDefaultFocus();
        ImGui::EndPopup();
    }

    // 4. About: passing a bool* adds a close [x] button to the modal
    L10_CenterNextWindow();
    if (ImGui::BeginPopupModal("About ImGui Lab", &L10_showAbout, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("ImGui Lab - mini engine editor");
        ImGui::Text("Dear ImGui %s", ImGui::GetVersion());
        ImGui::Separator();
        ImGui::BulletText("Win32 + OpenGL fixed function");
        ImGui::BulletText("%d entities, %d textures", gEntityCount, gTextureCount);
        if (ImGui::Button("Close", ImVec2(-FLT_MIN, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

// ======================================================================================
//  Lesson hooks
// ======================================================================================
void Lesson_Init(void)
{
    Engine_Log("Lesson 10 started - try Ctrl+D, Delete, Ctrl+N, F1");
}

void Lesson_UI(void)
{
    L10_MainMenuBar();
    L10_Shortcuts();
    L10_ScenePanel();
    L10_PropertiesPanel();
    L10_StatsPanel();
    if (L10_showDemo) ImGui::ShowDemoWindow(&L10_showDemo);
    L10_Modals();                       // last: popups are drawn on top anyway
}
