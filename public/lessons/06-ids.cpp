// Lesson 6 - The ID stack: an entity list where every row has the SAME button labels
// "Select" and "Hide" appear once per entity. Without PushID they would all share one ID
// and clicking any of them would only ever affect the first row. PushID(i) fixes that.

void Lesson_Init(void)
{
    Engine_Log("Lesson 6 - IDs. Open the ID Stack Tool and hover the buttons.");
}

void Lesson_UI(void)
{
    static bool showIdTool  = true;
    static bool showMetrics = false;

    // =================================================================================
    // Window 1: Scene list (the FIXED version - compare with the broken one in the lesson)
    // =================================================================================
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(380, 420), ImGuiCond_FirstUseEver);
    ImGui::Begin("Scene List");

    ImGui::TextDisabled("%d entities", gEntityCount);
    ImGui::Separator();

    for (int i = 0; i < gEntityCount; i++)
    {
        Entity *e = &gEntities[i];

        // Every widget ID below is now hash("Scene List" + i + label) -> unique per row.
        // Using the index is fine here. Lesson text explains when to prefer a pointer/stable id.
        ImGui::PushID(i);

        // Checkbox with a HIDDEN label: "##vis" is the ID, nothing is displayed.
        ImGui::Checkbox("##vis", &e->visible);
        ImGui::SetItemTooltip("Visible");
        ImGui::SameLine();

        // Show the name as text (selected one highlighted)
        if (i == gSelectedEntity)
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "%-16s", e->name);
        else
            ImGui::Text("%-16s", e->name);
        ImGui::SameLine(220);

        // Same label on every row - safe because of PushID(i)
        if (ImGui::SmallButton("Select"))
        {
            gSelectedEntity = i;
            Engine_Log("Selected '%s' from the list", e->name);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton(e->visible ? "Hide" : "Show"))   // label changes, ID changes too!
            e->visible = !e->visible;

        ImGui::PopID();                                         // ALWAYS balance PushID
    }

    ImGui::Separator();
    if (ImGui::Button("Add cube"))
        Engine_AddEntity("Cube", MESH_CUBE);
    ImGui::SameLine();
    if (ImGui::Button("Show all"))
        for (int i = 0; i < gEntityCount; i++) gEntities[i].visible = true;

    // ---- "###" : the visible label changes, the ID stays the same ----
    ImGui::SeparatorText("### stable IDs");
    static int clicks = 0;
    char label[64];
    snprintf(label, sizeof(label), "Clicked %d times###clickCounter", clicks);
    if (ImGui::Button(label))      // ID = hash("clickCounter") no matter what the text says
        clicks++;

    // GetID: what ID would a label get at this point of the stack?
    ImGui::Text("GetID(\"Select\") here = 0x%08X", ImGui::GetID("Select"));
    ImGui::PushID(0);
    ImGui::Text("inside PushID(0)     = 0x%08X", ImGui::GetID("Select"));
    ImGui::PopID();

    ImGui::Separator();
    ImGui::Checkbox("ID Stack Tool", &showIdTool);
    ImGui::SameLine();
    ImGui::Checkbox("Metrics/Debugger", &showMetrics);
    ImGui::End();

    // A window title is part of the ID too: same title -> same window.
    // "###" works for windows as well: the title shows the entity name, the ID stays "Inspector6".
    char title[64];
    snprintf(title, sizeof(title), "Selected: %s###Inspector6",
             (gSelectedEntity >= 0 && gSelectedEntity < gEntityCount) ? gEntities[gSelectedEntity].name : "none");
    ImGui::SetNextWindowPos(ImVec2(420, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);
    ImGui::Begin(title);
    if (gSelectedEntity >= 0 && gSelectedEntity < gEntityCount)
    {
        Entity *e = &gEntities[gSelectedEntity];
        ImGui::DragFloat3("Position", e->position, 0.05f);
        ImGui::TextWrapped("Notice: the window keeps its position when the title changes, "
                           "because the part after ### is its real ID.");
    }
    else
        ImGui::TextDisabled("Select something in the Scene List.");
    ImGui::End();

    if (showIdTool)  ImGui::ShowIDStackToolWindow(&showIdTool);    // hover any widget to see its ID path
    if (showMetrics) ImGui::ShowMetricsWindow(&showMetrics);
}
