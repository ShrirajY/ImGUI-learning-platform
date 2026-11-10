// Lesson 16 - Textures & an asset browser
//   "Texture Viewer"    - ImGui::Image with uv0/uv1 tricks: tiling, flipping, zooming, tint
//   "Asset Browser"     - a wrapping grid of ImageButton thumbnails. Click = select,
//                         double-click = apply to the selected entity, drag = drop on an entity
//   "Texture Generator" - build a new texture on the CPU and upload it with Engine_CreateTexture
//
// Reminder: an ImGui texture is just YOUR handle. With the OpenGL backend it is the GLuint
// returned by glGenTextures, cast to ImTextureID. Engine_TexID(i) does that cast for you.

#define L16_PAYLOAD_TEXTURE "TEXTURE_INDEX"     // int: index into gTextures[], -1 = none

static int L16_selectedAsset = 0;               // asset highlighted in the browser

static void L16_ApplyTexture(int entity, int tex)
{
    if (entity < 0 || entity >= gEntityCount) { Engine_LogWarn("Select an entity first"); return; }
    gEntities[entity].texture = tex;
    Engine_Log("'%s' texture -> %s", gEntities[entity].name, tex >= 0 ? gTextures[tex].name : "(none)");
}

// ======================================================================================
//  1. Texture viewer: what uv0 / uv1 can do
// ======================================================================================
static void L16_TextureViewer(void)
{
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(330, 560), ImGuiCond_FirstUseEver);
    ImGui::Begin("Texture Viewer");

    if (gTextureCount == 0) { ImGui::Text("No textures"); ImGui::End(); return; }
    if (L16_selectedAsset >= gTextureCount) L16_selectedAsset = 0;
    const TextureAsset *t = &gTextures[L16_selectedAsset];
    ImGui::Text("%s  (%dx%d, GL id %u)", t->name, t->width, t->height, t->glId);

    static int   tiles  = 1;
    static bool  flipU  = false, flipV = false;
    static float zoom   = 1.0f;
    static float centerUV[2] = { 0.5f, 0.5f };
    static float tint[4] = { 1, 1, 1, 1 };
    static float bg[4]   = { 0, 0, 0, 0 };

    ImGui::SliderInt("Tiles", &tiles, 1, 8);
    ImGui::Checkbox("Flip U", &flipU); ImGui::SameLine(); ImGui::Checkbox("Flip V", &flipV);
    ImGui::SliderFloat("Zoom", &zoom, 1.0f, 16.0f, "%.1fx", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat2("Centre", centerUV, 0.0f, 1.0f);
    ImGui::ColorEdit4("Tint", tint, ImGuiColorEditFlags_NoInputs); ImGui::SameLine();
    ImGui::ColorEdit4("Background", bg, ImGuiColorEditFlags_NoInputs);

    // Build the UV rectangle.
    //  - Zoom: show a window of size 1/zoom around the centre (a sub-rectangle of the texture)
    //  - Tiles: UVs beyond 1.0 repeat because the engine creates textures with GL_REPEAT
    //  - Flip: swap the two coordinates on that axis
    float half = 0.5f / zoom;
    ImVec2 uv0(centerUV[0] - half, centerUV[1] - half);
    ImVec2 uv1(centerUV[0] + half, centerUV[1] + half);
    uv0.x *= tiles; uv0.y *= tiles; uv1.x *= tiles; uv1.y *= tiles;
    if (flipU) { float s = uv0.x; uv0.x = uv1.x; uv1.x = s; }
    if (flipV) { float s = uv0.y; uv0.y = uv1.y; uv1.y = s; }

    float side = ImGui::GetContentRegionAvail().x;
    if (side > 300.0f) side = 300.0f;
    // ImageWithBg = Image + background colour + tint colour
    ImGui::ImageWithBg(Engine_TexID(L16_selectedAsset), ImVec2(side, side), uv0, uv1,
                       ImVec4(bg[0], bg[1], bg[2], bg[3]), ImVec4(tint[0], tint[1], tint[2], tint[3]));
    ImGui::TextDisabled("uv0 (%.2f, %.2f)  uv1 (%.2f, %.2f)", uv0.x, uv0.y, uv1.x, uv1.y);

    // Hover magnifier: show a 4x zoom of the texels under the mouse
    if (ImGui::BeginItemTooltip())
    {
        ImVec2 rmin = ImGui::GetItemRectMin(), rmax = ImGui::GetItemRectMax();
        ImVec2 m = ImGui::GetIO().MousePos;
        float fx = (m.x - rmin.x) / (rmax.x - rmin.x), fy = (m.y - rmin.y) / (rmax.y - rmin.y);
        float u = uv0.x + (uv1.x - uv0.x) * fx, v = uv0.y + (uv1.y - uv0.y) * fy;
        float r = (uv1.x - uv0.x) * 0.125f, rv = (uv1.y - uv0.y) * 0.125f;
        ImGui::Text("uv (%.3f, %.3f)", u, v);
        ImGui::Image(Engine_TexID(L16_selectedAsset), ImVec2(128, 128), ImVec2(u - r, v - rv), ImVec2(u + r, v + rv));
        ImGui::EndTooltip();
    }
    if (ImGui::Button("Reset view")) { tiles = 1; flipU = flipV = false; zoom = 1; centerUV[0] = centerUV[1] = 0.5f; }
    ImGui::End();
}

// ======================================================================================
//  2. Asset browser
// ======================================================================================
static void L16_AssetBrowser(void)
{
    ImGui::SetNextWindowPos(ImVec2(360, 560), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(830, 270), ImGuiCond_FirstUseEver);
    ImGui::Begin("Asset Browser");

    static float thumb = 64.0f;
    ImGui::SetNextItemWidth(160);
    ImGui::SliderFloat("Thumbnail size", &thumb, 32.0f, 128.0f, "%.0f px");
    ImGui::SameLine();
    ImGui::TextDisabled("| %d / %d textures | double-click or drag onto an entity", gTextureCount, MAX_TEXTURES);

    // Left: the thumbnail grid in a child region. Right: entity drop list.
    float listWidth = 200.0f;
    ImGui::BeginChild("##grid", ImVec2(-listWidth, 0), ImGuiChildFlags_Borders);
    {
        ImGuiStyle &style = ImGui::GetStyle();
        float cellW = thumb + style.FramePadding.x * 2.0f;              // ImageButton adds FramePadding
        int columns = (int)((ImGui::GetContentRegionAvail().x + style.ItemSpacing.x) / (cellW + style.ItemSpacing.x));
        if (columns < 1) columns = 1;

        for (int i = 0; i < gTextureCount; i++)
        {
            ImGui::PushID(i);
            ImGui::BeginGroup();                                        // thumbnail + caption = one cell
            bool selected = (i == L16_selectedAsset);
            if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

            // ImageButton(str_id, texture, size, uv0, uv1, bg_col, tint_col)
            if (ImGui::ImageButton("##thumb", Engine_TexID(i), ImVec2(thumb, thumb)))
                L16_selectedAsset = i;
            if (selected) ImGui::PopStyleColor();

            // ---- interactions on the thumbnail (it is the last item) ----
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                L16_ApplyTexture(gSelectedEntity, i);

            if (ImGui::BeginDragDropSource())
            {
                ImGui::SetDragDropPayload(L16_PAYLOAD_TEXTURE, &i, sizeof(int));
                ImGui::Image(Engine_TexID(i), ImVec2(48, 48));            // preview under the cursor
                ImGui::SameLine();
                ImGui::Text("%s", gTextures[i].name);
                ImGui::EndDragDropSource();
            }

            if (ImGui::BeginItemTooltip())                               // large preview
            {
                ImGui::Text("%s", gTextures[i].name);
                ImGui::TextDisabled("%d x %d", gTextures[i].width, gTextures[i].height);
                ImGui::Image(Engine_TexID(i), ImVec2(192, 192));
                ImGui::EndTooltip();
            }

            // Selection outline drawn over the button
            if (selected)
                ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                                                    IM_COL32(255, 210, 60, 255), 3.0f, 0, 2.0f);

            // Caption, clipped to the cell width
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + cellW);
            ImGui::TextUnformatted(gTextures[i].name);
            ImGui::PopTextWrapPos();
            ImGui::EndGroup();
            ImGui::PopID();

            // Wrap: stay on the same line until the row is full
            if ((i % columns) != columns - 1 && i + 1 < gTextureCount)
                ImGui::SameLine();
        }
    }
    ImGui::EndChild();

    // ---- right side: entities as drop targets ----
    ImGui::SameLine();
    ImGui::BeginChild("##targets", ImVec2(0, 0), ImGuiChildFlags_Borders);
    ImGui::TextDisabled("Drop textures here");
    if (ImGui::Selectable("(no texture -> selected)")) L16_ApplyTexture(gSelectedEntity, -1);
    ImGui::Separator();
    for (int i = 0; i < gEntityCount; i++)
    {
        Entity *e = &gEntities[i];
        ImGui::PushID(i);
        // little texture icon in front of the name
        if (e->texture >= 0) ImGui::Image(Engine_TexID(e->texture), ImVec2(16, 16));
        else                 ImGui::Dummy(ImVec2(16, 16));
        ImGui::SameLine();
        if (ImGui::Selectable(e->name, gSelectedEntity == i)) gSelectedEntity = i;
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *p = ImGui::AcceptDragDropPayload(L16_PAYLOAD_TEXTURE))
                L16_ApplyTexture(i, *(const int *)p->Data);
            ImGui::EndDragDropTarget();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::End();
}

// ======================================================================================
//  3. Texture generator: CPU pixels -> Engine_CreateTexture -> new asset
// ======================================================================================
// One function decides "colour A or B?" for a pixel, so the preview and the real texture match.
static bool L16_PatternIsA(int x, int y, int size, int pattern, int cells)
{
    int cellPx = size / cells; if (cellPx < 1) cellPx = 1;
    if (pattern == 0) return (((x / cellPx) + (y / cellPx)) & 1) == 0;          // checker
    if (pattern == 1) return (((x + y) / cellPx) & 1) == 0;                       // diagonal stripes
    int dx = (x % cellPx) - cellPx / 2, dy = (y % cellPx) - cellPx / 2;           // dots
    return dx * dx + dy * dy > (cellPx * cellPx) / 9;
}

static void L16_FillPattern(unsigned char *px, int size, int pattern, int cells, const float a[3], const float b[3])
{
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++)
        {
            const float *c = L16_PatternIsA(x, y, size, pattern, cells) ? a : b;
            unsigned char *p = &px[(y * size + x) * 4];          // RGBA, row-major, 4 bytes per pixel
            p[0] = (unsigned char)(c[0] * 255.0f);
            p[1] = (unsigned char)(c[1] * 255.0f);
            p[2] = (unsigned char)(c[2] * 255.0f);
            p[3] = 255;
        }
}

static void L16_Generator(void)
{
    ImGui::SetNextWindowPos(ImVec2(1210, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320, 470), ImGuiCond_FirstUseEver);
    ImGui::Begin("Texture Generator");

    static float colA[3] = { 0.95f, 0.95f, 0.95f };
    static float colB[3] = { 0.20f, 0.45f, 0.85f };
    static int   cells   = 8;
    static int   pattern = 0;
    static int   sizeIdx = 1;
    static char  name[32] = "MyChecker";
    const int    sizes[] = { 32, 64, 128, 256 };
    const char  *sizeNames[] = { "32", "64", "128", "256" };
    const char  *patternNames[] = { "Checker", "Stripes", "Dots" };

    ImGui::Combo("Pattern", &pattern, patternNames, IM_ARRAYSIZE(patternNames));
    ImGui::ColorEdit3("Colour A", colA);
    ImGui::ColorEdit3("Colour B", colB);
    ImGui::SliderInt("Cells", &cells, 1, 32);
    ImGui::Combo("Size", &sizeIdx, sizeNames, IM_ARRAYSIZE(sizeNames));
    ImGui::InputText("Name", name, sizeof(name));

    // Live preview WITHOUT a GPU texture: evaluate the pattern at a low resolution (64x64)
    // and draw each sample as a small rectangle with the draw list.
    ImGui::SeparatorText("Preview");
    const int P = 64;
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    float side = 160.0f, cs = side / P;
    ImGui::Dummy(ImVec2(side, side));
    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImU32 ua = ImGui::ColorConvertFloat4ToU32(ImVec4(colA[0], colA[1], colA[2], 1));
    ImU32 ub = ImGui::ColorConvertFloat4ToU32(ImVec4(colB[0], colB[1], colB[2], 1));
    dl->AddRectFilled(p0, ImVec2(p0.x + side, p0.y + side), ua);
    for (int y = 0; y < P; y++)
        for (int x = 0; x < P; x++)
            if (!L16_PatternIsA(x, y, P, pattern, cells))          // only draw the B samples
                dl->AddRectFilled(ImVec2(p0.x + x * cs, p0.y + y * cs), ImVec2(p0.x + (x + 1) * cs, p0.y + (y + 1) * cs), ub);
    dl->AddRect(p0, ImVec2(p0.x + side, p0.y + side), IM_COL32(255, 255, 255, 80));

    // ---- Generate ----
    bool full = gTextureCount >= MAX_TEXTURES;
    ImGui::BeginDisabled(full);
    if (ImGui::Button("Generate texture", ImVec2(-FLT_MIN, 0)))
    {
        static unsigned char px[256 * 256 * 4];          // biggest size; static = not on the stack
        int sz = sizes[sizeIdx];
        L16_FillPattern(px, sz, pattern, cells, colA, colB);
        int idx = Engine_CreateTexture(name, sz, sz, px);  // glGenTextures + glTexImage2D inside
        if (idx >= 0)
        {
            Engine_Log("Created texture #%d '%s' (%dx%d)", idx, name, sz, sz);
            L16_selectedAsset = idx;
            if (gSelectedEntity >= 0) L16_ApplyTexture(gSelectedEntity, idx);
        }
    }
    ImGui::EndDisabled();
    if (full) ImGui::TextColored(ImVec4(1, 0.5f, 0.3f, 1), "Texture slots full (%d)", MAX_TEXTURES);
    ImGui::TextDisabled("New textures are applied to the selected entity.");
    ImGui::End();
}

void Lesson_Init(void)
{
    Engine_Log("Lesson 16: %d built-in textures loaded", gTextureCount);
    if (gSelectedEntity < 0 && gEntityCount > 1) gSelectedEntity = 1;   // start with the crate selected
}

void Lesson_UI(void)
{
    L16_TextureViewer();
    L16_AssetBrowser();
    L16_Generator();
}
