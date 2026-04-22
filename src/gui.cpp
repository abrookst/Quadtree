#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include "imgui.h"
#include <SDL2/SDL.h>

#include "gui.h"
#include "terrain.h"
#include "bomb.h"
#include "player.h"
#include "appcontext.h"
#include <filesystem>





// Global console buffer storage
static std::string g_console_output;
std::queue<std::string> ConsoleBuffer::s_command_queue;




std::string& ConsoleBuffer::get_output()
{
    return g_console_output;
}

void ConsoleBuffer::clear_output()
{
    g_console_output.clear();
}

std::string ConsoleBuffer::get_command()
{
    if (s_command_queue.empty())
        return "";
    
    std::string cmd = s_command_queue.front();
    s_command_queue.pop();
    return cmd;
}

void ConsoleBuffer::submit_command(const std::string& cmd)
{
    s_command_queue.push(cmd);
}

int ConsoleBuffer::overflow(int c)
{
    if (c != EOF)
    {
        g_console_output += static_cast<char>(c);
    }
    return c;
}

static bool g_show_quadtree_overlay = false;
static int g_max_depth = 11;
static float g_brush_radius = 50.0f;
static int g_terrain_display_width = 256;
static int g_terrain_display_height = 256;
static ImVec4 g_terrain_solid_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
static ImVec4 g_terrain_air_color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
static int g_terrain_window_tab = 0; // 0 = View, 1 = Draw

// Current terrain pointer for display
static const Quadtree* g_current_terrain = nullptr;

void gui_set_current_terrain(const Quadtree* terrain)
{
    g_current_terrain = terrain;
}

void gui_set_terrain_resolution(int width, int height)
{
    g_terrain_display_width = width;
    g_terrain_display_height = height;
}

// Terrain draw mode namespace implementation
namespace TerrainDraw
{
    static TerrainDraw::DrawBitmap g_draw_bitmap;
    static bool g_draw_mode_active = false;
    static bool g_request_terrain_reload = false;
    static std::string g_terrain_reload_file;
    static bool g_reload_from_draw = false;

    DrawBitmap& get_draw_bitmap()
    {
        return g_draw_bitmap;
    }
    
    bool is_draw_mode_active()
    {
        return g_draw_mode_active;
    }
    
    void set_draw_mode(bool active, int width, int height)
    {
        g_draw_mode_active = active;
        if (active)
        {
            g_draw_bitmap.width = width;
            g_draw_bitmap.height = height;
            // Initialize as all air (0)
            g_draw_bitmap.data.assign(width * height, 0);
        }
    }
    
    void request_terrain_reload_from_draw()
    {
        g_request_terrain_reload = true;
        g_reload_from_draw = true;
    }
    
    void request_terrain_reload(const std::string& terrain_file)
    {
        g_request_terrain_reload = true;
        g_terrain_reload_file = terrain_file;
        g_reload_from_draw = false;
    }
    
    bool has_reload_request()
    {
        return g_request_terrain_reload;
    }
    
    void clear_reload_request()
    {
        g_request_terrain_reload = false;
    }
    
    std::string get_reload_file()
    {
        return g_terrain_reload_file;
    }
    
    bool is_reload_from_draw()
    {
        return g_reload_from_draw;
    }
}

// Terrain destruction request namespace implementation
namespace TerrainDestruct
{
    static bool g_has_destruction_request = false;
    static float g_destruct_world_x = 0.0f;
    static float g_destruct_world_y = 0.0f;
    static float g_destruct_radius = 0.0f;
    
    void request_destruction(float world_x, float world_y, float radius)
    {
        g_destruct_world_x = world_x;
        g_destruct_world_y = world_y;
        g_destruct_radius = radius;
        g_has_destruction_request = true;
    }
    
    bool has_destruction_request()
    {
        return g_has_destruction_request;
    }
    
    DestructionRequest get_and_clear_request()
    {
        DestructionRequest req;
        req.world_x = g_destruct_world_x;
        req.world_y = g_destruct_world_y;
        req.radius = g_destruct_radius;
        g_has_destruction_request = false;
        return req;
    }
}

bool gui_get_show_quadtree_overlay()
{
    return g_show_quadtree_overlay;
}

int gui_get_max_depth()
{
    return g_max_depth;
}

float gui_get_brush_radius()
{
    return g_brush_radius;
}

int gui_get_terrain_display_width()
{
    return g_terrain_display_width;
}

int gui_get_terrain_display_height()
{
    return g_terrain_display_height;
}

    static float bomb_radius = 5.0f;
    static float bomb_gravity = 98.0f;
    static float bomb_bounce = 0.5f;
    static float bomb_explode_time = 3.0f;
    static float bomb_explode_radius = 10.0f;
    static bool bomb_timed_explosion = false;
    static int bomb_hits_to_explode = 3;
    static ImVec4 bomb_trail_color = ImVec4(0.78f, 0.78f, 0.78f, 0.59f); // approx 200, 200, 200, 150
    static ImVec4 parabola_color = ImVec4(1.0f, 1.0f, 0.0f, 0.78f); // YELLOW / approx 255, 255, 0, 200

    // Explosion polygon settings (vertices stored as normalized -1 to 1)
    static ExplosionType explosion_type = ExplosionType::Circle;
    static std::vector<glm::vec2> explosion_polygon;
    static std::vector<glm::vec2> polygon_editing_points;
    static float vertex_input_x = 0.0f;
    static float vertex_input_y = 0.0f;
    static float canvas_pan_x = 0.0f;
    static float canvas_pan_y = 0.0f;

float gui_get_bomb_radius() { return bomb_radius; }
float gui_get_bomb_gravity() { return bomb_gravity; }
float gui_get_bomb_bounce() { return bomb_bounce; }
float gui_get_bomb_explode_time() { return bomb_explode_time; }
float gui_get_bomb_explode_radius() { return bomb_explode_radius; }
bool gui_get_bomb_timed_explosion() { return bomb_timed_explosion; }
int gui_get_bomb_hits_to_explode() { return bomb_hits_to_explode; }
ImVec4 gui_get_bomb_trail_color() { return bomb_trail_color; }
ImVec4 gui_get_parabola_color() { return parabola_color; }

ExplosionType gui_get_explosion_type() { return explosion_type; }
std::vector<glm::vec2> gui_get_explosion_polygon() { return explosion_polygon; }

static bool g_spawn_character_mode = false;
bool gui_get_spawn_character_mode() { return g_spawn_character_mode; }
void gui_set_spawn_character_mode(bool mode) { g_spawn_character_mode = mode; }

ImVec4 gui_get_terrain_solid_color()
{
    return g_terrain_solid_color;
}

ImVec4 gui_get_terrain_air_color()
{
    return g_terrain_air_color;
}

void gui_render(AppContext& app)
{
    // Setup docking space
    static bool first_run = true;
    ImGuiID dockspace_id = 0;
    if (first_run)
    {
        first_run = false;
        ImGuiIO& io = ImGui::GetIO();
        dockspace_id = ImGui::GetID("DockSpace");
    }
    
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpaceWindow", nullptr, window_flags);
    ImGui::PopStyleVar(3);
    
    ImGuiID dockspace = ImGui::GetID("DockSpace");
    ImGui::DockSpace(dockspace, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
    ImGui::End();
    
    // Keep a rolling history of FPS for plotting.
    // (Static storage keeps it simple and avoids allocations.)
    static float fps_history[240] = {};
    static int fps_history_offset = 0;

    const float fps_now = ImGui::GetIO().Framerate;
    fps_history[fps_history_offset] = fps_now;
    fps_history_offset = (fps_history_offset + 1) % (int)(sizeof(fps_history) / sizeof(fps_history[0]));

    // Demo window toggle
    static bool show_demo_window = false;
    if (show_demo_window)
    {
        ImGui::ShowDemoWindow(&show_demo_window);
    }

    // Sample control panel
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(480, 500), ImGuiCond_FirstUseEver);
    ImGui::Begin("Quadtree Control Panel");

    ImGui::Text("Quadtree");
    ImGui::TextDisabled("Controls & debug tools");
    ImGui::Spacing();

    static bool show_console = true;

    if (ImGui::CollapsingHeader("Windows", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Show ImGui Demo Window", &show_demo_window);
        ImGui::Checkbox("Show Console", &show_console);
        ImGui::Spacing();
    }

    if (ImGui::CollapsingHeader("Visualization", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Show Quadtree Overlay", &g_show_quadtree_overlay);

        ImGui::Spacing();
        ImGui::Text("ImGui Style:");
        static int style_idx = 0;
        if (ImGui::Combo("##style", &style_idx, "Classic\0Dark\0Light\0"))
        {
            switch (style_idx)
            {
                case 0: ImGui::StyleColorsClassic(); break;
                case 1: ImGui::StyleColorsDark(); break;
                case 2: ImGui::StyleColorsLight(); break;
            }
        }
        
        ImGui::Spacing();
        ImGui::Text("Colors");
        ImGui::ColorEdit3("Terrain Solid Color", (float*)&g_terrain_solid_color);
        ImGui::ColorEdit3("Terrain Air Color", (float*)&g_terrain_air_color);
        ImGui::ColorEdit4("Bomb Trail Color", (float*)&bomb_trail_color);
        ImGui::ColorEdit4("Aim Parabola Color", (float*)&parabola_color);
        ImGui::Spacing();
    }

    if (ImGui::CollapsingHeader("Bomb Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::PushItemWidth(150);
        ImGui::SliderFloat("Bomb Radius", &bomb_radius, 1.0f, 50.0f);
        ImGui::SliderFloat("Gravity", &bomb_gravity, 0.0f, 300.0f);
        ImGui::SliderFloat("Bounce Strength", &bomb_bounce, 0.0f, 2.0f);
        ImGui::SliderFloat("Explosion Radius", &bomb_explode_radius, 1.0f, 100.0f);
        
        ImGui::Separator();
        
        // Explosion type selector
        ImGui::Text("Explosion Type:");
        bool is_circle = (explosion_type == ExplosionType::Circle);
        bool is_polygon = (explosion_type == ExplosionType::Polygon);
        
        if (ImGui::RadioButton("Circle Explosion", is_circle)) {
            explosion_type = ExplosionType::Circle;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Polygon Explosion", is_polygon)) {
            explosion_type = ExplosionType::Polygon;
        }
        
        ImGui::Separator();
        
        bool timed = bomb_timed_explosion;
        if (ImGui::RadioButton("Hits to Explode", !timed)) bomb_timed_explosion = false;
        ImGui::SameLine();
        if (ImGui::RadioButton("Time to Explode", timed)) bomb_timed_explosion = true;
        
        if (bomb_timed_explosion) {
            ImGui::SliderFloat("Explosion Delay", &bomb_explode_time, 0.1f, 10.0f);
        } else {
            ImGui::SliderInt("Hits required", &bomb_hits_to_explode, 1, 10);
        }

        ImGui::Separator();
        if (gui_get_spawn_character_mode()) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
            if (ImGui::Button("Next Click: Spawning Character")) {
                gui_set_spawn_character_mode(false); // cancel
            }
            ImGui::PopStyleColor();
        } else {
            if (ImGui::Button("Spawn Character")) {
                gui_set_spawn_character_mode(true);
            }
        }
        
        ImGui::PopItemWidth();
        ImGui::Spacing();
    }

    if (ImGui::CollapsingHeader("Polygon Editor", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Create custom explosion polygons:");
        
        // Display current polygon vertices
        if (!explosion_polygon.empty()) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Active Polygon: %zu vertices", explosion_polygon.size());
        } else if (!polygon_editing_points.empty()) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Editing: %zu vertices", polygon_editing_points.size());
        } else {
            ImGui::TextDisabled("No polygon created yet");
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Canvas for polygon visualization
        ImVec2 canvas_size(300, 300);
        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        
        // Draw canvas background
        draw_list->AddRectFilled(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), IM_COL32(50, 50, 50, 255));
        draw_list->AddRect(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), IM_COL32(200, 200, 200, 255));
        
        // Check for mouse interaction in canvas
        ImVec2 mouse_pos = ImGui::GetMousePos();
        bool is_mouse_over_canvas = (mouse_pos.x >= canvas_pos.x && mouse_pos.x <= canvas_pos.x + canvas_size.x &&
                                     mouse_pos.y >= canvas_pos.y && mouse_pos.y <= canvas_pos.y + canvas_size.y);
        
        // Handle canvas panning with middle mouse
        if (is_mouse_over_canvas && ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
            ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Middle);
            canvas_pan_x += delta.x / 100.0f;
            canvas_pan_y += delta.y / 100.0f;
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Middle);
        }
        
        // Draw center crosshair
        ImVec2 canvas_center(canvas_pos.x + canvas_size.x / 2, canvas_pos.y + canvas_size.y / 2);
        float crosshair_size = 10.0f;
        draw_list->AddLine(ImVec2(canvas_center.x - crosshair_size, canvas_center.y), ImVec2(canvas_center.x + crosshair_size, canvas_center.y), IM_COL32(100, 100, 100, 128));
        draw_list->AddLine(ImVec2(canvas_center.x, canvas_center.y - crosshair_size), ImVec2(canvas_center.x, canvas_center.y + crosshair_size), IM_COL32(100, 100, 100, 128));
        
        // Grid lines (from -1 to 1 range, 0.5 step)
        float scale = canvas_size.x / 2.0f;  // Maps -1 to 1 to canvas width
        for (float x = -1.0f; x <= 1.0f; x += 0.5f) {
            float screen_x = canvas_center.x + x * scale + canvas_pan_x;
            draw_list->AddLine(ImVec2(screen_x, canvas_pos.y), ImVec2(screen_x, canvas_pos.y + canvas_size.y), 
                              (fabsf(x) < 0.01f ? IM_COL32(100, 100, 100, 128) : IM_COL32(80, 80, 80, 64)));
        }
        for (float y = -1.0f; y <= 1.0f; y += 0.5f) {
            float screen_y = canvas_center.y - y * scale + canvas_pan_y;
            draw_list->AddLine(ImVec2(canvas_pos.x, screen_y), ImVec2(canvas_pos.x + canvas_size.x, screen_y), 
                              (fabsf(y) < 0.01f ? IM_COL32(100, 100, 100, 128) : IM_COL32(80, 80, 80, 64)));
        }
        
        // Lambda to convert normalized coords to canvas coordinates
        auto norm_to_canvas = [&](float nx, float ny) -> ImVec2 {
            return ImVec2(
                canvas_center.x + nx * scale + canvas_pan_x,
                canvas_center.y - ny * scale + canvas_pan_y  // Invert Y for screen coordinates
            );
        };
        
        // Get explosion radius for preview scaling
        float explosion_radius = gui_get_bomb_explode_radius();
        
        // Draw editing polygon being built
        if (polygon_editing_points.size() >= 2) {
            for (size_t i = 0; i < polygon_editing_points.size(); ++i) {
                ImVec2 p1 = norm_to_canvas(polygon_editing_points[i].x, polygon_editing_points[i].y);
                ImVec2 p2 = norm_to_canvas(polygon_editing_points[(i + 1) % polygon_editing_points.size()].x, polygon_editing_points[(i + 1) % polygon_editing_points.size()].y);
                draw_list->AddLine(p1, p2, IM_COL32(0, 255, 0, 200), 2.0f);
            }
        }
        
        // Draw editing vertices as circles
        for (size_t i = 0; i < polygon_editing_points.size(); ++i) {
            ImVec2 p = norm_to_canvas(polygon_editing_points[i].x, polygon_editing_points[i].y);
            draw_list->AddCircleFilled(p, 5.0f, IM_COL32(0, 255, 0, 255));
            draw_list->AddCircle(p, 5.0f, IM_COL32(255, 255, 0, 255), 12, 2.0f);
        }
        
        // Draw saved polygon (if no editing)
        if (polygon_editing_points.empty() && !explosion_polygon.empty()) {
            if (explosion_polygon.size() >= 2) {
                for (size_t i = 0; i < explosion_polygon.size(); ++i) {
                    ImVec2 p1 = norm_to_canvas(explosion_polygon[i].x, explosion_polygon[i].y);
                    ImVec2 p2 = norm_to_canvas(explosion_polygon[(i + 1) % explosion_polygon.size()].x, explosion_polygon[(i + 1) % explosion_polygon.size()].y);
                    draw_list->AddLine(p1, p2, IM_COL32(0, 150, 255, 200), 2.0f);
                }
            }
            
            for (size_t i = 0; i < explosion_polygon.size(); ++i) {
                ImVec2 p = norm_to_canvas(explosion_polygon[i].x, explosion_polygon[i].y);
                draw_list->AddCircleFilled(p, 5.0f, IM_COL32(0, 150, 255, 255));
                draw_list->AddCircle(p, 5.0f, IM_COL32(150, 255, 255, 255), 12, 2.0f);
            }
        }
        
        ImGui::Dummy(canvas_size);
        ImGui::Spacing();
        
        // Vertex input controls (normalized -1 to 1)
        ImGui::PushItemWidth(100);
        ImGui::SliderFloat("Vertex X##poly", &vertex_input_x, -1.0f, 1.0f);
        ImGui::SliderFloat("Vertex Y##poly", &vertex_input_y, -1.0f, 1.0f);
        ImGui::PopItemWidth();
        
        if (ImGui::Button("Add Vertex", ImVec2(100, 0))) {
            polygon_editing_points.push_back({vertex_input_x, vertex_input_y});
            std::cout << "[POLYGON] Added vertex: (" << vertex_input_x << ", " << vertex_input_y << ")" << std::endl;
        }
        
        ImGui::Spacing();
        
        // Display and manage editing vertices
        if (!polygon_editing_points.empty()) {
            ImGui::Text("Editing Vertices:");
            std::vector<int> to_remove;
            for (size_t i = 0; i < polygon_editing_points.size(); ++i) {
                ImGui::BulletText("[%zu] X: %.2f, Y: %.2f", i, polygon_editing_points[i].x, polygon_editing_points[i].y);
                ImGui::SameLine();
                ImGui::PushID((int)i);
                if (ImGui::SmallButton("X")) {
                    to_remove.push_back(i);
                }
                ImGui::PopID();
            }
            // Remove marked vertices in reverse order
            for (int i = (int)to_remove.size() - 1; i >= 0; --i) {
                polygon_editing_points.erase(polygon_editing_points.begin() + to_remove[i]);
            }
            ImGui::Spacing();
        }
        
        ImGui::Separator();
        ImGui::Spacing();
        
        // Preset polygons
        ImGui::Text("Preset Shapes:");
        ImGui::Spacing();
        
        if (ImGui::Button("Diamond##preset", ImVec2(70, 0))) {
            explosion_polygon = {
                {0.0f, 1.0f},      // top
                {1.0f, 0.0f},      // right
                {0.0f, -1.0f},     // bottom
                {-1.0f, 0.0f}      // left
            };
            polygon_editing_points.clear();
            ImGui::OpenPopup("PresetLoaded_Diamond");
        }
        ImGui::SameLine();
        if (ImGui::Button("Square##preset", ImVec2(70, 0))) {
            float r = 0.707f;  // sqrt(2)/2 for normalized square
            explosion_polygon = {
                {r, r},      // top-right
                {r, -r},     // bottom-right
                {-r, -r},    // bottom-left
                {-r, r}      // top-left
            };
            polygon_editing_points.clear();
            ImGui::OpenPopup("PresetLoaded_Square");
        }
        ImGui::SameLine();
        if (ImGui::Button("Triangle##preset", ImVec2(70, 0))) {
            explosion_polygon = {
                {0.0f, 1.0f},      // top
                {0.866f, -0.5f},   // bottom-right (cos(60°), -sin(60°))
                {-0.866f, -0.5f}   // bottom-left
            };
            polygon_editing_points.clear();
            ImGui::OpenPopup("PresetLoaded_Triangle");
        }
        ImGui::SameLine();
        if (ImGui::Button("Star##preset", ImVec2(70, 0))) {
            float outer = 1.0f;
            float inner = 0.4f;
            float angle_step = 3.14159265f / 5.0f;
            explosion_polygon.clear();
            for (int i = 0; i < 10; ++i) {
                float angle = i * angle_step - 1.5708f;
                float radius = (i % 2 == 0) ? outer : inner;
                explosion_polygon.push_back({
                    radius * cosf(angle),
                    radius * sinf(angle)
                });
            }
            polygon_editing_points.clear();
            ImGui::OpenPopup("PresetLoaded_Star");
        }
        ImGui::SameLine();
        if (ImGui::Button("Hexagon##preset", ImVec2(70, 0))) {
            explosion_polygon.clear();
            for (int i = 0; i < 6; ++i) {
                float angle = (i * 3.14159265f / 3.0f) - 1.5708f;
                explosion_polygon.push_back({
                    cosf(angle),
                    sinf(angle)
                });
            }
            polygon_editing_points.clear();
            ImGui::OpenPopup("PresetLoaded_Hexagon");
        }
        ImGui::SameLine();
        if (ImGui::Button("Pentagon##preset", ImVec2(70, 0))) {
            explosion_polygon.clear();
            for (int i = 0; i < 5; ++i) {
                float angle = (i * 2.0f * 3.14159265f / 5.0f) - 1.5708f;
                explosion_polygon.push_back({
                    cosf(angle),
                    sinf(angle)
                });
            }
            polygon_editing_points.clear();
            ImGui::OpenPopup("PresetLoaded_Pentagon");
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Finish editing button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
        if (ImGui::Button("Save & Use Custom Polygon", ImVec2(-1, 0))) {
            if (polygon_editing_points.size() >= 3) {
                explosion_polygon = polygon_editing_points;
                polygon_editing_points.clear();
                ImGui::OpenPopup("PolygonSaved");
            } else {
                ImGui::OpenPopup("InvalidPolygon");
            }
        }
        ImGui::PopStyleColor();
        
        // Popups
        if (ImGui::BeginPopupModal("PolygonSaved", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Polygon saved with %zu vertices!", explosion_polygon.size());
            if (ImGui::Button("OK##saved", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        
        if (ImGui::BeginPopupModal("InvalidPolygon", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Polygon must have at least 3 vertices!");
            if (ImGui::Button("OK##invalid", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        
        // Preset loaded popups
        if (ImGui::BeginPopupModal("PresetLoaded_Diamond", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Diamond preset loaded!");
            if (ImGui::Button("OK##diamond", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        
        if (ImGui::BeginPopupModal("PresetLoaded_Square", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Square preset loaded!");
            if (ImGui::Button("OK##square", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        
        if (ImGui::BeginPopupModal("PresetLoaded_Triangle", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Triangle preset loaded!");
            if (ImGui::Button("OK##triangle", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        
        if (ImGui::BeginPopupModal("PresetLoaded_Star", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Star preset loaded!");
            if (ImGui::Button("OK##star", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        
        if (ImGui::BeginPopupModal("PresetLoaded_Hexagon", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Hexagon preset loaded!");
            if (ImGui::Button("OK##hexagon", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        
        if (ImGui::BeginPopupModal("PresetLoaded_Pentagon", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Pentagon preset loaded!");
            if (ImGui::Button("OK##pentagon", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        
        if (ImGui::Button("Clear All", ImVec2(-1, 0))) {
            explosion_polygon.clear();
            polygon_editing_points.clear();
            vertex_input_x = 0.0f;
            vertex_input_y = 0.0f;
        }
        
        ImGui::Spacing();
    }

    if (ImGui::CollapsingHeader("Terrain", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::PushItemWidth(150);
        ImGui::SliderInt("Max Depth", &g_max_depth, 1, 20);
        ImGui::SliderFloat("Brush Radius", &g_brush_radius, 1.0f, 500.0f);
        ImGui::PopItemWidth();
        
        ImGui::Spacing();
        ImGui::Text("Mode:");
        ImGui::SameLine(100);
        if (ImGui::Button("View Terrain", ImVec2(100, 0)))
            g_terrain_window_tab = 0;
        ImGui::SameLine();
        if (ImGui::Button("Draw Terrain", ImVec2(100, 0)))
            g_terrain_window_tab = 1;
        
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        if (ImGui::Button("Clear Terrain##button", ImVec2(-1, 0)))
        {
            ConsoleBuffer::submit_command("terrain_clear");
        }
        ImGui::PopStyleColor(2);
        
        ImGui::Spacing();
    }

    if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("FPS: %.1f", fps_now);
        ImGui::Spacing();
        ImGui::PlotLines(
            "Framerate (FPS)",
            fps_history,
            (int)(sizeof(fps_history) / sizeof(fps_history[0])),
            fps_history_offset,
            nullptr,
            0.0f,
            240.0f,
            ImVec2(0, 60));
        ImGui::Spacing();
    }

    ImGui::End();

    // Console window
    if (show_console)
    {
        ImGui::SetNextWindowPos(ImVec2(320, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_FirstUseEver);
        ImGui::Begin("Console", &show_console);
        
        ImGui::BeginChild("ConsoleOutput", ImVec2(0, -40), true, ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::TextUnformatted(ConsoleBuffer::get_output().c_str());
        ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
        
        // Command input
        static char input_buffer[256] = "";
        static bool reclaim_focus = false;
        
        if (reclaim_focus) {
            ImGui::SetKeyboardFocusHere(); // Focus the next item (InputText)
            reclaim_focus = false;
        }

        bool submit_pressed = ImGui::InputText("##command", input_buffer, IM_ARRAYSIZE(input_buffer), ImGuiInputTextFlags_EnterReturnsTrue);
        
        ImGui::SameLine();
        if (ImGui::Button("Send"))
        {
            submit_pressed = true;
        }

        if (submit_pressed)
        {
            if (input_buffer[0] != '\0') {
                std::cout << "> " << input_buffer << std::endl;
                ConsoleBuffer::submit_command(input_buffer);
                input_buffer[0] = '\0';
            }
            reclaim_focus = true;
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Clear##console"))
        {
            ConsoleBuffer::clear_output();
        }
        
        ImGui::End();
    }

    // Terrain display window (dockable, non-resizable)
    static bool show_terrain_window = true;
    ImGui::SetNextWindowPos(ImVec2(830, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(512, 512), ImGuiCond_FirstUseEver);
    ImGui::Begin("Terrain", &show_terrain_window, ImGuiWindowFlags_NoResize);
    
    // View terrain mode
    if (g_terrain_window_tab == 0)
    {
        if (g_current_terrain && g_current_terrain->get_root())
        {
        
        // Get terrain bounds
        const QuadtreeNode* root = g_current_terrain->get_root();
        const Bounds& bounds = root->get_bounds();
        float world_min_x = bounds.get_min_x();
        float world_max_x = bounds.get_max_x();
        float world_min_y = bounds.get_min_y();
        float world_max_y = bounds.get_max_y();
        
        float range_x = world_max_x - world_min_x;
        float range_y = world_max_y - world_min_y;
        
        if (range_x > 0.0f && range_y > 0.0f)
        {
            // Calculate zoom to fit within child window
            ImVec2 available = ImGui::GetContentRegionAvail();
            float zoom_x = available.x / static_cast<float>(g_terrain_display_width);
            float zoom_y = available.y / static_cast<float>(g_terrain_display_height);
            float zoom = std::min(zoom_x, zoom_y);
            zoom = std::max(1.0f, zoom);
            
            float canvas_width = static_cast<float>(g_terrain_display_width) * zoom;
            float canvas_height = static_cast<float>(g_terrain_display_height) * zoom;
            
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            ImVec2 base_pos = ImGui::GetCursorScreenPos();
            ImVec2 canvas_pos = base_pos;
            ImVec2 canvas_max(canvas_pos.x + canvas_width, canvas_pos.y + canvas_height);
            
            // Draw canvas background
            draw_list->AddRectFilled(canvas_pos, canvas_max, IM_COL32(50, 50, 50, 255));
            
            const ImU32 color_solid = ImGui::GetColorU32(g_terrain_solid_color);
            const ImU32 color_air = ImGui::GetColorU32(g_terrain_air_color);
            
            // Helper to convert world coords to screen coords
            auto world_to_screen = [&](float wx, float wy) -> ImVec2
            {
                float disp_x = (wx - world_min_x) / range_x * canvas_width;
                float disp_y = (world_max_y - wy) / range_y * canvas_height; // invert Y
                return ImVec2(canvas_pos.x + disp_x, canvas_pos.y + disp_y);
            };
            
            // Helper to draw node
            auto draw_node = [&](const QuadtreeNode* node)
            {
                if (!node || !node->is_leaf()) return;
                
                const Bounds& nb = node->get_bounds();
                ImVec2 pmin = world_to_screen(nb.get_min_x(), nb.get_max_y());
                ImVec2 pmax = world_to_screen(nb.get_max_x(), nb.get_min_y());
                
                const FillState s = node->get_state();
                if (s == FillState::Solid)
                    draw_list->AddRectFilled(pmin, pmax, color_solid);
                else
                    draw_list->AddRectFilled(pmin, pmax, color_air);
            };
            
            // Draw all leaf nodes
            std::vector<const QuadtreeNode*> stack;
            stack.reserve(4096);
            stack.push_back(root);
            
            while (!stack.empty())
            {
                const QuadtreeNode* node = stack.back();
                stack.pop_back();
                if (!node) continue;
                
                if (node->is_leaf())
                    draw_node(node);
                else
                {
                    for (int i = 0; i < 4; i++)
                    {
                        const QuadtreeNode* child = node->get_child(i);
                        if (child) stack.push_back(child);
                    }
                }
            }
            
            // Draw border
            draw_list->AddRect(canvas_pos, canvas_max, IM_COL32(150, 150, 150, 255), 1.0f);
            
            // Draw quadtree overlay if enabled
            if (gui_get_show_quadtree_overlay())
            {
                auto draw_node_outline = [&](const QuadtreeNode* node)
                {
                    if (!node) return;
                    
                    const Bounds& nb = node->get_bounds();
                    ImVec2 pmin = world_to_screen(nb.get_min_x(), nb.get_max_y());
                    ImVec2 pmax = world_to_screen(nb.get_max_x(), nb.get_min_y());
                    
                    draw_list->AddRect(pmin, pmax, IM_COL32(255, 0, 0, 200), 1.0f);
                };
                
                std::vector<const QuadtreeNode*> overlay_stack;
                overlay_stack.reserve(4096);
                overlay_stack.push_back(root);
                
                while (!overlay_stack.empty())
                {
                    const QuadtreeNode* node = overlay_stack.back();
                    overlay_stack.pop_back();
                    if (!node) continue;
                    
                    if (node->is_leaf())
                        draw_node_outline(node);
                    else
                    {
                        draw_node_outline(node);
                        for (int i = 0; i < 4; i++)
                        {
                            const QuadtreeNode* child = node->get_child(i);
                            if (child) overlay_stack.push_back(child);
                        }
                    }
                }
            }
            
            // Create invisible button for interaction
            ImGui::SetCursorScreenPos(canvas_pos);
            ImGui::InvisibleButton("terrain_canvas", ImVec2(canvas_width, canvas_height));
            
            // Handle left-click for destruction
            if (ImGui::IsItemHovered())
            {
                ImGuiIO& io = ImGui::GetIO();
                ImVec2 mouse_pos = io.MousePos;
                
                // Convert screen to canvas-relative
                float canvas_rel_x = mouse_pos.x - canvas_pos.x;
                float canvas_rel_y = mouse_pos.y - canvas_pos.y;
                
                // Convert to world coordinates
                float norm_x = canvas_rel_x / canvas_width;
                float norm_y = canvas_rel_y / canvas_height;
                
                float world_x = world_min_x + norm_x * range_x;
                float world_y = world_max_y - norm_y * range_y;

                // Draw prediction parabola if player exists and not spawning character
                if (app.player && app.player->isActive() && !gui_get_spawn_character_mode()) {
                    float px = app.player->getX();
                    float py = app.player->getY();
                    float T = 1.0f; // 1 second flight time to reach mouse
                    float g = gui_get_bomb_gravity();
                    float vx = (world_x - px) / T;
                    float vy = (world_y - py + 0.5f * g * T * T) / T;

                    // Draw dotted line until hitting ground
                    ImVec4 p_col = gui_get_parabola_color();
                    ImU32 p_col32 = IM_COL32((int)(p_col.x * 255), (int)(p_col.y * 255), (int)(p_col.z * 255), (int)(p_col.w * 255));
                    
                    float t = 0.0f;
                    float dt = 0.05f;
                    bool hit_ground = false;
                    float sim_x = px;
                    float sim_y = py;

                    while (!hit_ground && t < 10.0f) { // cap at 10s simulation
                        sim_x = px + vx * t;
                        sim_y = py + vy * t - 0.5f * g * t * t;
                        
                        // Check out of bounds or ground hit
                        if (sim_x < world_min_x || sim_x > world_max_x || sim_y < world_min_y) {
                            break;
                        }
                        
                        if (app.terrain && app.terrain->is_filled(sim_x, sim_y)) {
                            hit_ground = true;
                            // Draw an X where it hit
                            ImVec2 hit_pos = world_to_screen(sim_x, sim_y);
                            float x_size = 5.0f; // 5 pixels
                            draw_list->AddLine(ImVec2(hit_pos.x - x_size, hit_pos.y - x_size), 
                                               ImVec2(hit_pos.x + x_size, hit_pos.y + x_size), p_col32, 2.0f);
                            draw_list->AddLine(ImVec2(hit_pos.x - x_size, hit_pos.y + x_size), 
                                               ImVec2(hit_pos.x + x_size, hit_pos.y - x_size), p_col32, 2.0f);
                            break;
                        }

                        // Draw a dot every few steps if within bounds
                        if (sim_y <= world_max_y) {
                            ImVec2 dot_pos = world_to_screen(sim_x, sim_y);
                            // Only draw a dot every couple iterations for dotted effect, or just draw every iteration
                            // Since dt is 0.05f, we get nice spacing.
                            draw_list->AddCircleFilled(dot_pos, 2.0f, p_col32);
                        }
                        
                        t += dt;
                    }
                }

                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    float brush_radius = gui_get_brush_radius();
                    TerrainDestruct::request_destruction(world_x, world_y, brush_radius);
                }
            }
            
            // Draw active bombs using mapped screen coordinates
            for (auto& bomb : app.bombs) {
                if (bomb.isActive()) {
                    ImVec2 screen_pos = world_to_screen(bomb.getX(), bomb.getY());
                    
                    // Note: bomb.radius in world coords. In screen coords it scales by (canvas_width / range_x)
                    // We assume uniform aspect ratio, so canvas_width / range_x = zoom
                    float screen_radius = bomb.getRadius() * (canvas_width / range_x);
                    
                    // If bomb moves off screen, hide or draw it? Usually we just draw it!
                    bomb.draw(draw_list, screen_pos, screen_radius);

                    // Draw trail
                    const auto& trail = bomb.getTrail();
                    if (trail.size() > 1) {
                        ImVec4 bt_col = gui_get_bomb_trail_color();
                        int tr = (int)(bt_col.x * 255.0f);
                        int tg = (int)(bt_col.y * 255.0f);
                        int tb = (int)(bt_col.z * 255.0f);
                        float base_a = bt_col.w * 255.0f;

                        for (size_t i = 0; i < trail.size() - 1; ++i) {
                            ImVec2 p1 = world_to_screen(trail[i].first, trail[i].second);
                            ImVec2 p2 = world_to_screen(trail[i + 1].first, trail[i + 1].second);
                            
                            // Calculate fade based on position in trail
                            float alpha = (float)i / (float)trail.size();
                            ImU32 trailColor = IM_COL32(tr, tg, tb, (int)(base_a * alpha));
                            
                            draw_list->AddLine(p1, p2, trailColor, 2.0f);
                        }
                        ImVec2 pLast = world_to_screen(trail.back().first, trail.back().second);
                        draw_list->AddLine(pLast, screen_pos, IM_COL32(tr, tg, tb, (int)base_a), 2.0f);
                    }
                }
            }

            // Draw player
            if (app.player && app.player->isActive()) {
                ImVec2 screen_pos = world_to_screen(app.player->getX(), app.player->getY());
                float zoom_x = canvas_width / range_x;
                app.player->draw(draw_list, screen_pos, zoom_x);
            }

            // Reserve space for canvas
            ImGui::Dummy(ImVec2(canvas_width, canvas_height));
        }
        }
        else
        {
            ImGui::TextDisabled("No terrain loaded");
        }
    }
    
    // Draw terrain mode
    if (g_terrain_window_tab == 1)
    {
        TerrainDraw::DrawBitmap& draw_bmp = TerrainDraw::get_draw_bitmap();
            int bmp_width = draw_bmp.width;
            int bmp_height = draw_bmp.height;
            
            ImGui::Text("Draw Terrain - Resolution: %dx%d", bmp_width, bmp_height);
            ImGui::Spacing();
            
            // Resolution controls
            const int MAX_RESOLUTION = 1024;
            ImGui::Text("Canvas Resolution:");
            ImGui::SetNextItemWidth(100);
            if (ImGui::InputInt("Width##res", &bmp_width, 1, 10))
            {
                bmp_width = std::max(64, std::min(bmp_width, MAX_RESOLUTION));
                if (bmp_width != draw_bmp.width)
                {
                    draw_bmp.width = bmp_width;
                    draw_bmp.data.assign(draw_bmp.width * draw_bmp.height, 0);
                }
            }
            
            ImGui::SetNextItemWidth(100);
            if (ImGui::InputInt("Height##res", &bmp_height, 1, 10))
            {
                bmp_height = std::max(64, std::min(bmp_height, MAX_RESOLUTION));
                if (bmp_height != draw_bmp.height)
                {
                    draw_bmp.height = bmp_height;
                    draw_bmp.data.assign(draw_bmp.width * draw_bmp.height, 0);
                }
            }
            ImGui::Spacing();
            
            // Calculate cell size to fit canvas nicely (max 400x400 pixels, or scale to fit)
            const float max_canvas_size = 400.0f;
            const float scale = max_canvas_size / static_cast<float>(std::max(bmp_width, bmp_height));
            const float cell_size = std::max(2.0f, scale);
            
            ImVec2 canvas_size(static_cast<float>(bmp_width) * cell_size, static_cast<float>(bmp_height) * cell_size);
            
            // Draw mode controls
            static int draw_mode = 0; // 0 = Solid, 1 = Air
            static float brush_radius_draw = 5.0f;
            
            const char* mode_items[] = { "Draw Solid", "Draw Air" };
            ImGui::SetNextItemWidth(150);
            ImGui::Combo("##draw_mode", &draw_mode, mode_items, IM_ARRAYSIZE(mode_items));
            ImGui::SetNextItemWidth(150);
            ImGui::SliderFloat("Brush Radius##draw", &brush_radius_draw, 1.0f, 50.0f, "%.1f");
            ImGui::Spacing();
            
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            
            // Create the invisible button FIRST to register interaction
            ImGui::InvisibleButton("canvas##draw", canvas_size);
            
            // Now get the canvas position after button is registered
            ImVec2 canvas_pos = ImGui::GetItemRectMin();
            ImVec2 canvas_max = ImGui::GetItemRectMax();
            
            // Draw canvas background
            draw_list->AddRectFilled(canvas_pos, canvas_max, IM_COL32(50, 50, 50, 255));
            draw_list->AddRect(canvas_pos, canvas_max, IM_COL32(150, 150, 150, 255), 2.0f);

            const ImU32 color_solid = ImGui::GetColorU32(g_terrain_solid_color);
            const ImU32 color_air = ImGui::GetColorU32(g_terrain_air_color);

            // Draw pixels
            for (int y = 0; y < bmp_height; y++)
            {
                for (int x = 0; x < bmp_width; x++)
                {
                    const uint8_t pixel = draw_bmp.get_pixel(x, y);
                    ImU32 color = pixel ? color_solid : color_air;

                    ImVec2 pmin = ImVec2(canvas_pos.x + x * cell_size, canvas_pos.y + y * cell_size);
                    ImVec2 pmax = ImVec2(pmin.x + cell_size, pmin.y + cell_size);
                    draw_list->AddRectFilled(pmin, pmax, color);
                    if (cell_size > 3.0f)
                        draw_list->AddRect(pmin, pmax, IM_COL32(80, 80, 80, 128));
                }
            }

            // Handle mouse input for drawing - check if hovering the button
            if (ImGui::IsItemHovered())
            {
                ImGuiIO& io = ImGui::GetIO();
                ImVec2 mouse_pos = io.MousePos;

                // Convert screen to bitmap coordinates
                float rel_x = (mouse_pos.x - canvas_pos.x) / cell_size;
                float rel_y = (mouse_pos.y - canvas_pos.y) / cell_size;
                int center_x = static_cast<int>(rel_x);
                int center_y = static_cast<int>(rel_y);

                if (center_x >= 0 && center_x < bmp_width && center_y >= 0 && center_y < bmp_height)
                {
                    // Paint brush circle when mouse is down
                    if (io.MouseDown[ImGuiMouseButton_Left])
                    {
                        uint8_t paint_value = (draw_mode == 0) ? 1 : 0; // 0=Solid, 1=Air
                        float radius_sq = brush_radius_draw * brush_radius_draw;
                        
                        // Paint all pixels within brush radius
                        for (int y = 0; y < bmp_height; y++)
                        {
                            for (int x = 0; x < bmp_width; x++)
                            {
                                float dx = x - rel_x;
                                float dy = y - rel_y;
                                float dist_sq = dx * dx + dy * dy;
                                
                                if (dist_sq <= radius_sq)
                                {
                                    draw_bmp.get_pixel(x, y) = paint_value;
                                }
                            }
                        }
                    }
                }
            }
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Controls section with better styling
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.6f, 0.4f, 1.0f));
            if (ImGui::Button("Clear (All Air)##draw", ImVec2(150, 0)))
            {
                std::fill(draw_bmp.data.begin(), draw_bmp.data.end(), 0);
            }
            ImGui::SameLine();
            if (ImGui::Button("Fill (All Solid)##draw", ImVec2(150, 0)))
            {
                std::fill(draw_bmp.data.begin(), draw_bmp.data.end(), 1);
            }
            ImGui::PopStyleColor(2);
            
            ImGui::Spacing();
            ImGui::Text("Save and Load to Quadtree:");
            
            // Save/Load section
            static char save_filename_buf[256] = "example";
            ImGui::SetNextItemWidth(200);
            ImGui::InputText("Filename##save", save_filename_buf, IM_ARRAYSIZE(save_filename_buf));
            ImGui::SameLine();
            ImGui::TextDisabled("(.bitmap auto-added)");
            
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.3f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.4f, 0.4f, 1.0f));
            if (ImGui::Button("Save##draw", ImVec2(100, 0)))
            {
                std::string filename = save_filename_buf;

                // Ensure .bitmap extension
                if (filename.find(".bitmap") == std::string::npos)
                {
                    filename += ".bitmap";
                }

                // Try to save in assets/terrain directory - use absolute path resolution
                namespace fs = std::filesystem;
                fs::path probe = fs::current_path();
            
            // Walk up to find the assets directory
            fs::path save_dir;
            for (int i = 0; i < 8; i++)
            {
                fs::path candidate = probe / "assets" / "terrain";
                if (fs::exists(candidate))
                {
                    save_dir = candidate;
                    break;
                }
                if (!probe.has_parent_path()) break;
                probe = probe.parent_path();
            }
            
            if (save_dir.empty())
            {
                // Create assets/terrain if needed
                fs::path assets_dir = fs::current_path() / "assets" / "terrain";
                try
                {
                    fs::create_directories(assets_dir);
                    save_dir = assets_dir;
                }
                catch (...)
                {
                    std::cout << "[ERROR] Could not create assets/terrain directory" << std::endl;
                    ImGui::PopStyleColor(2);
                    ImGui::End();
                    return;
                }
            }
            
            fs::path full_path = save_dir / filename;
            std::string error;
            if (terrain_save_bitmap_file(full_path.generic_string(), draw_bmp.data.data(), bmp_width, bmp_height, error))
            {
                std::cout << "[DRAW] Terrain saved to: " << full_path.generic_string() << std::endl;
            }
            else
            {
                std::cout << "[ERROR] Failed to save terrain: " << error << std::endl;
            }
        }
        ImGui::SameLine();
        
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.6f, 0.4f, 1.0f));
        if (ImGui::Button("Display##draw", ImVec2(100, 0)))
        {
            // Request terrain reload from current draw bitmap (without saving)
            TerrainDraw::request_terrain_reload_from_draw();
            std::cout << "[DRAW] Loading current drawing into quadtree" << std::endl;
        }
        ImGui::PopStyleColor(4);
    }
    
    ImGui::End();
}
