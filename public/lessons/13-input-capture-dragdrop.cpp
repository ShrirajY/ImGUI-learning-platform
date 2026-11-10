// Lesson 13 - Input capture & drag and drop
// Three windows:
//   "Asset Palette" - mesh buttons and color swatches you can DRAG
//   "Scene"         - entity list that ACCEPTS meshes/colors and can be drag-reordered,
//                     plus a "Spawn here" drop zone and a touch-pad made with InvisibleButton
//   "Input State"   - live readout of what ImGui and the engine think the mouse/keyboard are doing
// Build & Run, then drag things around. Watch WantCaptureMouse flip as you hover windows.

// Payload type names: any string up to 32 chars. Names starting with '_' are reserved by ImGui.
#define L13_PAYLOAD_MESH   "MESH_TYPE"       // int: a MeshType value
#define L13_PAYLOAD_ENTITY "ENTITY_INDEX"    // int: index into gEntities[]

// A reorder request recorded while we iterate the list and applied afterwards.
// (Never shuffle an array while you are still looping over it to draw it.)
static int  L13_moveFrom = -1, L13_moveTo = -1;
static char L13_lastEvent[128] = "(nothing yet)";

// Named colour swatches shown in the palette
struct L13_Swatch { const char *name; float rgba[4]; };
static L13_Swatch L13_swatches[] = {
    { "Red",    { 0.90f, 0.25f, 0.25f, 1.0f } },
    { "Green",  { 0.30f, 0.85f, 0.40f, 1.0f } },
    { "Blue",   { 0.25f, 0.55f, 1.00f, 1.0f } },
    { "Gold",   { 1.00f, 0.80f, 0.20f, 1.0f } },
    { "White",  { 0.95f, 0.95f, 0.95f, 1.0f } },
    { "Ghost",  { 0.70f, 0.80f, 1.00f, 0.35f } },   // alpha < 1 -> engine draws it blended
};
static float L13_customColor[4] = { 0.8f, 0.3f, 0.9f, 1.0f };

static void L13_SetEvent(const char *fmt, ...)
{
    va_list a; va_start(a, fmt);
    _vsnprintf(L13_lastEvent, sizeof(L13_lastEvent) - 1, fmt, a);
    L13_lastEvent[sizeof(L13_lastEvent) - 1] = 0;
    va_end(a);
    Engine_Log("%s", L13_lastEvent);
}

// Move entity 'from' so it ends up at index 'to', shifting the others. Keeps the selection pointing
// at the same entity.
static void L13_MoveEntity(int from, int to)
{
    if (from == to || from < 0 || to < 0 || from >= gEntityCount || to >= gEntityCount) return;
    Entity tmp = gEntities[from];
    if (from < to) for (int i = from; i < to; i++) gEntities[i] = gEntities[i + 1];
    else           for (int i = from; i > to; i--) gEntities[i] = gEntities[i - 1];
    gEntities[to] = tmp;

    if (gSelectedEntity == from) gSelectedEntity = to;
    else if (from < to && gSelectedEntity > from && gSelectedEntity <= to) gSelectedEntity--;
    else if (from > to && gSelectedEntity >= to && gSelectedEntity < from) gSelectedEntity++;
}

// Every drop target in this lesson accepts the same payloads, so one helper handles them.
// Call it right after the item you want to be the target. 'entity' = -1 means "no entity".
static void L13_AcceptColorAndMesh(int entity)
{
    if (!ImGui::BeginDragDropTarget()) return;

    if (const ImGuiPayload *p = ImGui::AcceptDragDropPayload(L13_PAYLOAD_MESH))
    {
        int mesh = *(const int *)p->Data;       // the payload was copied by ImGui; read it back
        gEntities[entity].mesh = mesh;
        L13_SetEvent("'%s' mesh -> %s", gEntities[entity].name, gMeshNames[mesh]);
    }
    // ColorButton / ColorEdit are built-in drag SOURCES of these two standard types
    if (const ImGuiPayload *p = ImGui::AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_3F))
    {
        memcpy(gEntities[entity].color, p->Data, sizeof(float) * 3);
        gEntities[entity].color[3] = 1.0f;
        L13_SetEvent("'%s' got an RGB colour", gEntities[entity].name);
    }
    if (const ImGuiPayload *p = ImGui::AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_4F))
    {
        memcpy(gEntities[entity].color, p->Data, sizeof(float) * 4);
        L13_SetEvent("'%s' got an RGBA colour (a=%.2f)", gEntities[entity].name, gEntities[entity].color[3]);
    }
    ImGui::EndDragDropTarget();
}

// --------------------------------------------------------------------------------------
//  Window 1: things you can drag
// --------------------------------------------------------------------------------------
static void L13_AssetPalette(void)
{
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_FirstUseEver);
    ImGui::Begin("Asset Palette");

    ImGui::SeparatorText("Meshes (drag me)");
    for (int m = 0; m < MESH_COUNT; m++)
    {
        if (m > 0) ImGui::SameLine();
        ImGui::Button(gMeshNames[m], ImVec2(52, 40));

        // A drag SOURCE must be submitted right after the item it belongs to.
        if (ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload(L13_PAYLOAD_MESH, &m, sizeof(int));   // data is COPIED
            ImGui::Text("Mesh: %s", gMeshNames[m]);                       // tooltip preview
            ImGui::TextDisabled("Drop on an entity or on 'Spawn here'");
            ImGui::EndDragDropSource();
        }
        ImGui::SetItemTooltip("Drag %s onto the Scene window", gMeshNames[m]);
    }

    ImGui::SeparatorText("Colours (drag me)");
    for (int i = 0; i < IM_ARRAYSIZE(L13_swatches); i++)
    {
        if (i > 0) ImGui::SameLine();
        const float *c = L13_swatches[i].rgba;
        // ColorButton is a drag source of "_COL4F" automatically (or "_COL3F" with NoAlpha).
        ImGui::ColorButton(L13_swatches[i].name, ImVec4(c[0], c[1], c[2], c[3]),
                           ImGuiColorEditFlags_AlphaPreviewHalf, ImVec2(34, 34));
    }
    // ColorEdit4 is both a drag source (its swatch) AND a drop target. Try dropping a swatch on it.
    ImGui::ColorEdit4("Custom", L13_customColor, ImGuiColorEditFlags_AlphaPreviewHalf);

    ImGui::Spacing();
    ImGui::TextWrapped("Tip: drag an entity's colour square in the Scene window back onto 'Custom' to copy it.");
    ImGui::End();
}

// --------------------------------------------------------------------------------------
//  Window 2: things you can drop onto
// --------------------------------------------------------------------------------------
static void L13_SceneWindow(void)
{
    ImGui::SetNextWindowPos(ImVec2(20, 335), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 480), ImGuiCond_FirstUseEver);
    ImGui::Begin("Scene");

    ImGui::TextDisabled("Drag rows to reorder. Double-click = focus.");
    for (int i = 0; i < gEntityCount; i++)
    {
        Entity *e = &gEntities[i];
        ImGui::PushID(i);

        // A small colour square per row: drag source (built-in) + drop target.
        ImGui::ColorButton("##col", ImVec4(e->color[0], e->color[1], e->color[2], e->color[3]),
                           ImGuiColorEditFlags_AlphaPreviewHalf, ImVec2(18, 18));
        L13_AcceptColorAndMesh(i);
        ImGui::SameLine();

        char label[64];
        _snprintf(label, sizeof(label) - 1, "%s  [%s]", e->name, gMeshNames[e->mesh]);
        label[sizeof(label) - 1] = 0;
        if (ImGui::Selectable(label, gSelectedEntity == i))
            gSelectedEntity = i;

        // Item queries refer to the LAST submitted item (the Selectable)
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            Engine_FocusEntity(i);
            L13_SetEvent("Focused '%s'", e->name);
        }

        // Each row is a drag source of ENTITY_INDEX (for reordering) ...
        if (ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload(L13_PAYLOAD_ENTITY, &i, sizeof(int));
            ImGui::Text("Move '%s'", e->name);
            ImGui::EndDragDropSource();
        }
        // ... and a drop target for meshes, colours AND other rows.
        L13_AcceptColorAndMesh(i);
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *p = ImGui::AcceptDragDropPayload(L13_PAYLOAD_ENTITY))
            {
                L13_moveFrom = *(const int *)p->Data;   // defer: don't reorder mid-loop
                L13_moveTo   = i;
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::PopID();
    }

    // Apply the deferred reorder now that the loop is done
    if (L13_moveFrom >= 0)
    {
        L13_SetEvent("Moved '%s' from #%d to #%d", gEntities[L13_moveFrom].name, L13_moveFrom, L13_moveTo);
        L13_MoveEntity(L13_moveFrom, L13_moveTo);
        L13_moveFrom = L13_moveTo = -1;
    }

    // ---- "Spawn here" drop zone: a big button that only reacts to drops ----
    ImGui::Spacing();
    ImGui::SeparatorText("Spawn zone");
    const ImGuiPayload *active = ImGui::GetDragDropPayload();          // what is being dragged right now?
    bool meshInFlight = active && active->IsDataType(L13_PAYLOAD_MESH);
    if (meshInFlight) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.3f, 1.0f));
    ImGui::Button(meshInFlight ? "Drop to spawn!" : "Spawn here (drop a mesh)", ImVec2(-FLT_MIN, 50));
    if (meshInFlight) ImGui::PopStyleColor();
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload *p = ImGui::AcceptDragDropPayload(L13_PAYLOAD_MESH))
        {
            int mesh = *(const int *)p->Data;
            char name[32];
            _snprintf(name, sizeof(name) - 1, "%s %d", gMeshNames[mesh], gEntityCount);
            name[31] = 0;
            int idx = Engine_AddEntity(name, mesh);
            if (idx >= 0)
            {
                // place it in front of the camera target, spread out a little
                gEntities[idx].position[0] = gCamera.target[0] + (float)((idx % 5) - 2);
                gEntities[idx].position[1] = 0.5f;
                gEntities[idx].position[2] = gCamera.target[2] + 2.0f;
                gSelectedEntity = idx;
                L13_SetEvent("Spawned '%s'", name);
            }
        }
        ImGui::EndDragDropTarget();
    }

    // ---- Touch pad: InvisibleButton = an empty interactive rectangle ----
    ImGui::SeparatorText("Touch pad (drag = rotate selected)");
    ImVec2 padPos  = ImGui::GetCursorScreenPos();
    ImVec2 padSize = ImVec2(ImGui::GetContentRegionAvail().x, 70);
    ImGui::InvisibleButton("##pad", padSize, ImGuiButtonFlags_MouseButtonLeft);
    bool padHovered = ImGui::IsItemHovered();
    bool padActive  = ImGui::IsItemActive();        // true while the button is held down
    if (padActive && gSelectedEntity >= 0)
        gEntities[gSelectedEntity].rotation[1] += ImGui::GetIO().MouseDelta.x;   // degrees per pixel
    // draw something so the invisible button is visible
    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImU32 padCol = padActive ? IM_COL32(80, 160, 255, 120) : padHovered ? IM_COL32(80, 160, 255, 60) : IM_COL32(255, 255, 255, 20);
    dl->AddRectFilled(padPos, ImVec2(padPos.x + padSize.x, padPos.y + padSize.y), padCol, 6.0f);
    dl->AddText(ImVec2(padPos.x + 8, padPos.y + 8), IM_COL32(220, 220, 230, 200),
                gSelectedEntity >= 0 ? "drag left/right" : "select an entity first");

    ImGui::End();
}

// --------------------------------------------------------------------------------------
//  Window 3: input readout + hotkeys
// --------------------------------------------------------------------------------------
static void L13_InputState(void)
{
    ImGuiIO &io = ImGui::GetIO();

    // Hotkeys for the 3D view: only when ImGui does NOT want the keyboard (e.g. no text field active)
    if (!io.WantCaptureKeyboard)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_F, false) && gSelectedEntity >= 0)
        { Engine_FocusEntity(gSelectedEntity); L13_SetEvent("[F] focus '%s'", gEntities[gSelectedEntity].name); }
        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && gSelectedEntity >= 0)
        { L13_SetEvent("[Del] removed '%s'", gEntities[gSelectedEntity].name); Engine_RemoveEntity(gSelectedEntity); }
        if (ImGui::IsKeyPressed(ImGuiKey_H, false))
        { gShowHelpOverlay = !gShowHelpOverlay; L13_SetEvent("[H] help overlay %s", gShowHelpOverlay ? "on" : "off"); }
    }

    ImGui::SetNextWindowPos(ImVec2(1180, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 430), ImGuiCond_FirstUseEver);
    ImGui::Begin("Input State");

    // IsWindowHovered/Focused refer to the CURRENT window (the one between Begin/End)
    ImGui::Text("This window: hovered=%d focused=%d", ImGui::IsWindowHovered(), ImGui::IsWindowFocused());

    ImGui::SeparatorText("Who owns the input?");
    ImVec4 on = ImVec4(1.0f, 0.55f, 0.3f, 1.0f), off = ImVec4(0.4f, 0.9f, 0.5f, 1.0f);
    ImGui::TextColored(io.WantCaptureMouse ? on : off, "WantCaptureMouse    = %s", io.WantCaptureMouse ? "true  (ImGui)" : "false (engine)");
    ImGui::TextColored(io.WantCaptureKeyboard ? on : off, "WantCaptureKeyboard = %s", io.WantCaptureKeyboard ? "true  (ImGui)" : "false (engine)");
    ImGui::Text("WantTextInput       = %s", io.WantTextInput ? "true (typing)" : "false");
    ImGui::TextDisabled("Keyboard nav is on: a FOCUSED window owns the keyboard.\nClick empty 3D space to give it back.");
    ImGui::Text("Any window hovered: %s", ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) ? "yes" : "no");
    ImGui::Text("Any item active:    %s", ImGui::IsAnyItemActive() ? "yes" : "no");

    ImGui::SeparatorText("Mouse");
    ImGui::Text("Pos   (%.0f, %.0f)", io.MousePos.x, io.MousePos.y);
    ImGui::Text("Delta (%.1f, %.1f)", io.MouseDelta.x, io.MouseDelta.y);
    ImGui::Text("Down  L:%d R:%d M:%d   Wheel %.1f", io.MouseDown[0], io.MouseDown[1], io.MouseDown[2], io.MouseWheel);

    ImGui::SeparatorText("Drag & drop");
    if (const ImGuiPayload *p = ImGui::GetDragDropPayload())
        ImGui::Text("Dragging payload \"%s\" (%d bytes)", p->DataType, p->DataSize);
    else
        ImGui::TextDisabled("No drag in progress");
    ImGui::TextWrapped("Last event: %s", L13_lastEvent);

    ImGui::SeparatorText("Item queries demo");
    ImGui::Button("Hover / hold / click me");
    bool hov = ImGui::IsItemHovered(), act = ImGui::IsItemActive(), clk = ImGui::IsItemClicked(ImGuiMouseButton_Right);
    if (clk) L13_SetEvent("Right-clicked the demo button");
    ImGui::Text("hovered=%d active=%d (right-click logs)", hov, act);

    ImGui::SeparatorText("Engine hotkeys (when keyboard is free)");
    ImGui::BulletText("F = focus selected, Del = delete, H = help overlay");
    ImGui::BulletText("Click the 3D view first so no window is focused");
    static char scratch[64] = "";
    ImGui::InputText("Type here", scratch, sizeof(scratch));
    ImGui::TextDisabled("While this window is focused, W/A/S/D and hotkeys are ignored.");

    ImGui::End();
}

void Lesson_Init(void)
{
    Engine_Log("Lesson 13: drag meshes/colours from the palette onto the Scene window");
}

void Lesson_UI(void)
{
    L13_AssetPalette();
    L13_SceneWindow();
    L13_InputState();
    // After this returns, the engine's UpdateCamera() checks io.WantCaptureMouse/Keyboard
    // and only orbits/picks/moves when ImGui doesn't want the input.
}
