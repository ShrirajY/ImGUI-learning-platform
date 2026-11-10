// Lesson 15 - Custom drawing with ImDrawList
//   "Draw List Basics" - every primitive on a small canvas
//   "Camera Widgets"   - a custom knob and a 2D joystick that drive the camera
//   "Mini-map"         - top-down view of the scene; click/drag to move the camera target
//   3D overlays        - entity name labels + screen-space bounding brackets (background list)
//                        and an orientation gizmo in the top-right corner
//
// Note: OGL.c does not define IMGUI_DEFINE_MATH_OPERATORS, so ImVec2 has no + - * operators.
//       We use tiny helpers instead (L15_Add etc).

static ImVec2 L15_Add(ImVec2 a, ImVec2 b)    { return ImVec2(a.x + b.x, a.y + b.y); }
static ImVec2 L15_Sub(ImVec2 a, ImVec2 b)    { return ImVec2(a.x - b.x, a.y - b.y); }
static ImVec2 L15_Scale(ImVec2 a, float s)   { return ImVec2(a.x * s, a.y * s); }
static float  L15_Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

static const float L15_PI = 3.14159265358979f;

// Overlay toggles
static bool L15_showLabels  = true;
static bool L15_showBrackets = true;
static bool L15_showGizmo   = true;

// --------------------------------------------------------------------------------------
//  Camera basis (same maths as the engine's orbit camera)
// --------------------------------------------------------------------------------------
static void L15_CameraEye(float eye[3])
{
    float yaw = gCamera.yaw * L15_PI / 180.0f, pitch = gCamera.pitch * L15_PI / 180.0f;
    eye[0] = gCamera.target[0] + gCamera.distance * cosf(pitch) * sinf(yaw);
    eye[1] = gCamera.target[1] + gCamera.distance * sinf(pitch);
    eye[2] = gCamera.target[2] + gCamera.distance * cosf(pitch) * cosf(yaw);
}

// forward = from eye towards target, right = forward x worldUp, up = right x forward
static void L15_CameraBasis(float fwd[3], float right[3], float up[3])
{
    float eye[3];
    L15_CameraEye(eye);
    for (int k = 0; k < 3; k++) fwd[k] = gCamera.target[k] - eye[k];
    float len = sqrtf(fwd[0] * fwd[0] + fwd[1] * fwd[1] + fwd[2] * fwd[2]);
    for (int k = 0; k < 3; k++) fwd[k] /= len;
    right[0] = -fwd[2]; right[1] = 0.0f; right[2] = fwd[0];            // fwd x (0,1,0)
    len = sqrtf(right[0] * right[0] + right[2] * right[2]);
    right[0] /= len; right[2] /= len;
    up[0] = right[1] * fwd[2] - right[2] * fwd[1];
    up[1] = right[2] * fwd[0] - right[0] * fwd[2];
    up[2] = right[0] * fwd[1] - right[1] * fwd[0];
}

// ======================================================================================
//  1. Primitive showcase
// ======================================================================================
static void L15_BasicsWindow(void)
{
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360, 330), ImGuiCond_FirstUseEver);
    ImGui::Begin("Draw List Basics");

    static float thickness = 2.0f;
    static float col[4] = { 1.0f, 0.75f, 0.25f, 1.0f };
    ImGui::SliderFloat("Thickness", &thickness, 1.0f, 8.0f);
    ImGui::ColorEdit4("Colour", col, ImGuiColorEditFlags_NoInputs);
    ImU32 c = ImGui::ColorConvertFloat4ToU32(ImVec4(col[0], col[1], col[2], col[3]));

    // Reserve a canvas: InvisibleButton takes the space so layout continues below it
    ImVec2 p0 = ImGui::GetCursorScreenPos();              // SCREEN coordinates, top-left of canvas
    ImVec2 size(ImGui::GetContentRegionAvail().x, 200.0f);
    if (size.x < 50.0f) size.x = 50.0f;
    ImGui::InvisibleButton("##canvas", size);
    ImVec2 p1 = L15_Add(p0, size);

    ImDrawList *dl = ImGui::GetWindowDrawList();          // drawn with this window, clipped to it
    dl->AddRectFilled(p0, p1, IM_COL32(25, 28, 36, 255), 4.0f);
    dl->AddRect(p0, p1, IM_COL32(90, 100, 120, 255), 4.0f);

    dl->PushClipRect(p0, p1, true);                       // keep shapes inside the canvas
    float x = p0.x + 30.0f, y = p0.y + 40.0f, step = 65.0f;
    dl->AddLine(ImVec2(x - 20, y - 20), ImVec2(x + 20, y + 20), c, thickness);
    dl->AddRect(ImVec2(x + step - 20, y - 20), ImVec2(x + step + 20, y + 20), c, 0.0f, 0, thickness);
    dl->AddRectFilled(ImVec2(x + 2 * step - 20, y - 20), ImVec2(x + 2 * step + 20, y + 20), c, 8.0f);
    dl->AddCircle(ImVec2(x + 3 * step, y), 20.0f, c, 0, thickness);
    dl->AddCircleFilled(ImVec2(x + 4 * step, y), 20.0f, c);

    float y2 = p0.y + 120.0f;
    dl->AddTriangleFilled(ImVec2(x, y2 - 22), ImVec2(x + 22, y2 + 20), ImVec2(x - 22, y2 + 20), c);
    dl->AddBezierCubic(ImVec2(x + step - 25, y2 + 20), ImVec2(x + step - 10, y2 - 50),
                       ImVec2(x + step + 50, y2 + 60), ImVec2(x + step + 70, y2 - 20), c, thickness);
    // Path API: build a path point by point, then stroke or fill it
    dl->PathArcTo(ImVec2(x + 3 * step + 20, y2), 22.0f, 0.0f, L15_PI * 1.5f);   // 270 degree arc
    dl->PathStroke(c, ImDrawFlags_None, thickness);

    dl->AddText(ImVec2(p0.x + 8, p1.y - 40), IM_COL32(255, 255, 255, 255), "AddText at a screen position");
    // Mouse position inside the canvas (screen -> canvas local)
    ImVec2 m = L15_Sub(ImGui::GetIO().MousePos, p0);
    if (ImGui::IsItemHovered())
    {
        dl->AddCircle(ImGui::GetIO().MousePos, 6.0f, IM_COL32(120, 200, 255, 255));
        char buf[48]; _snprintf(buf, sizeof(buf) - 1, "local (%.0f, %.0f)", m.x, m.y); buf[47] = 0;
        dl->AddText(ImVec2(p0.x + 8, p1.y - 22), IM_COL32(120, 200, 255, 255), buf);
    }
    dl->PopClipRect();
    ImGui::End();
}

// ======================================================================================
//  2. Custom widgets: knob + joystick
// ======================================================================================

// A rotary knob. Returns true when the value changed (same contract as built-in widgets).
static bool L15_Knob(const char *label, float *v, float vmin, float vmax, float radius)
{
    ImGuiIO &io = ImGui::GetIO();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 center(pos.x + radius, pos.y + radius);
    float lineH = ImGui::GetTextLineHeight();

    ImGui::InvisibleButton(label, ImVec2(radius * 2.0f, radius * 2.0f + lineH + 4.0f));
    bool hovered = ImGui::IsItemHovered();
    bool active  = ImGui::IsItemActive();
    bool changed = false;
    if (active && io.MouseDelta.x != 0.0f)
    {
        *v = L15_Clamp(*v + io.MouseDelta.x * (vmax - vmin) / 200.0f, vmin, vmax);   // 200 px = full range
        changed = true;
    }

    // 270 degree sweep, starting bottom-left
    float t = (*v - vmin) / (vmax - vmin);
    float aMin = L15_PI * 0.75f, aMax = L15_PI * 2.25f;
    float a = aMin + (aMax - aMin) * t;

    ImDrawList *dl = ImGui::GetWindowDrawList();
    dl->AddCircleFilled(center, radius, ImGui::GetColorU32(active ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg), 32);
    dl->PathArcTo(center, radius - 4.0f, aMin, aMax, 32);                       // track
    dl->PathStroke(IM_COL32(255, 255, 255, 40), ImDrawFlags_None, 3.0f);
    dl->PathArcTo(center, radius - 4.0f, aMin, a, 32);                          // value
    dl->PathStroke(ImGui::GetColorU32(ImGuiCol_SliderGrabActive), ImDrawFlags_None, 3.0f);
    dl->AddLine(ImVec2(center.x + cosf(a) * radius * 0.3f, center.y + sinf(a) * radius * 0.3f),
                ImVec2(center.x + cosf(a) * (radius - 6.0f), center.y + sinf(a) * (radius - 6.0f)),
                IM_COL32(255, 255, 255, 230), 2.0f);

    // Label below (text up to "##" only)
    const char *end = strstr(label, "##");
    dl->AddText(ImVec2(pos.x, pos.y + radius * 2.0f + 2.0f), ImGui::GetColorU32(ImGuiCol_Text), label, end);

    if (active || hovered)
        ImGui::SetTooltip("%.1f", *v);
    return changed;
}

// A 2D joystick: drag the stick, it returns an offset in [-1,1]^2 and springs back on release.
static ImVec2 L15_Joystick(const char *id, float size)
{
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(id, ImVec2(size, size));
    ImVec2 center(pos.x + size * 0.5f, pos.y + size * 0.5f);
    float  range = size * 0.5f - 12.0f;

    ImVec2 out(0.0f, 0.0f);
    if (ImGui::IsItemActive())
    {
        ImVec2 d = L15_Sub(ImGui::GetIO().MousePos, center);
        float len = sqrtf(d.x * d.x + d.y * d.y);
        if (len > range) d = L15_Scale(d, range / len);            // keep the stick in the ring
        out = ImVec2(d.x / range, d.y / range);
    }
    ImVec2 stick = L15_Add(center, L15_Scale(out, range));

    ImDrawList *dl = ImGui::GetWindowDrawList();
    dl->AddCircleFilled(center, size * 0.5f, ImGui::GetColorU32(ImGuiCol_FrameBg), 40);
    dl->AddCircle(center, range, IM_COL32(255, 255, 255, 50), 40);
    dl->AddLine(ImVec2(center.x - range, center.y), ImVec2(center.x + range, center.y), IM_COL32(255, 255, 255, 30));
    dl->AddLine(ImVec2(center.x, center.y - range), ImVec2(center.x, center.y + range), IM_COL32(255, 255, 255, 30));
    dl->AddLine(center, stick, IM_COL32(255, 255, 255, 90), 3.0f);
    dl->AddCircleFilled(stick, 11.0f, ImGui::GetColorU32(ImGui::IsItemActive() ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab), 20);
    return out;
}

static void L15_CameraWidgets(void)
{
    ImGui::SetNextWindowPos(ImVec2(20, 365), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360, 270), ImGuiCond_FirstUseEver);
    ImGui::Begin("Camera Widgets");

    // The engine's yaw grows without limit; wrap it into -180..180 for the knob
    gCamera.yaw = fmodf(gCamera.yaw + 540.0f, 360.0f) - 180.0f;

    L15_Knob("Yaw", &gCamera.yaw, -180.0f, 180.0f, 30.0f);
    ImGui::SameLine();
    L15_Knob("Pitch", &gCamera.pitch, -89.0f, 89.0f, 30.0f);
    ImGui::SameLine();
    L15_Knob("Distance", &gCamera.distance, 2.0f, 40.0f, 30.0f);
    ImGui::SameLine();
    L15_Knob("FOV", &gCamera.fov, 20.0f, 100.0f, 30.0f);

    ImGui::Separator();
    static float speed = 90.0f;          // degrees per second at full deflection
    ImVec2 j = L15_Joystick("##joy", 120.0f);
    gCamera.yaw   -= j.x * speed * gDeltaTime;
    gCamera.pitch  = L15_Clamp(gCamera.pitch + j.y * speed * gDeltaTime, -89.0f, 89.0f);
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::Text("Joystick");
    ImGui::Text("x %+.2f  y %+.2f", j.x, j.y);
    ImGui::SetNextItemWidth(120);
    ImGui::SliderFloat("deg/s", &speed, 10.0f, 360.0f, "%.0f");
    ImGui::TextDisabled("Drag the stick to orbit");
    ImGui::EndGroup();
    ImGui::End();
}

// ======================================================================================
//  3. World-space overlays on the 3D view
// ======================================================================================
static void L15_WorldOverlays(void)
{
    // Background list: drawn after the 3D scene but BEHIND every ImGui window.
    ImDrawList *bg = ImGui::GetBackgroundDrawList();

    for (int i = 0; i < gEntityCount; i++)
    {
        const Entity *e = &gEntities[i];
        if (!e->visible || e->mesh == MESH_PLANE) continue;
        bool sel = (i == gSelectedEntity);
        ImU32 col = sel ? IM_COL32(255, 220, 60, 255) : IM_COL32(220, 225, 235, 200);

        // Project the 8 corners of the (unrotated) box and take the 2D min/max
        ImVec2 mn(1e9f, 1e9f), mx(-1e9f, -1e9f);
        int visibleCorners = 0;
        for (int k = 0; k < 8; k++)
        {
            float w[3] = { e->position[0] + ((k & 1) ? 0.5f : -0.5f) * e->scale[0],
                           e->position[1] + ((k & 2) ? 0.5f : -0.5f) * e->scale[1],
                           e->position[2] + ((k & 4) ? 0.5f : -0.5f) * e->scale[2] };
            ImVec2 s;
            if (!Engine_WorldToScreen(w, &s)) continue;            // behind the camera
            visibleCorners++;
            if (s.x < mn.x) mn.x = s.x; if (s.y < mn.y) mn.y = s.y;
            if (s.x > mx.x) mx.x = s.x; if (s.y > mx.y) mx.y = s.y;
        }
        if (visibleCorners < 8) continue;                          // partly behind: skip

        if (L15_showBrackets)
        {
            float L = (mx.x - mn.x) * 0.2f;                        // bracket arm length
            float t = sel ? 2.0f : 1.0f;
            bg->AddLine(mn, ImVec2(mn.x + L, mn.y), col, t);  bg->AddLine(mn, ImVec2(mn.x, mn.y + L), col, t);
            bg->AddLine(ImVec2(mx.x, mn.y), ImVec2(mx.x - L, mn.y), col, t); bg->AddLine(ImVec2(mx.x, mn.y), ImVec2(mx.x, mn.y + L), col, t);
            bg->AddLine(ImVec2(mn.x, mx.y), ImVec2(mn.x + L, mx.y), col, t); bg->AddLine(ImVec2(mn.x, mx.y), ImVec2(mn.x, mx.y - L), col, t);
            bg->AddLine(mx, ImVec2(mx.x - L, mx.y), col, t);  bg->AddLine(mx, ImVec2(mx.x, mx.y - L), col, t);
        }
        if (L15_showLabels)
        {
            // Name centred above the box, on a dark plate so it reads on any background
            ImVec2 ts = ImGui::CalcTextSize(e->name);
            ImVec2 tp(((mn.x + mx.x) - ts.x) * 0.5f, mn.y - ts.y - 6.0f);
            bg->AddRectFilled(ImVec2(tp.x - 4, tp.y - 1), ImVec2(tp.x + ts.x + 4, tp.y + ts.y + 1), IM_COL32(0, 0, 0, 140), 3.0f);
            bg->AddText(tp, col, e->name);
        }
    }
}

// Orientation gizmo: world X/Y/Z projected onto the camera's right/up axes.
static void L15_AxisGizmo(void)
{
    float fwd[3], right[3], up[3];
    L15_CameraBasis(fwd, right, up);

    const ImGuiViewport *vp = ImGui::GetMainViewport();
    ImVec2 c(vp->WorkPos.x + vp->WorkSize.x - 70.0f, vp->WorkPos.y + 70.0f);
    const float len = 45.0f;
    ImDrawList *bg = ImGui::GetBackgroundDrawList();
    bg->AddCircleFilled(c, len + 12.0f, IM_COL32(0, 0, 0, 90), 40);

    static const char *names[3] = { "X", "Y", "Z" };
    static const ImU32 cols[3] = { IM_COL32(235, 70, 70, 255), IM_COL32(80, 220, 80, 255), IM_COL32(70, 120, 240, 255) };

    // Draw the axis pointing away from the viewer first (painter's algorithm)
    int order[3] = { 0, 1, 2 };
    float depth[3] = { fwd[0], fwd[1], fwd[2] };   // dot(axis_i, fwd) == fwd[i]
    for (int a = 0; a < 2; a++) for (int b = a + 1; b < 3; b++)
        if (depth[order[b]] > depth[order[a]]) { int t = order[a]; order[a] = order[b]; order[b] = t; }

    for (int n = 0; n < 3; n++)
    {
        int i = order[n];
        // dot(axis_i, right) == right[i]; screen y goes down so negate up
        ImVec2 tip(c.x + right[i] * len, c.y - up[i] * len);
        bool away = depth[i] > 0.0f;
        bg->AddLine(c, tip, cols[i], away ? 1.5f : 3.0f);
        bg->AddCircleFilled(tip, 9.0f, cols[i], 16);
        bg->AddText(ImVec2(tip.x - 4.0f, tip.y - 7.0f), IM_COL32(0, 0, 0, 255), names[i]);
    }
}

// ======================================================================================
//  4. Mini-map
// ======================================================================================
static void L15_MiniMap(void)
{
    ImGui::SetNextWindowPos(ImVec2(1250, 160), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(280, 340), ImGuiCond_FirstUseEver);
    ImGui::Begin("Mini-map");

    static float worldHalf = 12.0f;          // map shows -worldHalf..+worldHalf on X and Z
    ImGui::SliderFloat("Range", &worldHalf, 4.0f, 40.0f, "%.0f m");
    ImGui::Checkbox("Labels", &L15_showLabels); ImGui::SameLine();
    ImGui::Checkbox("Brackets", &L15_showBrackets); ImGui::SameLine();
    ImGui::Checkbox("Gizmo", &L15_showGizmo);

    float side = ImGui::GetContentRegionAvail().x;
    float availY = ImGui::GetContentRegionAvail().y;
    if (availY < side) side = availY;
    if (side < 60.0f) side = 60.0f;
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##map", ImVec2(side, side));
    ImVec2 p1(p0.x + side, p0.y + side);
    ImVec2 mid(p0.x + side * 0.5f, p0.y + side * 0.5f);
    float k = side * 0.5f / worldHalf;       // pixels per metre

    // world (x,z) <-> map pixel. Looking down from +Y: +X right, +Z down.
    ImGuiIO &io = ImGui::GetIO();
    if (ImGui::IsItemActive())               // click or drag = move the camera target there
    {
        gCamera.target[0] = L15_Clamp((io.MousePos.x - mid.x) / k, -worldHalf, worldHalf);
        gCamera.target[2] = L15_Clamp((io.MousePos.y - mid.y) / k, -worldHalf, worldHalf);
    }

    ImDrawList *dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p0, p1, IM_COL32(18, 22, 28, 255));
    dl->PushClipRect(p0, p1, true);
    for (int g = -(int)worldHalf; g <= (int)worldHalf; g += 2)         // 2 m grid
    {
        float gx = mid.x + g * k, gy = mid.y + g * k;
        dl->AddLine(ImVec2(gx, p0.y), ImVec2(gx, p1.y), IM_COL32(255, 255, 255, 18));
        dl->AddLine(ImVec2(p0.x, gy), ImVec2(p1.x, gy), IM_COL32(255, 255, 255, 18));
    }
    dl->AddLine(ImVec2(p0.x, mid.y), ImVec2(p1.x, mid.y), IM_COL32(230, 70, 70, 120));   // X axis
    dl->AddLine(ImVec2(mid.x, p0.y), ImVec2(mid.x, p1.y), IM_COL32(70, 120, 240, 120));  // Z axis

    int hoveredEntity = -1;
    for (int i = 0; i < gEntityCount; i++)
    {
        const Entity *e = &gEntities[i];
        if (!e->visible) continue;
        ImVec2 ep(mid.x + e->position[0] * k, mid.y + e->position[2] * k);
        ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(e->color[0], e->color[1], e->color[2], 1.0f));
        if (e->mesh == MESH_PLANE)
        {   // planes as outlines of their footprint
            dl->AddRect(ImVec2(ep.x - e->scale[0] * 0.5f * k, ep.y - e->scale[2] * 0.5f * k),
                        ImVec2(ep.x + e->scale[0] * 0.5f * k, ep.y + e->scale[2] * 0.5f * k), IM_COL32(255, 255, 255, 50));
            continue;
        }
        float r = 4.0f + e->scale[0] * k * 0.3f;
        dl->AddCircleFilled(ep, r, col, 16);
        if (i == gSelectedEntity) dl->AddCircle(ep, r + 3.0f, IM_COL32(255, 220, 60, 255), 16, 2.0f);
        float dx = io.MousePos.x - ep.x, dy = io.MousePos.y - ep.y;
        if (ImGui::IsItemHovered() && dx * dx + dy * dy < r * r) hoveredEntity = i;
    }

    // Camera: target cross + eye position + view direction
    float eye[3]; L15_CameraEye(eye);
    ImVec2 tp(mid.x + gCamera.target[0] * k, mid.y + gCamera.target[2] * k);
    ImVec2 ey(mid.x + eye[0] * k, mid.y + eye[2] * k);
    dl->AddLine(ey, tp, IM_COL32(255, 255, 255, 120), 1.0f);
    dl->AddCircleFilled(ey, 5.0f, IM_COL32(255, 255, 255, 230));
    dl->AddLine(ImVec2(tp.x - 6, tp.y), ImVec2(tp.x + 6, tp.y), IM_COL32(255, 255, 255, 255), 2.0f);
    dl->AddLine(ImVec2(tp.x, tp.y - 6), ImVec2(tp.x, tp.y + 6), IM_COL32(255, 255, 255, 255), 2.0f);
    dl->PopClipRect();
    dl->AddRect(p0, p1, IM_COL32(90, 100, 120, 255));

    if (hoveredEntity >= 0)
        ImGui::SetTooltip("%s\n(%.1f, %.1f)", gEntities[hoveredEntity].name,
                          gEntities[hoveredEntity].position[0], gEntities[hoveredEntity].position[2]);
    ImGui::End();
}

void Lesson_Init(void)
{
    Engine_Log("Lesson 15: draw lists. Try the knobs, the joystick and the mini-map.");
}

void Lesson_UI(void)
{
    L15_BasicsWindow();
    L15_CameraWidgets();
    L15_MiniMap();
    L15_WorldOverlays();
    if (L15_showGizmo) L15_AxisGizmo();

    // Foreground list: drawn on top of EVERYTHING, including windows. Good for debug text.
    ImGui::GetForegroundDrawList()->AddText(ImVec2(400, 8), IM_COL32(255, 255, 255, 120),
                                            "foreground draw list: always on top");
}
