// Lesson 12 - An editor / in-game console
//   - Reads the engine log (Engine_GetLogCount / Engine_GetLog), coloured by level
//   - Level filter checkboxes + ImGuiTextFilter search + auto-scroll
//   - Scrolling region with a reserved footer for the input line
//   - Command line: EnterReturnsTrue + keep focus, a tiny command parser,
//     history on Up/Down and Tab completion through an InputText callback
// Toggle the console with the ` key (left of 1). Type "help" to start.

// ======================================================================================
//  Console state
// ======================================================================================
#define L12_HISTORY_MAX 32
#define L12_INPUT_SIZE  256

static bool  L12_open        = true;
static bool  L12_showLevel[3] = { true, true, true };      // LOG_INFO, LOG_WARN, LOG_ERROR
static bool  L12_autoScroll  = true;
static bool  L12_scrollToBottom = false;                   // force once (after a command)
static bool  L12_showTime    = true;
static ImGuiTextFilter L12_filter;
static char  L12_input[L12_INPUT_SIZE] = "";

static char  L12_history[L12_HISTORY_MAX][L12_INPUT_SIZE]; // oldest first
static int   L12_historyCount = 0;
static int   L12_historyPos   = -1;                        // -1 = editing a new line

static const char *L12_commands[] = { "help", "clear", "list", "spawn", "delete", "select",
                                      "focus", "timescale", "pause", "grid", "wireframe", "reset" };
static const int   L12_commandCount = (int)(sizeof(L12_commands) / sizeof(L12_commands[0]));

// ======================================================================================
//  Small string helpers (C style, no std::string)
// ======================================================================================
static void L12_Trim(char *s)                               // in-place trim of spaces
{
    char *p = s;
    while (*p == ' ' || *p == '\t') p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t')) s[--n] = 0;
}

static int L12_FindEntity(const char *name)                 // case-insensitive, -1 if not found
{
    for (int i = 0; i < gEntityCount; i++)
        if (_stricmp(gEntities[i].name, name) == 0) return i;
    return -1;
}

static int L12_FindMesh(const char *name)
{
    for (int m = 0; m < MESH_COUNT; m++)
        if (_stricmp(gMeshNames[m], name) == 0) return m;
    return -1;
}

static bool L12_ParseOnOff(const char *arg, bool current)   // "on" / "off" / "" (= toggle)
{
    if (_stricmp(arg, "on") == 0 || strcmp(arg, "1") == 0)   return true;
    if (_stricmp(arg, "off") == 0 || strcmp(arg, "0") == 0)  return false;
    return !current;
}

// ======================================================================================
//  Command parser:  <command> <rest of line>
// ======================================================================================
static void L12_Execute(const char *line)
{
    char buf[L12_INPUT_SIZE];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    L12_Trim(buf);
    if (!buf[0]) return;

    Engine_Log("> %s", buf);                                 // echo (note: "%s", never the raw text!)

    // ---- history: drop an identical older entry, append to the end ----
    for (int h = L12_historyCount - 1; h >= 0; h--)
        if (_stricmp(L12_history[h], buf) == 0)
        {
            for (int k = h; k < L12_historyCount - 1; k++) strcpy(L12_history[k], L12_history[k + 1]);
            L12_historyCount--;
            break;
        }
    if (L12_historyCount == L12_HISTORY_MAX)                 // full: forget the oldest
    {
        for (int k = 0; k < L12_HISTORY_MAX - 1; k++) strcpy(L12_history[k], L12_history[k + 1]);
        L12_historyCount--;
    }
    strcpy(L12_history[L12_historyCount++], buf);
    L12_historyPos = -1;

    // ---- split "cmd args..." ----
    char *args = buf;
    while (*args && *args != ' ') args++;
    if (*args) { *args = 0; args++; }                        // buf = command, args = the rest
    L12_Trim(args);
    const char *cmd = buf;

    if (_stricmp(cmd, "help") == 0)
    {
        Engine_Log("Commands:");
        Engine_Log("  help                      this text");
        Engine_Log("  clear                     clear the log");
        Engine_Log("  list                      list entities");
        Engine_Log("  spawn <mesh> [name]       mesh = cube|sphere|pyramid|cylinder|plane");
        Engine_Log("  delete <name>             remove an entity");
        Engine_Log("  select <name> / focus     select / focus camera on selection");
        Engine_Log("  timescale <x>             e.g. timescale 0.5");
        Engine_Log("  pause | grid | wireframe  [on|off], no argument = toggle");
        Engine_Log("  reset                     restore the default scene");
        Engine_Log("Up/Down = history, Tab = complete, ` = toggle console");
    }
    else if (_stricmp(cmd, "clear") == 0)
    {
        Engine_ClearLog();
    }
    else if (_stricmp(cmd, "list") == 0)
    {
        for (int i = 0; i < gEntityCount; i++)
        {
            const Entity *e = &gEntities[i];
            Engine_Log("  [%2d] %-20s %-9s (%.1f, %.1f, %.1f)%s", i, e->name, gMeshNames[e->mesh],
                       e->position[0], e->position[1], e->position[2], e->visible ? "" : "  hidden");
        }
        Engine_Log("%d entities", gEntityCount);
    }
    else if (_stricmp(cmd, "spawn") == 0)
    {
        // first word = mesh, the rest (optional) = name
        char meshName[32] = "";
        const char *name = args;
        int n = 0;
        while (*name && *name != ' ' && n < 31) meshName[n++] = *name++;
        meshName[n] = 0;
        while (*name == ' ') name++;

        int mesh = L12_FindMesh(meshName);
        if (mesh < 0) { Engine_LogError("spawn: unknown mesh '%s' (try: spawn cube)", meshName); return; }
        int i = Engine_AddEntity(name[0] ? name : gMeshNames[mesh], mesh);
        if (i < 0) { Engine_LogError("spawn: scene is full"); return; }
        for (int k = 0; k < 3; k++) gEntities[i].position[k] = gCamera.target[k];
        gSelectedEntity = i;
        Engine_Log("Spawned '%s' (%s) at camera target", gEntities[i].name, gMeshNames[mesh]);
    }
    else if (_stricmp(cmd, "delete") == 0)
    {
        int i = L12_FindEntity(args);
        if (i < 0) { Engine_LogError("delete: no entity named '%s'", args); return; }
        Engine_Log("Deleted '%s'", gEntities[i].name);
        Engine_RemoveEntity(i);
    }
    else if (_stricmp(cmd, "select") == 0)
    {
        int i = L12_FindEntity(args);
        if (i < 0) { Engine_LogError("select: no entity named '%s'", args); return; }
        gSelectedEntity = i;
        Engine_Log("Selected '%s'", gEntities[i].name);
    }
    else if (_stricmp(cmd, "focus") == 0)
    {
        if (gSelectedEntity < 0) { Engine_LogWarn("focus: nothing selected"); return; }
        Engine_FocusEntity(gSelectedEntity);
    }
    else if (_stricmp(cmd, "timescale") == 0)
    {
        if (!args[0]) { Engine_Log("timescale = %.2f", gTimeScale); return; }
        float v = (float)atof(args);
        if (v < 0.0f || v > 10.0f) { Engine_LogError("timescale: expected 0..10, got '%s'", args); return; }
        gTimeScale = v;
        Engine_Log("timescale = %.2f", gTimeScale);
    }
    else if (_stricmp(cmd, "pause") == 0)     { gPaused    = L12_ParseOnOff(args, gPaused);    Engine_Log("pause %s", gPaused ? "on" : "off"); }
    else if (_stricmp(cmd, "grid") == 0)      { gShowGrid  = L12_ParseOnOff(args, gShowGrid);  Engine_Log("grid %s", gShowGrid ? "on" : "off"); }
    else if (_stricmp(cmd, "wireframe") == 0) { gWireframe = L12_ParseOnOff(args, gWireframe); Engine_Log("wireframe %s", gWireframe ? "on" : "off"); }
    else if (_stricmp(cmd, "reset") == 0)     { Engine_ResetScene(); Engine_Log("Scene reset"); }
    else
    {
        Engine_LogError("Unknown command '%s'. Type 'help'.", cmd);
    }
}

// ======================================================================================
//  InputText callback: history (Up/Down) and completion (Tab)
// ======================================================================================
static int L12_InputCallback(ImGuiInputTextCallbackData *data)
{
    if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory)
    {
        int prev = L12_historyPos;
        if (data->EventKey == ImGuiKey_UpArrow)
        {
            if (L12_historyPos == -1)      L12_historyPos = L12_historyCount - 1;   // start at newest
            else if (L12_historyPos > 0)   L12_historyPos--;
        }
        else if (data->EventKey == ImGuiKey_DownArrow)
        {
            if (L12_historyPos != -1 && ++L12_historyPos >= L12_historyCount)
                L12_historyPos = -1;                                              // past newest = empty line
        }
        if (prev != L12_historyPos)
        {
            // Replace the whole buffer. DeleteChars/InsertChars keep InputText's state consistent.
            data->DeleteChars(0, data->BufTextLen);
            if (L12_historyPos >= 0)
                data->InsertChars(0, L12_history[L12_historyPos]);
        }
    }
    else if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion)
    {
        // Complete the FIRST word only (the command name)
        int wordEnd = 0;
        while (wordEnd < data->BufTextLen && data->Buf[wordEnd] != ' ') wordEnd++;
        if (wordEnd == data->BufTextLen && wordEnd > 0)                // still typing the command
        {
            int matches = 0, last = -1;
            for (int c = 0; c < L12_commandCount; c++)
                if (_strnicmp(L12_commands[c], data->Buf, (size_t)wordEnd) == 0) { matches++; last = c; }

            if (matches == 1)
            {
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, L12_commands[last]);
                data->InsertChars(data->CursorPos, " ");
            }
            else if (matches > 1)
            {
                char list[256] = "";
                for (int c = 0; c < L12_commandCount; c++)
                    if (_strnicmp(L12_commands[c], data->Buf, (size_t)wordEnd) == 0)
                    { strncat(list, L12_commands[c], sizeof(list) - strlen(list) - 2); strcat(list, " "); }
                Engine_Log("Possible: %s", list);
            }
        }
    }
    return 0;
}

// ======================================================================================
//  Console window
// ======================================================================================
static void L12_Console(void)
{
    if (!L12_open) return;

    ImGui::SetNextWindowPos(ImVec2(20, 470), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(900, 360), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Console", &L12_open)) { ImGui::End(); return; }

    // ---- Count lines per level for the checkbox labels ----
    int count = Engine_GetLogCount();
    int perLevel[3] = { 0, 0, 0 };
    for (int i = 0; i < count; i++)
    {
        const LogLine *l = Engine_GetLog(i);
        if (l && l->level >= 0 && l->level < 3) perLevel[l->level]++;
    }

    // ---- Toolbar ----
    char label[32];
    snprintf(label, sizeof(label), "Info (%d)###info", perLevel[LOG_INFO]);     // ### = stable ID
    ImGui::Checkbox(label, &L12_showLevel[LOG_INFO]);  ImGui::SameLine();
    snprintf(label, sizeof(label), "Warn (%d)###warn", perLevel[LOG_WARN]);
    ImGui::Checkbox(label, &L12_showLevel[LOG_WARN]);  ImGui::SameLine();
    snprintf(label, sizeof(label), "Error (%d)###err", perLevel[LOG_ERROR]);
    ImGui::Checkbox(label, &L12_showLevel[LOG_ERROR]); ImGui::SameLine();
    ImGui::Checkbox("Time", &L12_showTime);            ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &L12_autoScroll);   ImGui::SameLine();
    bool copy = ImGui::Button("Copy");                 ImGui::SameLine();
    if (ImGui::Button("Clear")) Engine_ClearLog();
    L12_filter.Draw("Filter (inc,-exc)", 220.0f);

    ImGui::Separator();

    // ---- Scrolling log region: leave exactly one framed row for the input line ----
    const float footer = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    if (ImGui::BeginChild("LogRegion", ImVec2(0, -footer), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
    {
        if (copy) ImGui::LogToClipboard();             // everything drawn until LogFinish() is copied
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));   // tighter lines

        count = Engine_GetLogCount();                  // re-read: buttons above may have cleared it
        for (int i = 0; i < count; i++)
        {
            const LogLine *l = Engine_GetLog(i);
            if (!l) continue;
            if (l->level < 0 || l->level > 2 || !L12_showLevel[l->level]) continue;
            if (!L12_filter.PassFilter(l->text)) continue;

            ImVec4 col;
            bool hasCol = true;
            if      (l->level == LOG_ERROR)                  col = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
            else if (l->level == LOG_WARN)                   col = ImVec4(1.0f, 0.8f, 0.3f, 1.0f);
            else if (l->text[0] == '>' && l->text[1] == ' ') col = ImVec4(0.5f, 0.8f, 1.0f, 1.0f);  // echoed command
            else hasCol = false;

            if (L12_showTime)
            {
                ImGui::TextDisabled("[%7.2f]", l->time);
                ImGui::SameLine();
            }
            if (hasCol) ImGui::PushStyleColor(ImGuiCol_Text, col);
            ImGui::TextUnformatted(l->text);           // NOT Text(l->text): log lines may contain '%'
            if (hasCol) ImGui::PopStyleColor();
        }

        ImGui::PopStyleVar();
        if (copy) ImGui::LogFinish();

        // Stick to the bottom only if we were already at the bottom (so the user can scroll up to read)
        if (L12_scrollToBottom || (L12_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
            ImGui::SetScrollHereY(1.0f);
        L12_scrollToBottom = false;
    }
    ImGui::EndChild();
    ImGui::Separator();

    // ---- Command line ----
    bool reclaimFocus = false;
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue      // true only on Enter
                              | ImGuiInputTextFlags_EscapeClearsAll       // Esc clears the line
                              | ImGuiInputTextFlags_CallbackHistory       // Up/Down -> callback
                              | ImGuiInputTextFlags_CallbackCompletion;   // Tab -> callback
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputTextWithHint("##cmd", "type a command, e.g. help", L12_input, sizeof(L12_input),
                                 flags, L12_InputCallback))
    {
        L12_Execute(L12_input);
        L12_input[0] = 0;
        L12_scrollToBottom = true;
        reclaimFocus = true;                           // Enter deactivates the field; take it back
    }

    ImGui::SetItemDefaultFocus();                      // focus the input when the window appears
    if (reclaimFocus)
        ImGui::SetKeyboardFocusHere(-1);               // -1 = the PREVIOUS widget (the input)

    ImGui::End();
}

// ======================================================================================
//  Lesson hooks
// ======================================================================================
void Lesson_Init(void)
{
    Engine_Log("Lesson 12 started - type 'help' in the console");
    Engine_LogWarn("This is a warning (try the Warn checkbox)");
    Engine_LogError("This is an error (try the Error checkbox)");
}

void Lesson_UI(void)
{
    // ` toggles the console (not while typing in some other text field)
    if (!ImGui::GetIO().WantTextInput && ImGui::Shortcut(ImGuiKey_GraveAccent, ImGuiInputFlags_RouteGlobal))
        L12_open = !L12_open;

    L12_Console();

    // Tiny helper window so it's easy to generate log traffic
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(260, 0), ImGuiCond_FirstUseEver);
    ImGui::Begin("Log Spammer", NULL, ImGuiWindowFlags_AlwaysAutoResize);
    if (ImGui::Button("Info"))  Engine_Log("Info at frame %d", gFrameCount);
    ImGui::SameLine();
    if (ImGui::Button("Warn"))  Engine_LogWarn("Texture '%s' is not power of two (pretend)", gTextures[0].name);
    ImGui::SameLine();
    if (ImGui::Button("Error")) Engine_LogError("Failed to load 'missing.obj' (pretend)");
    if (ImGui::Button("100 lines"))
        for (int k = 0; k < 100; k++) Engine_Log("Spam line %d (100%% fake)", k);
    ImGui::Checkbox("Console open", &L12_open);
    ImGui::End();
}
