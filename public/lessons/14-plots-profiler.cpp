// Lesson 14 - Plots & a tiny profiler
//   "Profiler"   - frame-time graphs (PlotLines / PlotHistogram), min/avg/max, VSync toggle,
//                  a frame-budget ProgressBar and per-section CPU timings from a scope profiler
//   corner HUD   - a small transparent overlay pinned to a screen corner.
//                  Right-click it to choose a corner.
// Turn VSync off and add some "fake work" to see the graphs react.

// ======================================================================================
//  A minimal CPU scope profiler built on QueryPerformanceCounter
// ======================================================================================
#define L14_MAX_SECTIONS 8

struct L14_Section
{
    const char *name;
    LONGLONG    start;                    // ticks when Begin was called
    float       lastMs;                   // duration of the most recent Begin/End pair
    float       history[FRAME_HISTORY];   // ring buffer of lastMs, one entry per frame
    int         historyOffset;
};

static L14_Section   L14_sections[L14_MAX_SECTIONS];
static int           L14_sectionCount = 0;
static LARGE_INTEGER L14_freq;

static LONGLONG L14_Now(void) { LARGE_INTEGER t; QueryPerformanceCounter(&t); return t.QuadPart; }

// Find a section by name (pointer compare is enough: we always pass string literals), or create it.
static L14_Section *L14_Get(const char *name)
{
    for (int i = 0; i < L14_sectionCount; i++)
        if (L14_sections[i].name == name) return &L14_sections[i];
    if (L14_sectionCount >= L14_MAX_SECTIONS) return NULL;
    L14_Section *s = &L14_sections[L14_sectionCount++];
    memset(s, 0, sizeof(*s));
    s->name = name;
    return s;
}

static void L14_Begin(const char *name)
{
    if (L14_Section *s = L14_Get(name)) s->start = L14_Now();
}

static void L14_End(const char *name)
{
    L14_Section *s = L14_Get(name);
    if (!s) return;
    s->lastMs = (float)((double)(L14_Now() - s->start) * 1000.0 / (double)L14_freq.QuadPart);
    s->history[s->historyOffset] = s->lastMs;
    s->historyOffset = (s->historyOffset + 1) % FRAME_HISTORY;
}

// C++ convenience: the destructor calls End when the scope closes, so you can't forget it.
struct L14_Scope
{
    const char *name;
    L14_Scope(const char *n) : name(n) { L14_Begin(n); }
    ~L14_Scope()                         { L14_End(name); }
};
// Two-step concat so __LINE__ is expanded first: gives each scope a unique variable name.
#define L14_CAT2(a, b) a##b
#define L14_CAT(a, b)  L14_CAT2(a, b)
#define L14_PROFILE(name) L14_Scope L14_CAT(l14_scope_, __LINE__)(name)

static float L14_Average(const float *v, int n)
{
    float s = 0.0f; for (int i = 0; i < n; i++) s += v[i];
    return n ? s / n : 0.0f;
}

// ======================================================================================
//  Fake work so there is something to measure
// ======================================================================================
static int            L14_workIterations = 0;   // slider-controlled
static volatile float L14_sink = 0.0f;           // volatile: stops the optimiser deleting the loop

static void L14_FakeWork(void)
{
    float acc = 0.0f;
    for (int i = 0; i < L14_workIterations; i++)
        acc += sinf((float)i * 0.001f) * cosf((float)i * 0.0007f);
    L14_sink = acc;
}

// ======================================================================================
//  Profiler window
// ======================================================================================
static void L14_ProfilerWindow(void)
{
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(440, 640), ImGuiCond_FirstUseEver);
    ImGui::Begin("Profiler");

    // ---- stats over the whole ring buffer ----
    float minMs = 1e9f, maxMs = 0.0f, sum = 0.0f;
    for (int i = 0; i < FRAME_HISTORY; i++)
    {
        float v = gFrameTimes[i];
        if (v < minMs) minMs = v;
        if (v > maxMs) maxMs = v;
        sum += v;
    }
    float avgMs = sum / FRAME_HISTORY;
    ImGui::Text("FPS %.1f   frame %.2f ms", avgMs > 0.0f ? 1000.0f / avgMs : 0.0f, avgMs);
    ImGui::Text("min %.2f   avg %.2f   max %.2f ms", minMs, avgMs, maxMs);
    ImGui::TextDisabled("(%d frames of history, ImGui says %.1f FPS)", FRAME_HISTORY, ImGui::GetIO().Framerate);

    // ---- frame time graph ----
    ImGui::SeparatorText("Frame time");
    static bool autoScale = false;
    static float scaleMax = 33.3f;
    ImGui::Checkbox("Auto scale", &autoScale);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    ImGui::SliderFloat("Max ms", &scaleMax, 5.0f, 100.0f, "%.1f");

    char overlay[64];
    _snprintf(overlay, sizeof(overlay) - 1, "avg %.2f ms", avgMs);
    overlay[sizeof(overlay) - 1] = 0;

    // values_offset = gFrameTimeOffset: the plot starts at the OLDEST sample, so the newest
    // value is always drawn on the right edge and the graph scrolls left.
    // FLT_MAX for scale min/max = let ImGui auto-fit.
    ImGui::PlotLines("##frametimes", gFrameTimes, FRAME_HISTORY, gFrameTimeOffset, overlay,
                     0.0f, autoScale ? FLT_MAX : scaleMax, ImVec2(-FLT_MIN, 90));

    ImGui::PlotHistogram("##framehist", gFrameTimes, FRAME_HISTORY, gFrameTimeOffset, "same data as bars",
                         0.0f, autoScale ? FLT_MAX : scaleMax, ImVec2(-FLT_MIN, 60));

    // ---- frame budget ----
    ImGui::SeparatorText("Budget");
    static int targetFps = 60;
    ImGui::SetNextItemWidth(150);
    ImGui::SliderInt("Target FPS", &targetFps, 30, 240);
    float budgetMs = 1000.0f / targetFps;
    float used = avgMs / budgetMs;                         // 1.0 = exactly on budget
    char budgetText[64];
    _snprintf(budgetText, sizeof(budgetText) - 1, "%.2f / %.2f ms (%.0f%%)", avgMs, budgetMs, used * 100.0f);
    budgetText[sizeof(budgetText) - 1] = 0;
    // colour the bar: green under budget, red over
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, used <= 1.0f ? ImVec4(0.3f, 0.8f, 0.4f, 1.0f) : ImVec4(0.9f, 0.3f, 0.25f, 1.0f));
    ImGui::ProgressBar(used > 1.0f ? 1.0f : used, ImVec2(-FLT_MIN, 0), budgetText);
    ImGui::PopStyleColor();

    // ---- settings that change frame time ----
    ImGui::SeparatorText("Experiments");
    ImGui::Checkbox("VSync", &gVSync);
    ImGui::SameLine();
    ImGui::TextDisabled(gVSync ? "(frame time locked to the monitor)" : "(running as fast as possible)");
    ImGui::SetNextItemWidth(220);
    ImGui::SliderInt("Fake work", &L14_workIterations, 0, 3000000, "%d iters", ImGuiSliderFlags_Logarithmic);

    // ---- per-section timings from the scope profiler ----
    ImGui::SeparatorText("CPU sections (last frame)");
    float total = 0.0f;
    for (int i = 0; i < L14_sectionCount; i++) total += L14_sections[i].lastMs;
    for (int i = 0; i < L14_sectionCount; i++)
    {
        L14_Section *s = &L14_sections[i];
        ImGui::PushID(i);
        float avg = L14_Average(s->history, FRAME_HISTORY);
        ImGui::Text("%-12s %7.3f ms  (avg %.3f)", s->name, s->lastMs, avg);
        // fraction of the measured total, as a thin bar
        ImGui::ProgressBar(total > 0.0f ? s->lastMs / total : 0.0f, ImVec2(-FLT_MIN, 4), "");
        ImGui::PlotLines("##hist", s->history, FRAME_HISTORY, s->historyOffset, NULL, 0.0f, FLT_MAX, ImVec2(-FLT_MIN, 28));
        ImGui::PopID();
    }
    ImGui::TextDisabled("Note: the 'Profiler UI' row shows the previous frame.");
    ImGui::End();
}

// ======================================================================================
//  Corner HUD
// ======================================================================================
static int  L14_hudCorner = 1;     // 0 = top-left, 1 = top-right, 2 = bottom-left, 3 = bottom-right, -1 = free
static bool L14_showHud   = true;

static void L14_Hud(void)
{
    if (!L14_showHud) return;

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav;
    if (L14_hudCorner != -1)
    {
        // WorkPos/WorkSize = the usable part of the screen (excludes a main menu bar, if any)
        const ImGuiViewport *vp = ImGui::GetMainViewport();
        const float pad = 10.0f;
        bool right  = (L14_hudCorner & 1) != 0;
        bool bottom = (L14_hudCorner & 2) != 0;
        ImVec2 pos(right  ? vp->WorkPos.x + vp->WorkSize.x - pad : vp->WorkPos.x + pad,
                   bottom ? vp->WorkPos.y + vp->WorkSize.y - pad : vp->WorkPos.y + pad);
        // pivot = which point of the window sits at 'pos': (1,1) = its bottom-right corner
        ImVec2 pivot(right ? 1.0f : 0.0f, bottom ? 1.0f : 0.0f);
        ImGui::SetNextWindowPos(pos, ImGuiCond_Always, pivot);
        flags |= ImGuiWindowFlags_NoMove;
    }
    ImGui::SetNextWindowBgAlpha(0.35f);           // translucent background

    if (ImGui::Begin("##PerfHUD", NULL, flags))
    {
        float avg = Engine_AverageFrameTimeMs();
        ImGui::Text("%.0f FPS  %.2f ms", avg > 0.0f ? 1000.0f / avg : 0.0f, avg);
        ImGui::PlotLines("##hudplot", gFrameTimes, FRAME_HISTORY, gFrameTimeOffset, NULL, 0.0f, 40.0f, ImVec2(160, 30));
        ImGui::Text("Entities %d  VSync %s", gEntityCount, gVSync ? "on" : "off");
        ImGui::TextDisabled("right-click for options");

        // Right-click anywhere on the HUD window
        if (ImGui::BeginPopupContextWindow())
        {
            if (ImGui::MenuItem("Custom position", NULL, L14_hudCorner == -1)) L14_hudCorner = -1;
            if (ImGui::MenuItem("Top-left",        NULL, L14_hudCorner == 0))  L14_hudCorner = 0;
            if (ImGui::MenuItem("Top-right",       NULL, L14_hudCorner == 1))  L14_hudCorner = 1;
            if (ImGui::MenuItem("Bottom-left",     NULL, L14_hudCorner == 2))  L14_hudCorner = 2;
            if (ImGui::MenuItem("Bottom-right",    NULL, L14_hudCorner == 3))  L14_hudCorner = 3;
            ImGui::Separator();
            ImGui::MenuItem("VSync", NULL, &gVSync);
            if (ImGui::MenuItem("Close HUD")) L14_showHud = false;
            ImGui::EndPopup();
        }
    }
    ImGui::End();
}

void Lesson_Init(void)
{
    QueryPerformanceFrequency(&L14_freq);
    Engine_Log("Lesson 14: profiler ready (QPC frequency %lld Hz)", (long long)L14_freq.QuadPart);
}

void Lesson_UI(void)
{
    L14_PROFILE("Lesson_UI");             // whole function (closes at the final brace)

    L14_Begin("Fake work");
    L14_FakeWork();
    L14_End("Fake work");

    L14_Begin("Profiler UI");
    L14_ProfilerWindow();
    L14_End("Profiler UI");

    {
        L14_PROFILE("HUD");               // scoped version: ends at the closing brace
        L14_Hud();
    }

    if (!L14_showHud)
    {
        ImGui::SetNextWindowPos(ImVec2(480, 20), ImGuiCond_FirstUseEver);
        ImGui::Begin("HUD closed", NULL, ImGuiWindowFlags_AlwaysAutoResize);
        if (ImGui::Button("Show HUD again")) L14_showHud = true;
        ImGui::End();
    }
}
