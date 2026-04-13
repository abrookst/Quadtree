#ifndef GUI_H
#define GUI_H

#include <string>
#include <sstream>
#include <queue>
#include <iostream>
#include <vector>
#include <cstdint>

#include "bomb.h"
#include "appcontext.h"

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
void gui_render(AppContext& app);

// Debug overlay toggle (set via ImGui)
bool gui_get_show_quadtree_overlay();

// Terrain build parameter (controlled via ImGui)
int gui_get_max_depth();

// Brush radius for circle destruction (controlled via ImGui)
float gui_get_brush_radius();

// Visualization colors (controlled via ImGui)
ImVec4 gui_get_terrain_solid_color();
ImVec4 gui_get_terrain_air_color();

// Terrain draw mode
namespace TerrainDraw
{
    // Bitmap data for drawing (width * height array of 0=air, 1=solid)
    struct DrawBitmap
    {
        std::vector<uint8_t> data;
        int width = 0;
        int height = 0;
        
        uint8_t& get_pixel(int x, int y)
        {
            return data[y * width + x];
        }
        
        const uint8_t& get_pixel(int x, int y) const
        {
            return data[y * width + x];
        }
    };

    // Get the current draw bitmap
    DrawBitmap& get_draw_bitmap();
    
    // Returns true if in draw mode
    bool is_draw_mode_active();
    
    // Activate/deactivate draw mode with given size
    void set_draw_mode(bool active, int width = 256, int height = 256);
    
    // Requests terrain reload from current draw bitmap (without saving)
    void request_terrain_reload_from_draw();
    
    // Requests terrain reload from saved file
    void request_terrain_reload(const std::string& terrain_file);
    
    // Internal functions for checking reload request
    bool has_reload_request();
    void clear_reload_request();
    std::string get_reload_file();
    bool is_reload_from_draw();
}

#endif // GUI_H
