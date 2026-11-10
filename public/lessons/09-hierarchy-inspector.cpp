// Lesson 9 - Scene Hierarchy + Inspector: the core editor pair
//   "Hierarchy" (left)  - tree of entities, click to select, right-click menus, rename, delete
//   "Inspector" (right) - edits whatever gSelectedEntity points at
// Selection is shared with the 3D view: LMB in the viewport also changes gSelectedEntity.
// Change something, then Build & Run (Ctrl+Enter).

// ======================================================================================
//  EDITOR STATE  (belongs to the editor UI, never saved with the scene)
//  SCENE DATA    (gEntities, gEntityCount, gSelectedEntity...) lives in the engine.
// ======================================================================================
struct L09_EditorState
{
    bool groupByMesh;        // show folder nodes per mesh type
    int  renameIndex;        // entity being renamed inline, -1 = none
    bool renameFocus;        // true on the first frame of a rename -> focus the InputText
    char renameBuf[32];      // edit buffer (committed to the entity on Enter)
    int  pendingDelete;      // deferred actions: applied AFTER all UI has been drawn
    int  pendingDuplicate;
    int  prevSelection;      // to detect selection changes made elsewhere (3D view)
    bool scrollToSelection;  // scroll the hierarchy to the selected row once
};
static L09_EditorState L09_ed = { false, -1, false, "", -1, -1, -1, false };

// ---- small helpers ------------------------------------------------------------------
static bool L09_ValidIndex(int i) { return i >= 0 && i < gEntityCount; }

static void L09_StartRename(int i)
{
    L09_ed.renameIndex = i;
    L09_ed.renameFocus = true;
    strncpy(L09_ed.renameBuf, gEntities[i].name, sizeof(L09_ed.renameBuf) - 1);
    L09_ed.renameBuf[sizeof(L09_ed.renameBuf) - 1] = 0;
}

static void L09_CreateEntity(int mesh)
{
    int i = Engine_AddEntity(gMeshNames[mesh], mesh);
    if (i < 0) return;                                   // scene full (engine logs a warning)
    for (int k = 0; k < 3; k++) gEntities[i].position[k] = gCamera.target[k];   // spawn at camera focus
    if (mesh != MESH_PLANE) gEntities[i].position[1] += 0.5f;
    gSelectedEntity = i;
    Engine_Log("Created %s", gEntities[i].name);
}

// Property grid helpers (same pattern as lesson 7)
static bool L09_BeginProps(const char *id)
{
    if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp)) return false;
    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableSetupColumn("Value",    ImGuiTableColumnFlags_WidthStretch);
    return true;
}
static void L09_PropRow(const char *label)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(-FLT_MIN);
}

// ======================================================================================
//  One hierarchy row
// ======================================================================================
static void L09_EntityRow(int i)
{
    Entity *e = &gEntities[i];
    ImGui::PushID(i);                                    // everything in this row gets a unique ID

    // ---- Inline rename mode: show an InputText instead of the tree node ----
    if (L09_ed.renameIndex == i)
    {
        if (L09_ed.renameFocus) { ImGui::SetKeyboardFocusHere(); L09_ed.renameFocus = false; }
        ImGui::SetNextItemWidth(-FLT_MIN);
        bool commit = ImGui::InputText("##rename", L09_ed.renameBuf, sizeof(L09_ed.renameBuf),
                                       ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
        if (commit)
        {
            if (L09_ed.renameBuf[0]) strcpy(e->name, L09_ed.renameBuf);   // both are char[32]
            L09_ed.renameIndex = -1;
        }
        else if (ImGui::IsItemDeactivated())             // clicked away or pressed Escape -> cancel
            L09_ed.renameIndex = -1;
        ImGui::PopID();
        return;
    }

    // ---- Normal mode: a leaf tree node ----
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf               // no arrow, no children
                             | ImGuiTreeNodeFlags_NoTreePushOnOpen   // so no TreePop() needed
                             | ImGuiTreeNodeFlags_SpanAvailWidth;    // whole row is clickable
    if (gSelectedEntity == i) flags |= ImGuiTreeNodeFlags_Selected;  // highlight

    if (!e->visible) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TreeNodeEx("##node", flags, "%s", e->name);  // label shown = name, ID = "##node" + PushID(i)
    if (!e->visible) ImGui::PopStyleColor();

    // Scroll to it once if the selection was changed from the 3D view
    if (gSelectedEntity == i && L09_ed.scrollToSelection)
    {
        ImGui::SetScrollHereY(0.5f);
        L09_ed.scrollToSelection = false;
    }

    // Click to select (IsItemClicked = mouse went down on this item this frame)
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
        gSelectedEntity = i;
    // Double-click to focus the camera
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        Engine_FocusEntity(i);

    // Right-click context menu attached to the item we just drew
    if (ImGui::BeginPopupContextItem("##rowmenu"))
    {
        gSelectedEntity = i;                             // right-click also selects (feels natural)
        ImGui::TextDisabled("%s", e->name);
        ImGui::Separator();
        if (ImGui::MenuItem("Rename", "F2"))       L09_StartRename(i);
        if (ImGui::MenuItem("Duplicate"))          L09_ed.pendingDuplicate = i;   // deferred!
        if (ImGui::MenuItem("Focus", "dbl-click")) Engine_FocusEntity(i);
        ImGui::MenuItem("Visible", NULL, &e->visible);   // bool* = checkmark toggle
        ImGui::Separator();
        if (ImGui::MenuItem("Delete", "Del"))      L09_ed.pendingDelete = i;      // deferred!
        ImGui::EndPopup();
    }

    ImGui::PopID();
}

// ======================================================================================
//  Hierarchy window
// ======================================================================================
static void L09_Hierarchy(void)
{
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 560), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Hierarchy")) { ImGui::End(); return; }

    ImGui::Checkbox("Group by mesh", &L09_ed.groupByMesh);
    ImGui::SameLine();
    ImGui::TextDisabled("(%d)", gEntityCount);
    ImGui::Separator();

    // The tree lives in a child that fills the rest of the window. Inside it,
    // "the window" means the child, so empty-space clicks ignore the title bar.
    ImGui::BeginChild("##tree", ImVec2(0, 0));

    // Root node "Scene": a real tree node with an arrow, open by default
    ImGuiTreeNodeFlags rootFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow
                                 | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (L09_ed.scrollToSelection) ImGui::SetNextItemOpen(true);       // make sure the row is visible
    if (ImGui::TreeNodeEx("Scene", rootFlags))
    {
        if (!L09_ed.groupByMesh)
        {
            for (int i = 0; i < gEntityCount; i++)
                L09_EntityRow(i);
        }
        else
        {
            for (int m = 0; m < MESH_COUNT; m++)
            {
                // "Folder" nodes: open with the arrow or a double-click
                ImGuiTreeNodeFlags f = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick
                                     | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
                if (L09_ed.scrollToSelection && L09_ValidIndex(gSelectedEntity) && gEntities[gSelectedEntity].mesh == m)
                    ImGui::SetNextItemOpen(true);
                if (ImGui::TreeNodeEx(gMeshNames[m], f))
                {
                    for (int i = 0; i < gEntityCount; i++)
                        if (gEntities[i].mesh == m)
                            L09_EntityRow(i);
                    ImGui::TreePop();                    // a normal node DOES need TreePop
                }
            }
        }
        ImGui::TreePop();
    }

    // Right-click on EMPTY space (not over an item): create menu
    if (ImGui::BeginPopupContextWindow("##hierarchyMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
    {
        ImGui::TextDisabled("Create");
        ImGui::Separator();
        for (int m = 0; m < MESH_COUNT; m++)
            if (ImGui::MenuItem(gMeshNames[m]))
                L09_CreateEntity(m);
        ImGui::Separator();
        if (ImGui::MenuItem("Reset scene")) Engine_ResetScene();
        ImGui::EndPopup();
    }

    // Left-click on empty space clears the selection
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered())
        gSelectedEntity = -1;

    ImGui::EndChild();

    // Keyboard: only while this window (or its child) is focused and we're not typing
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && L09_ed.renameIndex < 0 && L09_ValidIndex(gSelectedEntity))
    {
        if (ImGui::IsKeyPressed(ImGuiKey_Delete)) L09_ed.pendingDelete = gSelectedEntity;
        if (ImGui::IsKeyPressed(ImGuiKey_F2))     L09_StartRename(gSelectedEntity);
    }

    ImGui::End();
}

// ======================================================================================
//  Inspector window
// ======================================================================================
static void L09_Inspector(void)
{
    ImGui::SetNextWindowPos(ImVec2(1150, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(380, 600), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Inspector")) { ImGui::End(); return; }

    if (!L09_ValidIndex(gSelectedEntity))
    {
        ImGui::TextDisabled("No entity selected.");
        ImGui::TextWrapped("Click an object in the 3D view or in the Hierarchy. Right-click empty space in the Hierarchy to create one.");
        ImGui::End();
        return;
    }

    int idx = gSelectedEntity;
    Entity *e = &gEntities[idx];
    ImGui::PushID(idx);   // widget state (e.g. an active drag) belongs to THIS entity

    // ---- Header: visible toggle + name, like Unity ----
    ImGui::Checkbox("##visible", &e->visible);
    ImGui::SetItemTooltip("Visible");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputText("##name", e->name, sizeof(e->name));        // edits the scene data directly
    ImGui::TextDisabled("Entity #%d  -  %s", idx, gMeshNames[e->mesh]);

    // ---- Transform ----
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (L09_BeginProps("##xform"))
        {
            L09_PropRow("Position"); ImGui::DragFloat3("##pos", e->position, 0.05f, 0.0f, 0.0f, "%.2f");
            L09_PropRow("Rotation"); ImGui::DragFloat3("##rot", e->rotation, 0.5f, -360.0f, 360.0f, "%.1f");
            L09_PropRow("Scale");    ImGui::DragFloat3("##scl", e->scale, 0.02f, 0.01f, 100.0f, "%.2f");
            ImGui::EndTable();
        }
        if (ImGui::SmallButton("Reset transform"))
        {
            for (int k = 0; k < 3; k++) { e->position[k] = 0; e->rotation[k] = 0; e->scale[k] = 1; }
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Focus")) Engine_FocusEntity(idx);
    }

    // ---- Mesh renderer ----
    if (ImGui::CollapsingHeader("Mesh Renderer", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (L09_BeginProps("##render"))
        {
            L09_PropRow("Mesh");  ImGui::Combo("##mesh", &e->mesh, gMeshNames, MESH_COUNT);
            L09_PropRow("Color"); ImGui::ColorEdit4("##color", e->color);
            L09_PropRow("Texture");
            const char *preview = (e->texture >= 0 && e->texture < gTextureCount) ? gTextures[e->texture].name : "None";
            if (ImGui::BeginCombo("##tex", preview))
            {
                if (ImGui::Selectable("None", e->texture < 0)) e->texture = -1;
                for (int t = 0; t < gTextureCount; t++)
                {
                    ImGui::PushID(t);
                    if (ImGui::Selectable(gTextures[t].name, e->texture == t)) e->texture = t;
                    if (e->texture == t) ImGui::SetItemDefaultFocus();
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
            ImGui::EndTable();
        }
        if (e->texture >= 0 && e->texture < gTextureCount)
            ImGui::Image(Engine_TexID(e->texture), ImVec2(48, 48));
    }

    // ---- Behaviour ----
    if (ImGui::CollapsingHeader("Spin Behaviour", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (L09_BeginProps("##spin"))
        {
            L09_PropRow("Enabled"); ImGui::Checkbox("##spinOn", &e->spin);
            L09_PropRow("Speed");
            ImGui::BeginDisabled(!e->spin);
            ImGui::DragFloat("##spinSpd", &e->spinSpeed, 1.0f, -720.0f, 720.0f, "%.0f deg/s");
            ImGui::EndDisabled();
            ImGui::EndTable();
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    if (ImGui::Button("Duplicate")) L09_ed.pendingDuplicate = idx;
    ImGui::SameLine();
    if (ImGui::Button("Delete"))    L09_ed.pendingDelete = idx;

    ImGui::PopID();
    ImGui::End();
}

// ======================================================================================
//  Deferred actions: run once per frame, AFTER every window has finished iterating
// ======================================================================================
static void L09_ApplyPendingActions(void)
{
    if (L09_ValidIndex(L09_ed.pendingDuplicate))
    {
        int n = Engine_DuplicateEntity(L09_ed.pendingDuplicate);
        if (n >= 0) { gSelectedEntity = n; Engine_Log("Duplicated -> %s", gEntities[n].name); }
    }
    if (L09_ValidIndex(L09_ed.pendingDelete))
    {
        Engine_Log("Deleted %s", gEntities[L09_ed.pendingDelete].name);
        Engine_RemoveEntity(L09_ed.pendingDelete);   // shifts later entities down, fixes gSelectedEntity
        L09_ed.renameIndex = -1;                     // indices changed: cancel any rename
    }
    L09_ed.pendingDuplicate = -1;
    L09_ed.pendingDelete    = -1;
}

// ======================================================================================
//  Lesson hooks
// ======================================================================================
void Lesson_Init(void)
{
    Engine_Log("Lesson 9 started - Hierarchy + Inspector");
}

void Lesson_UI(void)
{
    // Did someone else (the 3D view picking) change the selection since last frame?
    if (gSelectedEntity != L09_ed.prevSelection && L09_ValidIndex(gSelectedEntity))
        L09_ed.scrollToSelection = true;

    L09_Hierarchy();
    L09_Inspector();
    L09_ApplyPendingActions();

    L09_ed.prevSelection = gSelectedEntity;          // remember for next frame
}
