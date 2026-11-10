// Your First Window
// Everything below runs inside the engine's OGL.c. Change something, then Build & Run (Ctrl+Enter).

void Lesson_Init(void)
{
    Engine_Log("Lesson 3 started");
}

void Lesson_UI(void)
{
    static int  clickCount = 0;        // 'static' = keeps its value between frames
    static bool showAbout  = false;

    // Optional: position/size used the FIRST time the window appears
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(340, 0), ImGuiCond_FirstUseEver);

    ImGui::Begin("Engine Controls");                     // ---- window starts

    ImGui::Text("Hello, engine! Frame %d", gFrameCount);
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Entities in scene: %d", gEntityCount);
    ImGui::BulletText("Right-drag in the 3D view to orbit");
    ImGui::Separator();

    // Checkbox edits a bool directly through a pointer
    ImGui::Checkbox("Show grid", &gShowGrid);
    ImGui::SameLine();                                   // next widget on the same line
    ImGui::Checkbox("Wireframe", &gWireframe);
    ImGui::Checkbox("Pause animation", &gPaused);

    ImGui::Spacing();

    // Button returns true only on the frame it was clicked
    if (ImGui::Button("Reset scene"))
    {
        Engine_ResetScene();
        clickCount++;
        Engine_Log("Scene reset (%d times)", clickCount);
    }
    ImGui::SameLine();
    if (ImGui::Button("About"))
        showAbout = true;

    ImGui::Text("Reset clicked %d times", clickCount);

    // Radio buttons: several choices writing into one int
    static int speedMode = 1;
    ImGui::SeparatorText("Animation speed");
    if (ImGui::RadioButton("Slow",   &speedMode, 0)) gTimeScale = 0.25f;
    ImGui::SameLine();
    if (ImGui::RadioButton("Normal", &speedMode, 1)) gTimeScale = 1.0f;
    ImGui::SameLine();
    if (ImGui::RadioButton("Fast",   &speedMode, 2)) gTimeScale = 3.0f;

    ImGui::End();                                        // ---- window ends (ALWAYS call it)

    // A second window with a close button: passing &showAbout adds the [x]
    if (showAbout)
    {
        ImGui::Begin("About", &showAbout, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("ImGui Lab - lesson 3");
        ImGui::TextWrapped("Click the [x] in the title bar to close me. ImGui writes false into showAbout for you.");
        ImGui::End();
    }
}
