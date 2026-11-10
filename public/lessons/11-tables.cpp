// Lesson 11 - Tables: a sortable, editable "Entity Table"
//   - BeginTable flags (borders, row colours, resize, reorder, hide, sort, scroll)
//   - TableSetupColumn with flags, widths and user IDs, frozen header row
//   - Sorting through an INDEX array (qsort), never by shuffling gEntities[]
//   - Full-row selection (Selectable + SpanAllColumns) synced with gSelectedEntity
//   - Editable cells: name, mesh, position, visible, colour
// Change something, then Build & Run (Ctrl+Enter).

// ======================================================================================
//  Column IDs (passed as user_id to TableSetupColumn, read back in the sort specs).
//  Using IDs instead of column indices keeps sorting correct when columns are reordered.
// ======================================================================================
enum L11_ColumnID { L11_COL_INDEX = 0, L11_COL_NAME, L11_COL_MESH, L11_COL_POSITION, L11_COL_VISIBLE, L11_COL_COLOR };

static int  L11_order[MAX_ENTITIES];        // display order: L11_order[row] = entity index
static int  L11_orderCount = -1;            // -1 forces a rebuild on the first frame
static bool L11_isSorted   = false;         // order currently reflects sort specs
static const ImGuiTableSortSpecs *L11_sortSpecs = NULL;   // only valid DURING qsort

static ImGuiTableFlags L11_tableFlags =
      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg
    | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable
    | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti   // shift+click headers = multi-sort
    | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;

// ======================================================================================
//  Sorting
// ======================================================================================
static int L11_CompareFloat(float a, float b) { return (a < b) ? -1 : (a > b) ? 1 : 0; }

// qsort comparator. Walks every sort spec (1 normally, more with shift+click).
static int L11_CompareEntities(const void *pa, const void *pb)
{
    const int ia = *(const int *)pa, ib = *(const int *)pb;
    const Entity *a = &gEntities[ia], *b = &gEntities[ib];

    for (int n = 0; n < L11_sortSpecs->SpecsCount; n++)
    {
        const ImGuiTableColumnSortSpecs *s = &L11_sortSpecs->Specs[n];
        int d = 0;
        switch (s->ColumnUserID)
        {
        case L11_COL_INDEX:    d = ia - ib; break;
        case L11_COL_NAME:     d = _stricmp(a->name, b->name); break;
        case L11_COL_MESH:     d = a->mesh - b->mesh; break;
        case L11_COL_POSITION: d = L11_CompareFloat(a->position[0], b->position[0]);       // X, then Y, then Z
                               if (!d) d = L11_CompareFloat(a->position[1], b->position[1]);
                               if (!d) d = L11_CompareFloat(a->position[2], b->position[2]);
                               break;
        case L11_COL_VISIBLE:  d = (int)a->visible - (int)b->visible; break;
        default: break;
        }
        if (d != 0)
            return (s->SortDirection == ImGuiSortDirection_Ascending) ? d : -d;
    }
    return ia - ib;   // stable tie-break: keep scene order
}

static void L11_RebuildOrder(const ImGuiTableSortSpecs *specs)
{
    for (int i = 0; i < gEntityCount; i++) L11_order[i] = i;
    L11_orderCount = gEntityCount;
    L11_isSorted = (specs != NULL);
    if (specs && specs->SpecsCount > 0 && gEntityCount > 1)
    {
        L11_sortSpecs = specs;                       // qsort has no user pointer: pass via a static
        qsort(L11_order, (size_t)gEntityCount, sizeof(int), L11_CompareEntities);
        L11_sortSpecs = NULL;
    }
}

// ======================================================================================
//  The table
// ======================================================================================
static void L11_EntityTable(void)
{
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(760, 420), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Entity Table")) { ImGui::End(); return; }

    // ---- Live flag toggles: see what each flag does ----
    ImGui::CheckboxFlags("Borders", &L11_tableFlags, ImGuiTableFlags_Borders);         ImGui::SameLine();
    ImGui::CheckboxFlags("Row bg", &L11_tableFlags, ImGuiTableFlags_RowBg);            ImGui::SameLine();
    ImGui::CheckboxFlags("Resizable", &L11_tableFlags, ImGuiTableFlags_Resizable);     ImGui::SameLine();
    ImGui::CheckboxFlags("Reorderable", &L11_tableFlags, ImGuiTableFlags_Reorderable); ImGui::SameLine();
    ImGui::CheckboxFlags("Hideable", &L11_tableFlags, ImGuiTableFlags_Hideable);       ImGui::SameLine();
    ImGui::CheckboxFlags("Sortable", &L11_tableFlags, ImGuiTableFlags_Sortable);
    ImGui::TextDisabled("Click a header to sort (shift+click: multi-sort). Right-click a header to hide columns.");

    // Leave one text line at the bottom for the footer. ScrollY tables NEED an outer size.
    ImVec2 outer(0.0f, -ImGui::GetTextLineHeightWithSpacing());
    if (ImGui::BeginTable("##entities", 6, L11_tableFlags, outer))
    {
        // ---- Columns: label, flags, width (pixels if fixed, weight if stretch), user ID ----
        ImGui::TableSetupColumn("#",        ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 30.0f, L11_COL_INDEX);
        ImGui::TableSetupColumn("Name",     ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort, 1.2f, L11_COL_NAME);
        ImGui::TableSetupColumn("Mesh",     ImGuiTableColumnFlags_WidthFixed, 95.0f, L11_COL_MESH);
        ImGui::TableSetupColumn("Position", ImGuiTableColumnFlags_WidthStretch, 2.0f, L11_COL_POSITION);
        ImGui::TableSetupColumn("Visible",  ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending, 55.0f, L11_COL_VISIBLE);
        ImGui::TableSetupColumn("Color",    ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, 45.0f, L11_COL_COLOR);
        ImGui::TableSetupScrollFreeze(0, 1);         // keep the header row visible while scrolling
        ImGui::TableHeadersRow();

        // ---- Sort only when needed: specs changed, the entity list changed size,
        //      or sorting was just switched off (go back to scene order) ----
        ImGuiTableSortSpecs *specs = ImGui::TableGetSortSpecs();   // NULL if Sortable is off
        if ((specs && specs->SpecsDirty) || L11_orderCount != gEntityCount || (!specs && L11_isSorted))
        {
            L11_RebuildOrder(specs);
            if (specs) specs->SpecsDirty = false;    // we handled it
        }

        const float rowHeight = ImGui::GetFrameHeight();   // rows hold framed widgets
        int clickedEntity = -1;

        for (int row = 0; row < L11_orderCount; row++)
        {
            int i = L11_order[row];
            if (i >= gEntityCount) continue;         // safety if the scene shrank this frame
            Entity *e = &gEntities[i];

            ImGui::PushID(i);                        // per-ROW ID: every "##name" below is unique
            ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);

            // Column 0: full-row Selectable. AllowOverlap lets widgets in other cells still get clicks.
            ImGui::TableSetColumnIndex(0);
            char idx[16];
            snprintf(idx, sizeof(idx), "%d", i);
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));  // centre text vertically
            if (ImGui::Selectable(idx, gSelectedEntity == i,
                                  ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap,
                                  ImVec2(0, rowHeight)))
                clickedEntity = i;
            ImGui::PopStyleVar();
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                Engine_FocusEntity(i);

            // Column 1: editable name
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);       // fill the cell
            ImGui::InputText("##name", e->name, sizeof(e->name));
            if (ImGui::IsItemActivated()) clickedEntity = i;   // editing a cell also selects the row

            // Column 2: mesh combo
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::Combo("##mesh", &e->mesh, gMeshNames, MESH_COUNT);
            if (ImGui::IsItemActivated()) clickedEntity = i;

            // Column 3: position (editable)
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::DragFloat3("##pos", e->position, 0.05f, 0.0f, 0.0f, "%.1f");
            if (ImGui::IsItemActivated()) clickedEntity = i;

            // Column 4: visibility checkbox
            ImGui::TableNextColumn();
            ImGui::Checkbox("##vis", &e->visible);

            // Column 5: colour swatch, click opens the picker
            ImGui::TableNextColumn();
            ImGui::ColorEdit4("##col", e->color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);

            ImGui::PopID();
        }
        ImGui::EndTable();

        if (clickedEntity >= 0) gSelectedEntity = clickedEntity;   // apply after the loop
    }

    // Footer
    ImGui::Text("%d entities", gEntityCount);
    ImGui::SameLine();
    if (gSelectedEntity >= 0 && gSelectedEntity < gEntityCount)
        ImGui::TextDisabled("| selected: %s (double-click a row to focus)", gEntities[gSelectedEntity].name);
    ImGui::End();
}

// ======================================================================================
//  A small read-only table: the texture assets (simplest possible table)
// ======================================================================================
static void L11_TextureTable(void)
{
    ImGui::SetNextWindowPos(ImVec2(20, 455), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360, 260), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Textures")) { ImGui::End(); return; }

    if (ImGui::BeginTable("##tex", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthFixed, 40.0f);
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Size");
        ImGui::TableHeadersRow();
        for (int t = 0; t < gTextureCount; t++)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Image(Engine_TexID(t), ImVec2(24, 24));
            ImGui::TableNextColumn(); ImGui::TextUnformatted(gTextures[t].name);
            ImGui::TableNextColumn(); ImGui::Text("%d x %d", gTextures[t].width, gTextures[t].height);
        }
        ImGui::EndTable();
    }
    if (ImGui::Button("Add 10 cubes"))               // stress the table + re-sort path
        for (int k = 0; k < 10; k++)
        {
            char name[32];
            snprintf(name, sizeof(name), "Box %02d", gEntityCount);
            int i = Engine_AddEntity(name, MESH_CUBE);
            if (i < 0) break;
            gEntities[i].position[0] = (float)(rand() % 17 - 8);
            gEntities[i].position[1] = 0.5f;
            gEntities[i].position[2] = (float)(rand() % 17 - 8);
            gEntities[i].color[0] = (float)(rand() % 100) / 100.0f;
        }
    ImGui::SameLine();
    if (ImGui::Button("Reset scene")) Engine_ResetScene();
    ImGui::End();
}

// ======================================================================================
//  Lesson hooks
// ======================================================================================
void Lesson_Init(void)
{
    Engine_Log("Lesson 11 started - tables");
}

void Lesson_UI(void)
{
    L11_EntityTable();
    L11_TextureTable();
}
