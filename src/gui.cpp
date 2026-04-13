#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include "imgui.h"

#include "gui.h"
#include "terrain.h"
#include <filesystem>





// Global console buffer storage
static std::string g_console_output;
std::queue<std::string> ConsoleBuffer::s_command_queue;


// Application context structure
struct AppContext
{
    SDL_Window* window;
    SDL_GLContext gl_context;
    std::unique_ptr<Quadtree> terrain;
    std::string loaded_terrain_file;
    int loaded_terrain_depth;
    int last_depth_reload_attempt;
    
    // Cached world bounds for coordinate transformation
    float world_min_x = 0.0f;
    float world_max_x = 0.0f;
    float world_min_y = 0.0f;
    float world_max_y = 0.0f;

    std::vector<Bomb> bombs;
};


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
static ImVec4 g_terrain_solid_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
static ImVec4 g_terrain_air_color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

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

    ImDrawList* draw = ImGui::GetBackgroundDrawList();

    for (auto& bomb : app.bombs) {
            if (bomb.isActive()) {
                bomb.update(1.0f / 60.0f);
                bomb.draw(draw);
            }
        }   
    // Sample control panel
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360, 360), ImGuiCond_FirstUseEver);
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
        ImGui::Text("Colors");
        ImGui::ColorEdit3("Solid (Terrain)", (float*)&g_terrain_solid_color);
        ImGui::ColorEdit3("Air", (float*)&g_terrain_air_color);
        ImGui::Spacing();
    }

    if (ImGui::CollapsingHeader("Terrain", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::PushItemWidth(-1);
        ImGui::SliderInt("Max Depth", &g_max_depth, 1, 20);
        ImGui::SliderFloat("Brush Radius", &g_brush_radius, 1.0f, 500.0f);
        ImGui::PopItemWidth();
        
        ImGui::Spacing();
        static int draw_width = 256;
        static int draw_height = 256;
        ImGui::DragInt("Draw Width##terrain", &draw_width, 1, 64, 1024);
        ImGui::DragInt("Draw Height##terrain", &draw_height, 1, 64, 1024);
        
        if (ImGui::Button(TerrainDraw::is_draw_mode_active() ? "Exit Draw Mode" : "Enter Draw Mode", ImVec2(-1, 0)))
        {
            if (TerrainDraw::is_draw_mode_active())
            {
                TerrainDraw::set_draw_mode(false);
            }
            else
            {
                TerrainDraw::set_draw_mode(true, draw_width, draw_height);
            }
        }
        ImGui::Spacing();
    }

    if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("FPS: %.1f", fps_now);

        // Animated framerate graph (rolling history)
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
        ImGui::InputText("##command", input_buffer, IM_ARRAYSIZE(input_buffer), ImGuiInputTextFlags_EnterReturnsTrue);
        
        ImGui::SameLine();
        if (ImGui::Button("Send"))
        {
            if (input_buffer[0] != '\0')
            {
                std::cout << "> " << input_buffer << std::endl;
                ConsoleBuffer::submit_command(input_buffer);
                input_buffer[0] = '\0';
            }
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Clear##console"))
        {
            ConsoleBuffer::clear_output();
        }
        
        ImGui::End();
    }

    // Terrain draw mode window
    if (TerrainDraw::is_draw_mode_active())
    {
        ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(900, 750), ImGuiCond_FirstUseEver);
        
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
        ImGui::Begin("Terrain Draw Mode");
        ImGui::PopStyleColor();

        TerrainDraw::DrawBitmap& draw_bmp = TerrainDraw::get_draw_bitmap();
        int bmp_width = draw_bmp.width;
        int bmp_height = draw_bmp.height;
        
        // Add some padding and draw title
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.9f, 1.0f, 1.0f));
        ImGui::Text("Draw Terrain - Resolution: %dx%d", bmp_width, bmp_height);
        ImGui::PopStyleColor();
        ImGui::Separator();
        
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
        
        ImGui::SameLine();
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
        ImGui::SameLine();
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
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.9f, 1.0f, 1.0f));
        ImGui::Text("Save and Load to Quadtree:");
        ImGui::PopStyleColor();
        
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

        ImGui::End();
    }
}
