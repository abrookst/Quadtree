#include "imgui.h"

#include "gui.h"

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
static ImVec4 g_terrain_solid_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
static ImVec4 g_terrain_air_color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

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

void gui_render()
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
}
