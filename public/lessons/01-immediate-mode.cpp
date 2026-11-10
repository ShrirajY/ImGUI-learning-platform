// Lesson 1 - Immediate mode: the UI is a function you call every frame
// Build & Run (Ctrl+Enter), then move the mouse over the 3D view and over this window
// and watch the ImGuiIO values change live.

void Lesson_Init(void)
{
    // Runs ONCE, after ImGui::CreateContext(). Good place for one-time setup.
    Engine_Log("Lesson 1 started - immediate mode");
}

void Lesson_UI(void)
{
    // ---- Our own state --------------------------------------------------------------
    // ImGui does NOT store these for us. 'static' simply keeps them alive between frames,
    // exactly like a global variable in OGL.c would.
    static int  counter      = 0;
    static bool showIoWindow = true;

    // ---- Window 1: the classic counter -----------------------------------------------
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320, 0), ImGuiCond_FirstUseEver);   // 0 = auto height
    ImGui::Begin("Immediate Mode");

    // No Button object, no callback, no event queue:
    // this line DRAWS the button and RETURNS true on the frame it was clicked.
    if (ImGui::Button("Click me"))
        counter++;
    ImGui::SameLine();
    if (ImGui::Button("Reset"))
        counter = 0;

    ImGui::Text("counter = %d", counter);

    // The UI is just code, so ordinary C control flow decides what exists this frame.
    if (counter >= 5)
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "This line only exists while counter >= 5");

    ImGui::Separator();
    ImGui::Checkbox("Show ImGuiIO window", &showIoWindow);   // edits OUR bool directly
    ImGui::Text("Engine frame #%d", gFrameCount);            // changes every frame - no 'refresh' needed
    ImGui::End();

    // ---- Window 2: peek into ImGuiIO --------------------------------------------------
    // ImGuiIO is the bridge between your platform (Win32) and ImGui: inputs go in,
    // 'want capture' flags and stats come out.
    if (showIoWindow)
    {
        ImGuiIO &io = ImGui::GetIO();

        ImGui::SetNextWindowPos(ImVec2(20, 220), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(320, 0), ImGuiCond_FirstUseEver);
        ImGui::Begin("ImGuiIO (live)", &showIoWindow);       // &showIoWindow adds a close [x]

        ImGui::SeparatorText("Filled in by the backends");
        ImGui::Text("io.DisplaySize   %.0f x %.0f", io.DisplaySize.x, io.DisplaySize.y);
        ImGui::Text("io.DeltaTime     %.4f s", io.DeltaTime);
        if (ImGui::IsMousePosValid())
            ImGui::Text("io.MousePos      %.0f, %.0f", io.MousePos.x, io.MousePos.y);
        else
            ImGui::TextDisabled("io.MousePos      <outside window>");
        ImGui::Text("io.MouseDown[0]  %s", io.MouseDown[0] ? "pressed" : "-");

        ImGui::SeparatorText("Computed by ImGui");
        ImGui::Text("io.Framerate     %.1f FPS (%.2f ms)", io.Framerate, 1000.0f / io.Framerate);

        // The two flags every engine must respect:
        ImVec4 on  = ImVec4(1.0f, 0.6f, 0.2f, 1.0f);
        ImVec4 off = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
        ImGui::TextColored(io.WantCaptureMouse ? on : off,
                           "io.WantCaptureMouse    = %s", io.WantCaptureMouse ? "true" : "false");
        ImGui::TextColored(io.WantCaptureKeyboard ? on : off,
                           "io.WantCaptureKeyboard = %s", io.WantCaptureKeyboard ? "true" : "false");
        ImGui::TextWrapped("Hover a window: WantCaptureMouse turns true and the engine stops orbiting "
                           "the camera / picking entities. Move to the 3D view: it turns false.");

        ImGui::SeparatorText("Last frame's draw data");
        ImGui::Text("%d windows, %d vertices, %d indices",
                    io.MetricsRenderWindows, io.MetricsRenderVertices, io.MetricsRenderIndices);
        ImGui::End();
    }
}
