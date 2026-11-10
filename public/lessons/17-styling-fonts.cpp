// Lesson 17 - Styling & fonts
//   Lesson_Init loads Windows fonts (Segoe UI, Segoe UI Bold, Consolas) with fallbacks
//   "Theme"         - built-in themes + a custom engine theme, live style sliders, DPI scale
//   "Widget Preview"- a fake inspector to see the theme, PushStyleColor/Var examples, PushFont headings
//   "Style Editor"  - ImGui's own full editor (toggle from the Theme window)

// ======================================================================================
//  Fonts
// ======================================================================================
static ImFont *L17_fontBody    = NULL;   // first font added = default font for everything
static ImFont *L17_fontHeading = NULL;   // bigger/bold, used with PushFont for titles
static ImFont *L17_fontMono    = NULL;   // monospace for numbers, logs, code
static ImFont *L17_fontProggy  = NULL;   // ImGui's built-in font, kept for comparison

static bool L17_FileExists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

// Load a TTF if it exists, otherwise fall back to the built-in font so the code never breaks.
static ImFont *L17_LoadFont(const char *path, float sizePixels)
{
    ImGuiIO &io = ImGui::GetIO();
    if (L17_FileExists(path))
    {
        ImFont *f = io.Fonts->AddFontFromFileTTF(path, sizePixels);
        if (f) { Engine_Log("Loaded font %s @ %.0f px", path, sizePixels); return f; }
    }
    Engine_LogWarn("Font %s not found, using the default font", path);
    return io.Fonts->AddFontDefault();
}

// ======================================================================================
//  Themes
// ======================================================================================
enum { L17_THEME_DARK = 0, L17_THEME_LIGHT, L17_THEME_CLASSIC, L17_THEME_ENGINE, L17_THEME_COUNT };
static const char *L17_themeNames[L17_THEME_COUNT] = { "Dark", "Light", "Classic", "Engine (custom)" };

// A custom theme: neutral charcoal greys with one orange accent colour.
static void L17_StyleEngine(ImGuiStyle *s)
{
    ImGui::StyleColorsDark(s);             // start from a full, consistent palette

    // --- shape ---
    s->WindowRounding    = 6.0f;
    s->ChildRounding     = 4.0f;
    s->FrameRounding     = 4.0f;
    s->PopupRounding     = 4.0f;
    s->GrabRounding      = 4.0f;
    s->TabRounding       = 4.0f;
    s->ScrollbarRounding = 6.0f;
    s->WindowBorderSize  = 1.0f;
    s->FrameBorderSize   = 0.0f;
    s->WindowPadding     = ImVec2(10, 10);
    s->FramePadding      = ImVec2(8, 4);
    s->ItemSpacing       = ImVec2(8, 6);
    s->WindowTitleAlign  = ImVec2(0.5f, 0.5f);

    // --- colours ---
    ImVec4 *c = s->Colors;
    ImVec4 accent      = ImVec4(0.95f, 0.55f, 0.20f, 1.00f);
    ImVec4 accentHover = ImVec4(1.00f, 0.65f, 0.30f, 1.00f);
    ImVec4 accentDim   = ImVec4(0.95f, 0.55f, 0.20f, 0.45f);
    c[ImGuiCol_Text]                 = ImVec4(0.90f, 0.90f, 0.92f, 1.00f);
    c[ImGuiCol_TextDisabled]         = ImVec4(0.50f, 0.50f, 0.54f, 1.00f);
    c[ImGuiCol_WindowBg]             = ImVec4(0.11f, 0.11f, 0.12f, 0.96f);
    c[ImGuiCol_ChildBg]              = ImVec4(0.09f, 0.09f, 0.10f, 1.00f);
    c[ImGuiCol_PopupBg]              = ImVec4(0.12f, 0.12f, 0.13f, 0.98f);
    c[ImGuiCol_Border]               = ImVec4(0.22f, 0.22f, 0.24f, 1.00f);
    c[ImGuiCol_FrameBg]              = ImVec4(0.17f, 0.17f, 0.18f, 1.00f);
    c[ImGuiCol_FrameBgHovered]       = ImVec4(0.22f, 0.22f, 0.24f, 1.00f);
    c[ImGuiCol_FrameBgActive]        = ImVec4(0.26f, 0.26f, 0.28f, 1.00f);
    c[ImGuiCol_TitleBg]              = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
    c[ImGuiCol_TitleBgActive]        = ImVec4(0.13f, 0.13f, 0.14f, 1.00f);
    c[ImGuiCol_MenuBarBg]            = ImVec4(0.10f, 0.10f, 0.11f, 1.00f);
    c[ImGuiCol_CheckMark]            = accent;
    c[ImGuiCol_SliderGrab]           = accentDim;
    c[ImGuiCol_SliderGrabActive]     = accent;
    c[ImGuiCol_Button]               = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
    c[ImGuiCol_ButtonHovered]        = ImVec4(0.28f, 0.28f, 0.30f, 1.00f);
    c[ImGuiCol_ButtonActive]         = accentDim;
    c[ImGuiCol_Header]               = ImVec4(0.95f, 0.55f, 0.20f, 0.25f);
    c[ImGuiCol_HeaderHovered]        = ImVec4(0.95f, 0.55f, 0.20f, 0.35f);
    c[ImGuiCol_HeaderActive]         = accentDim;
    c[ImGuiCol_SeparatorHovered]     = accentDim;
    c[ImGuiCol_SeparatorActive]      = accent;
    c[ImGuiCol_ResizeGrip]           = ImVec4(0.95f, 0.55f, 0.20f, 0.15f);
    c[ImGuiCol_ResizeGripHovered]    = accentDim;
    c[ImGuiCol_ResizeGripActive]     = accent;
    c[ImGuiCol_Tab]                  = ImVec4(0.15f, 0.15f, 0.16f, 1.00f);
    c[ImGuiCol_TabHovered]           = accentHover;
    c[ImGuiCol_TabSelected]          = ImVec4(0.24f, 0.24f, 0.26f, 1.00f);
    c[ImGuiCol_TabSelectedOverline]  = accent;
    c[ImGuiCol_PlotHistogram]        = accent;
    c[ImGuiCol_TextSelectedBg]       = accentDim;
    c[ImGuiCol_DragDropTarget]       = accentHover;
    c[ImGuiCol_NavCursor]            = accent;
}

// Rebuild the whole style from scratch: theme colours + sizes, then DPI scale.
// (ScaleAllSizes MULTIPLIES the current sizes, so always start from a fresh style.)
static void L17_ApplyTheme(int theme, float uiScale)
{
    ImGuiStyle s;                                  // default constructor = default sizes
    switch (theme)
    {
    case L17_THEME_DARK:    ImGui::StyleColorsDark(&s);    break;
    case L17_THEME_LIGHT:   ImGui::StyleColorsLight(&s);   break;
    case L17_THEME_CLASSIC: ImGui::StyleColorsClassic(&s); break;
    default:                L17_StyleEngine(&s);           break;
    }
    s.ScaleAllSizes(uiScale);
    ImGui::GetStyle() = s;                         // copy into the live style
}

static int   L17_theme       = L17_THEME_ENGINE;
static float L17_uiScale     = 1.0f;
static bool  L17_showEditor  = false;

// Heading helper: big font + optional accent colour, always balanced
static void L17_Heading(const char *text)
{
    ImGui::PushFont(L17_fontHeading);              // NULL would mean "default font"
    ImGui::TextUnformatted(text);
    ImGui::PopFont();
}

// ======================================================================================
//  Theme window
// ======================================================================================
static void L17_ThemeWindow(void)
{
    ImGuiIO    &io    = ImGui::GetIO();
    ImGuiStyle &style = ImGui::GetStyle();

    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(380, 700), ImGuiCond_FirstUseEver);
    ImGui::Begin("Theme");

    L17_Heading("Theme");
    for (int t = 0; t < L17_THEME_COUNT; t++)
        if (ImGui::RadioButton(L17_themeNames[t], &L17_theme, t))
        {
            L17_ApplyTheme(L17_theme, L17_uiScale);
            Engine_Log("Theme: %s", L17_themeNames[t]);
        }
    ImGui::TextDisabled("ShowStyleSelector (built-in combo):");
    if (ImGui::ShowStyleSelector("##styles"))      // changes colours only, keeps sizes
        Engine_Log("Style changed with ShowStyleSelector");

    ImGui::Spacing();
    L17_Heading("Sizes");
    // Editing ImGuiStyle fields directly takes effect immediately (next widget, this frame)
    ImGui::SliderFloat("WindowRounding", &style.WindowRounding, 0.0f, 14.0f, "%.0f");
    ImGui::SliderFloat("FrameRounding",  &style.FrameRounding,  0.0f, 12.0f, "%.0f");
    ImGui::SliderFloat("GrabRounding",   &style.GrabRounding,   0.0f, 12.0f, "%.0f");
    ImGui::SliderFloat2("FramePadding",  (float *)&style.FramePadding, 0.0f, 20.0f, "%.0f");
    ImGui::SliderFloat2("ItemSpacing",   (float *)&style.ItemSpacing,  0.0f, 20.0f, "%.0f");
    ImGui::SliderFloat("WindowBorderSize", &style.WindowBorderSize, 0.0f, 1.0f, "%.0f");
    ImGui::SliderFloat("Alpha", &style.Alpha, 0.3f, 1.0f, "%.2f");   // never let it reach 0!

    ImGui::Spacing();
    L17_Heading("Colours");
    ImGui::ColorEdit4("WindowBg", (float *)&style.Colors[ImGuiCol_WindowBg], ImGuiColorEditFlags_AlphaPreviewHalf);
    ImGui::ColorEdit4("Button",   (float *)&style.Colors[ImGuiCol_Button]);
    ImGui::ColorEdit4("Header",   (float *)&style.Colors[ImGuiCol_Header]);
    ImGui::ColorEdit4("Text",     (float *)&style.Colors[ImGuiCol_Text]);

    ImGui::Spacing();
    L17_Heading("Scale / DPI");
    ImGui::SliderFloat("UI scale", &L17_uiScale, 0.75f, 2.0f, "%.2f");
    if (ImGui::IsItemDeactivatedAfterEdit())       // rebuild once when the slider is released
        L17_ApplyTheme(L17_theme, L17_uiScale);
    ImGui::SameLine(); ImGui::TextDisabled("(?)");
    ImGui::SetItemTooltip("style.ScaleAllSizes(scale) on a fresh style:\npaddings, spacings, roundings, scrollbars...");
    ImGui::SliderFloat("FontGlobalScale", &io.FontGlobalScale, 0.5f, 2.0f, "%.2f");
    ImGui::SetItemTooltip("Stretches the already-baked font texture.\nQuick, but blurry above 1.0.\nFor sharp text, load the font at a bigger size.");
    if (ImGui::Button("Reset scale")) { L17_uiScale = 1.0f; io.FontGlobalScale = 1.0f; L17_ApplyTheme(L17_theme, 1.0f); }

    ImGui::Spacing();
    L17_Heading("Fonts");
    ImGui::ShowFontSelector("Default font");      // sets io.FontDefault
    ImGui::Checkbox("Show ImGui Style Editor", &L17_showEditor);
    ImGui::End();
}

// ======================================================================================
//  Widget preview with Push/Pop examples
// ======================================================================================
static void L17_PreviewWindow(void)
{
    ImGui::SetNextWindowPos(ImVec2(1150, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(380, 640), ImGuiCond_FirstUseEver);
    ImGui::Begin("Widget Preview");

    // Big title using the heading font
    ImGui::PushFont(L17_fontHeading);
    if (gSelectedEntity >= 0) ImGui::Text("%s", gEntities[gSelectedEntity].name);
    else                      ImGui::TextUnformatted("Inspector");
    ImGui::PopFont();
    ImGui::TextDisabled("Select an entity in the 3D view");
    ImGui::Separator();

    if (gSelectedEntity >= 0)
    {
        Entity *e = &gEntities[gSelectedEntity];
        ImGui::DragFloat3("Position", e->position, 0.05f);
        ImGui::DragFloat3("Rotation", e->rotation, 1.0f);
        ImGui::DragFloat3("Scale",    e->scale, 0.02f);
        ImGui::ColorEdit4("Colour",   e->color);
        ImGui::Checkbox("Visible", &e->visible); ImGui::SameLine();
        ImGui::Checkbox("Spin", &e->spin);
        ImGui::SliderFloat("Spin speed", &e->spinSpeed, -360.0f, 360.0f, "%.0f deg/s");
    }
    else
    {
        static float f = 0.5f; static int i = 3; static bool b = true;
        ImGui::SliderFloat("Slider", &f, 0.0f, 1.0f);
        ImGui::DragInt("Drag int", &i);
        ImGui::Checkbox("Checkbox", &b);
        ImGui::ProgressBar(f);
    }

    ImGui::SeparatorText("PushStyleColor");
    // A red "dangerous action" button: push 3 colours, pop 3 colours
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.70f, 0.18f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.25f, 0.25f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.95f, 0.30f, 0.30f, 1.0f));
    if (ImGui::Button("Delete selected") && gSelectedEntity >= 0)
    {
        Engine_Log("Deleted '%s'", gEntities[gSelectedEntity].name);
        Engine_RemoveEntity(gSelectedEntity);
    }
    ImGui::PopStyleColor(3);                       // ALWAYS pop what you pushed
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.55f, 0.30f, 1.0f));
    if (ImGui::Button("Reset scene")) Engine_ResetScene();
    ImGui::PopStyleColor();

    ImGui::SeparatorText("PushStyleVar");
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);       // float var
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16, 8)); // ImVec2 var
    ImGui::Button("Pill button");
    ImGui::SameLine();
    ImGui::Button("Another");
    ImGui::PopStyleVar(2);
    ImGui::Button("Back to normal");

    ImGui::SeparatorText("Fonts");
    ImGui::PushFont(L17_fontMono);
    ImGui::Text("Consolas: pos = (%.2f, %.2f, %.2f)", gCamera.target[0], gCamera.target[1], gCamera.target[2]);
    ImGui::Text("0O 1lI  {}[]()  ->  frame %d", gFrameCount);
    ImGui::PopFont();
    ImGui::PushFont(L17_fontProggy);
    ImGui::TextUnformatted("ProggyClean 13px: ImGui's built-in font");
    ImGui::PopFont();
    ImGui::TextUnformatted("Segoe UI: the body font of this lesson");

    // Status colours as semantic text
    ImGui::SeparatorText("Semantic colours");
    ImGui::TextColored(ImVec4(0.40f, 0.85f, 0.45f, 1), "OK  Saved scene.json");
    ImGui::TextColored(ImVec4(1.00f, 0.75f, 0.25f, 1), "WARN  Texture missing, using fallback");
    ImGui::TextColored(ImVec4(1.00f, 0.40f, 0.40f, 1), "ERROR  Shader failed to compile");
    ImGui::End();
}

void Lesson_Init(void)
{
    // Fonts must be added BEFORE the first frame: the backend bakes them into a texture
    // in its first NewFrame(). The first font added becomes the default font.
    L17_fontBody    = L17_LoadFont("C:\\Windows\\Fonts\\segoeui.ttf",  18.0f);
    L17_fontHeading = L17_LoadFont("C:\\Windows\\Fonts\\segoeuib.ttf", 26.0f);
    L17_fontMono    = L17_LoadFont("C:\\Windows\\Fonts\\consola.ttf",  16.0f);
    L17_fontProggy  = ImGui::GetIO().Fonts->AddFontDefault();   // 13 px built-in

    L17_ApplyTheme(L17_theme, L17_uiScale);
    gSelectedEntity = 2;                                        // the planet, so the inspector has content
}

void Lesson_UI(void)
{
    L17_ThemeWindow();
    L17_PreviewWindow();

    if (L17_showEditor)
    {
        ImGui::SetNextWindowPos(ImVec2(420, 20), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(460, 600), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Style Editor", &L17_showEditor))
            ImGui::ShowStyleEditor();        // a block of widgets, not a window: we host it
        ImGui::End();
    }
}
