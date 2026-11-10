// Lesson 7 - Layout: lines, groups, children, tabs and a property grid
// Two windows:
//   "Layout Basics" (left)  - a playground for SameLine / Indent / Group / Dummy / widths
//   "Inspector"     (right) - a real engine-style panel: tabs + collapsing headers + property grid
// Change something, then Build & Run (Ctrl+Enter).

// --------------------------------------------------------------------------------------
//  Property grid helpers (label on the left, widget on the right)
//  Prefix "L07_" keeps our names away from the engine's own static functions.
// --------------------------------------------------------------------------------------

// Opens a 2-column table. Returns false if the table is clipped: then skip the rows
// and do NOT call EndTable() (same rule as BeginTabBar / BeginCombo).
static bool L07_BeginProps(const char *id)
{
    if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable))
        return false;
    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 110.0f); // label column
    ImGui::TableSetupColumn("Value",    ImGuiTableColumnFlags_WidthStretch);        // widget column
    return true;
}

// Starts one row: writes the label in column 0, moves to column 1 and makes the next
// widget fill that column. Call the widget right after, with a "##hidden" label.
static void L07_PropRow(const char *label)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();     // text baseline = framed widget baseline
    ImGui::TextUnformatted(label);
    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(-FLT_MIN);    // -FLT_MIN = stretch to the right edge
}

// --------------------------------------------------------------------------------------
//  Window 1: layout playground
// --------------------------------------------------------------------------------------
static void L07_LayoutBasics(void)
{
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(380, 560), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Layout Basics"))
    {
        ImGui::End();                       // collapsed: still must call End()
        return;
    }

    // ---- SameLine variants ----
    ImGui::SeparatorText("SameLine");
    ImGui::Button("Play");  ImGui::SameLine();          // default spacing
    ImGui::Button("Pause"); ImGui::SameLine(0, 30);     // exactly 30 px gap
    ImGui::Button("Stop");

    // SameLine(x) = absolute x position -> aligned "columns" without a table
    ImGui::Text("Entities");  ImGui::SameLine(120); ImGui::Text("%d", gEntityCount);
    ImGui::Text("Selected");  ImGui::SameLine(120);
    if (gSelectedEntity >= 0) ImGui::TextUnformatted(gEntities[gSelectedEntity].name);
    else                      ImGui::TextDisabled("none");
    ImGui::Text("Frame");     ImGui::SameLine(120); ImGui::Text("%d", gFrameCount);

    // ---- Indent ----
    ImGui::SeparatorText("Indent");
    ImGui::Text("Level 0");
    ImGui::Indent();
        ImGui::Text("Level 1 (default indent)");
        ImGui::Indent(40.0f);
            ImGui::Text("Level 2 (+40 px)");
        ImGui::Unindent(40.0f);              // must undo with the SAME amount
    ImGui::Unindent();

    // ---- Groups: two blocks side by side ----
    ImGui::SeparatorText("Groups");
    ImGui::BeginGroup();
        if (ImGui::Button("Add cube", ImVec2(110, 0)))   Engine_AddEntity("Cube", MESH_CUBE);
        if (ImGui::Button("Add sphere", ImVec2(110, 0))) Engine_AddEntity("Sphere", MESH_SPHERE);
        if (ImGui::Button("Reset", ImVec2(110, 0)))      Engine_ResetScene();
    ImGui::EndGroup();
    ImGui::SameLine();                       // next to the WHOLE group above
    ImGui::BeginGroup();
        ImGui::Text("Scene");
        ImGui::BulletText("%d entities", gEntityCount);
        ImGui::BulletText("%d textures", gTextureCount);
        ImGui::BulletText("%.2f ms / frame", Engine_AverageFrameTimeMs());
    ImGui::EndGroup();
    if (ImGui::IsItemHovered())              // "item" = the second group
        ImGui::SetTooltip("A group acts like one big item");

    // ---- Dummy as a spacer ----
    ImGui::Dummy(ImVec2(0, 10));

    // ---- Widths ----
    ImGui::SeparatorText("Widths");
    ImGui::SetNextItemWidth(100);
    ImGui::DragFloat("Fixed 100 px", &gTimeScale, 0.01f, 0.0f, 5.0f);

    ImGui::PushItemWidth(-110);              // leave 110 px on the right for labels
    ImGui::SliderFloat("Camera FOV", &gCamera.fov, 20.0f, 120.0f, "%.0f");
    ImGui::DragFloat("Move speed", &gCamera.moveSpeed, 0.1f, 0.5f, 50.0f);
    ImGui::PopItemWidth();

    // Two buttons that exactly share the available width
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float half = (avail.x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    if (ImGui::Button("Grid on/off", ImVec2(half, 0)))      gShowGrid = !gShowGrid;
    ImGui::SameLine();
    if (ImGui::Button("Wireframe on/off", ImVec2(half, 0))) gWireframe = !gWireframe;

    ImGui::TextDisabled("Available region: %.0f x %.0f", avail.x, avail.y);
    ImGui::End();
}

// --------------------------------------------------------------------------------------
//  Window 2: Inspector-style panel
// --------------------------------------------------------------------------------------
static void L07_SceneTab(void)
{
    // Selected entity section
    if (ImGui::CollapsingHeader("Selected Entity", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (gSelectedEntity < 0 || gSelectedEntity >= gEntityCount)
        {
            ImGui::TextDisabled("Click an object in the 3D view (or in the list below).");
        }
        else
        {
            Entity *e = &gEntities[gSelectedEntity];
            if (L07_BeginProps("##entityProps"))
            {
                L07_PropRow("Name");     ImGui::Text("%s", e->name);
                L07_PropRow("Mesh");     ImGui::Text("%s", gMeshNames[e->mesh]);
                L07_PropRow("Position"); ImGui::DragFloat3("##pos", e->position, 0.05f);
                L07_PropRow("Rotation"); ImGui::DragFloat3("##rot", e->rotation, 1.0f, -360.0f, 360.0f, "%.0f");
                L07_PropRow("Scale");    ImGui::DragFloat3("##scl", e->scale, 0.02f, 0.01f, 100.0f);
                L07_PropRow("Color");    ImGui::ColorEdit4("##col", e->color);
                L07_PropRow("Visible");  ImGui::Checkbox("##vis", &e->visible);
                L07_PropRow("Spin");     ImGui::Checkbox("##spin", &e->spin);
                ImGui::EndTable();
            }
        }
    }

    // Entity list in a scrollable, resizable child, grouped by mesh with TreeNodes
    if (ImGui::CollapsingHeader("Entities", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::BeginChild("EntityTree", ImVec2(0, 180), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY))
        {
            for (int m = 0; m < MESH_COUNT; m++)
            {
                // ImGui remembers open/closed per label. SetNextItemOpen(.., FirstUseEver) = default open.
                ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
                if (ImGui::TreeNode(gMeshNames[m]))
                {
                    for (int i = 0; i < gEntityCount; i++)
                    {
                        if (gEntities[i].mesh != m) continue;
                        ImGui::PushID(i);   // names may repeat ("Cube", "Cube") -> unique IDs
                        if (ImGui::Selectable(gEntities[i].name, gSelectedEntity == i))
                            gSelectedEntity = i;
                        ImGui::PopID();
                    }
                    ImGui::TreePop();
                }
            }
        }
        ImGui::EndChild();                  // ALWAYS paired with BeginChild

        // Buttons filling the row under the child
        float w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        if (ImGui::Button("Reset scene", ImVec2(w, 0))) Engine_ResetScene();
        ImGui::SameLine();
        if (ImGui::Button("Clear scene", ImVec2(w, 0))) Engine_ClearScene();
    }
}

static void L07_CameraTab(void)
{
    if (ImGui::CollapsingHeader("Orbit", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (L07_BeginProps("##orbit"))
        {
            L07_PropRow("Target");   ImGui::DragFloat3("##target", gCamera.target, 0.05f);
            L07_PropRow("Yaw");      ImGui::SliderFloat("##yaw", &gCamera.yaw, -180.0f, 180.0f, "%.1f deg");
            L07_PropRow("Pitch");    ImGui::SliderFloat("##pitch", &gCamera.pitch, -89.0f, 89.0f, "%.1f deg");
            L07_PropRow("Distance"); ImGui::DragFloat("##dist", &gCamera.distance, 0.1f, 1.0f, 200.0f, "%.2f");
            ImGui::EndTable();
        }
    }
    if (ImGui::CollapsingHeader("Lens & Movement", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (L07_BeginProps("##lens"))
        {
            L07_PropRow("Field of view"); ImGui::SliderFloat("##fov", &gCamera.fov, 20.0f, 120.0f, "%.0f deg");
            L07_PropRow("Move speed");    ImGui::DragFloat("##speed", &gCamera.moveSpeed, 0.1f, 0.5f, 50.0f);
            L07_PropRow("Input");         ImGui::Checkbox("Enabled##camin", &gCameraInputEnabled);
            ImGui::EndTable();
        }
        // Buttons aligned without a table: SameLine(offset)
        if (ImGui::Button("Front")) { gCamera.yaw = 0;  gCamera.pitch = 10; }
        ImGui::SameLine();
        if (ImGui::Button("Side"))  { gCamera.yaw = 90; gCamera.pitch = 10; }
        ImGui::SameLine();
        if (ImGui::Button("Top"))   { gCamera.pitch = 89; }
        ImGui::SameLine();
        if (ImGui::Button("Focus selected")) Engine_FocusEntity(gSelectedEntity);
    }
}

static void L07_LightingTab(void)
{
    if (L07_BeginProps("##light"))
    {
        L07_PropRow("Enabled");   ImGui::Checkbox("##len", &gLight.enabled);
        L07_PropRow("Direction"); ImGui::SliderFloat3("##ldir", gLight.direction, -1.0f, 1.0f);
        L07_PropRow("Color");     ImGui::ColorEdit3("##lcol", gLight.color);
        L07_PropRow("Ambient");   ImGui::ColorEdit3("##lamb", gLight.ambient);
        ImGui::EndTable();
    }
    ImGui::Spacing();
    ImGui::TextWrapped("Directional light: the vector is the direction the light travels, so (0,-1,0) shines straight down.");
}

static void L07_RenderTab(void)
{
    if (ImGui::CollapsingHeader("Viewport", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (L07_BeginProps("##viewport"))
        {
            L07_PropRow("Background");  ImGui::ColorEdit3("##bg", gClearColor);
            L07_PropRow("Grid");        ImGui::Checkbox("##grid", &gShowGrid);
            L07_PropRow("Wireframe");   ImGui::Checkbox("##wire", &gWireframe);
            L07_PropRow("Outline sel."); ImGui::Checkbox("##outl", &gShowSelection);
            L07_PropRow("Picking");     ImGui::Checkbox("##pick", &gEnablePicking);
            L07_PropRow("Help overlay"); ImGui::Checkbox("##help", &gShowHelpOverlay);
            L07_PropRow("VSync");       ImGui::Checkbox("##vsync", &gVSync);
            ImGui::EndTable();
        }
    }
    if (ImGui::CollapsingHeader("Time"))     // closed by default
    {
        if (L07_BeginProps("##time"))
        {
            L07_PropRow("Paused");     ImGui::Checkbox("##paused", &gPaused);
            L07_PropRow("Time scale"); ImGui::SliderFloat("##ts", &gTimeScale, 0.0f, 4.0f, "%.2fx");
            L07_PropRow("Game time");  ImGui::Text("%.2f s", gTime);
            ImGui::EndTable();
        }
    }
}

static void L07_Inspector(void)
{
    ImGui::SetNextWindowPos(ImVec2(1120, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(410, 780), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Inspector"))
    {
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("InspectorTabs"))
    {
        // Only the selected tab's body runs each frame
        if (ImGui::BeginTabItem("Scene"))    { L07_SceneTab();    ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Camera"))   { L07_CameraTab();   ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Lighting")) { L07_LightingTab(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Render"))   { L07_RenderTab();   ImGui::EndTabItem(); }
        ImGui::EndTabBar();                  // only because BeginTabBar returned true
    }
    ImGui::End();
}

// --------------------------------------------------------------------------------------
//  Lesson hooks
// --------------------------------------------------------------------------------------
void Lesson_Init(void)
{
    gSelectedEntity = 2;                     // start with "Planet" selected so the Inspector has data
    Engine_Log("Lesson 7 started - layout");
}

void Lesson_UI(void)
{
    L07_LayoutBasics();
    L07_Inspector();
}
