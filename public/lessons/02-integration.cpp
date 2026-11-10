// Lesson 2 - Integration
// The engine's OGL.c already contains every IMGUI section from the starter OGL.c:
//   headers, WndProc forwarding, IMGUI_initialize(), IMGUI_display(), IMGUI_uninitialize().
// The only part left to you is the UI code in the middle of IMGUI_display(),
// between ImGui::NewFrame() and ImGui::Render(). In this platform that is Lesson_UI().
//
// Your job in this lesson: open the Demo window and EXPLORE it. It is the official
// reference for every widget, and its source (imgui_demo.cpp) shows how each one is used.

void Lesson_Init(void)
{
    // Proof that ImGui is alive: CreateContext() already ran before this was called.
    Engine_Log("Lesson 2 - Dear ImGui %s is integrated and running", ImGui::GetVersion());
}

void Lesson_UI(void)
{
    // Toggles for ImGui's built-in tool windows (they live in imgui_demo.cpp / imgui.cpp)
    static bool showDemo    = true;
    static bool showMetrics = false;
    static bool showAbout   = false;

    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(330, 0), ImGuiCond_FirstUseEver);
    ImGui::Begin("Integration Check");

    ImGui::Text("Dear ImGui %s", ImGui::GetVersion());
    ImGui::TextDisabled("Backends: %s + %s",
                        ImGui::GetIO().BackendPlatformName ? ImGui::GetIO().BackendPlatformName : "?",
                        ImGui::GetIO().BackendRendererName ? ImGui::GetIO().BackendRendererName : "?");
    ImGui::Separator();

    // Each tool window takes a bool* so its own [x] button can close it again
    ImGui::Checkbox("Demo window (the reference!)", &showDemo);
    ImGui::Checkbox("Metrics / Debugger",           &showMetrics);
    ImGui::Checkbox("About Dear ImGui",             &showAbout);

    ImGui::SeparatorText("Pitfall test");
    // The scene uses glPolygonMode(GL_LINE) for wireframe. ImGui still draws solid because
    // the OpenGL2 backend saves your GL state, forces GL_FILL, draws, then restores it
    // (and the engine also resets GL_FILL itself - good hygiene). Try it:
    ImGui::Checkbox("Wireframe scene", &gWireframe);
    ImGui::SetItemTooltip("The 3D scene goes wireframe, ImGui stays solid:\n"
                          "ImGui_ImplOpenGL2_RenderDrawData() sets up its own GL state.");

    ImGui::SeparatorText("Input routing");
    ImGuiIO &io = ImGui::GetIO();
    ImGui::Text("WantCaptureMouse:    %s", io.WantCaptureMouse    ? "yes (UI owns mouse)"    : "no (engine owns mouse)");
    ImGui::Text("WantCaptureKeyboard: %s", io.WantCaptureKeyboard ? "yes (UI owns keyboard)" : "no (WASD moves camera)");

    ImGui::Separator();
    ImGui::Text("%.1f FPS", io.Framerate);
    ImGui::End();

    // These functions call Begin()/End() themselves - just call them every frame you want them visible.
    if (showDemo)    ImGui::ShowDemoWindow(&showDemo);
    if (showMetrics) ImGui::ShowMetricsWindow(&showMetrics);
    if (showAbout)   ImGui::ShowAboutWindow(&showAbout);
}
