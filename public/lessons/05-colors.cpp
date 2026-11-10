// Lesson 5 - Colors: an Environment panel + a Material palette
// Every ColorEdit/ColorPicker edits a float array IN PLACE (0..1 floats).
// Right-click any color widget for its options menu (RGB/HSV/Hex, 0..255 vs 0..1).

// A palette of preset colors for the "Material" window. ImVec4 = r,g,b,a floats.
static const ImVec4 L5_palette[] =
{
    ImVec4(0.90f, 0.20f, 0.20f, 1.0f), ImVec4(0.95f, 0.55f, 0.20f, 1.0f), ImVec4(0.95f, 0.85f, 0.30f, 1.0f),
    ImVec4(0.45f, 0.85f, 0.35f, 1.0f), ImVec4(0.25f, 0.60f, 1.00f, 1.0f), ImVec4(0.55f, 0.35f, 0.95f, 1.0f),
    ImVec4(0.95f, 0.45f, 0.75f, 1.0f), ImVec4(0.95f, 0.95f, 0.95f, 1.0f), ImVec4(0.50f, 0.50f, 0.50f, 1.0f),
    ImVec4(0.15f, 0.15f, 0.15f, 1.0f), ImVec4(0.55f, 0.40f, 0.25f, 1.0f), ImVec4(0.30f, 0.80f, 0.80f, 1.0f),
};
static const int L5_paletteCount = (int)(sizeof(L5_palette) / sizeof(L5_palette[0]));

void Lesson_Init(void)
{
    if (gSelectedEntity < 0 && gEntityCount > 2)
        gSelectedEntity = 2;                       // pre-select the Planet
    Engine_Log("Lesson 5 - colors");
}

void Lesson_UI(void)
{
    // =================================================================================
    // Window 1: Environment (clear color + light)
    // =================================================================================
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360, 0), ImGuiCond_FirstUseEver);
    ImGui::Begin("Environment");

    ImGui::SeparatorText("Sky / clear color");
    // gClearColor is float[4]. ColorEdit3 only touches the first 3 floats.
    ImGui::ColorEdit3("Clear color", gClearColor);

    ImGui::SeparatorText("Directional light");
    ImGui::Checkbox("Light enabled", &gLight.enabled);
    // Float: show 0.000..1.000 instead of 0..255.  HDR: allow values above 1.0 (overbright light).
    ImGui::ColorEdit3("Light color", gLight.color, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
    // DisplayHSV: same data (still RGB in memory), edited as Hue/Saturation/Value.
    ImGui::ColorEdit3("Ambient", gLight.ambient, ImGuiColorEditFlags_DisplayHSV);
    ImGui::DragFloat3("Direction", gLight.direction, 0.01f, -1.0f, 1.0f, "%.2f");

    // A full picker with a hue wheel, embedded right in the panel.
    static bool showWheel = false;
    ImGui::Checkbox("Show light color wheel", &showWheel);
    if (showWheel)
        ImGui::ColorPicker3("##lightWheel", gLight.color,
                            ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_NoSidePreview);

    // Presets: a time-of-day "mood" with one click each
    ImGui::SeparatorText("Presets");
    if (ImGui::Button("Day"))
    {
        float sky[3] = { 0.45f, 0.62f, 0.85f }, sun[3] = { 1.0f, 0.97f, 0.9f }, amb[3] = { 0.30f, 0.32f, 0.38f };
        for (int k = 0; k < 3; k++) { gClearColor[k] = sky[k]; gLight.color[k] = sun[k]; gLight.ambient[k] = amb[k]; }
    }
    ImGui::SameLine();
    if (ImGui::Button("Sunset"))
    {
        float sky[3] = { 0.55f, 0.30f, 0.25f }, sun[3] = { 1.0f, 0.55f, 0.25f }, amb[3] = { 0.25f, 0.15f, 0.20f };
        for (int k = 0; k < 3; k++) { gClearColor[k] = sky[k]; gLight.color[k] = sun[k]; gLight.ambient[k] = amb[k]; }
    }
    ImGui::SameLine();
    if (ImGui::Button("Night"))
    {
        float sky[3] = { 0.02f, 0.03f, 0.07f }, sun[3] = { 0.35f, 0.45f, 0.80f }, amb[3] = { 0.05f, 0.06f, 0.10f };
        for (int k = 0; k < 3; k++) { gClearColor[k] = sky[k]; gLight.color[k] = sun[k]; gLight.ambient[k] = amb[k]; }
    }
    ImGui::End();

    // =================================================================================
    // Window 2: Material of the selected entity
    // =================================================================================
    ImGui::SetNextWindowPos(ImVec2(20, 400), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360, 0), ImGuiCond_FirstUseEver);
    ImGui::Begin("Material");

    if (gSelectedEntity < 0 || gSelectedEntity >= gEntityCount)
    {
        ImGui::TextDisabled("Click an object in the 3D view to edit its color.");
    }
    else
    {
        Entity *e = &gEntities[gSelectedEntity];
        ImGui::Text("Entity: %s", e->name);

        // ColorEdit4 + AlphaBar: the 4th float is alpha. Alpha < 1 renders blended in the engine.
        ImGui::ColorEdit4("Albedo", e->color, ImGuiColorEditFlags_AlphaBar);

        // ---- The palette: ColorButton returns true when clicked ----
        ImGui::SeparatorText("Palette (click to apply)");
        for (int i = 0; i < L5_paletteCount; i++)
        {
            // Each button needs a unique ID: the "desc_id" string doubles as the tooltip title.
            char id[32];
            snprintf(id, sizeof(id), "Swatch %d", i);
            if (ImGui::ColorButton(id, L5_palette[i], ImGuiColorEditFlags_NoAlpha, ImVec2(26, 26)))
            {
                e->color[0] = L5_palette[i].x;     // ImVec4 -> float[4]
                e->color[1] = L5_palette[i].y;
                e->color[2] = L5_palette[i].z;     // keep the entity's own alpha
                Engine_Log("'%s' color -> swatch %d", e->name, i);
            }
            if ((i + 1) % 6 != 0)
                ImGui::SameLine();                 // 6 swatches per row
        }

        // ---- Compact color square only: NoInputs + NoLabel ----
        ImGui::SeparatorText("Compact");
        ImGui::ColorEdit4("##compact", e->color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
        ImGui::SameLine();
        ImGui::Text("<- just the square (click it)");

        // ---- Conversions: float[4] -> ImU32 (packed 0xAABBGGRR) and HSV ----
        ImGui::SeparatorText("Conversions");
        ImVec4 c4 = ImVec4(e->color[0], e->color[1], e->color[2], e->color[3]);
        ImU32  packed = ImGui::ColorConvertFloat4ToU32(c4);
        float h, s, v;
        ImGui::ColorConvertRGBtoHSV(e->color[0], e->color[1], e->color[2], h, s, v);
        ImGui::Text("ImU32 : 0x%08X", packed);
        ImGui::Text("HSV   : %.2f  %.2f  %.2f", h, s, v);
        ImGui::Text("Hex   : #%02X%02X%02X",
                    (int)(e->color[0] * 255.0f + 0.5f), (int)(e->color[1] * 255.0f + 0.5f), (int)(e->color[2] * 255.0f + 0.5f));

        if (ImGui::Button("Shift hue +30 deg"))
        {
            h += 30.0f / 360.0f;
            if (h > 1.0f) h -= 1.0f;
            ImGui::ColorConvertHSVtoRGB(h, s, v, e->color[0], e->color[1], e->color[2]);
        }
        ImGui::SameLine();
        if (ImGui::Button("Random color"))
        {
            float rh = (float)(rand() % 360) / 360.0f;
            ImGui::ColorConvertHSVtoRGB(rh, 0.65f, 0.95f, e->color[0], e->color[1], e->color[2]);
        }

        // IM_COL32 builds an ImU32 at compile time - used by draw lists (lesson 15)
        ImDrawList *dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        dl->AddRectFilled(p, ImVec2(p.x + 120, p.y + 12), packed);
        dl->AddRect(p, ImVec2(p.x + 120, p.y + 12), IM_COL32(255, 255, 255, 120));
        ImGui::Dummy(ImVec2(120, 14));             // reserve the space we drew into
    }
    ImGui::End();
}
