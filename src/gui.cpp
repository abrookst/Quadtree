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

bool gui_get_show_quadtree_overlay()
{
    return g_show_quadtree_overlay;
}

int gui_get_max_depth()
{
    return g_max_depth;
}

void gui_render()
{
    // Demo window toggle
    static bool show_demo_window = false;
    if (show_demo_window)
    {
        ImGui::ShowDemoWindow(&show_demo_window);
    }

    // Sample control panel
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_FirstUseEver);
    ImGui::Begin("Quadtree Control Panel");
    ImGui::Text("Quadtree Visualization");
    ImGui::Separator();
    ImGui::Checkbox("Show Demo Window", &show_demo_window);
    
    static bool show_console = true;
    ImGui::Checkbox("Show Console", &show_console);

    ImGui::Checkbox("Show Quadtree Overlay", &g_show_quadtree_overlay);

    ImGui::Separator();
    ImGui::Text("Terrain Build");
    ImGui::SliderInt("Max Depth", &g_max_depth, 1, 20);
    
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::TextWrapped("Add your quadtree controls here.");
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
