// Lesson 8 - Combos, list boxes, filters and huge lists
// Three windows:
//   "Asset Pickers"  - mesh / texture / enum combos for the selected entity
//   "Entity List"    - a filterable list box synced with gSelectedEntity
//   "Asset Browser"  - 10,000 fake assets drawn with ImGuiListClipper
// Change something, then Build & Run (Ctrl+Enter).

// --------------------------------------------------------------------------------------
//  An editor-side enum shown as a combo. Each value maps onto engine settings.
// --------------------------------------------------------------------------------------
enum L08_ShadingMode { L08_SHADE_LIT = 0, L08_SHADE_UNLIT, L08_SHADE_WIREFRAME, L08_SHADE_COUNT };
static const char *L08_ShadingNames[L08_SHADE_COUNT] = { "Lit", "Unlit", "Wireframe" };
static int L08_shading = L08_SHADE_LIT;

static void L08_ApplyShading(int mode)
{
    gLight.enabled = (mode != L08_SHADE_UNLIT);
    gWireframe     = (mode == L08_SHADE_WIREFRAME);
}

// --------------------------------------------------------------------------------------
//  Fake asset database for the clipper demo
// --------------------------------------------------------------------------------------
#define L08_ASSET_COUNT 10000
static const char *L08_AssetTypes[5] = { "Mesh", "Texture", "Material", "Sound", "Script" };
static const char *L08_AssetWords[8] = { "rock", "tree", "crate", "hero", "door", "lamp", "wall", "barrel" };
static char L08_assetNames[L08_ASSET_COUNT][32];
static int  L08_assetType[L08_ASSET_COUNT];
static int  L08_filtered[L08_ASSET_COUNT];      // indices that pass the filter
static int  L08_filteredCount = 0;
static int  L08_selectedAsset = -1;
static ImGuiTextFilter L08_assetFilter;          // owns its own 256-char input buffer
static ImGuiTextFilter L08_entityFilter;

static void L08_RebuildAssetFilter(void)
{
    L08_filteredCount = 0;
    for (int i = 0; i < L08_ASSET_COUNT; i++)
        if (L08_assetFilter.PassFilter(L08_assetNames[i]))
            L08_filtered[L08_filteredCount++] = i;
}

// --------------------------------------------------------------------------------------
//  Window 1: combos for the selected entity
// --------------------------------------------------------------------------------------
static void L08_AssetPickers(void)
{
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(380, 330), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Asset Pickers")) { ImGui::End(); return; }

    // ---- 1. Enum combo (editor setting, no entity needed) ----
    ImGui::SeparatorText("View");
    if (ImGui::Combo("Shading", &L08_shading, L08_ShadingNames, L08_SHADE_COUNT))
        L08_ApplyShading(L08_shading);              // react only when the value changed

    // ---- 2. Combo from a zero-separated string (quick, for fixed lists) ----
    static int speedPreset = 2;
    static const float speeds[] = { 0.0f, 0.5f, 1.0f, 2.0f, 4.0f };
    if (ImGui::Combo("Time scale", &speedPreset, "Paused\0Half\0Normal\0Double\0Quad\0"))
        gTimeScale = speeds[speedPreset];

    ImGui::SeparatorText("Selected entity");
    if (gSelectedEntity < 0 || gSelectedEntity >= gEntityCount)
    {
        ImGui::TextDisabled("Nothing selected - click an object or use the Entity List.");
        ImGui::End();
        return;
    }
    Entity *e = &gEntities[gSelectedEntity];
    ImGui::Text("Editing: %s", e->name);

    // ---- 3. Simple combo from an array: writes the chosen index straight into e->mesh ----
    ImGui::Combo("Mesh", &e->mesh, gMeshNames, MESH_COUNT);

    // ---- 4. Custom combo: BeginCombo / Selectable / EndCombo ----
    // The preview string is whatever we want to show when closed.
    const char *preview = (e->texture >= 0 && e->texture < gTextureCount) ? gTextures[e->texture].name : "None";
    if (ImGui::BeginCombo("Texture", preview))
    {
        // "None" entry: index -1
        bool noneSelected = (e->texture < 0);
        if (ImGui::Selectable("None", noneSelected)) e->texture = -1;
        if (noneSelected) ImGui::SetItemDefaultFocus();   // keyboard nav starts here + scrolls to it

        for (int t = 0; t < gTextureCount; t++)
        {
            ImGui::PushID(t);
            bool isSelected = (e->texture == t);
            // A small thumbnail, then the selectable on the same line
            ImGui::Image(Engine_TexID(t), ImVec2(16, 16));
            ImGui::SameLine();
            if (ImGui::Selectable(gTextures[t].name, isSelected))
                e->texture = t;
            if (isSelected) ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }
        ImGui::EndCombo();                           // only when BeginCombo returned true
    }

    // Show a bigger preview of the current texture
    if (e->texture >= 0 && e->texture < gTextureCount)
    {
        ImGui::Image(Engine_TexID(e->texture), ImVec2(64, 64));
        ImGui::SameLine();
        ImGui::Text("%s\n%d x %d", gTextures[e->texture].name, gTextures[e->texture].width, gTextures[e->texture].height);
    }

    ImGui::ColorEdit4("Tint", e->color);
    ImGui::End();
}

// --------------------------------------------------------------------------------------
//  Window 2: filterable entity list
// --------------------------------------------------------------------------------------
static void L08_EntityList(void)
{
    ImGui::SetNextWindowPos(ImVec2(20, 365), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(380, 400), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Entity List")) { ImGui::End(); return; }

    // Filter box. Syntax: "cr" = contains cr, "cr,pl" = cr OR pl, "-ground" = exclude.
    L08_entityFilter.Draw("Filter##ents", -60.0f);   // width -60 leaves room for the label
    ImGui::SameLine();
    if (ImGui::SmallButton("x")) L08_entityFilter.Clear();

    // List box that fills the space down to one row of buttons
    ImVec2 size(-FLT_MIN, -ImGui::GetFrameHeightWithSpacing());
    if (ImGui::BeginListBox("##entities", size))
    {
        int shown = 0;
        for (int i = 0; i < gEntityCount; i++)
        {
            if (!L08_entityFilter.PassFilter(gEntities[i].name)) continue;
            shown++;

            char label[64];
            snprintf(label, sizeof(label), "%s  [%s]", gEntities[i].name, gMeshNames[gEntities[i].mesh]);

            ImGui::PushID(i);                        // duplicate names stay unique
            bool isSelected = (gSelectedEntity == i);
            if (ImGui::Selectable(label, isSelected, ImGuiSelectableFlags_AllowDoubleClick))
            {
                gSelectedEntity = i;                 // engine state IS the selection
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    Engine_FocusEntity(i);           // double-click = focus camera
            }
            if (isSelected) ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }
        if (shown == 0) ImGui::TextDisabled("No match");
        ImGui::EndListBox();
    }

    if (ImGui::Button("Add cube"))   gSelectedEntity = Engine_AddEntity("Cube", MESH_CUBE);
    ImGui::SameLine();
    if (ImGui::Button("Add sphere")) gSelectedEntity = Engine_AddEntity("Sphere", MESH_SPHERE);
    ImGui::SameLine();
    ImGui::TextDisabled("%d entities", gEntityCount);
    ImGui::End();
}

// --------------------------------------------------------------------------------------
//  Window 3: 10,000 assets with ImGuiListClipper
// --------------------------------------------------------------------------------------
static void L08_AssetBrowser(void)
{
    ImGui::SetNextWindowPos(ImVec2(1110, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(420, 560), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Asset Browser")) { ImGui::End(); return; }

    // Draw() returns true when the text changed -> only then rebuild the index list
    if (L08_assetFilter.Draw("Search", -70.0f))
        L08_RebuildAssetFilter();
    ImGui::Text("%d / %d assets", L08_filteredCount, L08_ASSET_COUNT);

    if (ImGui::BeginChild("AssetScroll", ImVec2(0, -ImGui::GetTextLineHeightWithSpacing() * 2), ImGuiChildFlags_Borders))
    {
        // The clipper figures out which rows are visible. All rows must have the same height.
        ImGuiListClipper clipper;
        clipper.Begin(L08_filteredCount);
        while (clipper.Step())
        {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
            {
                int a = L08_filtered[row];
                ImGui::PushID(a);
                if (ImGui::Selectable(L08_assetNames[a], L08_selectedAsset == a))
                    L08_selectedAsset = a;
                ImGui::SameLine(260);                // fake "type" column
                ImGui::TextDisabled("%s", L08_AssetTypes[L08_assetType[a]]);
                ImGui::PopID();
            }
        }
        // clipper.End() is called automatically when Step() returns false
    }
    ImGui::EndChild();

    if (L08_selectedAsset >= 0)
        ImGui::Text("Selected: %s (%s)", L08_assetNames[L08_selectedAsset], L08_AssetTypes[L08_assetType[L08_selectedAsset]]);
    else
        ImGui::TextDisabled("Selected: none");
    ImGui::TextDisabled("Only the visible rows are submitted each frame.");
    ImGui::End();
}

// --------------------------------------------------------------------------------------
//  Lesson hooks
// --------------------------------------------------------------------------------------
void Lesson_Init(void)
{
    static const char *ext[5] = { "obj", "png", "mat", "wav", "lua" };
    for (int i = 0; i < L08_ASSET_COUNT; i++)
    {
        int type = i % 5;
        L08_assetType[i] = type;
        snprintf(L08_assetNames[i], sizeof(L08_assetNames[i]), "%s_%s_%04d.%s",
                 L08_AssetTypes[type], L08_AssetWords[(i / 5) % 8], i, ext[type]);
    }
    L08_RebuildAssetFilter();                        // empty filter = everything passes
    gSelectedEntity = 1;                             // start with the Crate selected
    Engine_Log("Lesson 8 started - %d fake assets generated", L08_ASSET_COUNT);
}

void Lesson_UI(void)
{
    L08_AssetPickers();
    L08_EntityList();
    L08_AssetBrowser();
}
