#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <iostream>
#include <cstdlib>
#include <memory>
#include <sstream>
#include <vector>
#include <filesystem>

#ifdef _WIN32
    #include <windows.h>
    #include <io.h>
    #include <fcntl.h>
#endif

#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"
#include "gui.h"
#include "terrain.h"
#include "bomb.h"
#include "appcontext.h"

void gui_render();




AppContext app;










static std::vector<std::string> split_ws(const std::string& s)
{
    std::istringstream iss(s);
    std::vector<std::string> out;
    std::string tok;
    while (iss >> tok) out.push_back(tok);
    return out;
}

static std::string fillstate_string(FillState fs){
    if(fs == FillState::Empty){
        return "Empty";
    }
    if(fs == FillState::Solid){
        return "Solid";
    }
    if(fs == FillState::Mixed){
        return "Mixed";
    }
    return "NULL";
}

static void screen_to_world(float screen_x, float screen_y, const AppContext& app, float& out_world_x, float& out_world_y)
{
    ImGuiViewport* vp = ImGui::GetMainViewport();
    const ImVec2 vp_pos = vp->Pos;
    const ImVec2 vp_size = vp->Size;
    
    float range_x = app.world_max_x - app.world_min_x;
    float range_y = app.world_max_y - app.world_min_y;
    
    if (range_x <= 0.0f || range_y <= 0.0f)
    {
        out_world_x = 0.0f;
        out_world_y = 0.0f;
        return;
    }
    
    // Normalize screen coordinates relative to viewport
    float norm_x = (screen_x - vp_pos.x) / vp_size.x;
    float norm_y = (screen_y - vp_pos.y) / vp_size.y;
    
    // Convert to world coordinates
    out_world_x = app.world_min_x + norm_x * range_x;
    out_world_y = app.world_max_y - norm_y * range_y; // Invert Y like in rendering
}

static void draw_quadtree_leaf_cells(const Quadtree* terrain, bool show_quadtree_overlay, ImU32 solid_color, AppContext& app)
{
    if (!terrain) return;
    const QuadtreeNode* root = terrain->get_root();
    if (!root) return;

    const Bounds& b = root->get_bounds();
    const float min_x = b.get_min_x();
    const float max_x = b.get_max_x();
    const float min_y = b.get_min_y();
    const float max_y = b.get_max_y();

    // Cache world bounds for coordinate transformation
    app.world_min_x = min_x;
    app.world_max_x = max_x;
    app.world_min_y = min_y;
    app.world_max_y = max_y;

    const float range_x = (max_x - min_x);
    const float range_y = (max_y - min_y);
    if (range_x <= 0.0f || range_y <= 0.0f) return;

    ImGuiViewport* vp = ImGui::GetMainViewport();
    const ImVec2 vp_pos = vp->Pos;
    const ImVec2 vp_size = vp->Size;

    auto world_to_screen = [&](float wx, float wy) -> ImVec2
    {
        const float tx = (wx - min_x) / range_x;
        const float ty = (max_y - wy) / range_y; // invert Y
        return ImVec2(vp_pos.x + tx * vp_size.x, vp_pos.y + ty * vp_size.y);
    };

    auto node_bounds_to_screen = [&](const Bounds& nb, ImVec2& out_min, ImVec2& out_max)
    {
        const float nmin_x = nb.get_min_x();
        const float nmax_x = nb.get_max_x();
        const float nmin_y = nb.get_min_y();
        const float nmax_y = nb.get_max_y();

        // screen min is top-left
        out_min = world_to_screen(nmin_x, nmax_y);
        out_max = world_to_screen(nmax_x, nmin_y);
    };

    ImDrawList* dl = ImGui::GetBackgroundDrawList(vp);
    const ImU32 outline = IM_COL32(255, 0, 0, 255);

    // Iterative stack to avoid recursion, but must be dynamic
    // (terrain quadtrees can easily exceed 1024 nodes).
    std::vector<const QuadtreeNode*> stack;
    stack.reserve(4096);
    stack.push_back(root);

    while (!stack.empty())
    {
        const QuadtreeNode* node = stack.back();
        stack.pop_back();
        if (!node) continue;

        if (!node->is_leaf())
        {
            for (int i = 0; i < 4; i++)
            {
                const QuadtreeNode* c = node->get_child(i);
                if (c) stack.push_back(c);
            }
            continue;
        }

        ImVec2 pmin, pmax;
        node_bounds_to_screen(node->get_bounds(), pmin, pmax);

        const FillState s = node->get_state();
        if (s == FillState::Solid)
        {
            dl->AddRectFilled(pmin, pmax, solid_color);
        }

        if (show_quadtree_overlay)
        {
            dl->AddRect(pmin, pmax, outline);
        }
    }
}

// Spawn bomb
void spawnBomb(float x, float y, AppContext& app) {
    float radius = gui_get_bomb_radius();
    float explodeTime = gui_get_bomb_explode_time();
    bool timedExplosion = gui_get_bomb_timed_explosion();
    int hitsToExplode = gui_get_bomb_hits_to_explode();

    Bomb b(x, y, radius, timedExplosion, explodeTime, hitsToExplode, app);
    b.setGravity(gui_get_bomb_gravity());
    b.setBounceStrength(gui_get_bomb_bounce());
    b.setExplodeRadius(gui_get_bomb_explode_radius());
    
    app.bombs.push_back(b);
}


// Forward declarations
bool app_init(AppContext& app);
void app_run(AppContext& app);
void app_cleanup(AppContext& app);

bool app_init(AppContext& app)
{
    std::cout << "[START] Quadtree application starting..." << std::endl;
    std::cout.flush();

    try
    {
        // Initialize SDL
        std::cout << "[INIT] Initializing SDL..." << std::endl;
        std::cout.flush();
        
        if (SDL_Init(SDL_INIT_VIDEO) < 0)
        {
            std::cerr << "[ERROR] Failed to initialize SDL: " << SDL_GetError() << std::endl;
            std::cerr.flush();
            return false;
        }
        std::cout << "[OK] SDL initialized" << std::endl;
        std::cout.flush();

        // GL attributes
        std::cout << "[INIT] Setting OpenGL attributes..." << std::endl;
        std::cout.flush();
        
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        std::cout << "[OK] OpenGL attributes set" << std::endl;
        std::cout.flush();

        // Create window
        std::cout << "[INIT] Creating window..." << std::endl;
        std::cout.flush();
        
        app.window = SDL_CreateWindow(
            "Quadtree Visualization",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            1280, 720,
            SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
        );

        if (!app.window)
        {
            std::cerr << "[ERROR] Failed to create SDL window: " << SDL_GetError() << std::endl;
            std::cerr.flush();
            SDL_Quit();
            return false;
        }
        std::cout << "[OK] SDL window created (1280x720)" << std::endl;
        std::cout.flush();

        // Create OpenGL context
        std::cout << "[INIT] Creating OpenGL context..." << std::endl;
        std::cout.flush();
        
        app.gl_context = SDL_GL_CreateContext(app.window);
        if (!app.gl_context)
        {
            std::cerr << "[ERROR] Failed to create OpenGL context: " << SDL_GetError() << std::endl;
            std::cerr.flush();
            SDL_DestroyWindow(app.window);
            SDL_Quit();
            return false;
        }
        std::cout << "[OK] OpenGL context created" << std::endl;
        std::cout.flush();

        // Enable vsync
        SDL_GL_SetSwapInterval(1);

        // Setup Dear ImGui context
        std::cout << "[INIT] Initializing ImGui..." << std::endl;
        std::cout.flush();
        
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;  // Enable docking
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable keyboard navigation

        // Setup Dear ImGui style
        ImGui::StyleColorsClassic();
        
        ImGuiStyle& style = ImGui::GetStyle();
        style.FrameRounding = 12.0f;
        style.WindowBorderSize = 1.0f;

        // Setup Platform/Renderer backends
        const char* glsl_version = "#version 330";
        ImGui_ImplSDL2_InitForOpenGL(app.window, app.gl_context);
        ImGui_ImplOpenGL3_Init(glsl_version);
        
        // Load default layout from src directory
        std::filesystem::path exe_dir = std::filesystem::current_path();
        std::filesystem::path layout_file = exe_dir.parent_path().parent_path() / "src" / "defaultlayout.ini";
        std::cout << "[DEBUG] Current working directory: " << exe_dir << std::endl;
        std::cout << "[DEBUG] Attempting to load layout from: " << layout_file << std::endl;
        ImGui::LoadIniSettingsFromDisk(layout_file.string().c_str());
        
        std::cout << "[OK] ImGui initialized" << std::endl;
        std::cout << "\n[RUN] ============================================" << std::endl;
        std::cout << "[RUN]  Application Running" << std::endl;
        std::cout << "[RUN]  Press ESC or close window to exit" << std::endl;
        std::cout << "[RUN] ============================================\n" << std::endl;
        std::cout.flush();

        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "\n[EXCEPTION] Initialization failed: " << e.what() << std::endl;
        std::cerr.flush();
        return false;
    }
}

void app_run(AppContext& app)
{
    bool running = true;
    SDL_Event event;

    // Main loop
    while (running)
    {
        // Read in keys
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            
            switch (event.type)
            {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE)
                        running = false;
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == SDL_BUTTON_LEFT && app.terrain)
                    {
                        ImGuiIO& io = ImGui::GetIO();
                        if (!io.WantCaptureMouse)
                        {
                            float world_x, world_y;
                            screen_to_world((float)event.button.x, (float)event.button.y, app, world_x, world_y);
                            
                            // spawn bomb at click
                            spawnBomb((float)event.button.x, (float)event.button.y, app);

                            float brush_radius = gui_get_brush_radius();
                            //call setcircle here
                            
                            //std::cout << "[ACTION] Destroyed circle at (" << world_x << ", " << world_y 
                            //          << ") with radius " << brush_radius << std::endl;
                            //app.terrain->set_circle(brush_radius, world_x, world_y);
                        }
                    }
                    
                    break;
            }
        }

        // Check for console commands
        std::string command = ConsoleBuffer::get_command();
        if (!command.empty())
        {
            std::cout << "[COMMAND] Processing: " << command << std::endl;

            const auto args = split_ws(command);
            if (!args.empty() && (args[0] == "quit" || args[0] == "exit"))
            {
                std::cout << "[RUN] Quit requested" << std::endl;
                running = false;
            }
            else if (!args.empty() && args[0] == "help")
            {
                std::cout << "[HELP] Available commands:" << std::endl;
                std::cout << "  help                      - Show this help" << std::endl;
                std::cout << "  quit | exit               - Quit the application" << std::endl;
                std::cout << "  terrain_list              - Print all filenames in terrain folder" << std::endl;
                std::cout << "  terrain_load <file>       - Load a bitmap terrain file" << std::endl;
                std::cout << "                              (uses GUI Max Depth)" << std::endl;
                std::cout << "  terrain_clear             - Clear the current terrain" << std::endl;
            }
            else if (!args.empty() && args[0] == "terrain_list")
            {
                std::cout << "[TERRAIN] Available terrain files:" << std::endl;
                
                std::filesystem::path dir("assets/terrain");
                std::filesystem::path probe = std::filesystem::current_path();
                bool found = false;
                
                for (int i = 0; i < 8; i++) {
                    std::filesystem::path test_dir = probe / "assets" / "terrain";
                    if (std::filesystem::exists(test_dir) && std::filesystem::is_directory(test_dir)) {
                        dir = test_dir;
                        found = true;
                        break;
                    }
                    if (!probe.has_parent_path()) break;
                    std::filesystem::path parent = probe.parent_path();
                    if (parent == probe) break;
                    probe = parent;
                }
                
                if (found) {
                    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                        if (entry.is_regular_file() && entry.path().extension() == ".bitmap") {
                            std::cout << "  " << entry.path().filename().string() << std::endl;
                        }
                    }
                } else {
                    std::cout << "  [ERROR] Directory 'assets/terrain' not found." << std::endl;
                }
            }
            else if (!args.empty() && args[0] == "terrain_load")
            {
                const std::string file = (args.size() >= 2) ? args[1] : "default.bitmap";

                std::unique_ptr<Quadtree> loaded;
                std::string error;
                if (!terrain_load_bitmap_file_into_quadtree(file, loaded, gui_get_max_depth(), error))
                {
                    std::cout << "[TERRAIN] Load failed: " << error << std::endl;
                }
                else
                {
                    app.terrain = std::move(loaded);
                    app.loaded_terrain_file = file;
                    app.loaded_terrain_depth = gui_get_max_depth();
                    app.last_depth_reload_attempt = app.loaded_terrain_depth;
                    
                    // Set display resolution to bitmap dimensions
                    int bmp_width, bmp_height;
                    std::string dim_error;
                    if (terrain_get_bitmap_dimensions(file, bmp_width, bmp_height, dim_error))
                    {
                        gui_set_terrain_resolution(bmp_width, bmp_height);
                    }
                    
                    std::cout << "[TERRAIN] Loaded bitmap into quadtree: nodes=" << app.terrain->get_node_count()
                              << " depth=" << app.terrain->get_depth() << std::endl;
                }
            }
            else if (!args.empty() && args[0] == "terrain_clear")
            {
                if (app.terrain)
                {
                    int terrain_w = gui_get_terrain_display_width();
                    int terrain_h = gui_get_terrain_display_height();
                    const Bounds bounds(0.0f, 0.0f, static_cast<float>(terrain_w), static_cast<float>(terrain_h));
                    app.terrain = std::make_unique<Quadtree>(bounds, gui_get_max_depth(), false);
                    std::cout << "[TERRAIN] Terrain cleared" << std::endl;
                }
                else
                {
                    std::cout << "[TERRAIN] No terrain to clear" << std::endl;
                }
            }
            else
            {
                std::cout << "[ERROR] Unknown command: " << (args.empty() ? command : args[0]) << std::endl;
            }
        }

        // Check for terrain reload request from draw mode
        if (TerrainDraw::has_reload_request())
        {
            bool from_draw = TerrainDraw::is_reload_from_draw();
            TerrainDraw::clear_reload_request();

            std::unique_ptr<Quadtree> loaded;
            
            if (from_draw)
            {
                // Load from draw buffer directly
                const TerrainDraw::DrawBitmap& draw_bmp = TerrainDraw::get_draw_bitmap();
                if (terrain_build_quadtree_from_bitmap(
                    draw_bmp.data.data(),
                    draw_bmp.width,
                    draw_bmp.height,
                    gui_get_max_depth(),
                    loaded))
                {
                    app.terrain = std::move(loaded);
                    app.loaded_terrain_file = "[drawn]";
                    app.loaded_terrain_depth = gui_get_max_depth();
                    app.last_depth_reload_attempt = app.loaded_terrain_depth;
                    
                    // Set display resolution to draw bitmap dimensions
                    gui_set_terrain_resolution(draw_bmp.width, draw_bmp.height);
                    
                    std::cout << "[TERRAIN] Loaded drawn terrain into quadtree: nodes=" << app.terrain->get_node_count()
                              << " depth=" << app.terrain->get_depth() << std::endl;
                }
            }
            else
            {
                // Load from file
                const std::string file = TerrainDraw::get_reload_file();
                std::string error;
                if (!terrain_load_bitmap_file_into_quadtree(file, loaded, gui_get_max_depth(), error))
                {
                    std::cout << "[TERRAIN] Load failed: " << error << std::endl;
                }
                else
                {
                    app.terrain = std::move(loaded);
                    app.loaded_terrain_file = file;
                    app.loaded_terrain_depth = gui_get_max_depth();
                    app.last_depth_reload_attempt = app.loaded_terrain_depth;
                    
                    // Set display resolution to bitmap dimensions
                    int bmp_width, bmp_height;
                    std::string dim_error;
                    if (terrain_get_bitmap_dimensions(file, bmp_width, bmp_height, dim_error))
                    {
                        gui_set_terrain_resolution(bmp_width, bmp_height);
                    }
                    
                    std::cout << "[TERRAIN] Loaded drawn terrain into quadtree: nodes=" << app.terrain->get_node_count()
                              << " depth=" << app.terrain->get_depth() << std::endl;
                }
            }
        }

        // Check for terrain destruction request from terrain window clicks
        if (TerrainDestruct::has_destruction_request())
        {
            auto req = TerrainDestruct::get_and_clear_request();
            if (app.terrain)
            {
                // Spawn bomb where clicked in terrain window
                spawnBomb(req.world_x, req.world_y, app);
            }
        }

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // Set current terrain for GUI display
        gui_set_current_terrain(app.terrain.get());

        // Render GUI
        gui_render(app);

        // If the depth slider changes, rebuild the quadtree by reloading the last loaded file.
        const int requested_depth = gui_get_max_depth();
        if (app.terrain && !app.loaded_terrain_file.empty() && requested_depth != app.loaded_terrain_depth)
        {
            // Only attempt a reload once per distinct requested depth.
            if (requested_depth != app.last_depth_reload_attempt)
            {
                app.last_depth_reload_attempt = requested_depth;

                std::unique_ptr<Quadtree> reloaded;
                std::string error;
                if (!terrain_load_bitmap_file_into_quadtree(app.loaded_terrain_file, reloaded, requested_depth, error))
                {
                    std::cout << "[TERRAIN] Rebuild failed: " << error << std::endl;
                }
                else
                {
                    app.terrain = std::move(reloaded);
                    app.loaded_terrain_depth = requested_depth;
                    std::cout << "[TERRAIN] Rebuilt quadtree: nodes=" << app.terrain->get_node_count()
                              << " depth=" << app.terrain->get_depth() << std::endl;
                }
            }
        }

        // Update world bounds for coordinate transformation (needed for click detection)
        if (app.terrain && app.terrain->get_root())
        {
            const Bounds& b = app.terrain->get_root()->get_bounds();
            app.world_min_x = b.get_min_x();
            app.world_max_x = b.get_max_x();
            app.world_min_y = b.get_min_y();
            app.world_max_y = b.get_max_y();
        }

        // Rendering
        ImGui::Render();
        
        // Clear and render scene
        const ImVec4 air_col = gui_get_terrain_air_color();
        glClearColor(air_col.x, air_col.y, air_col.z, air_col.w);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Render ImGui draw data
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Swap buffers
        SDL_GL_SwapWindow(app.window);
    }
}

void app_cleanup(AppContext& app)
{
    std::cout << "\n[CLEANUP] Shutting down..." << std::endl;
    std::cout.flush();

    // Cleanup ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    // Cleanup SDL
    SDL_GL_DeleteContext(app.gl_context);
    SDL_DestroyWindow(app.window);
    SDL_Quit();

    std::cout << "[EXIT] Application closed cleanly" << std::endl;
    std::cout.flush();
}

int main(int argc, char* argv[])
{
    // Setup console capture to ImGui
    static ConsoleBuffer console_buffer;
    std::cout.rdbuf(&console_buffer);
    std::cerr.rdbuf(&console_buffer);

    AppContext app = { nullptr, nullptr, nullptr, "", 0, 0 };

    try
    {
        if (!app_init(app))
            return -1;
        app_run(app);
        app_cleanup(app);
    }
    catch (const std::exception& e)
    {
        std::cerr << "\n[EXCEPTION] Unhandled exception: " << e.what() << std::endl;
        std::cerr.flush();
        return -1;
    }
    catch (...)
    {
        std::cerr << "\n[EXCEPTION] Unknown error occurred" << std::endl;
        std::cerr.flush();
        return -1;
    }

    return 0;
}
