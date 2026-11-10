// Lesson 4 - Input widgets: a Transform inspector for the selected entity
// Click an object in the 3D view (LMB) to select it, then edit it here.
// Tips: drag on a number to change it, Ctrl+Click (or double-click) to type a value.

// ---- A tiny one-step undo, to show IsItemActivated / IsItemDeactivatedAfterEdit ----
// Prefixed with 'L4_' so the names can never clash with anything in OGL.c.
static Entity L4_undoBefore;          // copy of the entity when an edit STARTED
static int    L4_undoIndex = -1;      // which entity that copy belongs to
static int    L4_editCount = 0;       // finished edits (one per drag, not one per frame!)

// Call right after a widget: remembers "before" on press, commits on release.
static void L4_TrackUndo(Entity *e, int index, const char *what)
{
    if (ImGui::IsItemActivated())                 // mouse went down / typing started
        L4_undoBefore = *e;                       // snapshot BEFORE any change
    if (ImGui::IsItemDeactivatedAfterEdit())      // released AND the value changed
    {
        L4_undoIndex = index;
        L4_editCount++;
        Engine_Log("Edit #%d: %s of '%s' (undo available)", L4_editCount, what, e->name);
    }
}

void Lesson_Init(void)
{
    if (gSelectedEntity < 0 && gEntityCount > 1)
        gSelectedEntity = 1;                      // pre-select the Crate so the panel isn't empty
    Engine_Log("Lesson 4 - select an entity and edit its transform");
}

void Lesson_UI(void)
{
    // =================================================================================
    // Window 1: Inspector (Transform of the selected entity)
    // =================================================================================
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360, 0), ImGuiCond_FirstUseEver);
    ImGui::Begin("Inspector");

    if (gSelectedEntity < 0 || gSelectedEntity >= gEntityCount)
    {
        ImGui::TextDisabled("Nothing selected. Click an object in the 3D view.");
    }
    else
    {
        int     idx = gSelectedEntity;
        Entity *e   = &gEntities[idx];

        // ---- Name: InputText edits a char buffer IN PLACE. Always pass sizeof(buffer). ----
        ImGui::InputText("Name", e->name, sizeof(e->name));
        L4_TrackUndo(e, idx, "rename");

        ImGui::TextDisabled("Mesh: %s", gMeshNames[e->mesh]);

        // ---- Transform: DragFloat3 edits float[3] directly ----
        ImGui::SeparatorText("Transform");
        // speed 0.05 = 20 pixels of mouse movement per 1 unit. min == max == 0 -> no limits.
        ImGui::DragFloat3("Position", e->position, 0.05f, 0.0f, 0.0f, "%.2f");
        L4_TrackUndo(e, idx, "move");

        // Format strings can add units. %.1f keeps the display tidy.
        ImGui::DragFloat3("Rotation", e->rotation, 0.5f, -360.0f, 360.0f, "%.1f deg");
        L4_TrackUndo(e, idx, "rotate");

        // AlwaysClamp: even Ctrl+Click typing cannot produce a zero/negative scale.
        ImGui::DragFloat3("Scale", e->scale, 0.01f, 0.01f, 100.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        L4_TrackUndo(e, idx, "scale");

        // A uniform scale helper: our own temp value, applied to all 3 axes on change
        float uniform = e->scale[0];
        if (ImGui::SliderFloat("Uniform scale", &uniform, 0.1f, 10.0f, "%.2fx", ImGuiSliderFlags_Logarithmic))
            e->scale[0] = e->scale[1] = e->scale[2] = uniform;
        L4_TrackUndo(e, idx, "uniform scale");

        ImGui::SeparatorText("Animation");
        ImGui::Checkbox("Spin", &e->spin);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-FLT_MIN);        // fill the rest of the line
        ImGui::SliderFloat("##spinSpeed", &e->spinSpeed, -360.0f, 360.0f, "%.0f deg/s");

        // Texture index: InputInt has +/- step buttons. Clamp it ourselves afterwards.
        if (ImGui::InputInt("Texture", &e->texture))
        {
            if (e->texture < -1)             e->texture = -1;
            if (e->texture >= gTextureCount) e->texture = gTextureCount - 1;
        }
        ImGui::TextDisabled("%s", e->texture >= 0 ? gTextures[e->texture].name : "(none)");

        // ---- Undo button: restores the snapshot taken when the last edit started ----
        ImGui::Separator();
        bool canUndo = (L4_undoIndex == idx);
        ImGui::BeginDisabled(!canUndo);
        if (ImGui::Button("Undo last edit"))
        {
            *e = L4_undoBefore;
            L4_undoIndex = -1;
            Engine_Log("Undo on '%s'", e->name);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("%d edits so far", L4_editCount);

        if (ImGui::Button("Reset transform"))
        {
            for (int k = 0; k < 3; k++) { e->rotation[k] = 0.0f; e->scale[k] = 1.0f; }
        }
        ImGui::SameLine();
        if (ImGui::Button("Focus camera"))
            Engine_FocusEntity(idx);
    }
    ImGui::End();

    // =================================================================================
    // Window 2: Camera settings
    // =================================================================================
    ImGui::SetNextWindowPos(ImVec2(20, 440), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360, 0), ImGuiCond_FirstUseEver);
    ImGui::Begin("Camera");

    ImGui::SliderFloat("FOV", &gCamera.fov, 20.0f, 110.0f, "%.0f deg");
    // Logarithmic: fine control when close, big steps when far away
    ImGui::SliderFloat("Distance", &gCamera.distance, 1.0f, 200.0f, "%.1f m", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Yaw", &gCamera.yaw, -180.0f, 180.0f, "%.0f deg");
    ImGui::SliderFloat("Pitch", &gCamera.pitch, -89.0f, 89.0f, "%.0f deg", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat3("Target", gCamera.target, 0.05f, 0.0f, 0.0f, "%.2f");
    ImGui::InputFloat("Move speed", &gCamera.moveSpeed, 0.5f, 5.0f, "%.1f");

    // Time scale with DragFloat: limits + clamp
    ImGui::DragFloat("Time scale", &gTimeScale, 0.01f, 0.0f, 5.0f, "%.2fx", ImGuiSliderFlags_AlwaysClamp);

    if (ImGui::Button("Reset camera"))
    {
        gCamera.fov = 45.0f; gCamera.distance = 12.0f; gCamera.yaw = 35.0f; gCamera.pitch = 25.0f;
    }
    ImGui::End();
}
