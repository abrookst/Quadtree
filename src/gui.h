#ifndef GUI_H
#define GUI_H

#include <string>
#include <sstream>
#include <queue>
#include <iostream>

struct ImVec4;

// Console buffer for capturing cout
class ConsoleBuffer : public std::streambuf
{
public:
    static std::string& get_output();
    static void clear_output();
    static std::string get_command();
    static void submit_command(const std::string& cmd);

protected:
    virtual int overflow(int c) override;

private:
    static std::queue<std::string> s_command_queue;
};

// GUI rendering function
void gui_render();

// Debug overlay toggle (set via ImGui)
bool gui_get_show_quadtree_overlay();

// Terrain build parameter (controlled via ImGui)
int gui_get_max_depth();

// Brush radius for circle destruction (controlled via ImGui)
float gui_get_brush_radius();

// Visualization colors (controlled via ImGui)
ImVec4 gui_get_terrain_solid_color();
ImVec4 gui_get_terrain_air_color();

#endif // GUI_H
